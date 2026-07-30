#include <iostream>

#include "type.h"
#include "cpu.hpp"
#include "memory.hpp"
#include "decoder.hpp"

u32 CPU::read_reg(u8 idx) {
    return (idx == 0) ? 0 : reg[idx];
}

void CPU::write_reg(u8 idx, u32 val) {
    if (idx != 0) reg[idx] = val;
}

u32 CPU::forward_rs1(u8 rs1) {
    if (rs1 == 0) return 0;
    if (ex_mem.RegWrite && ex_mem.rd == rs1 && !ex_mem.MemRead)
        return ex_mem.alu_result;
    if (mem_wb.RegWrite && mem_wb.rd == rs1)
        return mem_wb.MemToReg ? mem_wb.mem_data : mem_wb.alu_result;
    return reg[rs1];
}

u32 CPU::forward_rs2(u8 rs2) {
    if (rs2 == 0) return 0;
    if (ex_mem.RegWrite && ex_mem.rd == rs2 && !ex_mem.MemRead)
        return ex_mem.alu_result;
    if (mem_wb.RegWrite && mem_wb.rd == rs2)
        return mem_wb.MemToReg ? mem_wb.mem_data : mem_wb.alu_result;
    return reg[rs2];
}

static void set_ctrl(ID_EX& idex, DecodedInstr& d) {
    idex.RegWrite = false; idex.MemRead  = false;
    idex.MemWrite = false; idex.Branch   = false;
    idex.Jump     = false;

    switch (d.instr) {
        case Instr::ADD: case Instr::SUB: case Instr::AND:
        case Instr::OR:  case Instr::XOR: case Instr::SLL:
        case Instr::SRL: case Instr::SRA:
        case Instr::SLT: case Instr::SLTU:
            idex.RegWrite = true; break;

        case Instr::ADDI: case Instr::ANDI: case Instr::ORI:
        case Instr::XORI: case Instr::SLLI: case Instr::SRLI:
        case Instr::SRAI: case Instr::SLTI: case Instr::SLTIU:
            idex.RegWrite = true; break;

        case Instr::LB: case Instr::LBU: case Instr::LH:
        case Instr::LHU: case Instr::LW:
            idex.RegWrite = true; idex.MemRead = true; break;

        case Instr::SB: case Instr::SH: case Instr::SW:
            idex.MemWrite = true; break;

        case Instr::BEQ: case Instr::BNE: case Instr::BLT:
        case Instr::BGE: case Instr::BLTU: case Instr::BGEU:
            idex.Branch = true; break;

        case Instr::JAL:
            idex.RegWrite = true; idex.Jump = true; break;
        case Instr::JALR:
            idex.RegWrite = true; idex.Jump = true; break;

        case Instr::AUIPC: idex.RegWrite = true; break;
        case Instr::LUI:   idex.RegWrite = true; break;

        default: break;
    }
}

void CPU::stage_IF() {
    u32 raw = mem.read_word(pc);
    if_id_nxt.pc          = pc;
    if_id_nxt.instruction = raw;
    if_id_nxt.valid       = true;
    pc += 4;
}

void CPU::stage_ID() {
    if (!if_id.valid) {
        id_ex_nxt = {};  // bubble
        return;
    }

    DecodedInstr d = decoder.decode(if_id.instruction);

    u32 v1 = forward_rs1(d.rs1);
    u32 v2 = forward_rs2(d.rs2);

    id_ex_nxt.pc       = if_id.pc;
    id_ex_nxt.op       = d.instr;
    id_ex_nxt.rd       = d.rd;
    id_ex_nxt.rs1      = d.rs1;
    id_ex_nxt.rs2      = d.rs2;
    id_ex_nxt.rs1_val  = v1;
    id_ex_nxt.rs2_val  = v2;
    id_ex_nxt.imm      = d.imm;
    id_ex_nxt.valid    = true;
    set_ctrl(id_ex_nxt, d);
}

void CPU::stage_EX() {
    if (!id_ex.valid) { ex_mem_nxt = {}; return; }

    u32 v1  = id_ex.rs1_val;
    u32 v2  = id_ex.rs2_val;
    i32 imm = id_ex.imm;
    u32 alu = 0;
    u32 btgt = 0;

    switch (id_ex.op) {
        case Instr::ADD: alu = v1 + v2; break;
        case Instr::SUB: alu = v1 - v2; break;
        case Instr::AND: alu = v1 & v2; break;
        case Instr::OR:  alu = v1 | v2; break;
        case Instr::XOR: alu = v1 ^ v2; break;
        case Instr::SLL: alu = v1 << (v2 & 0x1F); break;
        case Instr::SRL: alu = v1 >> (v2 & 0x1F); break;
        case Instr::SRA:
            alu = static_cast<u32>(static_cast<i32>(v1) >> (v2 & 0x1F)); break;
        case Instr::SLT:
            alu = (static_cast<i32>(v1) < static_cast<i32>(v2)) ? 1 : 0; break;
        case Instr::SLTU: alu = (v1 < v2) ? 1 : 0; break;

        case Instr::ADDI:  alu = v1 + imm; break;
        case Instr::ANDI:  alu = v1 & imm; break;
        case Instr::ORI:   alu = v1 | imm; break;
        case Instr::XORI:  alu = v1 ^ imm; break;
        case Instr::SLLI:  alu = v1 << (imm & 0x1F); break;
        case Instr::SRLI:  alu = v1 >> (imm & 0x1F); break;
        case Instr::SRAI:
            alu = static_cast<u32>(static_cast<i32>(v1) >> (imm & 0x1F)); break;
        case Instr::SLTI:
            alu = (static_cast<i32>(v1) < imm) ? 1 : 0; break;
        case Instr::SLTIU:
            alu = (v1 < static_cast<u32>(imm)) ? 1 : 0; break;

        case Instr::LB: case Instr::LBU: case Instr::LH:
        case Instr::LHU: case Instr::LW:
        case Instr::SB: case Instr::SH: case Instr::SW:
            alu = v1 + imm; break;

        case Instr::BEQ: case Instr::BNE: case Instr::BLT:
        case Instr::BGE: case Instr::BLTU: case Instr::BGEU:
            alu = v1 - v2;
            btgt = id_ex.pc + imm; break;

        case Instr::JAL:  alu = id_ex.pc + imm; break;
        case Instr::JALR: alu = (v1 + imm) & ~1u; break;
        case Instr::AUIPC: alu = id_ex.pc + imm; break;
        case Instr::LUI:   alu = static_cast<u32>(imm); break;
        default: break;
    }

    bool zero = (alu == 0);
    bool sign = (alu >> 31) & 1;
    bool overflow = false, carry = false;
    if (id_ex.Branch) {
        overflow = (((v1 ^ v2) & (v1 ^ alu)) >> 31) & 1;
        carry    = (v1 >= v2);
    }

    ex_mem_nxt.alu_result    = alu;
    ex_mem_nxt.store_data    = v2;
    ex_mem_nxt.branch_target = btgt;
    ex_mem_nxt.jal_ret_addr  = (id_ex.op == Instr::JAL || id_ex.op == Instr::JALR)
                                  ? id_ex.pc + 4 : 0;
    ex_mem_nxt.rd       = id_ex.rd;
    ex_mem_nxt.op       = id_ex.op;
    ex_mem_nxt.valid    = true;
    ex_mem_nxt.RegWrite = id_ex.RegWrite;
    ex_mem_nxt.MemRead  = id_ex.MemRead;
    ex_mem_nxt.MemWrite = id_ex.MemWrite;
    ex_mem_nxt.Branch   = id_ex.Branch;
    ex_mem_nxt.Jump     = id_ex.Jump;
    ex_mem_nxt.funct3   = id_ex.funct3;
    ex_mem_nxt.Zero     = zero;
    ex_mem_nxt.sign     = sign;
    ex_mem_nxt.overflow = overflow;
    ex_mem_nxt.carry    = carry;
}

void CPU::stage_MEM() {
    if (!ex_mem.valid) { mem_wb_nxt = {}; return; }

    if (ex_mem.op == Instr::HALT) {
        finished = true;
        std::cout << (reg[10] & 0xFF) << std::endl;
        mem_wb_nxt = {};
        return;
    }

    if (ex_mem.Branch) {
        bool take = false;
        switch (ex_mem.op) {
            case Instr::BEQ:  take = ex_mem.Zero;                        break;
            case Instr::BNE:  take = !ex_mem.Zero;                       break;
            case Instr::BLT:  take = ex_mem.sign ^ ex_mem.overflow;      break;
            case Instr::BGE:  take = !(ex_mem.sign ^ ex_mem.overflow);   break;
            case Instr::BLTU: take = !ex_mem.carry;                      break;
            case Instr::BGEU: take = ex_mem.carry;                       break;
            default: break;
        }
        if (take) {
            mem_wb_nxt = {};
            return;
        }
    }

    if (ex_mem.MemRead || ex_mem.MemWrite) {
        mem_wait = 2;  // 内存访问延迟
    }

    u32 mval = 0;
    if (ex_mem.MemRead) {
        u32 a = ex_mem.alu_result;
        switch (ex_mem.op) {
            case Instr::LB:
                mval = static_cast<u32>(static_cast<i32>(
                    static_cast<i8>(mem.read_byte(a)))); break;
            case Instr::LBU: mval = mem.read_byte(a); break;
            case Instr::LH:
                mval = static_cast<u32>(static_cast<i32>(
                    static_cast<i16>(mem.read_half(a)))); break;
            case Instr::LHU: mval = mem.read_half(a); break;
            case Instr::LW:  mval = mem.read_word(a); break;
            default: break;
        }
    }
    if (ex_mem.MemWrite) {
        u32 a = ex_mem.alu_result, d = ex_mem.store_data;
        switch (ex_mem.op) {
            case Instr::SB: mem.write_byte(a, d & 0xFF); break;
            case Instr::SH: mem.write_half(a, d & 0xFFFF); break;
            case Instr::SW: mem.write_word(a, d); break;
            default: break;
        }
    }

    mem_wb_nxt.valid     = true;
    mem_wb_nxt.rd        = ex_mem.rd;
    mem_wb_nxt.RegWrite  = ex_mem.RegWrite;
    mem_wb_nxt.MemToReg  = ex_mem.MemRead;

    if (ex_mem.op == Instr::JAL || ex_mem.op == Instr::JALR) {
        mem_wb_nxt.alu_result = ex_mem.jal_ret_addr;
        mem_wb_nxt.mem_data   = 0;
    } else if (ex_mem.MemRead) {
        mem_wb_nxt.mem_data   = mval;
        mem_wb_nxt.alu_result = 0;
    } else {
        mem_wb_nxt.alu_result = ex_mem.alu_result;
        mem_wb_nxt.mem_data   = 0;
    }
}

void CPU::stage_WB() {
    if (!mem_wb.valid) return;
    if (mem_wb.RegWrite && mem_wb.rd != 0) {
        u32 val = mem_wb.MemToReg ? mem_wb.mem_data : mem_wb.alu_result;
        write_reg(mem_wb.rd, val);
    }
}

void CPU::step() {
    if (mem_wait > 0) {
        mem_wait--;
        if_id_nxt = if_id;
        id_ex_nxt = id_ex;
        ex_mem_nxt = ex_mem;
        mem_wb_nxt = mem_wb;
        if_id  = if_id_nxt;
        id_ex  = id_ex_nxt;
        ex_mem = ex_mem_nxt;
        mem_wb = mem_wb_nxt;
        return;
    }

    bool branch_take = false;
    if (ex_mem.valid && ex_mem.Branch) {
        switch (ex_mem.op) {
            case Instr::BEQ:  branch_take = ex_mem.Zero;                      break;
            case Instr::BNE:  branch_take = !ex_mem.Zero;                     break;
            case Instr::BLT:  branch_take = ex_mem.sign ^ ex_mem.overflow;    break;
            case Instr::BGE:  branch_take = !(ex_mem.sign ^ ex_mem.overflow); break;
            case Instr::BLTU: branch_take = !ex_mem.carry;                    break;
            case Instr::BGEU: branch_take = ex_mem.carry;                     break;
            default: break;
        }
    }
    if (ex_mem.valid && (branch_take || ex_mem.Jump)) {
        if_id = {};
        id_ex = {};
        pc = ex_mem.Jump ? ex_mem.alu_result
                         : ex_mem.branch_target;
    }

    bool alu_stall = false;
    if (id_ex.valid && id_ex.RegWrite && id_ex.rd != 0 && !id_ex.MemRead) {
        DecodedInstr next = decoder.decode(if_id.instruction);
        if (if_id.valid && (id_ex.rd == next.rs1 || id_ex.rd == next.rs2))
            alu_stall = true;
    }

    if (id_ex.valid && id_ex.RegWrite && id_ex.rd != 0 && id_ex.MemRead) {
        DecodedInstr next = decoder.decode(if_id.instruction);
        if (if_id.valid && (id_ex.rd == next.rs1 || id_ex.rd == next.rs2))
            load_stall = 2;
    }

    if (ex_mem.valid && ex_mem.MemRead && ex_mem.RegWrite && ex_mem.rd != 0) {
        if (id_ex.valid && (ex_mem.rd == id_ex.rs1 || ex_mem.rd == id_ex.rs2))
            load_stall = 1;
        DecodedInstr nxt = decoder.decode(if_id.instruction);
        if (if_id.valid && (ex_mem.rd == nxt.rs1 || ex_mem.rd == nxt.rs2))
            load_stall = 1;
    }

    if (alu_stall || load_stall > 0) {
        if (load_stall > 0) load_stall--;
        if_id_nxt = if_id;
        id_ex_nxt = {};
        stage_EX();
        stage_MEM();
        stage_WB();
    } else {
        stage_IF();
        stage_ID();
        stage_EX();
        stage_MEM();
        stage_WB();
    }

    if_id  = if_id_nxt;
    id_ex  = id_ex_nxt;
    ex_mem = ex_mem_nxt;
    mem_wb = mem_wb_nxt;
}
