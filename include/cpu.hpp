#pragma once

#include "type.h"
#include "memory.hpp"
#include "decoder.hpp"
#include "register.hpp"
#include "rat.hpp"
#include "reservation_station.hpp"
#include "reorder_buffer.hpp"
#include "load_store_queue.hpp"
#include "branch_predictor.hpp"
#include "cdb.hpp"
#include "alu.hpp"

class CPU {
    Memory&            mem;
    Decoder            decoder;

    Register           rf;
    RegAliasTable      rat;
    ReservationStation alu_rs;
    ReservationStation ld_rs;
    ReservationStation st_rs;
    ReservationStation br_rs;
    ReorderBuffer      rob;
    LoadStoreQueue     lsq;
    BranchPredictor    bp;
    ALU                alu;

    struct FetchBuf { u32 pc; u32 instr; bool valid; };
    FetchBuf fb_old, fb_new;

    u32  pc_old,  pc_new;
    int  tag_old, tag_new;
    bool finished;
    u64  tb_old,  tb_new;
    u64  cb_old,  cb_new;

    bool issued_rd_old[REG_COUNT];
    bool issued_rd_new[REG_COUNT];

    bool stall_frontend_old, stall_frontend_new;

    ReservationStation& pick_rs(Instr op);
    static bool has_rs1(Instr op);
    static bool has_rs2(Instr op);

    CDB_Entry precompute_cdb();

    void step_fetch();
    void step_issue(const CDB_Entry& cdb);
    void step_execute(const CDB_Entry& cdb);
    void step_writeback(const CDB_Entry& cdb);
    void step_commit(const CDB_Entry& cdb);

    void snap_all();
    void update_all();

  public:
    CPU(Memory& m);

    void step();

    bool is_finished() const { return finished; }
    u64  total_br()    const { return tb_old; }
    u64  correct_br()  const { return cb_old; }
};
