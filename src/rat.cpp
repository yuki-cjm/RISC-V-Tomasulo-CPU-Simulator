#include "rat.hpp"

RegAliasTable::RegAliasTable() {
    for (u32 i = 0; i < REG_COUNT; ++i)
        old_[i] = new_[i] = {-1, false, 0, false};
}

void RegAliasTable::snap() {
    for (u32 i = 0; i < REG_COUNT; ++i)
        new_[i] = old_[i];
}

void RegAliasTable::upd() {
    for (u32 i = 0; i < REG_COUNT; ++i)
        old_[i] = new_[i];
}

int RegAliasTable::tag_o(u8 r) const { return old_[r].tag; }

bool RegAliasTable::busy_o(u8 r) const { return old_[r].busy; }

bool RegAliasTable::ready_o(u8 r) const { return old_[r].ready; }

u32 RegAliasTable::val_o(u8 r) const { return old_[r].value; }

const RAT_Entry& RegAliasTable::entry_o(u8 r) const { return old_[r]; }

void RegAliasTable::rename_n(u8 r, int t) {
    if (r != 0)
        new_[r] = {t, true, 0, false};
}

void RegAliasTable::capture_cdb_n(int tag, u32 value) {
    for (u32 i = 0; i < REG_COUNT; ++i) {
        if (new_[i].tag == tag) {
            new_[i].value = value;
            new_[i].ready = true;
        }
    }
}

void RegAliasTable::commit_clear_n(u8 r, int t) {
    if (r != 0 && new_[r].tag == t)
        new_[r] = {-1, false, 0, false};
}

void RegAliasTable::flush_all_n() {
    for (u32 i = 0; i < REG_COUNT; ++i)
        new_[i] = {-1, false, 0, false};
}