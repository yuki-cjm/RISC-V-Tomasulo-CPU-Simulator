#include <iostream>

#include "memory.hpp"
#include "cpu.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path/to/file.data>" << std::endl;
        return 1;
    }

    int clock = 0;
    int max_clock;
    std::cin >> max_clock;
    Memory mem;
    mem.load_ins(argv[1]);
    CPU cpu(mem);
    while (clock < max_clock && !cpu.finish()) {
        clock++;
        cpu.step();
    }
    std::cout << "总时钟数：" << clock << std::endl;
}