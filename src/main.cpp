#include <iostream>
#include "type.h"
#include "cpu.hpp"
#include "memory.hpp"

int main(int argc, char* argv[]) {
    Memory mem;

// 取消下面这行注释即可切回本地文件模式
// #define LOCAL_FILE_MODE
#ifdef LOCAL_FILE_MODE
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <data>\n";
        return 1;
    }
    mem.load_ins(argv[1]);
#else
    mem.load_ins(std::cin);
#endif

    CPU cpu(mem);
    while (!cpu.is_finished()) {
        cpu.step();
    }
}
