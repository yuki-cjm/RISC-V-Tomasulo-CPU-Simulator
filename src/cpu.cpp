#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "cpu.hpp"
#include "alu.hpp"

bool CPU::has_rs1(Instr op) {
    return op != Instr::LUI && op != Instr::AUIPC && op != Instr::JAL;
}

bool CPU::has_rs2(Instr op) {
    switch (op) {
        case Instr::ADD:  case Instr::SUB:  case Instr::AND:
        case Instr::OR:   case Instr::XOR:  case Instr::SLL:
        case Instr::SRL:  case Instr::SRA:  case Instr::SLT:
        case Instr::SLTU: case Instr::BEQ:  case Instr::BNE:
        case Instr::BLT:  case Instr::BGE:  case Instr::BLTU:
        case Instr::BGEU: case Instr::SB:   case Instr::SH:
        case Instr::SW:   return true;
        default:          return false;
    }
}

ReservationStation& CPU::pick_rs(Instr op) {
    switch (op) {
        case Instr::LB:  case Instr::LBU: case Instr::LH:
        case Instr::LHU: case Instr::LW:  return ld_rs;
        case Instr::SB:  case Instr::SH:  case Instr::SW: return st_rs;
        case Instr::BEQ: case Instr::BNE: case Instr::BLT:
        case Instr::BGE: case Instr::BLTU:case Instr::BGEU:
        case Instr::JAL: case Instr::JALR: return br_rs;
        default: return alu_rs;
    }
}

CPU::CPU(Memory& m)
    : mem(m)
    , alu_rs(RS_ALU_SIZE)
    , ld_rs(RS_LD_SIZE)
    , st_rs(RS_ST_SIZE)
    , br_rs(RS_BR_SIZE)
    , rob(ROB_SIZE)
    , lsq(LSQ_SIZE) {
    finished = false;
    tag_old = tag_new = 1;
    pc_old  = pc_new  = 0;
    fb_old  = fb_new  = {0, 0, false};
    tb_old  = tb_new  = 0;
    cb_old  = cb_new  = 0;
    for (u32 i = 0; i < REG_COUNT; ++i)
        issued_rd_old[i] = issued_rd_new[i] = false;
    stall_frontend_old = stall_frontend_new = false;
}

void CPU::update_all() {
    rf.update();
    rat.update();
    alu_rs.update();
    ld_rs.update();
    st_rs.update();
    br_rs.update();
    rob.update();
    lsq.update();
    bp.update();
    pc_old  = pc_new;
    fb_old  = fb_new;
    tag_old = tag_new;
    tb_old  = tb_new;
    cb_old  = cb_new;
    for (u32 i = 0; i < REG_COUNT; ++i) {
        issued_rd_old[i] = issued_rd_new[i];
        issued_rd_new[i] = false;
    }
    stall_frontend_old = stall_frontend_new;
}

void CPU::step_fetch() {
    if (stall_frontend_old) return;  // JALR 等待目标解析，前端停顿
    if (!fb_new.valid && !finished) {
        u32 pc    = pc_new;
        u32 ins   = mem.read_word(pc);
        fb_new    = {pc, ins, true};
        pc_new    = pc + 4;
    }
}

CDB_Entry CPU::precompute_cdb() {
    // LSQ 中刚完成内存延迟的 load
    for (int i = 0; i < lsq.size(); ++i) {
        const LSQ_Entry& le = lsq.entry_old(i);
        if (!le.busy || le.is_store || !le.addr_ready
            || le.completed || le.mem_delay != 0)
            continue;

        const ROB_Entry& re = rob.entry_old(le.rob_idx);
        if (!re.busy || re.flushed)
            continue;
        if (lsq.has_older_unresolved_store_old(le.tag))
            continue;

        // Store-to-load forwarding：查找可转发的 store
        u32 result = 0;
        bool forwarded = lsq.forward_old(le.addr, le.tag, result);
        if (forwarded) {                                                                                    
            return {true, result, re.dest_tag, false, 0, -1};
        }

        u32 addr = le.addr;
        Instr op = le.op;
        
        if (op == Instr::LB)
            result = (u32)(i32)(i8)mem.read_byte(addr);
        else if (op == Instr::LBU)
            result = (u32)mem.read_byte(addr);
        else if (op == Instr::LH)
            result = (u32)(i32)(i16)mem.read_half(addr);
        else if (op == Instr::LHU)
            result = (u32)mem.read_half(addr);
        else
            result = mem.read_word(addr);

        return {true, result, re.dest_tag, false, 0, -1};
    }

    // RS 中就绪的条目
    ReservationStation* rss[] = {&br_rs, &ld_rs, &st_rs, &alu_rs};
    for (auto* rs : rss) {
        for (int idx = 0; idx < rs->size(); ++idx) {
            const RS_Entry& e = rs->entry_old(idx);
            if (!e.busy) continue;
            if (e.Qj != -1 || e.Qk != -1) continue;
            if (e.MemRead) continue;  // load 由 LSQ 处理
            if (e.MemWrite) continue; // store 已在 execute 处理

            // Jump: 返回 pc+4
            if (e.Jump) {
                return {true, e.pc + 4, e.dest_tag, false, 0, -1};
            }

            // Branch: 计算实际分支方向
            if (e.Branch) {
                bool taken = false;
                u32  target = e.pc + e.imm;
                switch (e.op) {
                    case Instr::BEQ:  taken = (e.Vj == e.Vk); break;
                    case Instr::BNE:  taken = (e.Vj != e.Vk); break;
                    case Instr::BLT:  taken = ((i32)e.Vj < (i32)e.Vk); break;
                    case Instr::BGE:  taken = ((i32)e.Vj >= (i32)e.Vk); break;
                    case Instr::BLTU: taken = (e.Vj < e.Vk); break;
                    case Instr::BGEU: taken = (e.Vj >= e.Vk); break;
                    default: break;
                }

                const ROB_Entry& re = rob.entry_old(e.rob_idx);
                if (re.pred_taken != taken) {
                    u32 corr_pc = taken ? target : (e.pc + 4);
                    return {true, 0, e.dest_tag, true, corr_pc, e.rob_idx};
                }
                return {true, 0, e.dest_tag, false, 0, -1};
            }

            // ALU
            if (e.cycle_left > 0) continue;
            u32 result = alu.compute(e.op, e.Vj, e.Vk, e.imm, e.pc);
            return {true, result, e.dest_tag, false, 0, -1};
        }
    }

    return make_empty_cdb();
}

void CPU::step_issue(const CDB_Entry& cdb) {
    // 若 CDB 报告误预测，本周期不发射
    if (cdb.valid && cdb.mispredicted) return;
    // 前端停顿中不发射
    if (stall_frontend_old) return;
    if (finished || !fb_old.valid) return;

    u32          instr_pc = fb_old.pc;
    DecodedInstr d = decoder.decode(fb_old.instr);

    if (d.instr == Instr::HALT) {
        if (rob.empty_old()) {
            finished = true;
            std::cout << (rf.read_old(10) & 0xFF) << std::endl;
        }
        return;
    }

    if (d.instr == Instr::UNKNOWN) {
        fb_new = {0, 0, false};
        return;
    }

    ReservationStation& rs = pick_rs(d.instr);

    if (rob.full_old() || rs.full_old()) return;

    bool is_load  = (d.instr == Instr::LB  || d.instr == Instr::LBU
                  || d.instr == Instr::LH  || d.instr == Instr::LHU
                  || d.instr == Instr::LW);
    bool is_store = (d.instr == Instr::SB  || d.instr == Instr::SH
                  || d.instr == Instr::SW);

    if ((is_load || is_store) && lsq.full_old()) return;

    // 分配 ROB & RS & 标签
    int rob_idx = rob.alloc_new();
    int tag = tag_new++;
    rob.set_dest_new(rob_idx, d.rd, tag);
    rob.set_pc_new(rob_idx, instr_pc);

    int rs_idx = rs.alloc_new(tag);
    RS_Entry& e = rs.entry_new(rs_idx);
    e.op       = d.instr;
    e.dest_reg = d.rd;
    e.imm      = d.imm;
    e.pc       = instr_pc;
    e.rob_idx  = rob_idx;
    e.cycle_left = 1;

    bool rw = false, mr = false, mw = false, br = false, jp = false;
    switch (d.instr) {
        case Instr::ADD: case Instr::SUB: case Instr::AND:
        case Instr::OR:  case Instr::XOR: case Instr::SLL:
        case Instr::SRL: case Instr::SRA: case Instr::SLT:
        case Instr::SLTU: case Instr::ADDI: case Instr::ANDI:
        case Instr::ORI: case Instr::XORI: case Instr::SLLI:
        case Instr::SRLI: case Instr::SRAI: case Instr::SLTI:
        case Instr::SLTIU: case Instr::AUIPC: case Instr::LUI:
            rw = true; break;
        case Instr::LB: case Instr::LBU: case Instr::LH:
        case Instr::LHU: case Instr::LW:
            rw = true; mr = true; break;
        case Instr::SB: case Instr::SH: case Instr::SW:
            mw = true; break;
        case Instr::BEQ: case Instr::BNE: case Instr::BLT:
        case Instr::BGE: case Instr::BLTU: case Instr::BGEU:
            br = true; break;
        case Instr::JAL: case Instr::JALR:
            rw = true; jp = true; break;
        default: break;
    }
    e.RegWrite = rw;
    e.MemRead  = mr;
    e.MemWrite = mw;
    e.Branch   = br;
    e.Jump     = jp;

    auto read_oldp = [&](u8 reg, int& Q, u32& V) {
        if (reg == 0) { V = 0; Q = -1; return; }
        if (rat.busy_old(reg)) {
            if (rat.ready_old(reg)) {
                // RAT 中已通过 CDB 拿到值
                V = rat.val_old(reg);
                Q = -1;
            } else {
                int producer = rat.tag_old(reg);
                // 同周期 CDB 旁路
                if (cdb.valid && cdb.tag == producer) {
                    V = cdb.value;
                    Q = -1;
                } else if (rob.tag_valid_old(producer)) {
                    // 生产者还在 ROB 中，设置等待标签
                    Q = producer;
                    V = 0;
                } else {
                    // 生产者已不在 ROB，已提交，从寄存器堆读
                    V = rf.read_old(reg);
                    Q = -1;
                }
            }
        } else {
            V = rf.read_old(reg);
            Q = -1;
        }
    };

    if (has_rs1(d.instr)) read_oldp(d.rs1, e.Qj, e.Vj);
    if (has_rs2(d.instr)) read_oldp(d.rs2, e.Qk, e.Vk);

    // RAT 重命名
    if (rw && d.rd != 0) {
        rat.rename_new(d.rd, tag);
        issued_rd_new[d.rd] = true;  // 标记本周期被 issue 重命名
    }

    // JAL/JALR: 需要 pc+4 作为链路地址写入 rd
    if (d.instr == Instr::JAL || d.instr == Instr::JALR) {
        e.Vk = instr_pc + 4;
        e.Qk = -1;
    }

    // LSQ 分配
    if (mr) {
        e.lsq_idx = lsq.alloc_load_new(rob_idx);
        lsq.set_tag_new(e.lsq_idx, tag);
    }
    if (mw) {
        e.lsq_idx = lsq.alloc_store_new(rob_idx);
        lsq.set_tag_new(e.lsq_idx, tag);
        rob.set_store_new(rob_idx);  // 在 ROB 中标记为 store
    }

    // 控制流处理 & 消耗取指缓冲
    if (jp && d.instr == Instr::JAL) {
        // JAL: 无条件跳转，总是 taken
        rob.set_branch_new(rob_idx, true, false, true, instr_pc + d.imm);
        pc_new = instr_pc + d.imm;
        fb_new = {0, 0, false};
    } else if (br) {
        bool pred         = bp.predict_old(instr_pc);
        bool bimodal_pred = bp.bimodal_pred_old(instr_pc);
        bool gshare_pred  = bp.gshare_pred_old(instr_pc);
        u32  target       = instr_pc + d.imm;
        rob.set_branch_new(rob_idx, pred, true, false, target);
        rob.set_ghr_snapshot_new(rob_idx, bp.get_old_ghr());
        rob.set_bimodal_pred_new(rob_idx, bimodal_pred);
        rob.set_gshare_pred_new(rob_idx, gshare_pred);
        if (pred) {
            pc_new = target;
        }
        fb_new = {0, 0, false};
    } else if (jp && d.instr == Instr::JALR) {
        rob.set_branch_new(rob_idx, true, false, true, 0);
        fb_new = {0, 0, false};
        stall_frontend_new = true;
    } else {
        fb_new = {0, 0, false};
    }
}

void CPU::step_execute(const CDB_Entry& cdb) {
    alu_rs.tick_all_alu_new();
    lsq.tick_mem_delay_new();

    // 处理刚完成 mem_delay 的 store
    for (int i = 0; i < lsq.size(); ++i) {
        const LSQ_Entry& lo = lsq.entry_old(i);
        const LSQ_Entry& ln = lsq.entry_new(i);
        if (lo.busy && lo.is_store && lo.addr_ready && lo.data_ready
            && !lo.completed && ln.completed) {
            rob.mark_store_done_new(lo.rob_idx);
            rob.write_result_new(lo.rob_idx, 0);
            // 释放对应 RS 条目
            ReservationStation* rss[] = {&st_rs, &alu_rs};
            for (auto* rs : rss) {
                int sj = rs->find_by_tag_old(lo.tag);
                if (sj >= 0) {
                    rs->free_new(sj);
                    break;
                }
            }
        }
    }

    // 为就绪的 mem 指令计算地址、启动内存访问
    ReservationStation* all_rs[] = {&st_rs, &alu_rs, &ld_rs, &br_rs};
    for (auto* rs : all_rs) {
        for (int j = 0; j < rs->size(); ++j) {
            const RS_Entry& eo = rs->entry_old(j);
            if (!eo.busy) continue;
            if (eo.Qj != -1 || eo.Qk != -1) continue;

            // Load: 计算地址，检查 store-to-load forwarding
            if (eo.MemRead && eo.lsq_idx >= 0
                && !lsq.entry_old(eo.lsq_idx).addr_ready) {
                u32 addr = eo.Vj + eo.imm;
                lsq.set_addr_new(eo.lsq_idx, addr, eo.op);

                // 检查是否有更早的未解析地址的 store → 等待
                if (lsq.has_older_unresolved_store_old(eo.dest_tag))
                    continue;

                // Store-to-load forwarding：检查 old_ 和 new_ LSQ（选 tag 最大的最新 store）
                u32 fwd_val = 0;
                bool fwd = lsq.forward_old(addr, eo.dest_tag, fwd_val);
                if (!fwd) {
                    int best_tag = -1;
                    for (int k = 0; k < lsq.size(); ++k) {
                        const LSQ_Entry& lk = lsq.entry_new(k);
                        if (lk.busy && lk.is_store && lk.completed
                            && lk.tag >= 0 && lk.tag < eo.dest_tag
                            && lk.addr == addr && lk.tag > best_tag) {
                            best_tag = lk.tag;
                            fwd_val = lk.value;
                            fwd = true;
                        }
                    }
                }
                if (fwd) {
                    // 记录转发值，设置 mem_delay=0，但不标记 completed
                    // precompute_cdb 会在下一周期发现 mem_delay=0 并生成 CDB
                    lsq.entry_new(eo.lsq_idx).value = fwd_val;
                    lsq.entry_new(eo.lsq_idx).mem_delay = 0;
                }
            }
            // Store: 计算地址、写入数据，等待 mem_delay 倒计时
            else if (eo.MemWrite && eo.lsq_idx >= 0
                     && !lsq.entry_old(eo.lsq_idx).addr_ready) {
                u32 addr = eo.Vj + eo.imm;
                lsq.set_addr_new(eo.lsq_idx, addr, eo.op);
                lsq.set_store_data_new(eo.lsq_idx, eo.Vk, eo.op);
            }
        }
    }

    if (!cdb.valid) return;

    ReservationStation* all_rs2[] = {&br_rs, &alu_rs, &ld_rs, &st_rs};
    for (auto* rs : all_rs2) {
        int j = rs->find_by_tag_old(cdb.tag);
        if (j < 0) continue;
        const RS_Entry& eo = rs->entry_old(j);
        if (!eo.busy) continue;

        // 分支预测正确
        if (eo.Branch && !cdb.mispredicted) {
            bool taken = false;
            switch (eo.op) {
                case Instr::BEQ:  taken = (eo.Vj == eo.Vk); break;
                case Instr::BNE:  taken = (eo.Vj != eo.Vk); break;
                case Instr::BLT:  taken = ((i32)eo.Vj < (i32)eo.Vk); break;
                case Instr::BGE:  taken = ((i32)eo.Vj >= (i32)eo.Vk); break;
                case Instr::BLTU: taken = (eo.Vj < eo.Vk); break;
                case Instr::BGEU: taken = (eo.Vj >= eo.Vk); break;
                default: break;
            }
            const ROB_Entry& re = rob.entry_old(eo.rob_idx);
            bp.updateate_new(eo.pc, taken, re.ghr_snapshot,
                        re.bimodal_pred, re.gshare_pred, false);
            tb_new++;
            cb_new++;
            return;
        }

        // 分支预测失败
        if (eo.Branch && cdb.mispredicted) {
            bool taken = false;
            u32  target = eo.pc + eo.imm;
            switch (eo.op) {
                case Instr::BEQ:  taken = (eo.Vj == eo.Vk); break;
                case Instr::BNE:  taken = (eo.Vj != eo.Vk); break;
                case Instr::BLT:  taken = ((i32)eo.Vj < (i32)eo.Vk); break;
                case Instr::BGE:  taken = ((i32)eo.Vj >= (i32)eo.Vk); break;
                case Instr::BLTU: taken = (eo.Vj < eo.Vk); break;
                case Instr::BGEU: taken = (eo.Vj >= eo.Vk); break;
                default: break;
            }
            u32 correct_pc = taken ? target : (eo.pc + 4);

            const ROB_Entry& re = rob.entry_old(eo.rob_idx);
            bp.updateate_new(eo.pc, taken, re.ghr_snapshot,
                        re.bimodal_pred, re.gshare_pred, true);
            tb_new++;

            // 冲刷流水线
            rob.flush_after_new(cdb.branch_rob);
            rob.write_result_new(cdb.branch_rob, 0);
            alu_rs.flush_younger_than_new(cdb.tag);
            ld_rs.flush_younger_than_new(cdb.tag);
            st_rs.flush_younger_than_new(cdb.tag);
            br_rs.flush_younger_than_new(cdb.tag);
            lsq.flush_younger_than_new(cdb.tag);

            // 重建 RAT
            rat.flush_all_new();
            int vcnt = rob.valid_count_old();
            for (int k = 0; k < vcnt; ++k) {
                int x = rob.nth_valid_old(k);
                if (x < 0) break;
                const ROB_Entry& rr = rob.entry_old(x);
                if (!rob.is_flushed_new(x) && rr.dest_reg != 0) {
                    rat.rename_new(rr.dest_reg, rr.dest_tag);
                    if (rr.ready)
                        rat.capture_cdb_new(rr.dest_tag, rr.value);
                }
            }

            // 修正 PC
            pc_new = correct_pc;
            fb_new = {0, 0, false};
            stall_frontend_new = false;
            return;
        }

        // JALR: 更新 PC 为目标地址
        if (eo.Jump && eo.op == Instr::JALR) {
            u32 target = (eo.Vj + eo.imm) & ~1u;
            pc_new = target;
            fb_new = {0, 0, false};
            stall_frontend_new = false;
            return;
        }

        return;
    }
}

void CPU::step_writeback(const CDB_Entry& cdb) {
    if (!cdb.valid) return;

    // 广播到所有 RS
    alu_rs.wakeup_cdb_new(cdb.tag, cdb.value);
    ld_rs.wakeup_cdb_new(cdb.tag, cdb.value);
    st_rs.wakeup_cdb_new(cdb.tag, cdb.value);
    br_rs.wakeup_cdb_new(cdb.tag, cdb.value);

    // 广播到 RAT
    rat.capture_cdb_new(cdb.tag, cdb.value);

    // 广播到 ROB
    int rob_idx = rob.find_by_tag_old(cdb.tag);
    if (rob_idx >= 0)
        rob.write_result_new(rob_idx, cdb.value);

    // 释放对应的 RS 条目
    ReservationStation* all_rs[] = {&alu_rs, &ld_rs, &st_rs, &br_rs};
    for (auto* rs : all_rs) {
        int j = rs->find_by_tag_old(cdb.tag);
        if (j >= 0) {
            const RS_Entry& eo = rs->entry_old(j);
            if (eo.busy && !eo.MemWrite) {
                rs->free_new(j);
                break;
            }
        }
    }

    // 标记 LSQ 中对应 load 为完成
    for (int i = 0; i < lsq.size(); ++i) {
        const LSQ_Entry& le = lsq.entry_old(i);
        if (le.busy && !le.is_store && le.tag == cdb.tag && !le.completed) {
            lsq.load_done_new(i, cdb.value);
            break;
        }
    }
}

void CPU::step_commit(const CDB_Entry& cdb) {
    if (cdb.valid && cdb.mispredicted) return;

    int cap = rob.cap();
    int h   = rob.head_old();
    int cnt = rob.count_old();

    for (int i = 0; i < cnt; ++i) {
        int idx = (h + i) % cap;
        const ROB_Entry& re = rob.entry_old(idx);

        if (!re.busy) break;

        bool ready = re.ready;
        if (!ready && cdb.valid && !cdb.mispredicted
            && cdb.tag == re.dest_tag)
            ready = true;

        // store 指令：需要 store_done
        if (re.is_store) {
            if (!re.store_done && !re.ready)
                break;
        } else {
            if (!ready && !re.flushed) 
                break;  // 队首未就绪则暂停
        }

        if (re.flushed) {
            // 被刷掉的条目直接跳过
            rob.commit_head_new();
            lsq.free_by_rob_new(idx);
            continue;
        }

        // 写寄存器堆
        if (re.dest_reg != 0) {
            const RAT_Entry& rat_e = rat.entry_old(re.dest_reg);
            if (rat_e.tag == re.dest_tag) {
                u32 commit_val = re.value;
                if (cdb.valid && !cdb.mispredicted && cdb.tag == re.dest_tag)
                    commit_val = cdb.value;
                rf.write_new(re.dest_reg, commit_val);
                if (!issued_rd_new[re.dest_reg]) {
                    rat.commit_clear_new(re.dest_reg, re.dest_tag);
                }
            }
        }

        // Store
        for (int j = 0; j < lsq.size(); ++j) {
            const LSQ_Entry& le = lsq.entry_old(j);
            if (le.busy && le.rob_idx == idx && le.is_store) {
                if (le.completed) {
                    switch (le.op) {
                        case Instr::SB:
                            mem.write_byte(le.addr, le.value & 0xFF);
                            break;
                        case Instr::SH:
                            mem.write_half(le.addr, le.value & 0xFFFF);
                            break;
                        case Instr::SW:
                            mem.write_word(le.addr, le.value);
                            break;
                        default: break;
                    }
                }
            }
        }

        rob.commit_head_new();
        lsq.free_by_rob_new(idx);
    }
}

void CPU::step() {
    step_fetch();
    CDB_Entry cdb = precompute_cdb();

    step_issue(cdb);
    step_writeback(cdb);
    step_execute(cdb);
    step_commit(cdb);
    
    update_all();
}
