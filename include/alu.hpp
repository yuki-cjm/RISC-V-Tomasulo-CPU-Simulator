#pragma once

#include "type.h"

class ALU {
  public:
    static u32 compute(Instr op, u32 a, u32 b, u32 imm, u32 pc);
};