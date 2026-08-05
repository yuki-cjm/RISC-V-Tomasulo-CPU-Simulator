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

    static ROB_Entry empty_entry();

  public:
    ReorderBuffer(int cap);

    // ---- 状态管理 ----
    void snap();
    void upd();

    // ---- 读取旧状态 ----
    bool full_o()  const;
    bool empty_o() const;
    int  head_o()  const;
    int  count_o() const;
    int  cap()     const;

    const ROB_Entry& entry_o(int idx) const;
    const ROB_Entry& entry_cur(int idx) const;

    // 按标签查找
    int find_by_tag_o(int tag) const;

    // 检查标签是否在 ROB 中有效（未被 flush）
    bool tag_valid_o(int tag) const;

    // 获取队首条目
    int head_entry_o() const;

    // 遍历有效条目（从 head 开始第 n 个）
    int nth_valid_o(int n) const;

    int valid_count_o() const;

    // 查找队首第一个未 flush 的条目索引
    int first_valid_o() const;

    // ---- 写入新状态（issue：分配条目） ----
    int alloc_n();

    void set_dest_n(int idx, u8 rd, int tag);

    void set_pc_n(int idx, u32 pc);

    void set_branch_n(int idx, bool pred, bool br, bool jp, u32 tgt);

    void set_store_n(int idx);

    // ---- 写入新状态（writeback：标记结果就绪） ----
    void write_result_n(int idx, u32 val);

    // ---- 写入新状态（execute：标记 store 已完成） ----
    void mark_store_done_n(int idx);

    // ---- 写入新状态（execute/mispredict：刷掉指定位置之后的所有条目） ----
    void flush_after_n(int idx);

    // 刷掉所有
    void flush_all_n();

    bool is_flushed_n(int idx) const;

    // ---- 写入新状态（commit：提交队首） ----
    void commit_head_n();

    ROB_Entry& entry_n(int idx);
};
