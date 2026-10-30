# Google Benchmark setup via FetchContent
include(FetchContent)

FetchContent_Declare(
    google_benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(google_benchmark)

message(STATUS "Google Benchmark ready to link with target 'benchmark::benchmark'")
