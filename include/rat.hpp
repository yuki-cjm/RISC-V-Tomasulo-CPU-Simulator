#pragma once

#include "type.h"

struct RAT_Entry {
    int  tag;      // 生产者标签，-1 表示未被重命名
    bool busy;     // 是否被重命名
    u32  value;    // 若 ready 则存值
    bool ready;    // 值是否已通过 CDB 到达
};

class RegAliasTable {
    RAT_Entry old_[REG_COUNT];
    RAT_Entry new_[REG_COUNT];

  public:
    RegAliasTable();

    void update();

    int  tag_old(u8 r)   const;
    bool busy_old(u8 r)  const;
    bool ready_old(u8 r) const;
    u32  val_old(u8 r)   const;
    const RAT_Entry& entry_old(u8 r) const;

    // issue：重命名目标寄存器
    void rename_new(u8 r, int t);

    // writeback：捕获 CDB 广播的值
    void capture_cdb_new(int tag, u32 value);

    // commit：清除重命名
    void commit_clear_new(u8 r, int t);

    // mispredict flush：完全清空
    void flush_all_new();
};
