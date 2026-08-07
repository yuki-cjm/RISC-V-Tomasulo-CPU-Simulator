#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <sstream>
#include <iomanip>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;

constexpr u32 REG_COUNT   = 32;
constexpr u32 MEM_SIZE    = 0x200000;
constexpr u32 INSTR_LEN   = 4;
constexpr u32 MEM_LATENCY = 3;
constexpr u32 ROB_SIZE    = 16;
constexpr u32 LSQ_SIZE    = 16;
constexpr u32 RS_ALU_SIZE = 4;
constexpr u32 RS_LD_SIZE  = 3;
constexpr u32 RS_ST_SIZE  = 3;
constexpr u32 RS_BR_SIZE  = 2;
struct FU_Result{u32 value;};

enum class Instr { 
    // Type: R      Opcode: 0b0110011
    ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU, 
    // Type: I/I*   Opcode: 0b0010011
    ADDI, ANDI, ORI, XORI, SLLI, SRLI, SRAI, SLTI, SLTIU, 
    // Type: I      Opcode: 0b0000011
    LB, LBU, LH, LHU, LW, 
    // Type: S      Opcode: 0b0100011
    SB, SH, SW, 
    // Type: B      Opcode: 0b1100011
    BEQ, BGE, BGEU, BLT, BLTU, BNE, 
    // Type: J      Opcode: 0b1101111
    JAL, 
    // Type: I      Opcode: 0b1100111
    JALR, 
    // Type: U      Opcode: 0b0010111
    AUIPC, 
    // Type: U      Opcode: 0b0110111
    LUI, 
    // Type: I      Opcode: 0b1110011
    EBREAK, ECALL, 
    // HALT
    HALT, 
    // UNKNOWN
    UNKNOWN,
};

enum class InstrState {
    NONE,
    FETCHED,
    ISSUED,
    EXECUTING,
    WRITTEN,
    COMMITTED,
};

static std::string to_hex(u32 val) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << val;
    return ss.str();
}

inline void fatal(const std::string& message) {
    fprintf(stderr, "FATAL: %s\n", message.c_str());
    std::exit(1);
}