#pragma once

#include "type.h"

struct DecodedInstr {
    Instr instr;
    u8 rd, rs1, rs2;
    i32 imm;
};

const DecodedInstr UNKNOWN_INSTR = DecodedInstr{Instr::UNKNOWN, 0, 0, 0, 0};

class Decoder {
  public:
    DecodedInstr decode(u32 raw);
};