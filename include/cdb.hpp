#pragma once

#include "type.h"

// CDB: Common Data Bus
struct CDB_Entry {
    bool valid;          // 本周期是否有广播
    u32  value;          // 广播的值
    int  tag;            // 生产者标签
    bool mispredicted;   // 分支预测失败
    u32  correct_pc;     // 预测失败时的正确 PC
    int  branch_rob;     // 分支在 ROB 中的索引（用于 flush）
};

CDB_Entry make_empty_cdb();
