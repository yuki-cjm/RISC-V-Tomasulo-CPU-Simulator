#pragma once

#include "type.h"

// ============================================================================
// BranchPredictor: Tournament 预测器（Alpha 21264 风格）
//
// 三张表：
//   Bimodal PHT:  PC 索引，适合方向一致的分支
//   Gshare PHT:   PC XOR GHR 索引，适合关联分支
//   Choice table: GHR 索引的 2-bit 计数器，动态选择信任哪个预测器
//
// 预测流程：
//   1. Bimodal 和 Gshare 各自独立预测
//   2. Choice table 选择最终预测（0-1→Bimodal, 2-3→Gshare）
//
// 更新流程（both-train）：
//   1. 两张 PHT 都用实际方向训练（保持 warm）
//   2. Choice table：一正一误 → 偏向正确方；同对同错 → 不变
//   3. GHR：正确预测时追加；误预测时从 snapshot 修复
//
// update_n 使用预测时刻的 GHR（ghr_snapshot）索引 PHT，保证训练一致性。
// bimod_pred/gshare_pred 存入 ROB，在执行阶段用于 choice table 更新。
// ============================================================================
class BranchPredictor {
    static const u32 BIMOD_SIZE  = 512;
    static const u32 GSHARE_SIZE = 512;
    static const u32 CHOICE_SIZE = 512;
    static const u32 GHR_BITS    = 10;
    static const u32 GHR_MASK    = (1u << GHR_BITS) - 1;

    // old state (clock cycle start)
    u8  old_bimod_[BIMOD_SIZE];
    u8  old_gshare_[GSHARE_SIZE];
    u8  old_choice_[CHOICE_SIZE];
    u32 old_ghr_;

    // new state (clock cycle writes)
    u8  new_bimod_[BIMOD_SIZE];
    u8  new_gshare_[GSHARE_SIZE];
    u8  new_choice_[CHOICE_SIZE];
    u32 new_ghr_;

    static void inc(u8& c) { if (c < 3) c++; }
    static void dec(u8& c) { if (c > 0) c--; }

  public:
    BranchPredictor();

    // ---- 状态管理 ----
    void snap();
    void upd();

    // ---- 预测（读旧状态） ----
    // 返回最终预测。同时需要获取 bimod_pred 和 gshare_pred 用于存入 ROB。
    bool predict_o(u32 pc) const;
    bool bimod_pred_o(u32 pc) const;   // Bimodal 单独预测
    bool gshare_pred_o(u32 pc) const;  // Gshare 单独预测

    // ---- 读取旧 GHR（issue 阶段存入 ROB） ----
    u32 get_old_ghr() const { return old_ghr_; }

    // ---- 更新（写新状态） ----
    // ghr_snapshot:  预测时刻的 GHR，用于 PHT 索引
    // bimod_pred:    Bimodal 预测器的预测（用于 choice 更新）
    // gshare_pred:   Gshare 预测器的预测（用于 choice 更新）
    // mispredict:    是否误预测，决定 GHR 修复策略
    void update_n(u32 pc, bool taken, u32 ghr_snapshot,
                  bool bimod_pred, bool gshare_pred, bool mispredict);
};
