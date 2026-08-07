#pragma once

#include "type.h"

struct LSQ_Entry {
    bool  busy;
    bool  is_store;
    bool  addr_ready;
    bool  data_ready;
    bool  completed;
    u32   addr;
    u32   value;
    int   rob_idx;
    int   tag;
    Instr op;
    int   mem_delay;
};

class LoadStoreQueue {
    static const int MAX_SIZE = 32;
    LSQ_Entry old_[MAX_SIZE];
    LSQ_Entry new_[MAX_SIZE];
    int size_;

    static LSQ_Entry empty_entry();

  public:
    LoadStoreQueue(int size);

    void update();

    int  size() const;
    const LSQ_Entry& entry_old(int idx) const;
    LSQ_Entry& entry_new(int idx);

    bool full_old() const;

    bool has_older_unresolved_store_old(int tag) const;

    bool forward_old(u32 addr, int tag, u32& result) const;

    int alloc_new(int rob_idx, bool is_store);
    int alloc_load_new(int rob_idx);
    int alloc_store_new(int rob_idx);

    void set_tag_new(int idx, int tag);
    void set_addr_new(int idx, u32 addr, Instr op);
    void set_store_data_new(int idx, u32 data, Instr op);
    void mark_store_ready_new(int idx);
    int tick_mem_delay_new();
    void load_done_new(int idx, u32 value);
    void free_by_rob_new(int rob_idx);

    void flush_younger_than_new(int branch_tag);
};
