#pragma once

#include <cstring>

#include "type.h"

class Memory {
  private:
    u8 bytes[MEM_SIZE];
  public:
    Memory() : bytes{} {}

    void write_byte(u32 addr, u8 val);
    void write_half(u32 addr, u16 val);
    void write_word(u32 addr, u32 val);
    u8 read_byte(u32 addr);
    u16 read_half(u32 addr);
    u32 read_word(u32 addr);

    void load_ins(const std::string& path);
    void load_ins(std::istream& is);

    void print(); // use for debugging
};