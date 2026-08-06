#include <iostream>
#include "type.h"
#include "cpu.hpp"
#include "memory.hpp"
int main(int argc,char*argv[]) {
    if(argc < 2 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <data> [max_cycles] [perm]\n";
        std::cerr << "  perm: 步骤排列 0-23 (0=IEWC 默认)\n";
        return 1;
    }
    u64 max_c = 500000000ULL;
    if (argc >= 3) {
        max_c = std::stoull(argv[2]);
    }
    int perm = 0;
    if (argc >= 4) {
        perm = std::stoi(argv[3]);
        if (perm < 0 || perm >= PERM_COUNT) {
            std::cerr << "perm must be 0-23\n";
            return 1;
        }
    }
    Memory mem;
    mem.load_ins(argv[1]);
    CPU cpu(mem, perm);
    u64 cyc = 0;
    while( cyc < max_c && !cpu.is_finished()) {
        cyc++;
        cpu.step();
    }
    std::cout << "总时钟数：" << cyc << std::endl;
    u64 tb = cpu.total_br();
    if(tb > 0) {
        std::cout << "分支预测准确率：" << (100.0 * cpu.correct_br() / tb) << "% (" << cpu.correct_br() << "/" << tb << ")" << std::endl;
    } else {
        std::cout << "分支预测准确率: N/A (无分支指令)" << std::endl;
    }
}
