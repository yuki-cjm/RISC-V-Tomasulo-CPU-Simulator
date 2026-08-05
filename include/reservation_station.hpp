#pragma once

#include "type.h"

// ============================================================================
// RS_Entry: 保留站中的一条目
// ============================================================================
struct RS_Entry {
    bool  busy;
    bool  completed;     // 执行完成，等待 writeback 广播
    Instr op;
    u32   Vj, Vk;        // 操作数值（若 Qj/Qk == -1 则有效）
    u32   imm;
    u32   exec_result;   // 执行结果
    u32   pc;            // 指令 PC
    int   Qj, Qk;        // 等待的标签，-1 表示已就绪
    int   dest_tag;      // 本指令标签
    int   rob_idx;       // ROB 中索引
    int   lsq_idx;       // LSQ 中索引（访存用）
    int   cycle_left;    // 剩余执行周期
    u8    dest_reg;      // 目标寄存器
    bool  RegWrite;
    bool  MemRead;
    bool  MemWrite;
    bool  Branch;
    bool  Jump;
};

// ============================================================================
// ReservationStation: 保留站，缓存已发射但未完成的指令
// ============================================================================
class ReservationStation {
    static const int MAX_SZ = 16;
    RS_Entry old_[MAX_SZ];
    RS_Entry new_[MAX_SZ];
    int       size_;
    const char* name_;

    static RS_Entry empty_entry();

  public:
    ReservationStation(int sz, const char* nm);

    // ---- 状态管理 ----
    void snap();
    void upd();

    // ---- 读取旧状态 ----
    int  size()              const;
    const char* name()       const;
    const RS_Entry& entry_o(int i) const;

    bool full_o() const;

    // 查找操作数就绪且未完成的条目
    int find_ready_o() const;

    // 查找已完成但未释放的条目（用于 CDB 生成）
    int find_completed_o() const;

    // 按标签查找
    int find_by_tag_o(int tag) const;

    // ---- 写入新状态（issue：分配条目） ----
    int alloc_n(int tag);

    RS_Entry& entry_n(int i);

    // ---- 写入新状态（execute：递减周期计数） ----
    void tick_cycle_n(int i);

    // 批量递减所有非访存、非分支条目的 cycle_left
    void tick_all_alu_n();

    // ---- 写入新状态（execute：标记完成，结果已算出） ----
    void mark_completed_n(int i, u32 result);

    // ---- 写入新状态（writeback：捕获 CDB 值，唤醒操作数） ----
    void wakeup_cdb_n(int tag, u32 value);

    // ---- 写入新状态（writeback：释放已完成的条目） ----
    void free_n(int i);

    void free_by_tag_n(int tag);

    // ---- 写入新状态（mispredict flush：刷掉被 flush 的 ROB 条目对应的 RS 条目） ----
    void flush_by_rob_n(int rob_idx);

    // 刷掉所有条目
    void flush_all_n();

    // 分支预测失败时，只能刷掉比分支年轻的指令。
    // tag 单调递增，因此 dest_tag > branch_tag 表示 younger。
    void flush_younger_than_n(int branch_tag);
};
