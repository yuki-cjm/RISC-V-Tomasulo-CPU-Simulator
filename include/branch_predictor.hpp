#pragma once

#include "type.h"

class BranchPredictor {
    static const u32 BIMOD_SIZE  = 2048;
    static const u32 GSHARE_SIZE = 2048;
    static const u32 CHOICE_SIZE = 2048;
    static const u32 GHR_BITS    = 11;
    static const u32 GHR_MASK    = (1u << GHR_BITS) - 1;

    u8  old_bimodal[BIMOD_SIZE];
    u8  old_gshare[GSHARE_SIZE];
    u8  old_choice[CHOICE_SIZE];
    u32 old_ghr;

    u8  new_bimodal[BIMOD_SIZE];
    u8  new_gshare[GSHARE_SIZE];
    u8  new_choice[CHOICE_SIZE];
    u32 new_ghr;

    static void inc(u8& c) { if (c < 3) c++; }
    static void dec(u8& c) { if (c > 0) c--; }

  public:
    BranchPredictor();

    void update();

    bool predict_old(u32 pc) const;
    bool bimodal_pred_old(u32 pc) const;
    bool gshare_pred_old(u32 pc) const;

    u32 get_old_ghr() const { return old_ghr; }

    void updateate_new(u32 pc, bool taken, u32 ghr_snapshot,
                  bool bimodal_pred, bool gshare_pred, bool mispredict);
};
