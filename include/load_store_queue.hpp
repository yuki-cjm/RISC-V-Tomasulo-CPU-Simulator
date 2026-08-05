#pragma once

#include "type.h"

// ============================================================================
// LSQ_Entry: Load/Store 队列条目
// ============================================================================
struct LSQ_Entry {
    bool  busy;
    bool  is_store;
    bool  addr_ready;
    bool  data_ready;
    bool  completed;     // load 已得到值 / store 地址数据已就绪
    u32   addr;
    u32   value;
    int   rob_idx;
    int   tag;
    Instr op;
    int   mem_delay;
};

// ============================================================================
// LoadStoreQueue: 加载/存储队列，线性数组管理
// 已完成的 store 在 commit 写入内存后由 free_by_rob_n 释放。
// LSQ 满时阻塞发射，不回收未提交的 store。
// ============================================================================
class LoadStoreQueue {
    static const int MAX_SZ = 32;
    LSQ_Entry old_[MAX_SZ];
    LSQ_Entry new_[MAX_SZ];
    int cap_;

    static LSQ_Entry empty_entry();

  public:
    LoadStoreQueue(int cap);

    void snap();
    void upd();

    int  size() const;
    const LSQ_Entry& entry_o(int idx) const;
    LSQ_Entry& entry_n(int idx);

    // LSQ 满 → 没有空闲槽位
    bool full_o() const;

    // 是否有更早的 store 地址未解析
    bool has_older_unresolved_store_o(int tag) const;

    // store-to-load forwarding：找 tag 最大（最新）的匹配 store
    bool forward_o(u32 addr, int tag, u32& result) const;

    // 分配新条目（仅找空闲槽位，不回收未提交的 store）
    int alloc_n(int rob_idx, bool is_store);
    int alloc_load_n(int rob_idx);
    int alloc_store_n(int rob_idx);

    void set_tag_n(int idx, int tag);
    void set_addr_n(int idx, u32 addr);
    void set_store_data_n(int idx, u32 data, Instr op);
    void mark_store_ready_n(int idx);

    // 递减 load 的内存延迟，返回刚减到 0 的条目索引
    int tick_mem_delay_n();

    void load_done_n(int idx, u32 value);

    void free_by_rob_n(int rob_idx);
    void flush_by_rob_n(int rob_idx);
    void flush_all_n();

    // 分支预测失败时，只清除比分支年轻的访存条目，保留更老的未提交条目。
    void flush_younger_than_n(int branch_tag);
};
