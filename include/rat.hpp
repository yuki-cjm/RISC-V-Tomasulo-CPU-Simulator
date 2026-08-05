#pragma once

#include "type.h"

// ============================================================================
// RAT_Entry: 寄存器别名表条目
// ============================================================================
struct RAT_Entry {
    int  tag;      // 生产者标签，-1 表示未被重命名
    bool busy;     // 是否被重命名
    u32  value;    // 若 ready 则存值
    bool ready;    // 值是否已通过 CDB 到达
};

// ============================================================================
// RegAliasTable (RAT): 寄存器别名表，支持重命名和 CDB 捕获
// ============================================================================
class RegAliasTable {
    RAT_Entry old_[REG_COUNT];
    RAT_Entry new_[REG_COUNT];

  public:
    RegAliasTable() {
        for (u32 i = 0; i < REG_COUNT; ++i)
            old_[i] = new_[i] = {-1, false, 0, false};
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

    // ---- 读取旧状态 ----
    int  tag_o(u8 r)   const { return old_[r].tag; }
    bool busy_o(u8 r)  const { return old_[r].busy; }
    bool ready_o(u8 r) const { return old_[r].ready; }
    u32  val_o(u8 r)   const { return old_[r].value; }
    const RAT_Entry& entry_o(u8 r) const { return old_[r]; }

    // ---- 写入新状态（issue：重命名目标寄存器） ----
    void rename_n(u8 r, int t) {
        if (r != 0)
            new_[r] = {t, true, 0, false};
    }

    // ---- 写入新状态（writeback：捕获 CDB 广播的值） ----
    void capture_cdb_n(int tag, u32 value) {
        for (u32 i = 0; i < REG_COUNT; ++i) {
            if (new_[i].tag == tag) {
                new_[i].value = value;
                new_[i].ready = true;
            }
        }
    }

    // ---- 写入新状态（commit：清除重命名） ----
    // 只有当目标寄存器在本周期没有被 issue 重新命名时才清除
    // issued_regs 是 issue 阶段重命名的寄存器掩码
    void commit_clear_n(u8 r, int t) {
        if (r != 0 && new_[r].tag == t) {
            new_[r] = {-1, false, 0, false};
        }
    }

    // ---- 写入新状态（mispredict flush：完全清空） ----
    void flush_all_n() {
        for (u32 i = 0; i < REG_COUNT; ++i)
            new_[i] = {-1, false, 0, false};
    }
};
