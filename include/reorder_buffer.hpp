#pragma once

#include "type.h"

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
    bool  pred_taken;    // 分支预测方向（最终选择）
    u32   branch_target; // 分支目标地址
    bool  store_done;    // store 指令已执行完毕（等待提交时写内存）
    u32   ghr_snapshot;  // 分支预测时的 GHR 值（用于 PHT 训练 & 误预测修复）
    bool  bimodal_pred;    // Bimodal 预测器的独立预测
    bool  gshare_pred;   // Gshare 预测器的独立预测
};

class ReorderBuffer {
    static const int MAX_SIZE = 64;
    ROB_Entry old_[MAX_SIZE];
    ROB_Entry new_[MAX_SIZE];
    int head_old_, tail_old_, cnt_old_;
    int head_new_, tail_new_, cnt_new_;
    int cap_;

    static ROB_Entry empty_entry();

  public:
    ReorderBuffer(int cap);

    void update();

    bool full_old()  const;
    bool empty_old() const;
    int  head_old()  const;
    int  count_old() const;
    int  cap()     const;

    const ROB_Entry& entry_old(int idx) const;
    const ROB_Entry& entry_cur(int idx) const;

    // 按标签查找
    int find_by_tag_old(int tag) const;

    // 检查标签是否在 ROB 中有效(未被 flush)
    bool tag_valid_old(int tag) const;

    // 获取队首条目
    int head_entry_old() const;

    // 遍历有效条目(从 head 开始第 n 个)
    int nth_valid_old(int n) const;

    int valid_count_old() const;

    // 查找队首第一个未 flush 的条目索引
    int first_valid_old() const;

    // issue：分配条目
    int alloc_new();

    void set_dest_new(int idx, u8 rd, int tag);

    void set_pc_new(int idx, u32 pc);

    void set_branch_new(int idx, bool pred, bool br, bool jp, u32 tgt);

    void set_store_new(int idx);

    void set_ghr_snapshot_new(int idx, u32 ghr);

    void set_bimodal_pred_new(int idx, bool pred);

    void set_gshare_pred_new(int idx, bool pred);

    // writeback：标记结果就绪
    void write_result_new(int idx, u32 val);

    // execute：标记 store 已完成
    void mark_store_done_new(int idx);

    // execute/mispredict：刷掉指定位置之后的所有条目
    void flush_after_new(int idx);

    // 刷掉所有
    void flush_all_new();

    bool is_flushed_new(int idx) const;

    // commit：提交队首
    void commit_head_new();

    ROB_Entry& entry_new(int idx);
};
