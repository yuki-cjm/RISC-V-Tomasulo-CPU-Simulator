#include "register_file.hpp"

RegisterFile::RegisterFile() {
    for (u32 i = 0; i < REG_COUNT; ++i)
        old_[i] = new_[i] = 0;
}

void RegisterFile::snap() {
    for (u32 i = 0; i < REG_COUNT; ++i)
        new_[i] = old_[i];
}

void RegisterFile::upd() {
    for (u32 i = 0; i < REG_COUNT; ++i)
        old_[i] = new_[i];
}

u32 RegisterFile::read_o(u8 reg) const {
    return (reg == 0) ? 0 : old_[reg];
}

u32 RegisterFile::read_n(u8 reg) const {
    return (reg == 0) ? 0 : new_[reg];
}

void RegisterFile::write_n(u8 reg, u32 val) {
    if (reg != 0)
        new_[reg] = val;
}