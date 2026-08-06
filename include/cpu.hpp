#pragma once

#include "type.h"
#include "memory.hpp"
#include "decoder.hpp"
#include "register_file.hpp"
#include "rat.hpp"
#include "reservation_station.hpp"
#include "reorder_buffer.hpp"
#include "load_store_queue.hpp"
#include "branch_predictor.hpp"
#include "cdb.hpp"

// ============================================================================
// CPU: Tomasulo 架构顶层控制器
//
// 每周期执行流程（步骤 2-5 可任意交换）：
//   1. snap_all()        — 所有部件保存旧状态
//   2. cdb = precompute_cdb() — 从旧状态计算本周期 CDB 广播
//   3-6. 四个步骤（任意顺序均可）：
//        step_issue(cdb)
//        step_execute(cdb)
//        step_writeback(cdb)
//        step_commit(cdb)
//   7. upd_all()         — 所有部件更新到新状态
//
// 关键设计：每个步骤只读 _o 旧状态 + CDB，只写 _n 新状态。
// 不同步骤写入的状态字段互不重叠，因此顺序可任意交换。
//
// 步骤排列：perm 取 0-23，对应 24 种排列。
//   0=I 1=E 2=W 3=C，用 Lehmer 码解码。
// ============================================================================

// 步骤排列查找表：24 种排列，按字典序
// perm_table[p][0..3] = 步骤类型 (0=Issue, 1=Execute, 2=Writeback, 3=Commit)
constexpr int PERM_COUNT = 24;
extern const int PERM_TABLE[PERM_COUNT][4];

class CPU {
    // ---- 外部模块 ----
    Memory&            mem;
    Decoder            decoder;

    // ---- Tomasulo 组件 ----
    RegisterFile       rf;
    RegAliasTable      rat;
    ReservationStation alu_rs;
    ReservationStation ld_rs;
    ReservationStation st_rs;
    ReservationStation br_rs;
    ReorderBuffer      rob;
    LoadStoreQueue     lsq;
    BranchPredictor    bp;

    // ---- 步骤排列 ----
    int perm_;  // 0-23

    // ---- 取指缓冲区 (old/new) ----
    struct FetchBuf { u32 pc; u32 instr; bool valid; };
    FetchBuf fb_o, fb_n;

    // ---- CPU 级状态 (old/new) ----
    u32  pc_o,  pc_n;
    int  tag_o, tag_n;       // 递增标签计数器
    bool done;               // 是否停机
    u64  tb_o,  tb_n;        // 总分支数
    u64  cb_o,  cb_n;        // 预测正确分支数

    // 标记本周期 issue 阶段重命名了哪些寄存器
    // 用于 commit 阶段避免冲突：若 rd 在本周期被 issue 重命名，commit 不清除
    bool issued_rd_o_[REG_COUNT];
    bool issued_rd_n_[REG_COUNT];

    // JALR 前端停顿标志：发射 JALR 后等待目标地址解析
    bool stall_frontend_o_, stall_frontend_n_;

    // ---- 辅助方法 ----
    ReservationStation& pick_rs(Instr op);
    static bool has_rs1(Instr op);
    static bool has_rs2(Instr op);

    // 从旧状态计算 CDB
    CDB_Entry precompute_cdb();

    // 取指
    void step_fetch();

    // 四个流水线步骤（可任意交换顺序）
    void step_issue(const CDB_Entry& cdb);
    void step_execute(const CDB_Entry& cdb);
    void step_writeback(const CDB_Entry& cdb);
    void step_commit(const CDB_Entry& cdb);

    // 状态管理
    void snap_all();
    void upd_all();

  public:
    CPU(Memory& m, int perm = 0);

    // 执行一个时钟周期
    void step();

    bool is_finished() const { return done; }
    u64  total_br()    const { return tb_o; }
    u64  correct_br()  const { return cb_o; }
};
