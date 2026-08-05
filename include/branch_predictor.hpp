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
    BranchPredictor() {
        for (u32 i = 0; i < BHT_SIZE; ++i)
            old_[i] = new_[i] = 1;  // 初始 weakly not taken
    }

    // ---- 状态管理 ----
    void snap() {
        for (u32 i = 0; i < BHT_SIZE; ++i)
            new_[i] = old_[i];
    }

    void upd() {
        for (u32 i = 0; i < BHT_SIZE; ++i)
            old_[i] = new_[i];
    }

    // ---- 预测（读旧状态） ----
    bool predict_o(u32 pc) const {
        u32 idx = (pc >> 2) % BHT_SIZE;
        return old_[idx] >= 2;
    }

    // ---- 更新（写新状态） ----
    void update_n(u32 pc, bool taken) {
        u32 idx = (pc >> 2) % BHT_SIZE;
        if (taken) {
            if (new_[idx] < 3) new_[idx]++;
        } else {
            if (new_[idx] > 0) new_[idx]--;
        }
    }
};
