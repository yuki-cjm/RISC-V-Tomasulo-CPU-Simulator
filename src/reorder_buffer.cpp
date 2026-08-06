#include "reorder_buffer.hpp"

ROB_Entry ReorderBuffer::empty_entry() {
    return {false, false, false, 0, -1, 0, 0,
            false, false, false, false, 0, false, 0, false, false};
}

ReorderBuffer::ReorderBuffer(int cap) : cap_(cap) {
    head_o_ = tail_o_ = cnt_o_ = 0;
    head_n_ = tail_n_ = cnt_n_ = 0;
    for (int i = 0; i < MAX_SZ; ++i)
        old_[i] = new_[i] = empty_entry();
}

void ReorderBuffer::snap() {
    for (int i = 0; i < cap_; ++i)
        new_[i] = old_[i];
    head_n_ = head_o_;
    tail_n_ = tail_o_;
    cnt_n_ = cnt_o_;
}

void ReorderBuffer::upd() {
    for (int i = 0; i < cap_; ++i)
        old_[i] = new_[i];
    head_o_ = head_n_;
    tail_o_ = tail_n_;
    cnt_o_ = cnt_n_;
}

bool ReorderBuffer::full_o() const { return cnt_o_ == cap_; }

bool ReorderBuffer::empty_o() const { return cnt_o_ == 0; }

int ReorderBuffer::head_o() const { return head_o_; }

int ReorderBuffer::count_o() const { return cnt_o_; }

int ReorderBuffer::cap() const { return cap_; }

const ROB_Entry& ReorderBuffer::entry_o(int idx) const { return old_[idx]; }

const ROB_Entry& ReorderBuffer::entry_cur(int idx) const { return new_[idx]; }

int ReorderBuffer::find_by_tag_o(int tag) const {
    for (int i = 0; i < cap_; ++i) {
        if (old_[i].busy && !old_[i].flushed && old_[i].dest_tag == tag)
            return i;
    }
    return -1;
}

bool ReorderBuffer::tag_valid_o(int tag) const {
    return find_by_tag_o(tag) >= 0;
}

int ReorderBuffer::head_entry_o() const {
    if (cnt_o_ == 0) return -1;
    return head_o_;
}

int ReorderBuffer::nth_valid_o(int n) const {
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

int ReorderBuffer::valid_count_o() const {
    int c = 0;
    for (int i = 0; i < cap_; ++i) {
        int idx = (head_o_ + i) % cap_;
        if (old_[idx].busy && !old_[idx].flushed) c++;
    }
    return c;
}

int ReorderBuffer::first_valid_o() const {
    for (int i = 0; i < cnt_o_; ++i) {
        int idx = (head_o_ + i) % cap_;
        if (old_[idx].busy && !old_[idx].flushed)
            return idx;
    }
    return -1;
}

int ReorderBuffer::alloc_n() {
    if (cnt_n_ == cap_) return -1;
    int idx = tail_n_;
    new_[idx] = empty_entry();
    new_[idx].busy = true;
    tail_n_ = (tail_n_ + 1) % cap_;
    cnt_n_++;
    return idx;
}

void ReorderBuffer::set_dest_n(int idx, u8 rd, int tag) {
    new_[idx].dest_reg = rd;
    new_[idx].dest_tag = tag;
}

void ReorderBuffer::set_pc_n(int idx, u32 pc) {
    new_[idx].pc = pc;
}

void ReorderBuffer::set_branch_n(int idx, bool pred, bool br, bool jp, u32 tgt) {
    new_[idx].pred_taken = pred;
    new_[idx].branch = br;
    new_[idx].jump = jp;
    new_[idx].branch_target = tgt;
}

void ReorderBuffer::set_store_n(int idx) {
    new_[idx].is_store = true;
}

void ReorderBuffer::set_ghr_snapshot_n(int idx, u32 ghr) {
    new_[idx].ghr_snapshot = ghr;
}

void ReorderBuffer::set_bimod_pred_n(int idx, bool pred) {
    new_[idx].bimod_pred = pred;
}

void ReorderBuffer::set_gshare_pred_n(int idx, bool pred) {
    new_[idx].gshare_pred = pred;
}

void ReorderBuffer::write_result_n(int idx, u32 val) {
    new_[idx].ready = true;
    new_[idx].value = val;
}

void ReorderBuffer::mark_store_done_n(int idx) {
    new_[idx].store_done = true;
}

void ReorderBuffer::flush_after_n(int idx) {
    int cur = (idx + 1) % cap_;
    while (cur != tail_n_) {
        new_[cur].flushed = true;
        cur = (cur + 1) % cap_;
    }
}

void ReorderBuffer::flush_all_n() {
    for (int i = 0; i < cap_; ++i)
        new_[i] = empty_entry();
    head_n_ = tail_n_ = cnt_n_ = 0;
}

bool ReorderBuffer::is_flushed_n(int idx) const {
    return new_[idx].flushed;
}

void ReorderBuffer::commit_head_n() {
    new_[head_n_].busy = false;
    head_n_ = (head_n_ + 1) % cap_;
    cnt_n_--;
}

ROB_Entry& ReorderBuffer::entry_n(int idx) { return new_[idx]; }