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

    static RS_Entry empty_entry() {
        return {false, false, Instr::UNKNOWN, 0, 0, 0, 0, 0,
                -1, -1, -1, -1, -1, 1, 0,
                false, false, false, false, false};
    }

  public:
    ReservationStation(int sz, const char* nm) : size_(sz), name_(nm) {
        for (int i = 0; i < MAX_SZ; ++i)
            old_[i] = new_[i] = empty_entry();
    }

    // ---- 状态管理 ----
    void snap() {
        for (int i = 0; i < size_; ++i)
            new_[i] = old_[i];
    }

    void upd() {
        for (int i = 0; i < size_; ++i)
            old_[i] = new_[i];
    }

    // ---- 读取旧状态 ----
    int  size()              const { return size_; }
    const char* name()       const { return name_; }
    const RS_Entry& entry_o(int i) const { return old_[i]; }

    bool full_o() const {
        for (int i = 0; i < size_; ++i)
            if (!old_[i].busy) return false;
        return true;
    }

    // 查找操作数就绪且未完成的条目
    int find_ready_o() const {
        for (int i = 0; i < size_; ++i) {
            if (old_[i].busy && old_[i].Qj == -1 && old_[i].Qk == -1
                && !old_[i].completed)
                return i;
        }
        return -1;
    }

    // 查找已完成但未释放的条目（用于 CDB 生成）
    int find_completed_o() const {
        for (int i = 0; i < size_; ++i) {
            if (old_[i].busy && old_[i].completed)
                return i;
        }
        return -1;
    }

    // 按标签查找
    int find_by_tag_o(int tag) const {
        for (int i = 0; i < size_; ++i)
            if (old_[i].busy && old_[i].dest_tag == tag)
                return i;
        return -1;
    }

    // ---- 写入新状态（issue：分配条目） ----
    int alloc_n(int tag) {
        for (int i = 0; i < size_; ++i) {
            // issue 阶段只能看到周期开始时的 old_ 空闲槽位。
            // 不能复用本周期 writeback/flush 刚在 new_ 中释放的槽，
            // 否则 step_writeback 与 step_issue 的调用顺序会改变发射位置甚至周期数。
            if (!old_[i].busy && !new_[i].busy) {
                new_[i] = empty_entry();
                new_[i].busy = true;
                new_[i].dest_tag = tag;
                return i;
            }
        }
        return -1;
    }

    RS_Entry& entry_n(int i) { return new_[i]; }

    // ---- 写入新状态（execute：递减周期计数） ----
    void tick_cycle_n(int i) {
        if (old_[i].busy && new_[i].busy
            && new_[i].dest_tag == old_[i].dest_tag
            && old_[i].cycle_left > 0)
            new_[i].cycle_left--;
    }

    // 批量递减所有非访存、非分支条目的 cycle_left
    void tick_all_alu_n() {
        for (int i = 0; i < size_; ++i) {
            if (old_[i].busy && new_[i].busy
                && new_[i].dest_tag == old_[i].dest_tag
                && !old_[i].MemRead && !old_[i].MemWrite
                && !old_[i].Branch && !old_[i].Jump
                && old_[i].cycle_left > 0)
                new_[i].cycle_left--;
        }
    }

    // ---- 写入新状态（execute：标记完成，结果已算出） ----
    void mark_completed_n(int i, u32 result) {
        new_[i].completed = true;
        new_[i].exec_result = result;
    }

    // ---- 写入新状态（writeback：捕获 CDB 值，唤醒操作数） ----
    void wakeup_cdb_n(int tag, u32 value) {
        for (int i = 0; i < size_; ++i) {
            if (!new_[i].busy) continue;
            if (new_[i].Qj == tag) { new_[i].Vj = value; new_[i].Qj = -1; }
            if (new_[i].Qk == tag) { new_[i].Vk = value; new_[i].Qk = -1; }
        }
    }

    // ---- 写入新状态（writeback：释放已完成的条目） ----
    void free_n(int i) {
        new_[i] = empty_entry();
    }

    void free_by_tag_n(int tag) {
        for (int i = 0; i < size_; ++i) {
            if (new_[i].busy && new_[i].dest_tag == tag) {
                new_[i] = empty_entry();
                return;
            }
        }
    }

    // ---- 写入新状态（mispredict flush：刷掉被 flush 的 ROB 条目对应的 RS 条目） ----
    void flush_by_rob_n(int rob_idx) {
        for (int i = 0; i < size_; ++i) {
            if (new_[i].busy && new_[i].rob_idx == rob_idx)
                new_[i] = empty_entry();
        }
    }

    // 刷掉所有条目
    void flush_all_n() {
        for (int i = 0; i < size_; ++i)
            new_[i] = empty_entry();
    }

    // 分支预测失败时，只能刷掉比分支年轻的指令。
    // tag 单调递增，因此 dest_tag > branch_tag 表示 younger。
    void flush_younger_than_n(int branch_tag) {
        for (int i = 0; i < size_; ++i) {
            if (new_[i].busy && new_[i].dest_tag > branch_tag)
                new_[i] = empty_entry();
        }
    }
};
