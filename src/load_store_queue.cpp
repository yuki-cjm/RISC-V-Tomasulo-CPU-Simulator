#include "load_store_queue.hpp"

LSQ_Entry LoadStoreQueue::empty_entry() {
    return {false, false, false, false, false, 0, 0, -1, -1,
            Instr::UNKNOWN, 0};
}

LoadStoreQueue::LoadStoreQueue(int size) : size_(size) {
    for (int i = 0; i < MAX_SIZE; ++i)
        old_[i] = new_[i] = empty_entry();
}

void LoadStoreQueue::update() {
    for (int i = 0; i < size_; ++i)
        old_[i] = new_[i];
}

int LoadStoreQueue::size() const { return size_; }

const LSQ_Entry& LoadStoreQueue::entry_old(int idx) const { return old_[idx]; }

LSQ_Entry& LoadStoreQueue::entry_new(int idx) { return new_[idx]; }

bool LoadStoreQueue::full_old() const {
    for (int i = 0; i < size_; ++i) {
        if (!old_[i].busy) {
            return false;
        }
    }
    return true;
}

bool LoadStoreQueue::has_older_unresolved_store_old(int tag) const {
    for (int i = 0; i < size_; ++i) {
        const LSQ_Entry& e = old_[i];
        if (e.busy && e.is_store && e.tag >= 0 && e.tag < tag
            && !e.addr_ready) {
            return true;
        }
    }
    return false;
}

bool LoadStoreQueue::forward_old(u32 addr, int tag, u32& result) const {
    int  best_tag = -1;
    bool found    = false;
    for (int i = 0; i < size_; ++i) {
        const LSQ_Entry& e = old_[i];
        if (!e.busy || e.tag < 0 || e.tag >= tag) continue;
        if (e.is_store && e.addr_ready && e.addr == addr && e.data_ready) {
            if (e.tag > best_tag) {
                best_tag = e.tag;
                result   = e.value;
                found    = true;
            }
        }
    }
    return found;
}

int LoadStoreQueue::alloc_new(int rob_idx, bool is_store) {
    for (int i = 0; i < size_; ++i) {
        if (!old_[i].busy && !new_[i].busy) {
            new_[i] = empty_entry();
            new_[i].busy = true;
            new_[i].is_store = is_store;
            new_[i].rob_idx = rob_idx;
            return i;
        }
    }
    return -1;
}

int LoadStoreQueue::alloc_load_new(int rob_idx) {
    return alloc_new(rob_idx, false);
}

int LoadStoreQueue::alloc_store_new(int rob_idx) {
    return alloc_new(rob_idx, true);
}

void LoadStoreQueue::set_tag_new(int idx, int tag) {
    new_[idx].tag = tag;
}

void LoadStoreQueue::set_addr_new(int idx, u32 addr, Instr op) {
    new_[idx].addr = addr;
    new_[idx].addr_ready = true;
    new_[idx].op = op;
    new_[idx].mem_delay = MEM_LATENCY;
}

void LoadStoreQueue::set_store_data_new(int idx, u32 data, Instr op) {
    new_[idx].value = data;
    new_[idx].op = op;
    new_[idx].data_ready = true;
}

void LoadStoreQueue::mark_store_ready_new(int idx) {
    new_[idx].completed = true;
}

int LoadStoreQueue::tick_mem_delay_new() {
    for (int i = 0; i < size_; ++i) {
        if (!old_[i].busy || !new_[i].busy) continue;
        if (new_[i].tag != old_[i].tag) continue;
        if (old_[i].completed) continue;
        if (!old_[i].addr_ready) continue;
        if (old_[i].is_store && !old_[i].data_ready) continue;
        if (old_[i].mem_delay > 0) {
            new_[i].mem_delay--;
            if (new_[i].mem_delay == 0) {
                if (old_[i].is_store)
                    new_[i].completed = true;
                return i;
            }
        }
    }
    return -1;
}

void LoadStoreQueue::load_done_new(int idx, u32 value) {
    new_[idx].completed = true;
    new_[idx].value = value;
}

void LoadStoreQueue::free_by_rob_new(int rob_idx) {
    for (int i = 0; i < size_; ++i) {
        if (new_[i].busy && new_[i].rob_idx == rob_idx) {
            new_[i] = empty_entry();
            return;
        }
    }
}

void LoadStoreQueue::flush_younger_than_new(int branch_tag) {
    for (int i = 0; i < size_; ++i) {
        if (new_[i].busy && new_[i].tag > branch_tag)
            new_[i] = empty_entry();
    }
}