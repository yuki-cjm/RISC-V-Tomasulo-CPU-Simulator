#include "branch_predictor.hpp"

BranchPredictor::BranchPredictor() {
    for (u32 i = 0; i < BHT_SIZE; ++i)
        old_[i] = new_[i] = 1;
}

void BranchPredictor::snap() {
    for (u32 i = 0; i < BHT_SIZE; ++i)
        new_[i] = old_[i];
}

void BranchPredictor::upd() {
    for (u32 i = 0; i < BHT_SIZE; ++i)
        old_[i] = new_[i];
}

bool BranchPredictor::predict_o(u32 pc) const {
    u32 idx = (pc >> 2) % BHT_SIZE;
    return old_[idx] >= 2;
}

void BranchPredictor::update_n(u32 pc, bool taken) {
    u32 idx = (pc >> 2) % BHT_SIZE;
    if (taken) {
        if (new_[idx] < 3) new_[idx]++;
    } else {
        if (new_[idx] > 0) new_[idx]--;
    }
}