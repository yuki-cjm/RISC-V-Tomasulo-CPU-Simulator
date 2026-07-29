#include <iostream>

#include "type.h"
#include "cpu.hpp"
#include "memory.hpp"
#include "decoder.hpp"

u32 CPU::read(u8 idx) {
    if (idx == 0) return 0;
    return reg[idx];
}

void CPU::write(u8 idx, u32 val) {
    if (idx == 0) return;
    reg[idx] = val;
}

bool CPU::finish() {
    return end;
}

void CPU::step() {
    u32 raw = mem.read_word(pc);
    u32 old_pc = pc;
    pc += 4;
    DecodedInstr instr = decoder.decode(raw);
    execute(instr, old_pc);
}

void CPU::execute(DecodedInstr &instr, u32 old_pc) {
    switch (instr.instr) {
        case Instr::ADD:    write(instr.rd, reg[instr.rs1] + reg[instr.rs2]); break;
        case Instr::SUB:    write(instr.rd, reg[instr.rs1] - reg[instr.rs2]); break;
        case Instr::AND:    write(instr.rd, reg[instr.rs1] & reg[instr.rs2]); break;
        case Instr::OR:     write(instr.rd, reg[instr.rs1] | reg[instr.rs2]); break;
        case Instr::XOR:    write(instr.rd, reg[instr.rs1] ^ reg[instr.rs2]); break;
        case Instr::SLL:    write(instr.rd, reg[instr.rs1] << (reg[instr.rs2] & 0x1F)); break;
        case Instr::SRL:    write(instr.rd, reg[instr.rs1] >> (reg[instr.rs2] & 0x1F)); break;
        case Instr::SRA:    write(instr.rd, static_cast<u32>(static_cast<i32>(reg[instr.rs1]) >> (reg[instr.rs2] & 0x1F))); break;
        case Instr::SLT:    write(instr.rd, (static_cast<i32>(reg[instr.rs1]) < static_cast<i32>(reg[instr.rs2])) ? 1 : 0); break;
        case Instr::SLTU:   write(instr.rd, (reg[instr.rs1] < reg[instr.rs2]) ? 1 : 0); break;
        case Instr::ADDI:   write(instr.rd, reg[instr.rs1] + instr.imm); break;
        case Instr::ANDI:   write(instr.rd, reg[instr.rs1] & instr.imm); break;
        case Instr::ORI:    write(instr.rd, reg[instr.rs1] | instr.imm); break;
        case Instr::XORI:   write(instr.rd, reg[instr.rs1] ^ instr.imm); break;
        case Instr::SLLI:   write(instr.rd, reg[instr.rs1] << (instr.imm & 0x1F)); break;
        case Instr::SRLI:   write(instr.rd, reg[instr.rs1] >> (instr.imm & 0x1F)); break;
        case Instr::SRAI:   write(instr.rd, static_cast<u32>(static_cast<i32>(reg[instr.rs1]) >> (instr.imm & 0x1F))); break;
        case Instr::SLTI:   write(instr.rd, (static_cast<i32>(reg[instr.rs1]) < instr.imm) ? 1 : 0); break;
        case Instr::SLTIU:  write(instr.rd, (reg[instr.rs1] < static_cast<u32>(instr.imm)) ? 1 : 0); break;
        case Instr::LB:     write(instr.rd, static_cast<u32>(static_cast<i32>(static_cast<i8>(mem.read_byte(reg[instr.rs1] + instr.imm))))); break;
        case Instr::LBU:    write(instr.rd, static_cast<u32>(mem.read_byte(reg[instr.rs1] + instr.imm))); break;
        case Instr::LH:     write(instr.rd, static_cast<u32>(static_cast<i32>(static_cast<i16>(mem.read_half(reg[instr.rs1] + instr.imm))))); break;
        case Instr::LHU:    write(instr.rd, static_cast<u32>(mem.read_half(reg[instr.rs1] + instr.imm))); break;
        case Instr::LW:     write(instr.rd, mem.read_word(reg[instr.rs1] + instr.imm)); break;
        case Instr::SB:     mem.write_byte(reg[instr.rs1] + instr.imm, read(instr.rs2) & 0xFF); break;
        case Instr::SH:     mem.write_half(reg[instr.rs1] + instr.imm, read(instr.rs2) & 0xFFFF); break;
        case Instr::SW:     mem.write_word(reg[instr.rs1] + instr.imm, read(instr.rs2)); break;
        case Instr::BEQ:    if (reg[instr.rs1] == reg[instr.rs2]) pc = old_pc + instr.imm; break;
        case Instr::BGE:    if (static_cast<i32>(reg[instr.rs1]) >= static_cast<i32>(reg[instr.rs2])) pc = old_pc + instr.imm; break;
        case Instr::BGEU:   if (reg[instr.rs1] >= reg[instr.rs2]) pc = old_pc + instr.imm; break;
        case Instr::BLT:    if (static_cast<i32>(reg[instr.rs1]) < static_cast<i32>(reg[instr.rs2])) pc = old_pc + instr.imm; break;
        case Instr::BLTU:   if (reg[instr.rs1] < reg[instr.rs2]) pc = old_pc + instr.imm; break;
        case Instr::BNE:    if (reg[instr.rs1] != reg[instr.rs2]) pc = old_pc + instr.imm; break;
        case Instr::JAL:    write(instr.rd, pc); pc = old_pc + instr.imm; break;
        case Instr::JALR:   write(instr.rd, pc); pc = (read(instr.rs1) + instr.imm) & ~1u; break;
        case Instr::AUIPC:  write(instr.rd, old_pc + instr.imm); break;
        case Instr::LUI:    write(instr.rd, instr.imm); break;
        case Instr::EBREAK: return;
        case Instr::ECALL:  return;
        case Instr::HALT:   end = true; std::cout << (reg[10] & 0xFF) << std::endl; break;
        default: fatal("Unknown instruction at PC = 0x" + to_hex(old_pc));
    }
}