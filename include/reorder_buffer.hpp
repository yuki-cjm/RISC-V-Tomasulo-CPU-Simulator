#pragma once

#include "type.h"

// ============================================================================
// ROB_Entry: 重排序缓冲区条目
// ============================================================================
struct ROB_Entry {
    bool  busy;
    bool  ready;         // 结果已就绪，可以提交
    bool  flushed;       // 被分支误预测刷掉
    u8    dest_reg;      // 目标寄存器（0 表示无）
    int   dest_tag;      // 本指令标签
    u32   value;         // 结果值
    u32   pc;            // 指令 PC
    bool  branch;        // 是分支指令
    bool  jump;          // 是跳转指令
    bool  is_store;      // 是 store 指令
    bool  pred_taken;    // 分支预测方向
    u32   branch_target; // 分支目标地址
    bool  store_done;    // store 指令已执行完毕（等待提交时写内存）
};

// ============================================================================
// ReorderBuffer: 重排序缓冲区，环形队列
// ============================================================================
class ReorderBuffer {
    static const int MAX_SZ = 64;
    ROB_Entry old_[MAX_SZ];
    ROB_Entry new_[MAX_SZ];
    int head_o_, tail_o_, cnt_o_;
    int head_n_, tail_n_, cnt_n_;
    int cap_;

    static ROB_Entry empty_entry() {
        return {false, false, false, 0, -1, 0, 0, false, false, false, false, 0, false};
    }

  public:
    ReorderBuffer(int cap) : cap_(cap) {
        head_o_ = tail_o_ = cnt_o_ = 0;
        head_n_ = tail_n_ = cnt_n_ = 0;
        for (int i = 0; i < MAX_SZ; ++i)
            old_[i] = new_[i] = empty_entry();
    }

    // ---- 状态管理 ----
    void snap() {
        for (int i = 0; i < cap_; ++i)
            new_[i] = old_[i];
        head_n_ = head_o_; tail_n_ = tail_o_; cnt_n_ = cnt_o_;
    }

    void upd() {
        for (int i = 0; i < cap_; ++i)
            old_[i] = new_[i];
        head_o_ = head_n_; tail_o_ = tail_n_; cnt_o_ = cnt_n_;
    }

    // ---- 读取旧状态 ----
    bool full_o()  const { return cnt_o_ == cap_; }
    bool empty_o() const { return cnt_o_ == 0; }
    int  head_o()  const { return head_o_; }
    int  count_o() const { return cnt_o_; }
    int  cap()     const { return cap_; }

    const ROB_Entry& entry_o(int idx) const { return old_[idx]; }
    const ROB_Entry& entry_cur(int idx) const { return new_[idx]; }

    // 按标签查找
    int find_by_tag_o(int tag) const {
        for (int i = 0; i < cap_; ++i) {
            if (old_[i].busy && !old_[i].flushed && old_[i].dest_tag == tag)
                return i;
        }
        return -1;
    }

    // 检查标签是否在 ROB 中有效（未被 flush）
    bool tag_valid_o(int tag) const {
        return find_by_tag_o(tag) >= 0;
    }

    // 获取队首条目
    int head_entry_o() const {
        if (cnt_o_ == 0) return -1;
        return head_o_;
    }

    // 遍历有效条目（从 head 开始第 n 个）
    int nth_valid_o(int n) const {
        int found = 0;
        for (int i = 0; i < cap_; ++i) {
            int idx = (head_o_ + i) % cap_;
            if (old_[idx].busy && !old_[idx].flushed) {
                if (found == n) return idx;
                found++;
            }
        }
        return -1;
    }

    int valid_count_o() const {
        int c = 0;
        for (int i = 0; i < cap_; ++i) {
            int idx = (head_o_ + i) % cap_;
            if (old_[idx].busy && !old_[idx].flushed) c++;
        }
        return c;
    }

    // 查找队首第一个未 flush 的条目索引
    int first_valid_o() const {
        for (int i = 0; i < cnt_o_; ++i) {
            int idx = (head_o_ + i) % cap_;
            if (old_[idx].busy && !old_[idx].flushed)
                return idx;
        }
        return -1;
    }

    // ---- 写入新状态（issue：分配条目） ----
    int alloc_n() {
        if (cnt_n_ == cap_) return -1;
        int idx = tail_n_;
        new_[idx] = empty_entry();
        new_[idx].busy = true;
        tail_n_ = (tail_n_ + 1) % cap_;
        cnt_n_++;
        return idx;
    }

    void set_dest_n(int idx, u8 rd, int tag) {
        new_[idx].dest_reg = rd;
        new_[idx].dest_tag = tag;
    }

    void set_pc_n(int idx, u32 pc) {
        new_[idx].pc = pc;
    }

    void set_branch_n(int idx, bool pred, bool br, bool jp, u32 tgt) {
        new_[idx].pred_taken = pred;
        new_[idx].branch = br;
        new_[idx].jump = jp;
        new_[idx].branch_target = tgt;
    }

    void set_store_n(int idx) {
        new_[idx].is_store = true;
    }

    // ---- 写入新状态（writeback：标记结果就绪） ----
    void write_result_n(int idx, u32 val) {
        new_[idx].ready = true;
        new_[idx].value = val;
    }

    // ---- 写入新状态（execute：标记 store 已完成） ----
    void mark_store_done_n(int idx) {
        new_[idx].store_done = true;
    }

    // ---- 写入新状态（execute/mispredict：刷掉指定位置之后的所有条目） ----
    void flush_after_n(int idx) {
        // 标记从 idx 之后到队尾的所有条目为 flushed
        int cur = (idx + 1) % cap_;
        while (cur != tail_n_) {
            new_[cur].flushed = true;
            cur = (cur + 1) % cap_;
        }
    }

    // 刷掉所有
    void flush_all_n() {
        for (int i = 0; i < cap_; ++i)
            new_[i] = empty_entry();
        head_n_ = tail_n_ = cnt_n_ = 0;
    }

    bool is_flushed_n(int idx) const {
        return new_[idx].flushed;
    }

    // ---- 写入新状态（commit：提交队首） ----
    void commit_head_n() {
        new_[head_n_].busy = false;
        head_n_ = (head_n_ + 1) % cap_;
        cnt_n_--;
    }

    ROB_Entry& entry_n(int idx) { return new_[idx]; }
};
