cmake_minimum_required(VERSION 3.25)

project(
    minimax-parallel-benchmark
    VERSION 0.1.0
    DESCRIPTION "HPC research: Parallel Minimax vs Sequential Alpha-Beta in Connect 4"
    LANGUAGES CXX
)

# ============================================================
# C++20 Standard and Global Properties
# ============================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clangd / language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Platform-Specific Compilation Flags (Windows/Linux)
# ============================================================
if(MSVC)
    add_compile_options(
        /W4
        /WX-
    )
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
else()
    # GCC and Clang (Linux / WSL / MinGW)
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -Og -g3")
    # Standard threading library required on Linux
    find_package(Threads REQUIRED)
endif()

# ============================================================
# Project Configurable Options
# ============================================================
option(MMB_BUILD_TESTS      "Build unit tests"       ON)
option(MMB_BUILD_BENCHMARKS "Build Google Benchmark" OFF)
option(MMB_BUILD_TOOLS      "Build debug tools"      ON)

# ============================================================
# Core Library (Interface/Header-only target)
# ============================================================
add_library(minimax_core INTERFACE)
target_include_directories(minimax_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(minimax_core INTERFACE cxx_std_20)

if(NOT MSVC)
    target_link_libraries(minimax_core INTERFACE Threads::Threads)
endif()

# ============================================================
# Main Executable (Smoke test / CLI runner)
# ============================================================
add_executable(mmb_runner src/main.cpp)
target_link_libraries(mmb_runner PRIVATE minimax_core)

# ============================================================
# Developer Tools / Debugging
# ============================================================
if(MMB_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# ============================================================
# Tests (Placeholder for Phase 1)
# ============================================================
if(MMB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================
# Benchmarks (Google Benchmark Integration)
# ============================================================
if(MMB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ============================================================
# Configuration Summary
# ============================================================
message(STATUS "")
message(STATUS "=== minimax-parallel-benchmark ===")
message(STATUS "  Version     : ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler    : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Tests       : ${MMB_BUILD_TESTS}")
message(STATUS "  Benchmarks  : ${MMB_BUILD_BENCHMARKS}")
message(STATUS "  Tools       : ${MMB_BUILD_TOOLS}")
message(STATUS "")
