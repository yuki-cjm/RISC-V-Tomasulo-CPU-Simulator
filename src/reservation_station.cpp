#include "reservation_station.hpp"

RS_Entry ReservationStation::empty_entry() {
    return {false, false, Instr::UNKNOWN, 0, 0, 0, 0, 0,
            -1, -1, -1, -1, -1, 1, 0,
            false, false, false, false, false};
}

ReservationStation::ReservationStation(int sz, const char* nm)
    : size_(sz), name_(nm) {
    for (int i = 0; i < MAX_SZ; ++i)
        old_[i] = new_[i] = empty_entry();
}

void ReservationStation::snap() {
    for (int i = 0; i < size_; ++i)
        new_[i] = old_[i];
}

void ReservationStation::upd() {
    for (int i = 0; i < size_; ++i)
        old_[i] = new_[i];
}

int ReservationStation::size() const { return size_; }

const char* ReservationStation::name() const { return name_; }

const RS_Entry& ReservationStation::entry_o(int i) const { return old_[i]; }

bool ReservationStation::full_o() const {
    for (int i = 0; i < size_; ++i)
        if (!old_[i].busy) return false;
    return true;
}

int ReservationStation::find_ready_o() const {
    for (int i = 0; i < size_; ++i) {
        if (old_[i].busy && old_[i].Qj == -1 && old_[i].Qk == -1
            && !old_[i].completed)
            return i;
    }
    return -1;
}

int ReservationStation::find_completed_o() const {
    for (int i = 0; i < size_; ++i) {
        if (old_[i].busy && old_[i].completed)
            return i;
    }
    return -1;
}

int ReservationStation::find_by_tag_o(int tag) const {
    for (int i = 0; i < size_; ++i)
        if (old_[i].busy && old_[i].dest_tag == tag)
            return i;
    return -1;
}

int ReservationStation::alloc_n(int tag) {
    for (int i = 0; i < size_; ++i) {
        if (!old_[i].busy && !new_[i].busy) {
            new_[i] = empty_entry();
            new_[i].busy = true;
            new_[i].dest_tag = tag;
            return i;
        }
    }
    return -1;
}

RS_Entry& ReservationStation::entry_n(int i) { return new_[i]; }

void ReservationStation::tick_cycle_n(int i) {
    if (old_[i].busy && new_[i].busy
        && new_[i].dest_tag == old_[i].dest_tag
        && old_[i].cycle_left > 0)
        new_[i].cycle_left--;
}

void ReservationStation::tick_all_alu_n() {
    for (int i = 0; i < size_; ++i) {
        if (old_[i].busy && new_[i].busy
            && new_[i].dest_tag == old_[i].dest_tag
            && !old_[i].MemRead && !old_[i].MemWrite
            && !old_[i].Branch && !old_[i].Jump
            && old_[i].cycle_left > 0)
            new_[i].cycle_left--;
    }
}

void ReservationStation::mark_completed_n(int i, u32 result) {
    new_[i].completed = true;
    new_[i].exec_result = result;
}

void ReservationStation::wakeup_cdb_n(int tag, u32 value) {
    for (int i = 0; i < size_; ++i) {
        if (!new_[i].busy) continue;
        if (new_[i].Qj == tag) { new_[i].Vj = value; new_[i].Qj = -1; }
        if (new_[i].Qk == tag) { new_[i].Vk = value; new_[i].Qk = -1; }
    }
}

void ReservationStation::free_n(int i) {
    new_[i] = empty_entry();
}

void ReservationStation::free_by_tag_n(int tag) {
    for (int i = 0; i < size_; ++i) {
        if (new_[i].busy && new_[i].dest_tag == tag) {
            new_[i] = empty_entry();
            return;
        }
    }
}

void ReservationStation::flush_by_rob_n(int rob_idx) {
    for (int i = 0; i < size_; ++i) {
        if (new_[i].busy && new_[i].rob_idx == rob_idx)
            new_[i] = empty_entry();
    }
}

void ReservationStation::flush_all_n() {
    for (int i = 0; i < size_; ++i)
        new_[i] = empty_entry();
}

void ReservationStation::flush_younger_than_n(int branch_tag) {
    for (int i = 0; i < size_; ++i) {
        if (new_[i].busy && new_[i].dest_tag > branch_tag)
            new_[i] = empty_entry();
    }
}