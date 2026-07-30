#pragma once

#include "type.h"

struct IF_ID {
    u32 pc          = 0;
    u32 instruction = 0;
    bool valid      = false;   // false = bubble
};

struct ID_EX {
    u32 pc       = 0;
    Instr op     = Instr::UNKNOWN;
    u8  rd       = 0;
    u8  rs1      = 0;
    u8  rs2      = 0;
    u32 rs1_val  = 0;
    u32 rs2_val  = 0;
    i32 imm      = 0;
    bool valid   = false;

    bool RegWrite = false;
    bool MemRead  = false;
    bool MemWrite = false;
    bool Branch   = false;
    bool Jump     = false;
    u8   funct3   = 0;
};

struct EX_MEM {
    u32 alu_result    = 0;
    u32 store_data    = 0;
    u32 branch_target = 0;
    u32 jal_ret_addr  = 0;
    u8  rd            = 0;
    Instr op          = Instr::UNKNOWN;
    bool valid        = false;

    bool RegWrite = false;
    bool MemRead  = false;
    bool MemWrite = false;
    bool Branch   = false;
    bool Jump     = false;
    u8   funct3   = 0;

    bool Zero     = false;
    bool sign     = false;
    bool overflow = false;
    bool carry    = false;
};

struct MEM_WB {
    u32 mem_data   = 0;
    u32 alu_result = 0;
    u8  rd         = 0;
    bool valid     = false;
    bool MemToReg  = false;
    bool RegWrite  = false;
};
