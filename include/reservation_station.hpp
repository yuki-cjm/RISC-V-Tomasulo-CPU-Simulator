#pragma once

#include "type.h"

struct RS_Entry {
    bool  busy;
    Instr op;
    u32   Vj, Vk;        // 操作数值
    u32   imm;
    u32   pc;            // 指令 PC
    int   Qj, Qk;        // 等待的标签，-1 表示已就绪
    int   dest_tag;      // 本指令标签
    int   rob_idx;       // ROB 中索引
    int   lsq_idx;       // LSQ 中索引
    int   cycle_left;    // 剩余执行周期
    u8    dest_reg;      // 目标寄存器
    bool  RegWrite;
    bool  MemRead;
    bool  MemWrite;
    bool  Branch;
    bool  Jump;
};

class ReservationStation {
    static const int MAX_SIZE = 16;
    RS_Entry old_[MAX_SIZE];
    RS_Entry new_[MAX_SIZE];
    int size_;

    static RS_Entry empty_entry();

  public:
    ReservationStation(int size);

    void update();
    int size() const;
    const RS_Entry& entry_old(int i) const;
    RS_Entry& entry_new(int i);
    bool full_old() const;
    int find_by_tag_old(int tag) const;
    int alloc_new(int tag);
    void tick_all_alu_new();
    void wakeup_cdb_new(int tag, u32 value);
    void free_new(int i);
    void flush_younger_than_new(int branch_tag);
};
