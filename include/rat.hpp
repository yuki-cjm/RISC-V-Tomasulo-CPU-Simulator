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
    RegAliasTable();

    // ---- 状态管理 ----
    void snap();
    void upd();

    // ---- 读取旧状态 ----
    int  tag_o(u8 r)   const;
    bool busy_o(u8 r)  const;
    bool ready_o(u8 r) const;
    u32  val_o(u8 r)   const;
    const RAT_Entry& entry_o(u8 r) const;

    // ---- 写入新状态（issue：重命名目标寄存器） ----
    void rename_n(u8 r, int t);

    // ---- 写入新状态（writeback：捕获 CDB 广播的值） ----
    void capture_cdb_n(int tag, u32 value);

    // ---- 写入新状态（commit：清除重命名） ----
    // 只有当目标寄存器在本周期没有被 issue 重新命名时才清除
    // issued_regs 是 issue 阶段重命名的寄存器掩码
    void commit_clear_n(u8 r, int t);

    // ---- 写入新状态（mispredict flush：完全清空） ----
    void flush_all_n();
};
