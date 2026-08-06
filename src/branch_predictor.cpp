#include "branch_predictor.hpp"

BranchPredictor::BranchPredictor() {
    for (u32 i = 0; i < BIMOD_SIZE; ++i)
        old_bimod_[i] = new_bimod_[i] = 1;   // 弱不跳
    for (u32 i = 0; i < GSHARE_SIZE; ++i)
        old_gshare_[i] = new_gshare_[i] = 1;  // 弱不跳
    for (u32 i = 0; i < CHOICE_SIZE; ++i)
        old_choice_[i] = new_choice_[i] = 1;  // 初始偏向 Bimodal
    old_ghr_ = new_ghr_ = 0;
}

void BranchPredictor::snap() {
    for (u32 i = 0; i < BIMOD_SIZE; ++i)
        new_bimod_[i] = old_bimod_[i];
    for (u32 i = 0; i < GSHARE_SIZE; ++i)
        new_gshare_[i] = old_gshare_[i];
    for (u32 i = 0; i < CHOICE_SIZE; ++i)
        new_choice_[i] = old_choice_[i];
    new_ghr_ = old_ghr_;
}

void BranchPredictor::upd() {
    for (u32 i = 0; i < BIMOD_SIZE; ++i)
        old_bimod_[i] = new_bimod_[i];
    for (u32 i = 0; i < GSHARE_SIZE; ++i)
        old_gshare_[i] = new_gshare_[i];
    for (u32 i = 0; i < CHOICE_SIZE; ++i)
        old_choice_[i] = new_choice_[i];
    old_ghr_ = new_ghr_;
}

// ---- 预测 ----

bool BranchPredictor::bimod_pred_o(u32 pc) const {
    u32 idx = (pc >> 2) % BIMOD_SIZE;
    return old_bimod_[idx] >= 2;
}

bool BranchPredictor::gshare_pred_o(u32 pc) const {
    u32 idx = ((pc >> 2) ^ old_ghr_) % GSHARE_SIZE;
    return old_gshare_[idx] >= 2;
}

bool BranchPredictor::predict_o(u32 pc) const {
    u32 choice_idx = old_ghr_ % CHOICE_SIZE;
    // choice 0-1 → 信任 Bimodal; 2-3 → 信任 Gshare
    if (old_choice_[choice_idx] >= 2)
        return gshare_pred_o(pc);
    else
        return bimod_pred_o(pc);
}

// ---- 更新 ----

void BranchPredictor::update_n(u32 pc, bool taken, u32 ghr_snapshot,
                                bool bimod_pred, bool gshare_pred, bool mispredict) {
    // 1. 训练 Bimodal PHT（使用 PC 索引，与 GHR 无关）
    u32 bimod_idx = (pc >> 2) % BIMOD_SIZE;
    if (taken) inc(new_bimod_[bimod_idx]);
    else       dec(new_bimod_[bimod_idx]);

    // 2. 训练 Gshare PHT（使用预测时刻的 GHR）
    u32 gshare_idx = ((pc >> 2) ^ ghr_snapshot) % GSHARE_SIZE;
    if (taken) inc(new_gshare_[gshare_idx]);
    else       dec(new_gshare_[gshare_idx]);

    // 3. 更新 Choice table
    //    一正一误 → 偏向正确方；同对同错 → 不变
    bool bimod_correct  = (bimod_pred == taken);
    bool gshare_correct = (gshare_pred == taken);
    u32  choice_idx     = ghr_snapshot % CHOICE_SIZE;

    if (bimod_correct && !gshare_correct)
        dec(new_choice_[choice_idx]);  // 偏向 Bimodal
    else if (!bimod_correct && gshare_correct)
        inc(new_choice_[choice_idx]);  // 偏向 Gshare
    // else: 同对或同错，不动 choice

    // 4. 更新 GHR
    if (mispredict) {
        // 误预测：从 snapshot 修复 GHR，消除错误路径污染
        new_ghr_ = ((ghr_snapshot << 1) | (taken ? 1u : 0u)) & GHR_MASK;
    } else {
        // 正确预测：在当前 GHR 上追加实际方向
        new_ghr_ = ((new_ghr_ << 1) | (taken ? 1u : 0u)) & GHR_MASK;
    }
}