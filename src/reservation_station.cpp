#include "reservation_station.hpp"

RS_Entry ReservationStation::empty_entry() {
    return {false, Instr::UNKNOWN, 0, 0, 0, 0,
            -1, -1, -1, -1, -1, 1, 0,
            false, false, false, false, false};
}

ReservationStation::ReservationStation(int size)
    : size_(size) {
    for (int i = 0; i < MAX_SIZE; ++i)
        old_[i] = new_[i] = empty_entry();
}

void ReservationStation::update() {
    for (int i = 0; i < size_; ++i)
        old_[i] = new_[i];
}

int ReservationStation::size() const { return size_; }

const RS_Entry& ReservationStation::entry_old(int i) const { return old_[i]; }

RS_Entry& ReservationStation::entry_new(int i) { return new_[i]; }

bool ReservationStation::full_old() const {
    for (int i = 0; i < size_; ++i)
        if (!old_[i].busy) return false;
    return true;
}

int ReservationStation::find_by_tag_old(int tag) const {
    for (int i = 0; i < size_; ++i)
        if (old_[i].busy && old_[i].dest_tag == tag)
            return i;
    return -1;
}

int ReservationStation::alloc_new(int tag) {
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

void ReservationStation::tick_all_alu_new() {
    for (int i = 0; i < size_; ++i) {
        if (old_[i].busy && new_[i].busy
            && new_[i].dest_tag == old_[i].dest_tag
            && !old_[i].MemRead && !old_[i].MemWrite
            && !old_[i].Branch && !old_[i].Jump
            && old_[i].cycle_left > 0)
            new_[i].cycle_left--;
    }
}

void ReservationStation::wakeup_cdb_new(int tag, u32 value) {
    for (int i = 0; i < size_; ++i) {
        if (!new_[i].busy) continue;
        if (new_[i].Qj == tag) {
            new_[i].Vj = value;
            new_[i].Qj = -1;
        }
        if (new_[i].Qk == tag) {
            new_[i].Vk = value;
            new_[i].Qk = -1;
        }
    }
}

void ReservationStation::free_new(int i) {
    new_[i] = empty_entry();
}

void ReservationStation::flush_younger_than_new(int branch_tag) {
    for (int i = 0; i < size_; ++i) {
        if (new_[i].busy && new_[i].dest_tag > branch_tag)
            new_[i] = empty_entry();
    }
}