#pragma once

#include "type.h"

// ============================================================================
// LSQ_Entry: Load/Store 队列条目
// ============================================================================
struct LSQ_Entry {
    bool  busy;
    bool  is_store;
    bool  addr_ready;
    bool  data_ready;
    bool  completed;     // load 已得到值 / store 地址数据已就绪
    u32   addr;
    u32   value;
    int   rob_idx;
    int   tag;
    Instr op;
    int   mem_delay;
};

// ============================================================================
// LoadStoreQueue: 加载/存储队列，线性数组管理
// 已完成的 store 在 commit 写入内存后由 free_by_rob_n 释放。
// LSQ 满时阻塞发射，不回收未提交的 store。
// ============================================================================
class LoadStoreQueue {
    static const int MAX_SZ = 32;
    LSQ_Entry old_[MAX_SZ];
    LSQ_Entry new_[MAX_SZ];
    int cap_;

    static LSQ_Entry empty_entry() {
        return {false, false, false, false, false, 0, 0, -1, -1,
                Instr::UNKNOWN, 0};
    }

  public:
    LoadStoreQueue(int cap) : cap_(cap) {
        for (int i = 0; i < MAX_SZ; ++i)
            old_[i] = new_[i] = empty_entry();
    }

    void snap() { for (int i = 0; i < cap_; ++i) new_[i] = old_[i]; }
    void upd()  { for (int i = 0; i < cap_; ++i) old_[i] = new_[i]; }

    int  size() const { return cap_; }
    const LSQ_Entry& entry_o(int idx) const { return old_[idx]; }
    LSQ_Entry& entry_n(int idx) { return new_[idx]; }

    // LSQ 满 → 没有空闲槽位
    bool full_o() const {
        for (int i = 0; i < cap_; ++i)
            if (!old_[i].busy) return false;
        return true;
    }

    // 是否有更早的 store 地址未解析
    bool has_older_unresolved_store_o(int tag) const {
        for (int i = 0; i < cap_; ++i) {
            const LSQ_Entry& e = old_[i];
            if (e.busy && e.is_store && e.tag >= 0 && e.tag < tag
                && !e.addr_ready)
                return true;
        }
        return false;
    }

    // store-to-load forwarding：找 tag 最大（最新）的匹配 store
    bool forward_o(u32 addr, int tag, u32& result) const {
        int  best_tag = -1;
        bool found    = false;
        for (int i = 0; i < cap_; ++i) {
            const LSQ_Entry& e = old_[i];
            if (!e.busy || e.tag < 0 || e.tag >= tag) continue;
            if (e.is_store && e.addr_ready && e.addr == addr && e.data_ready) {
                if (e.tag > best_tag) {
                    best_tag = e.tag;
                    result   = e.value;
                    found    = true;
                }
            }
        }
        return found;
    }

    // 分配新条目（仅找空闲槽位，不回收未提交的 store）
    int alloc_n(int rob_idx, bool is_store) {
        for (int i = 0; i < cap_; ++i) {
            // 与 RS 一样，issue 只能占用 old_ 中已经空闲的槽位。
            // 若复用本周期 commit/flush 在 new_ 中释放的槽，会让
            // commit/writeback/issue 的调用顺序影响 LSQ 分配结果。
            if (!old_[i].busy && !new_[i].busy) {
                new_[i] = empty_entry();
                new_[i].busy = true;
                new_[i].is_store = is_store;
                new_[i].rob_idx = rob_idx;
                return i;
            }
        }
        return -1;
    }
    int alloc_load_n(int rob_idx)  { return alloc_n(rob_idx, false); }
    int alloc_store_n(int rob_idx) { return alloc_n(rob_idx, true); }

    void set_tag_n(int idx, int tag)  { new_[idx].tag = tag; }
    void set_addr_n(int idx, u32 addr) {
        new_[idx].addr = addr;
        new_[idx].addr_ready = true;
        if (!new_[idx].is_store)
            new_[idx].mem_delay = MEM_LATENCY;
    }
    void set_store_data_n(int idx, u32 data, Instr op) {
        new_[idx].value = data;
        new_[idx].op = op;
        new_[idx].data_ready = true;
    }
    void mark_store_ready_n(int idx) { new_[idx].completed = true; }

    // 递减 load 的内存延迟，返回刚减到 0 的条目索引
    int tick_mem_delay_n() {
        for (int i = 0; i < cap_; ++i) {
            if (!old_[i].busy || !new_[i].busy || old_[i].is_store) continue;
            if (new_[i].tag != old_[i].tag) continue;
            if (!old_[i].addr_ready || old_[i].completed) continue;
            if (old_[i].mem_delay > 0) {
                new_[i].mem_delay--;
                if (new_[i].mem_delay == 0) return i;
            }
        }
        return -1;
    }

    void load_done_n(int idx, u32 value) {
        new_[idx].completed = true;
        new_[idx].value = value;
    }

    void free_by_rob_n(int rob_idx) {
        for (int i = 0; i < cap_; ++i) {
            if (new_[i].busy && new_[i].rob_idx == rob_idx) {
                new_[i] = empty_entry();
                return;
            }
        }
    }
    void flush_by_rob_n(int rob_idx) { free_by_rob_n(rob_idx); }
    void flush_all_n() {
        for (int i = 0; i < cap_; ++i) new_[i] = empty_entry();
    }

    // 分支预测失败时，只清除比分支年轻的访存条目，保留更老的未提交条目。
    void flush_younger_than_n(int branch_tag) {
        for (int i = 0; i < cap_; ++i) {
            if (new_[i].busy && new_[i].tag > branch_tag)
                new_[i] = empty_entry();
        }
    }
};
