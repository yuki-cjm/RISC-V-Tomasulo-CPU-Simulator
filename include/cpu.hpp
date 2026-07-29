#pragma once

#include "type.h"
#include "memory.hpp"
#include "decoder.hpp"

class CPU {
  private:
    Memory& mem;
    Decoder decoder;
    u32 reg[32]{};
    u32 pc = 0;
    bool end = false;

    u32 read(u8 idx);
    void write(u8 idx, u32 val);
    
  public:
    CPU(Memory& mem) : mem(mem) {}

    bool finish();
    void step();
    void execute(DecodedInstr &instr, u32 old_pc);
};
