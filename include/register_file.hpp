#pragma once

#include "type.h"

// ============================================================================
// RegisterFile: 32 个整数寄存器，x0 恒为 0，支持 old/new 双状态
// ============================================================================
class RegisterFile {
    u32 old_[REG_COUNT];
    u32 new_[REG_COUNT];

  public:
    RegisterFile() {
        for (u32 i = 0; i < REG_COUNT; ++i)
            old_[i] = new_[i] = 0;
    }

    // ---- 状态管理 ----
    void snap() {
        for (u32 i = 0; i < REG_COUNT; ++i)
            new_[i] = old_[i];
    }

    void upd() {
        for (u32 i = 0; i < REG_COUNT; ++i)
            old_[i] = new_[i];
    }

    // ---- 读取旧状态（本周期只读） ----
    u32 read_o(u8 reg) const {
        return (reg == 0) ? 0 : old_[reg];
    }

    // ---- 读取新状态 ----
    u32 read_n(u8 reg) const {
        return (reg == 0) ? 0 : new_[reg];
    }

    // ---- 写入新状态（commit 时调用） ----
    void write_n(u8 reg, u32 val) {
        if (reg != 0)
            new_[reg] = val;
    }
};
