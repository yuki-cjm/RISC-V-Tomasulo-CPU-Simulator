#pragma once

#include "type.h"

// ============================================================================
// BranchPredictor: 2-bit 饱和计数器分支预测器
// ============================================================================
class BranchPredictor {
    static const u32 BHT_SIZE = 64;
    u8 old_[BHT_SIZE];  // 0=强不跳, 1=弱不跳, 2=弱跳, 3=强跳
    u8 new_[BHT_SIZE];

  public:
    BranchPredictor();

    // ---- 状态管理 ----
    void snap();
    void upd();

    // ---- 预测（读旧状态） ----
    bool predict_o(u32 pc) const;

    // ---- 更新（写新状态） ----
    void update_n(u32 pc, bool taken);
};
