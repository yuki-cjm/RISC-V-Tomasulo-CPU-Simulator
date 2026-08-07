#include "reorder_buffer.hpp"

ROB_Entry ReorderBuffer::empty_entry() {
    return {false, false, false, 0, -1, 0, 0,
            false, false, false, false, 0, false, 0, false, false};
}

ReorderBuffer::ReorderBuffer(int cap) : cap_(cap) {
    head_old_ = tail_old_ = cnt_old_ = 0;
    head_new_ = tail_new_ = cnt_new_ = 0;
    for (int i = 0; i < MAX_SIZE; ++i)
        old_[i] = new_[i] = empty_entry();
}

void ReorderBuffer::update() {
    for (int i = 0; i < cap_; ++i)
        old_[i] = new_[i];
    head_old_ = head_new_;
    tail_old_ = tail_new_;
    cnt_old_ = cnt_new_;
}

bool ReorderBuffer::full_old() const { return cnt_old_ == cap_; }

bool ReorderBuffer::empty_old() const { return cnt_old_ == 0; }

int ReorderBuffer::head_old() const { return head_old_; }

int ReorderBuffer::count_old() const { return cnt_old_; }

int ReorderBuffer::cap() const { return cap_; }

const ROB_Entry& ReorderBuffer::entry_old(int idx) const { return old_[idx]; }

const ROB_Entry& ReorderBuffer::entry_cur(int idx) const { return new_[idx]; }

int ReorderBuffer::find_by_tag_old(int tag) const {
    for (int i = 0; i < cap_; ++i) {
        if (old_[i].busy && !old_[i].flushed && old_[i].dest_tag == tag)
            return i;
    }
    return -1;
}

bool ReorderBuffer::tag_valid_old(int tag) const {
    return find_by_tag_old(tag) >= 0;
}

int ReorderBuffer::head_entry_old() const {
    if (cnt_old_ == 0) return -1;
    return head_old_;
}

int ReorderBuffer::nth_valid_old(int n) const {
    int found = 0;
    for (int i = 0; i < cap_; ++i) {
        int idx = (head_old_ + i) % cap_;
        if (old_[idx].busy && !old_[idx].flushed) {
            if (found == n) return idx;
            found++;
        }
    }
    return -1;
}

int ReorderBuffer::valid_count_old() const {
    int c = 0;
    for (int i = 0; i < cap_; ++i) {
        int idx = (head_old_ + i) % cap_;
        if (old_[idx].busy && !old_[idx].flushed) c++;
    }
    return c;
}

int ReorderBuffer::first_valid_old() const {
    for (int i = 0; i < cnt_old_; ++i) {
        int idx = (head_old_ + i) % cap_;
        if (old_[idx].busy && !old_[idx].flushed)
            return idx;
    }
    return -1;
}

int ReorderBuffer::alloc_new() {
    if (cnt_new_ == cap_) return -1;
    int idx = tail_new_;
    new_[idx] = empty_entry();
    new_[idx].busy = true;
    tail_new_ = (tail_new_ + 1) % cap_;
    cnt_new_++;
    return idx;
}

void ReorderBuffer::set_dest_new(int idx, u8 rd, int tag) {
    new_[idx].dest_reg = rd;
    new_[idx].dest_tag = tag;
}

void ReorderBuffer::set_pc_new(int idx, u32 pc) {
    new_[idx].pc = pc;
}

void ReorderBuffer::set_branch_new(int idx, bool pred, bool br, bool jp, u32 tgt) {
    new_[idx].pred_taken = pred;
    new_[idx].branch = br;
    new_[idx].jump = jp;
    new_[idx].branch_target = tgt;
}

void ReorderBuffer::set_store_new(int idx) {
    new_[idx].is_store = true;
}

void ReorderBuffer::set_ghr_snapshot_new(int idx, u32 ghr) {
    new_[idx].ghr_snapshot = ghr;
}

void ReorderBuffer::set_bimodal_pred_new(int idx, bool pred) {
    new_[idx].bimodal_pred = pred;
}

void ReorderBuffer::set_gshare_pred_new(int idx, bool pred) {
    new_[idx].gshare_pred = pred;
}

void ReorderBuffer::write_result_new(int idx, u32 val) {
    new_[idx].ready = true;
    new_[idx].value = val;
}

void ReorderBuffer::mark_store_done_new(int idx) {
    new_[idx].store_done = true;
}

void ReorderBuffer::flush_after_new(int idx) {
    int cur = (idx + 1) % cap_;
    while (cur != tail_new_) {
        new_[cur].flushed = true;
        cur = (cur + 1) % cap_;
    }
}

void ReorderBuffer::flush_all_new() {
    for (int i = 0; i < cap_; ++i)
        new_[i] = empty_entry();
    head_new_ = tail_new_ = cnt_new_ = 0;
}

bool ReorderBuffer::is_flushed_new(int idx) const {
    return new_[idx].flushed;
}

void ReorderBuffer::commit_head_new() {
    new_[head_new_].busy = false;
    head_new_ = (head_new_ + 1) % cap_;
    cnt_new_--;
}

ROB_Entry& ReorderBuffer::entry_new(int idx) { return new_[idx]; }