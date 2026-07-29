cmake_minimum_required(VERSION 3.14)
project(RISC-V-Tomasulo-CPU-Simulator
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# 编译选项
# ============================================================
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O2)
    endif()
endif()

# ============================================================
# 可执行文件
# ============================================================
add_executable(code
    src/main.cpp
    src/cpu.cpp
    src/decoder.cpp
    src/memory.cpp
)

target_include_directories(code PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)