# Catch2 Unit Test Suite (Subphase 1.7)
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)

FetchContent_MakeAvailable(Catch2)

add_executable(mmb_tests
    test_board.cpp
    test_move_orderer.cpp
    test_evaluators.cpp
    test_engines.cpp
)

target_link_libraries(mmb_tests PRIVATE minimax_core Catch2::Catch2WithMain)
target_compile_features(mmb_tests PRIVATE cxx_std_20)

include(CTest)
include(Catch)
catch_discover_tests(mmb_tests)
