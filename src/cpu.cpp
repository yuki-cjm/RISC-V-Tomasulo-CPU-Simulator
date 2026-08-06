#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "cpu.hpp"
#include "alu.hpp"

// ============================================================================
// 步骤排列查找表：24 种排列（字典序）
// 0=Issue, 1=Execute, 2=Writeback, 3=Commit
// ============================================================================
const int PERM_TABLE[PERM_COUNT][4] = {
    {0,1,2,3}, {0,1,3,2}, {0,2,1,3}, {0,2,3,1}, {0,3,1,2}, {0,3,2,1},
    {1,0,2,3}, {1,0,3,2}, {1,2,0,3}, {1,2,3,0}, {1,3,0,2}, {1,3,2,0},
    {2,0,1,3}, {2,0,3,1}, {2,1,0,3}, {2,1,3,0}, {2,3,0,1}, {2,3,1,0},
    {3,0,1,2}, {3,0,2,1}, {3,1,0,2}, {3,1,2,0}, {3,2,0,1}, {3,2,1,0}
};

// ============================================================================
// 辅助：判断指令类型
// ============================================================================
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

// ============================================================================
// 构造 & 状态管理
// ============================================================================
CPU::CPU(Memory& m, int perm)
    : mem(m)
    , alu_rs(RS_ALU_SZ, "ALU")
    , ld_rs(RS_LD_SZ, "LD")
    , st_rs(RS_ST_SZ, "ST")
    , br_rs(RS_BR_SZ, "BR")
    , rob(ROB_SIZE)
    , lsq(LSQ_SIZE)
    , perm_(perm)
{
    done = false;
    tag_o = tag_n = 1;
    pc_o  = pc_n  = 0;
    fb_o  = fb_n  = {0, 0, false};
    tb_o  = tb_n  = 0;
    cb_o  = cb_n  = 0;
    for (u32 i = 0; i < REG_COUNT; ++i)
        issued_rd_o_[i] = issued_rd_n_[i] = false;
    stall_frontend_o_ = stall_frontend_n_ = false;
}

void CPU::snap_all() {
    rf.snap();
    rat.snap();
    alu_rs.snap();
    ld_rs.snap();
    st_rs.snap();
    br_rs.snap();
    rob.snap();
    lsq.snap();
    bp.snap();
    pc_n  = pc_o;
    fb_n  = fb_o;
    tag_n = tag_o;
    tb_n  = tb_o;
    cb_n  = cb_o;
    for (u32 i = 0; i < REG_COUNT; ++i)
        issued_rd_n_[i] = false;  // 每周期重置
    stall_frontend_n_ = stall_frontend_o_;
}

void CPU::upd_all() {
    rf.upd();
    rat.upd();
    alu_rs.upd();
    ld_rs.upd();
    st_rs.upd();
    br_rs.upd();
    rob.upd();
    lsq.upd();
    bp.upd();
    pc_o  = pc_n;
    fb_o  = fb_n;
    tag_o = tag_n;
    tb_o  = tb_n;
    cb_o  = cb_n;
    for (u32 i = 0; i < REG_COUNT; ++i)
        issued_rd_o_[i] = issued_rd_n_[i];
    stall_frontend_o_ = stall_frontend_n_;
}

// ============================================================================
// 取指：始终最先运行
// ============================================================================
void CPU::step_fetch() {
    if (stall_frontend_o_) return;  // JALR 等待目标解析，前端停顿
    if (!fb_n.valid && !done) {
        u32 p   = pc_o;
        u32 ins = mem.read_word(p);
        fb_n    = {p, ins, true};
        pc_n    = p + 4;
    }
}

// ============================================================================
// precompute_cdb: 从各部件旧状态计算本周期 CDB 广播
//
// 从 old_ 状态中寻找"本周期可以产生结果"的指令：
//   1. LSQ 中 mem_delay==0 的 load → 读内存生成 CDB
//   2. RS 中操作数就绪且 cycle_left==0 的指令 → 计算生成 CDB
//
// 优先顺序: br > ld > st > alu
// ============================================================================
CDB_Entry CPU::precompute_cdb() {
    // ---- 优先级 1: LSQ 中刚完成内存延迟的 load ----
    for (int i = 0; i < lsq.size(); ++i) {
        const LSQ_Entry& le = lsq.entry_o(i);
        if (!le.busy || le.is_store || !le.addr_ready
            || le.completed || le.mem_delay != 0)
            continue;

        const ROB_Entry& re = rob.entry_o(le.rob_idx);
        if (!re.busy || re.flushed) continue;

        // 检查是否有更早的未解析地址的 store → load 不能提前
        if (lsq.has_older_unresolved_store_o(le.tag))
            continue;

        // Store-to-load forwarding：查找可转发的 store
        u32 result = 0;
        bool forwarded = lsq.forward_o(le.addr, le.tag, result);
        if (forwarded) {
            return {true, result, re.dest_tag, false, 0, -1};
        }

        // 从 RS 中找到该 load 的原始操作类型以确定宽度
        Instr op = Instr::LW;
        ReservationStation* all_rs[] = {&ld_rs, &alu_rs, &st_rs, &br_rs};
        for (auto* rs : all_rs) {
            for (int j = 0; j < rs->size(); ++j) {
                const RS_Entry& e = rs->entry_o(j);
                if (e.busy && e.dest_tag == re.dest_tag) {
                    op = e.op;
                    break;
                }
            }
        }

        u32 addr = le.addr;
        
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

    // ---- 优先级 2: RS 中就绪的条目 ----
    // 就绪条件: busy && !completed && Qj==-1 && Qk==-1
    // MemRead（load）跳过（由 LSQ 处理）
    ReservationStation* rss[] = {&br_rs, &ld_rs, &st_rs, &alu_rs};
    for (auto* rs : rss) {
        for (int idx = 0; idx < rs->size(); ++idx) {
            const RS_Entry& e = rs->entry_o(idx);
            if (!e.busy || e.completed) continue;
            if (e.Qj != -1 || e.Qk != -1) continue;
            if (e.MemRead) continue;  // load 由 LSQ 处理
            if (e.MemWrite) continue; // store 已在 execute 处理

            // Jump: 返回 pc+4 作为链路地址（单周期执行）
            if (e.Jump) {
                return {true, e.pc + 4, e.dest_tag, false, 0, -1};
            }

            // Branch: 在此计算实际分支方向（单周期执行）
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

                const ROB_Entry& re = rob.entry_o(e.rob_idx);
                if (re.pred_taken != taken) {
                    u32 corr_pc = taken ? target : (e.pc + 4);
                    return {true, 0, e.dest_tag, true, corr_pc, e.rob_idx};
                }
                return {true, 0, e.dest_tag, false, 0, -1};
            }

            // ALU 类指令: cycle_left 必须为 0
            if (e.cycle_left > 0) continue;
            u32 result = ALU::compute(e.op, e.Vj, e.Vk, e.imm, e.pc);
            return {true, result, e.dest_tag, false, 0, -1};
        }
    }

    return make_empty_cdb();
}

// ============================================================================
// step_issue: 发射阶段
//
// 只写：RAT（rename）、RS（alloc）、ROB（alloc）、LSQ（alloc）、
//        fb_n（消耗取指缓冲）、pc_n（分支预测方向）、issued_rd_n_
// 只读：_o 状态 + CDB
// ============================================================================
void CPU::step_issue(const CDB_Entry& cdb) {
    // 若 CDB 报告误预测，本周期不发射
    if (cdb.valid && cdb.mispredicted) return;
    // 前端停顿中不发射
    if (stall_frontend_o_) return;
    if (done || !fb_o.valid) return;

    u32          instr_pc = fb_o.pc;
    DecodedInstr d        = decoder.decode(fb_o.instr);

    // HALT: li a0, 255 → 不执行，等 ROB 排空后停机
    if (d.instr == Instr::HALT) {
        if (rob.empty_o()) {
            done = true;
            std::cout << (rf.read_o(10) & 0xFF) << std::endl;
        }
        return;
    }

    if (d.instr == Instr::UNKNOWN) {
        fb_n = {0, 0, false};
        return;
    }

    // 选保留站
    ReservationStation& rs = pick_rs(d.instr);

    // 结构冒险检查（读 _o 状态）
    if (rob.full_o() || rs.full_o()) return;

    bool is_load  = (d.instr == Instr::LB  || d.instr == Instr::LBU
                  || d.instr == Instr::LH  || d.instr == Instr::LHU
                  || d.instr == Instr::LW);
    bool is_store = (d.instr == Instr::SB  || d.instr == Instr::SH
                  || d.instr == Instr::SW);

    if ((is_load || is_store) && lsq.full_o()) return;

    // ---- 分配 ROB & RS & 标签 ----
    int rob_idx = rob.alloc_n();
    int tag     = tag_n++;
    rob.set_dest_n(rob_idx, d.rd, tag);
    rob.set_pc_n(rob_idx, instr_pc);

    int rs_idx = rs.alloc_n(tag);
    RS_Entry& e = rs.entry_n(rs_idx);
    e.op       = d.instr;
    e.dest_reg = d.rd;
    e.imm      = d.imm;
    e.pc       = instr_pc;
    e.rob_idx  = rob_idx;
    e.cycle_left = 1;  // ALU 默认至少 1 周期

    // ---- 指令分类 ----
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

    // ---- 读取操作数（RAT _o + CDB 旁路 + 寄存器堆 _o） ----
    auto read_op = [&](u8 reg, int& Q, u32& V) {
        if (reg == 0) { V = 0; Q = -1; return; }
        if (rat.busy_o(reg)) {
            if (rat.ready_o(reg)) {
                // RAT 中已通过 CDB 拿到值
                V = rat.val_o(reg);
                Q = -1;
            } else {
                int producer = rat.tag_o(reg);
                // 同周期 CDB 旁路
                if (cdb.valid && cdb.tag == producer) {
                    V = cdb.value;
                    Q = -1;
                } else if (rob.tag_valid_o(producer)) {
                    // 生产者还在 ROB 中，设置等待标签
                    Q = producer;
                    V = 0;
                } else {
                    // 生产者已不在 ROB（已提交），从寄存器堆读
                    V = rf.read_o(reg);
                    Q = -1;
                }
            }
        } else {
            V = rf.read_o(reg);
            Q = -1;
        }
    };

    if (has_rs1(d.instr)) read_op(d.rs1, e.Qj, e.Vj);
    if (has_rs2(d.instr)) read_op(d.rs2, e.Qk, e.Vk);

    // ---- RAT 重命名 ----
    if (rw && d.rd != 0) {
        rat.rename_n(d.rd, tag);
        issued_rd_n_[d.rd] = true;  // 标记本周期被 issue 重命名
    }

    // JAL/JALR: 需要 pc+4 作为链路地址写入 rd
    if (d.instr == Instr::JAL || d.instr == Instr::JALR) {
        e.Vk = instr_pc + 4;
        e.Qk = -1;
    }

    // ---- LSQ 分配 ----
    if (mr) {
        e.lsq_idx = lsq.alloc_load_n(rob_idx);
        lsq.set_tag_n(e.lsq_idx, tag);
    }
    if (mw) {
        e.lsq_idx = lsq.alloc_store_n(rob_idx);
        lsq.set_tag_n(e.lsq_idx, tag);
        rob.set_store_n(rob_idx);  // 在 ROB 中标记为 store
    }

    // ---- 控制流处理 & 消耗取指缓冲 ----
    if (jp && d.instr == Instr::JAL) {
        // JAL: 无条件跳转，总是 taken
        rob.set_branch_n(rob_idx, true, false, true, instr_pc + d.imm);
        pc_n = instr_pc + d.imm;  // 更新取指 PC
        fb_n = {0, 0, false};
    } else if (br) {
        // 条件分支：Tournament 预测器
        bool pred         = bp.predict_o(instr_pc);
        bool bimod_pred   = bp.bimod_pred_o(instr_pc);
        bool gshare_pred  = bp.gshare_pred_o(instr_pc);
        u32  target       = instr_pc + d.imm;
        rob.set_branch_n(rob_idx, pred, true, false, target);
        rob.set_ghr_snapshot_n(rob_idx, bp.get_old_ghr());
        rob.set_bimod_pred_n(rob_idx, bimod_pred);
        rob.set_gshare_pred_n(rob_idx, gshare_pred);
        if (pred) {
            pc_n = target;
        }
        fb_n = {0, 0, false};
    } else if (jp && d.instr == Instr::JALR) {
        // JALR: 无条件跳转，目标地址在 rs1 中，等执行阶段才知道
        rob.set_branch_n(rob_idx, true, false, true, 0);
        fb_n = {0, 0, false};
        stall_frontend_n_ = true;  // 停顿前端，等待目标解析
    } else {
        // 普通指令：消耗取指缓冲
        fb_n = {0, 0, false};
    }
}

// ============================================================================
// step_execute: 执行阶段
//
// 只写：RS（cycle_left 递减）、LSQ（addr、mem_delay、store data）、
//        ROB（store_done、flush，以及 store 结果的 write_result）、
//        bp（更新）、tb_n、cb_n
// 只读：_o 状态 + CDB
//
// 注意：
//   - ALU/Branch/Jump 的结果计算由 precompute_cdb 完成
//   - Load forwarding 不标记 LSQ 完成，由 precompute_cdb 在下一周期处理
//   - pc_n、fb_n 由专门的 resolve 步骤写入，此处不写
// ============================================================================
void CPU::step_execute(const CDB_Entry& cdb) {
    // ---- 1. 递减所有 RS 条目的 cycle_left ----
    alu_rs.tick_all_alu_n();
    ld_rs.tick_all_alu_n();
    st_rs.tick_all_alu_n();
    br_rs.tick_all_alu_n();

    // ---- 2. LSQ 内存延迟递减 ----
    lsq.tick_mem_delay_n();

    // ---- 3. 为就绪的 mem 指令计算地址、启动内存访问 ----
    // 先处理 store，再处理 load（确保 store data 在同周期对 load 可见）
    ReservationStation* all_rs[] = {&st_rs, &alu_rs, &ld_rs, &br_rs};
    for (auto* rs : all_rs) {
        for (int j = 0; j < rs->size(); ++j) {
            const RS_Entry& eo = rs->entry_o(j);
            if (!eo.busy || eo.completed) continue;
            if (eo.Qj != -1 || eo.Qk != -1) continue;

            // Load: 计算地址，检查 store-to-load forwarding
            if (eo.MemRead && eo.lsq_idx >= 0
                && !lsq.entry_o(eo.lsq_idx).addr_ready) {
                u32 addr = eo.Vj + eo.imm;
                lsq.set_addr_n(eo.lsq_idx, addr);

                // 检查是否有更早的未解析地址的 store → 等待
                if (lsq.has_older_unresolved_store_o(eo.dest_tag))
                    continue;

                // Store-to-load forwarding：检查 old_ 和 new_ LSQ（选 tag 最大的最新 store）
                u32 fwd_val = 0;
                bool fwd = lsq.forward_o(addr, eo.dest_tag, fwd_val);
                if (!fwd) {
                    int best_tag = -1;
                    for (int k = 0; k < lsq.size(); ++k) {
                        const LSQ_Entry& lk = lsq.entry_n(k);
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
                    lsq.entry_n(eo.lsq_idx).value = fwd_val;
                    lsq.entry_n(eo.lsq_idx).mem_delay = 0;
                }
            }
            // Store: 计算地址、写入数据，标记 LSQ 和 ROB 完成
            else if (eo.MemWrite && eo.lsq_idx >= 0
                     && !lsq.entry_o(eo.lsq_idx).addr_ready) {
                u32 addr = eo.Vj + eo.imm;
                lsq.set_addr_n(eo.lsq_idx, addr);
                lsq.set_store_data_n(eo.lsq_idx, eo.Vk, eo.op);
                lsq.mark_store_ready_n(eo.lsq_idx);
                rob.mark_store_done_n(eo.rob_idx);
                rob.write_result_n(eo.rob_idx, 0);
                rs->free_n(j);  // store 的 RS 条目释放
            }
        }
    }

    // ---- 4. 处理 CDB 上的分支解析结果（不影响 pc_n/fb_n，由专门步骤处理） ----
    if (!cdb.valid) return;

    ReservationStation* all_rs2[] = {&br_rs, &alu_rs, &ld_rs, &st_rs};
    for (auto* rs : all_rs2) {
        int j = rs->find_by_tag_o(cdb.tag);
        if (j < 0) continue;
        const RS_Entry& eo = rs->entry_o(j);
        if (!eo.busy) continue;

        // 分支预测正确：更新统计
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
            const ROB_Entry& re = rob.entry_o(eo.rob_idx);
            bp.update_n(eo.pc, taken, re.ghr_snapshot,
                        re.bimod_pred, re.gshare_pred, false);
            tb_n++;
            cb_n++;
            return;
        }

        // 分支预测失败：冲刷流水线
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

            const ROB_Entry& re = rob.entry_o(eo.rob_idx);
            bp.update_n(eo.pc, taken, re.ghr_snapshot,
                        re.bimod_pred, re.gshare_pred, true);
            tb_n++;

            // 冲刷流水线（写 new_ 状态）：只清除该分支之后的年轻指令。
            // 不能清空所有 RS/LSQ，否则比分支更老但尚未提交的 store/load 会丢失。
            rob.flush_after_n(cdb.branch_rob);
            rob.write_result_n(cdb.branch_rob, 0);
            alu_rs.flush_younger_than_n(cdb.tag);
            ld_rs.flush_younger_than_n(cdb.tag);
            st_rs.flush_younger_than_n(cdb.tag);
            br_rs.flush_younger_than_n(cdb.tag);
            lsq.flush_younger_than_n(cdb.tag);

            // 重建 RAT
            rat.flush_all_n();
            int vcnt = rob.valid_count_o();
            for (int k = 0; k < vcnt; ++k) {
                int x = rob.nth_valid_o(k);
                if (x < 0) break;
                const ROB_Entry& rr = rob.entry_o(x);
                if (!rob.is_flushed_n(x) && rr.dest_reg != 0) {
                    rat.rename_n(rr.dest_reg, rr.dest_tag);
                    if (rr.ready)
                        rat.capture_cdb_n(rr.dest_tag, rr.value);
                }
            }

            // 修正 PC（写新状态）
            pc_n = correct_pc;
            fb_n = {0, 0, false};
            stall_frontend_n_ = false;
            return;
        }

        // JALR: 更新 PC 为目标地址
        if (eo.Jump && eo.op == Instr::JALR) {
            u32 target = (eo.Vj + eo.imm) & ~1u;
            pc_n = target;
            fb_n = {0, 0, false};
            stall_frontend_n_ = false;
            return;
        }

        return;
    }
}

// ============================================================================
// step_writeback: 写回阶段
//
// 只写：RS（捕获 CDB、释放条目）、RAT（捕获 CDB）、ROB（标记 ready）、
//        LSQ（标记 load 完成）
// 只读：_o 状态 + CDB
// ============================================================================
void CPU::step_writeback(const CDB_Entry& cdb) {
    if (!cdb.valid) return;

    // 广播到所有 RS（唤醒等待的操作数）
    alu_rs.wakeup_cdb_n(cdb.tag, cdb.value);
    ld_rs.wakeup_cdb_n(cdb.tag, cdb.value);
    st_rs.wakeup_cdb_n(cdb.tag, cdb.value);
    br_rs.wakeup_cdb_n(cdb.tag, cdb.value);

    // 广播到 RAT
    rat.capture_cdb_n(cdb.tag, cdb.value);

    // 广播到 ROB
    int rob_idx = rob.find_by_tag_o(cdb.tag);
    if (rob_idx >= 0)
        rob.write_result_n(rob_idx, cdb.value);

    // 释放对应的 RS 条目（非 store）
    ReservationStation* all_rs[] = {&alu_rs, &ld_rs, &st_rs, &br_rs};
    for (auto* rs : all_rs) {
        int j = rs->find_by_tag_o(cdb.tag);
        if (j >= 0) {
            const RS_Entry& eo = rs->entry_o(j);
            if (eo.busy && !eo.MemWrite) {
                rs->free_n(j);
                break;
            }
        }
    }

    // 标记 LSQ 中对应 load 为完成
    for (int i = 0; i < lsq.size(); ++i) {
        const LSQ_Entry& le = lsq.entry_o(i);
        if (le.busy && !le.is_store && le.tag == cdb.tag && !le.completed) {
            lsq.load_done_n(i, cdb.value);
            break;
        }
    }
}

// ============================================================================
// step_commit: 提交阶段
//
// 只写：RegFile（提交值）、ROB（advance head）、RAT（清除重命名）、
//        LSQ（释放）、Memory（store 写入）
// 只读：_o 状态 + CDB
// ============================================================================
void CPU::step_commit(const CDB_Entry& cdb) {
    // 分支预测失败时，本周期交给 execute/flush 统一修正前端、ROB、RS、LSQ、RAT。
    // 若 commit 与 execute 顺序交换，允许 commit 先提交旧队首会导致 execute 阶段
    // 按 old ROB 重建 RAT 时重新引入“本周期刚提交”的旧映射，从而破坏顺序无关性。
    // 因此 mispredict 广播周期暂停提交一拍，保证 flush 是该周期唯一修改控制相关状态的动作。
    if (cdb.valid && cdb.mispredicted) return;

    int cap = rob.cap();
    int h   = rob.head_o();
    int cnt = rob.count_o();

    for (int i = 0; i < cnt; ++i) {
        int idx = (h + i) % cap;
        const ROB_Entry& re = rob.entry_o(idx);

        if (!re.busy) break;

        bool ready = re.ready;
        // 只能使用 ROB 旧状态和本周期预先计算出的 CDB。
        // 不读取 ROB new_，否则 step_commit 与 step_writeback / step_execute 的调用顺序
        // 会影响本周期能否提交，破坏“模块执行顺序可交换”。
        if (!ready && cdb.valid && !cdb.mispredicted
            && cdb.tag == re.dest_tag)
            ready = true;

        // store 指令：需要 store_done
        if (re.is_store) {
            if (!re.store_done && !re.ready) break;
        } else {
            if (!ready && !re.flushed) break;  // 队首未就绪则暂停
        }

        if (re.flushed) {
            // 被刷掉的条目直接跳过
            rob.commit_head_n();
            lsq.free_by_rob_n(idx);
            continue;
        }

        // 写寄存器堆
        if (re.dest_reg != 0) {
            // 检查 RAT：仅当 RAT 中该寄存器仍指向本指令时才提交
            const RAT_Entry& rat_e = rat.entry_o(re.dest_reg);
            if (rat_e.tag == re.dest_tag) {
                // 优先使用 CDB 值（同周期 writeback 写入 new_，old_ 尚未更新）
                u32 commit_val = re.value;
                if (cdb.valid && !cdb.mispredicted && cdb.tag == re.dest_tag)
                    commit_val = cdb.value;
                rf.write_n(re.dest_reg, commit_val);
                // 清除 RAT（除非本周期被 issue 重新重命名）
                if (!issued_rd_n_[re.dest_reg]) {
                    rat.commit_clear_n(re.dest_reg, re.dest_tag);
                }
            }
        }

        // Store: 真正写入内存
        for (int j = 0; j < lsq.size(); ++j) {
            const LSQ_Entry& le = lsq.entry_o(j);
            if (le.busy && le.rob_idx == idx && le.is_store) {
                // Store 提交也只依赖旧状态，不能读取 LSQ new_。
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

        rob.commit_head_n();
        lsq.free_by_rob_n(idx);
    }
}

// ============================================================================
// step: 一个时钟周期
//
// 步骤 2-5（issue/execute/writeback/commit）可任意交换。
// 通过预计算 CDB 和严格分离各步骤的写入目标来实现交换性。
// ============================================================================
void CPU::step() {
    snap_all();
    step_fetch();
    CDB_Entry cdb = precompute_cdb();

    step_commit(cdb);
    step_writeback(cdb);
    step_execute(cdb);
    step_issue(cdb);
    
    upd_all();
}
