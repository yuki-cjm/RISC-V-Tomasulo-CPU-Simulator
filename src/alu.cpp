#include "alu.hpp"

u32 ALU::compute(Instr op, u32 a, u32 b, u32 imm, u32 pc) {
    switch (op) {
        case Instr::ADD:   return a + b;
        case Instr::SUB:   return a - b;
        case Instr::AND:   return a & b;
        case Instr::OR:    return a | b;
        case Instr::XOR:   return a ^ b;
        case Instr::SLL:   return a << (b & 0x1F);
        case Instr::SRL:   return a >> (b & 0x1F);
        case Instr::SRA:   return (u32)((i32)a >> (b & 0x1F));
        case Instr::SLT:   return ((i32)a < (i32)b) ? 1 : 0;
        case Instr::SLTU:  return (a < b) ? 1 : 0;
        case Instr::ADDI:  return a + imm;
        case Instr::ANDI:  return a & imm;
        case Instr::ORI:   return a | imm;
        case Instr::XORI:  return a ^ imm;
        case Instr::SLLI:  return a << (imm & 0x1F);
        case Instr::SRLI:  return a >> (imm & 0x1F);
        case Instr::SRAI:  return (u32)((i32)a >> (imm & 0x1F));
        case Instr::SLTI:  return ((i32)a < (i32)imm) ? 1 : 0;
        case Instr::SLTIU: return (a < (u32)imm) ? 1 : 0;
        case Instr::AUIPC: return pc + imm;
        case Instr::LUI:   return imm;
        default:           return 0;
    }
}