#include "load_store_queue.hpp"

LSQ_Entry LoadStoreQueue::empty_entry() {
    return {false, false, false, false, false, 0, 0, -1, -1,
            Instr::UNKNOWN, 0};
}

LoadStoreQueue::LoadStoreQueue(int cap) : cap_(cap) {
    for (int i = 0; i < MAX_SZ; ++i)
        old_[i] = new_[i] = empty_entry();
}

void LoadStoreQueue::snap() {
    for (int i = 0; i < cap_; ++i)
        new_[i] = old_[i];
}

void LoadStoreQueue::upd() {
    for (int i = 0; i < cap_; ++i)
        old_[i] = new_[i];
}

int LoadStoreQueue::size() const { return cap_; }

const LSQ_Entry& LoadStoreQueue::entry_o(int idx) const { return old_[idx]; }

LSQ_Entry& LoadStoreQueue::entry_n(int idx) { return new_[idx]; }

bool LoadStoreQueue::full_o() const {
    for (int i = 0; i < cap_; ++i)
        if (!old_[i].busy) return false;
    return true;
}

bool LoadStoreQueue::has_older_unresolved_store_o(int tag) const {
    for (int i = 0; i < cap_; ++i) {
        const LSQ_Entry& e = old_[i];
        if (e.busy && e.is_store && e.tag >= 0 && e.tag < tag
            && !e.addr_ready)
            return true;
    }
    return false;
}

bool LoadStoreQueue::forward_o(u32 addr, int tag, u32& result) const {
    int  best_tag = -1;
    bool found    = false;
    for (int i = 0; i < cap_; ++i) {
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

int LoadStoreQueue::alloc_n(int rob_idx, bool is_store) {
    for (int i = 0; i < cap_; ++i) {
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

int LoadStoreQueue::alloc_load_n(int rob_idx) { return alloc_n(rob_idx, false); }

int LoadStoreQueue::alloc_store_n(int rob_idx) { return alloc_n(rob_idx, true); }

void LoadStoreQueue::set_tag_n(int idx, int tag) {
    new_[idx].tag = tag;
}

void LoadStoreQueue::set_addr_n(int idx, u32 addr) {
    new_[idx].addr = addr;
    new_[idx].addr_ready = true;
    if (!new_[idx].is_store)
        new_[idx].mem_delay = MEM_LATENCY;
}

void LoadStoreQueue::set_store_data_n(int idx, u32 data, Instr op) {
    new_[idx].value = data;
    new_[idx].op = op;
    new_[idx].data_ready = true;
}

void LoadStoreQueue::mark_store_ready_n(int idx) {
    new_[idx].completed = true;
}

int LoadStoreQueue::tick_mem_delay_n() {
    for (int i = 0; i < cap_; ++i) {
        if (!old_[i].busy || !new_[i].busy || old_[i].is_store) continue;
        if (new_[i].tag != old_[i].tag) continue;
        if (!old_[i].addr_ready || old_[i].completed) continue;
        if (old_[i].mem_delay > 0) {
            new_[i].mem_delay--;
            if (new_[i].mem_delay == 0) return i;
        }
    }
    return -1;
}

void LoadStoreQueue::load_done_n(int idx, u32 value) {
    new_[idx].completed = true;
    new_[idx].value = value;
}

void LoadStoreQueue::free_by_rob_n(int rob_idx) {
    for (int i = 0; i < cap_; ++i) {
        if (new_[i].busy && new_[i].rob_idx == rob_idx) {
            new_[i] = empty_entry();
            return;
        }
    }
}

void LoadStoreQueue::flush_by_rob_n(int rob_idx) {
    free_by_rob_n(rob_idx);
}

void LoadStoreQueue::flush_all_n() {
    for (int i = 0; i < cap_; ++i)
        new_[i] = empty_entry();
}

void LoadStoreQueue::flush_younger_than_n(int branch_tag) {
    for (int i = 0; i < cap_; ++i) {
        if (new_[i].busy && new_[i].tag > branch_tag)
            new_[i] = empty_entry();
    }
}