# ============================================================
# Sequential Benchmark Suite (Subphase 1.9)
# ============================================================
add_executable(mmb_benchmark sequential_benchmark.cpp)
target_link_libraries(mmb_benchmark PRIVATE minimax_core)
target_compile_features(mmb_benchmark PRIVATE cxx_std_20)

# ============================================================
# CPU Microarchitectural Benchmark Suite
# ============================================================
add_executable(mmb_micro_benchmark micro_benchmark.cpp)
target_link_libraries(mmb_micro_benchmark PRIVATE minimax_core)
target_compile_features(mmb_micro_benchmark PRIVATE cxx_std_20)

# Optional Google Benchmark integration when requested
# FetchContent is only executed if MMB_USE_GOOGLE_BENCHMARK is ON
option(MMB_USE_GOOGLE_BENCHMARK "Enable Google Benchmark integration" OFF)
if(MMB_USE_GOOGLE_BENCHMARK)
    include(FetchContent)
    FetchContent_Declare(
        google_benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(google_benchmark)
endif()
