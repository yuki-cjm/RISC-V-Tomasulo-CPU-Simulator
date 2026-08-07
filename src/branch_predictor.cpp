#include "branch_predictor.hpp"

BranchPredictor::BranchPredictor() {
    for (u32 i = 0; i < BIMOD_SIZE; ++i)
        old_bimodal[i] = new_bimodal[i] = 1;
    for (u32 i = 0; i < GSHARE_SIZE; ++i)
        old_gshare[i] = new_gshare[i] = 1;
    for (u32 i = 0; i < CHOICE_SIZE; ++i)
        old_choice[i] = new_choice[i] = 1;
    old_ghr = new_ghr = 0;
}

void BranchPredictor::update() {
    for (u32 i = 0; i < BIMOD_SIZE; ++i)
        old_bimodal[i] = new_bimodal[i];
    for (u32 i = 0; i < GSHARE_SIZE; ++i)
        old_gshare[i] = new_gshare[i];
    for (u32 i = 0; i < CHOICE_SIZE; ++i)
        old_choice[i] = new_choice[i];
    old_ghr = new_ghr;
}

bool BranchPredictor::bimodal_pred_old(u32 pc) const {
    u32 idx = (pc >> 2) % BIMOD_SIZE;
    return old_bimodal[idx] >= 2;
}

bool BranchPredictor::gshare_pred_old(u32 pc) const {
    u32 idx = ((pc >> 2) ^ old_ghr) % GSHARE_SIZE;
    return old_gshare[idx] >= 2;
}

bool BranchPredictor::predict_old(u32 pc) const {
    u32 choice_idx = old_ghr % CHOICE_SIZE;
    if (old_choice[choice_idx] >= 2)
        return gshare_pred_old(pc);
    else
        return bimodal_pred_old(pc);
}

void BranchPredictor::updateate_new(u32 pc, bool taken, u32 ghr_snapshot,
                                bool bimodal_pred, bool gshare_pred, bool mispredict) {
    u32 bimod_idx = (pc >> 2) % BIMOD_SIZE;
    if (taken) inc(new_bimodal[bimod_idx]);
    else       dec(new_bimodal[bimod_idx]);

    u32 gshare_idx = ((pc >> 2) ^ ghr_snapshot) % GSHARE_SIZE;
    if (taken) inc(new_gshare[gshare_idx]);
    else       dec(new_gshare[gshare_idx]);

    bool bimod_correct  = (bimodal_pred == taken);
    bool gshare_correct = (gshare_pred == taken);
    u32  choice_idx     = ghr_snapshot % CHOICE_SIZE;

    if (bimod_correct && !gshare_correct)
        dec(new_choice[choice_idx]);
    else if (!bimod_correct && gshare_correct)
        inc(new_choice[choice_idx]);

    if (mispredict) {
        new_ghr = ((ghr_snapshot << 1) | (taken ? 1u : 0u)) & GHR_MASK;
    } else {
        new_ghr = ((new_ghr << 1) | (taken ? 1u : 0u)) & GHR_MASK;
    }
}