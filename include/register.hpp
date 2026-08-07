#pragma once

#include "type.h"

class Register {
    u32 old_[REG_COUNT];
    u32 new_[REG_COUNT];

  public:
    Register();

    void update();

    u32 read_old(u8 reg) const;
    u32 read_new(u8 reg) const;

    void write_new(u8 reg, u32 val);
};
