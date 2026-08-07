#include "register.hpp"

Register::Register() {
    for (u32 i = 0; i < REG_COUNT; ++i)
        old_[i] = new_[i] = 0;
}

void Register::update() {
    for (u32 i = 0; i < REG_COUNT; ++i)
        old_[i] = new_[i];
}

u32 Register::read_old(u8 reg) const {
    return (reg == 0) ? 0 : old_[reg];
}

u32 Register::read_new(u8 reg) const {
    return (reg == 0) ? 0 : new_[reg];
}

void Register::write_new(u8 reg, u32 val) {
    if (reg != 0)
        new_[reg] = val;
}