#pragma once

#include "type.h"
#include "memory.hpp"
#include "decoder.hpp"
#include "pipeline_regs.hpp"

class CPU {
private:
    Memory&   mem;
    Decoder   decoder;
    u32 reg[32]{};
    u32 pc = 0;
    bool finished = false;

    IF_ID   if_id;
    ID_EX   id_ex;
    EX_MEM  ex_mem;
    MEM_WB  mem_wb;

    IF_ID   if_id_nxt;
    ID_EX   id_ex_nxt;
    EX_MEM  ex_mem_nxt;
    MEM_WB  mem_wb_nxt;

    int  load_stall = 0;   // load-use 多周期 stall 计数
    int  mem_wait   = 0;   // 内存访问剩余等待周期 (load/store 共需 3 cycles)

    u32  read_reg(u8 idx);
    void write_reg(u8 idx, u32 val);
    u32  forward_rs1(u8 rs1);
    u32  forward_rs2(u8 rs2);

    void stage_IF();
    void stage_ID();
    void stage_EX();
    void stage_MEM();
    void stage_WB();

public:
    CPU(Memory& m) : mem(m) {}
    void step();
    bool is_finished() const { return finished; }
};
