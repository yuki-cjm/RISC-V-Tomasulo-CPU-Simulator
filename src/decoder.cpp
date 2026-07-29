#include "type.h"
#include "memory.hpp"
#include "decoder.hpp"

DecodedInstr decode_R(u32 raw) {
    /*
    ┌──────────┬───────┬───────┬────────┬──────┬────────┐
    │ funct7   │ rs2   │ rs1   │ funct3 │ rd   │ opcode │
    │ [31:25]  │[24:20]│[19:15]│[14:12] │[11:7]│ [6:0]  │
    └──────────┴───────┴───────┴────────┴──────┴────────┘
    */
    u8 funct7 = (raw >> 25) & 0x7F;
    u8 rs2    = (raw >> 20) & 0x1F;
    u8 rs1    = (raw >> 15) & 0x1F;
    u8 funct3 = (raw >> 12) & 0x7;
    u8 rd     = (raw >> 7)  & 0x1F;
    switch (funct3) {
        case 0b000:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::ADD, rd, rs1, rs2, 0};
            if (funct7 == 0b0100000) return DecodedInstr{Instr::SUB, rd, rs1, rs2, 0};
            break;
        case 0b111:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::AND, rd, rs1, rs2, 0};
            break;
        case 0b110:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::OR, rd, rs1, rs2, 0};
            break;
        case 0b100:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::XOR, rd, rs1, rs2, 0};
            break;
        case 0b001:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::SLL, rd, rs1, rs2, 0};
            break;
        case 0b101:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::SRL, rd, rs1, rs2, 0};
            if (funct7 == 0b0100000) return DecodedInstr{Instr::SRA, rd, rs1, rs2, 0};
            break;
        case 0b010:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::SLT, rd, rs1, rs2, 0};
            break;
        case 0b011:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::SLTU, rd, rs1, rs2, 0};
            break;
    }
    return UNKNOWN_INSTR;
}

DecodedInstr decode_I1(u32 raw) {
    /*
    ┌───────────────────────┬───────┬────────┬───────┬────────┐
    │      imm[11:0]        │  rs1  │ funct3 │  rd   │ opcode │
    │       [31:20]         │[19:15]│[14:12] │[11:7] │ [6:0]  │
    └───────────────────────┴───────┴────────┴───────┴────────┘
    */

    u8 funct7 = (raw >> 25) & 0x7F;
    u8 rs1    = (raw >> 15) & 0x1F;
    u8 funct3 = (raw >> 12) & 0x7;
    u8 rd     = (raw >> 7) & 0x1F;
    i32 imm   = i32(raw) >> 20;

    switch (funct3) {
        case 0b000:
            return DecodedInstr{Instr::ADDI,  rd, rs1, 0, imm};
        case 0b111:
            return DecodedInstr{Instr::ANDI, rd, rs1, 0, imm};
        case 0b110:
            return DecodedInstr{Instr::ORI, rd, rs1, 0, imm};
        case 0b100:
            return DecodedInstr{Instr::XORI, rd, rs1, 0, imm};
        case 0b001:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::SLLI, rd, rs1, 0, imm};
            break;
        case 0b101:
            if (funct7 == 0b0000000) return DecodedInstr{Instr::SRLI, rd, rs1, 0, imm};
            if (funct7 == 0b0100000) return DecodedInstr{Instr::SRAI, rd, rs1, 0, imm};
            break;
        case 0b010:
            return DecodedInstr{Instr::SLTI, rd, rs1, 0, imm};
        case 0b011:
            return DecodedInstr{Instr::SLTIU, rd, rs1, 0, imm};
    }
    return UNKNOWN_INSTR;
}

DecodedInstr decode_I2(u32 raw) {
    /*
    ┌───────────────────────┬───────┬────────┬───────┬────────┐
    │      imm[11:0]        │  rs1  │ funct3 │  rd   │ opcode │
    │       [31:20]         │[19:15]│[14:12] │[11:7] │ [6:0]  │
    └───────────────────────┴───────┴────────┴───────┴────────┘
    */

    u8 rs1    = (raw >> 15) & 0x1F;
    u8 funct3 = (raw >> 12) & 0x7;
    u8 rd     = (raw >> 7) & 0x1F;
    i32 imm   = i32(raw) >> 20;

    switch (funct3) {
        case 0b000:
            return DecodedInstr{Instr::LB, rd, rs1, 0, imm};
        case 0b100:
            return DecodedInstr{Instr::LBU, rd, rs1, 0, imm};
        case 0b001:
            return DecodedInstr{Instr::LH, rd, rs1, 0, imm};
        case 0b101:
            return DecodedInstr{Instr::LHU, rd, rs1, 0, imm};
        case 0b010:
            return DecodedInstr{Instr::LW, rd, rs1, 0, imm};
    }
    
    return UNKNOWN_INSTR;
}

DecodedInstr decode_S(u32 raw) {
    /*
    ┌───────────────────────┬───────┬───────┬────────┬───────────────┬────────┐
    │      imm[11:5]        │  rs2  │  rs1  │ funct3 │  imm[4:0]     │ opcode │
    │       [31:25]         │[24:20]│[19:15]│[14:12] │   [11:7]      │ [6:0]  │
    └───────────────────────┴───────┴───────┴────────┴───────────────┴────────┘
    */

    u8 funct3 = (raw >> 12) & 0x7;
    u8 rs1    = (raw >> 15) & 0x1F;
    u8 rs2    = (raw >> 20) & 0x1F;

    u32 imm_11_5 = (raw >> 25) & 0x7F;
    u32 imm_4_0  = (raw >> 7) & 0x1F;
    u32 imm_u    = (imm_11_5 << 5) | imm_4_0;
    i32 imm      = i32(imm_u << 20) >> 20;

    switch (funct3) {
        case 0b000:
            return DecodedInstr{Instr::SB, 0, rs1, rs2, imm};
        case 0b001:
            return DecodedInstr{Instr::SH, 0, rs1, rs2, imm};
        case 0b010:
            return DecodedInstr{Instr::SW, 0, rs1, rs2, imm};
    }
    return UNKNOWN_INSTR;
}

DecodedInstr decode_B(u32 raw) {
    /*
    ┌─────────┬───────────┬───────┬───────┬────────┬───────────┬────────┐
    │ imm[12] │ imm[10:5] │  rs2  │  rs1  │ funct3 │ imm[4:1]  │ opcode │
    │  [31]   │  [30:25]  │[24:20]│[19:15]│[14:12] │ imm[11]   │ [6:0]  │
    │         │           │       │       │        │  [11:8|7] │        │
    └─────────┴───────────┴───────┴───────┴────────┴───────────┴────────┘
    */

    u8 funct3 = (raw >> 12) & 0x7;
    u8 rs1    = (raw >> 15) & 0x1F;
    u8 rs2    = (raw >> 20) & 0x1F;

    i32 imm = i32((raw >> 31) << 12      // imm[12]
            | ((raw >> 7) & 1) << 11     // imm[11]
            | ((raw >> 25) & 0x3F) << 5  // imm[10:5]
            | ((raw >> 8) & 0xF) << 1)   // imm[4:1]
            << 19 >> 19;                 // 符号扩展

    switch (funct3) {
        case 0b000: return DecodedInstr{Instr::BEQ, 0, rs1, rs2, imm};
        case 0b101: return DecodedInstr{Instr::BGE, 0,rs1, rs2, imm};
        case 0b111: return DecodedInstr{Instr::BGEU, 0, rs1, rs2, imm};
        case 0b100: return DecodedInstr{Instr::BLT, 0, rs1, rs2, imm};
        case 0b110: return DecodedInstr{Instr::BLTU, 0, rs1, rs2, imm};
        case 0b001: return DecodedInstr{Instr::BNE, 0, rs1, rs2, imm};
    }
    return UNKNOWN_INSTR;
}

DecodedInstr decode_J(u32 raw) {
    /*
    ┌─────────┬────────────┬─────────┬───────────────┬───────┬────────┐
    │ imm[20] │ imm[10:1]  │ imm[11] │  imm[19:12]   │  rd   │ opcode │
    │  [31]   │  [30:21]   │  [20]   │    [19:12]    │[11:7] │ [6:0]  │
    └─────────┴────────────┴─────────┴───────────────┴───────┴────────┘
    */

    u8 rd = (raw >> 7) & 0x1F;
    i32 imm = i32((raw >> 31) << 20
            | ((raw >> 12) & 0xFF) << 12
            | ((raw >> 20) & 1) << 11
            | ((raw >> 21) & 0x3FF) << 1)
            << 11 >> 11;

    return DecodedInstr{Instr::JAL, rd, 0, 0, imm};
}

DecodedInstr decode_I3(u32 raw) {
    /*
    ┌───────────────────────┬───────┬────────┬───────┬────────┐
    │      imm[11:0]        │  rs1  │ funct3 │  rd   │ opcode │
    │       [31:20]         │[19:15]│[14:12] │[11:7] │ [6:0]  │
    └───────────────────────┴───────┴────────┴───────┴────────┘
    */

    u8 rs1    = (raw >> 15) & 0x1F;
    u8 funct3 = (raw >> 12) & 0x7;
    u8 rd     = (raw >> 7) & 0x1F;
    i32 imm   = i32(raw) >> 20;

    if (funct3 == 0b000) return DecodedInstr{Instr::JALR, rd, rs1, 0, imm};
    return UNKNOWN_INSTR;
}

DecodedInstr decode_U1(u32 raw) {
    /*
    ┌───────────────────────┬───────┬────────┐
    │      imm[31:12]       │  rd   │ opcode │
    │       [31:12]         │[11:7] │ [6:0]  │
    └───────────────────────┴───────┴────────┘
    */

    u8 rd  = (raw >> 7) & 0x1F;
    i32 imm = raw & 0xFFFFF000;

    return DecodedInstr{Instr::AUIPC, rd, 0, 0, imm};
}

DecodedInstr decode_U2(u32 raw) {
    /*
    ┌───────────────────────┬───────┬────────┐
    │      imm[31:12]       │  rd   │ opcode │
    │       [31:12]         │[11:7] │ [6:0]  │
    └───────────────────────┴───────┴────────┘
    */

    u8 rd  = (raw >> 7) & 0x1F;
    i32 imm = raw & 0xFFFFF000;

    return DecodedInstr{Instr::LUI, rd, 0, 0, imm};
}

DecodedInstr decode_I4(u32 raw) {
    /*
    ┌───────────────────────┬───────┬────────┬───────┬────────┐
    │      imm[11:0]        │  rs1  │ funct3 │  rd   │ opcode │
    │       [31:20]         │[19:15]│[14:12] │[11:7] │ [6:0]  │
    └───────────────────────┴───────┴────────┴───────┴────────┘
    */

    u8 funct3 = (raw >> 12) & 0x7;
    u32 imm_u = (raw >> 20) & 0xFFF;

    if (funct3 != 0b000) return UNKNOWN_INSTR;

    switch (imm_u) {
        case 0x000: return DecodedInstr{Instr::EBREAK, 0, 0, 0, 0};
        case 0x001: return DecodedInstr{Instr::ECALL, 0, 0, 0, 0};
    }

    return UNKNOWN_INSTR; 
}

DecodedInstr Decoder::decode(u32 raw) {
    if (raw == 0x0FF00513) return DecodedInstr{Instr::HALT, 0, 0, 0, 0};
    u8 opcode = raw & 0x7F;
    switch (opcode) {
        case 0b0110011: return decode_R (raw);
        case 0b0010011: return decode_I1(raw);
        case 0b0000011: return decode_I2(raw);
        case 0b0100011: return decode_S (raw);
        case 0b1100011: return decode_B (raw);
        case 0b1101111: return decode_J (raw);
        case 0b1100111: return decode_I3(raw);
        case 0b0010111: return decode_U1(raw);
        case 0b0110111: return decode_U2(raw);
        case 0b1110011: return decode_I4(raw);
        default: return DecodedInstr{Instr::UNKNOWN, 0, 0, 0, 0};
    }
}