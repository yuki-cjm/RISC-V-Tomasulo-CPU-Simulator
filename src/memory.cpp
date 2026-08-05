#include <fstream>
#include <iostream>

#include "type.h"
#include "memory.hpp"


void Memory::write_byte(u32 addr, u8 val) {
    bytes[addr] = val;
}

void Memory::write_half(u32 addr, u16 val) {
    bytes[addr] = val & 0xFF;
    bytes[addr + 1] = (val >> 8) & 0xFF;
}

void Memory::write_word(u32 addr, u32 val) {
    if (addr + 3 >= MEM_SIZE) {
        fprintf(stderr, "Memory write_word out of bounds: addr=0x%x val=0x%x\n", addr, val);
        fatal("Memory write_word out of bounds");
    }
    bytes[addr] = val & 0xFF;
    bytes[addr + 1] = (val >> 8) & 0xFF;
    bytes[addr + 2] = (val >> 16) & 0xFF;
    bytes[addr + 3] = (val >> 24) & 0xFF;
}

u8 Memory::read_byte(u32 addr) {
    return bytes[addr];
}

u16 Memory::read_half(u32 addr) {
    return bytes[addr] | (bytes[addr + 1] << 8);
}

u32 Memory::read_word(u32 addr) {
    return bytes[addr] | (bytes[addr + 1] << 8) | (bytes[addr + 2] << 16) | (bytes[addr + 3] << 24);
}

void Memory::load_ins(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        fatal("Cannot open file: " + path);
    }

    std::string line;
    u32 addr = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        if (line[0] == '@') {
            addr = std::stoul(line.substr(1), nullptr, 16);
        } else {
            std::istringstream ss(line);
            std::string byte_str;
            while (ss >> byte_str) {
                if (addr >= MEM_SIZE) {
                    fatal("Memory overflow at 0x" + to_hex(addr));
                }
                bytes[addr++] = static_cast<u8>(std::stoul(byte_str, nullptr, 16));
            }
        }
    }
}

// use for debugging
void Memory::print() {
    for (u32 i = 0; i < MEM_SIZE; i++) {
        if (bytes[i] != 0) {
            std::cout << std::hex << std::setfill('0')
                      << std::setw(8) << i << ": "
                      << std::setw(2) << static_cast<int>(bytes[i])
                      << std::dec << std::endl;
        }
    }
}