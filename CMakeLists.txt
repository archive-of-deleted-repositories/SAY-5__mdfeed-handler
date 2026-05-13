cmake_minimum_required(VERSION 3.20)
project(mdfeed_handler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(MDFEED_BUILD_TESTS "Build unit + integration tests" ON)
option(MDFEED_BUILD_FUZZ  "Build libFuzzer harnesses (needs clang)" OFF)
option(MDFEED_ASAN        "Enable AddressSanitizer + UBSan" OFF)

# -------- Warnings + sanitizer flags --------
add_library(mdfeed_warnings INTERFACE)
target_compile_options(mdfeed_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
    -Wnon-virtual-dtor -Wdouble-promotion -Wnull-dereference
)

if(MDFEED_ASAN)
    add_library(mdfeed_asan INTERFACE)
    target_compile_options(mdfeed_asan INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -g
    )
    target_link_options(mdfeed_asan INTERFACE
        -fsanitize=address,undefined
    )
endif()

# -------- Core library --------
add_library(mdfeed_core STATIC
    src/core/normalize_a.cpp
    src/core/normalize_b.cpp
    src/core/bbo_book.cpp
    src/core/feed_handler.cpp
    src/net/udp_socket.cpp
    src/obs/clock.cpp
    src/obs/histogram.cpp
    src/sim/feed_simulator.cpp
)
target_include_directories(mdfeed_core PUBLIC src)
target_link_libraries(mdfeed_core PUBLIC mdfeed_warnings)
if(MDFEED_ASAN)
    target_link_libraries(mdfeed_core PUBLIC mdfeed_asan)
endif()
find_package(Threads REQUIRED)
target_link_libraries(mdfeed_core PUBLIC Threads::Threads)

# -------- Binaries --------
add_executable(mdfeed_handler src/main.cpp)
target_link_libraries(mdfeed_handler PRIVATE mdfeed_core)

add_executable(mdfeed_simulator src/sim_main.cpp)
target_link_libraries(mdfeed_simulator PRIVATE mdfeed_core)

add_executable(loopback_bench bench/loopback_bench.cpp)
target_link_libraries(loopback_bench PRIVATE mdfeed_core)

# -------- Tests --------
if(MDFEED_BUILD_TESTS)
    include(FetchContent)
    set(FETCHCONTENT_QUIET FALSE)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
        URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)

    add_executable(unit_tests
        tests/unit/normalize_a_test.cpp
        tests/unit/normalize_b_test.cpp
        tests/unit/bbo_book_test.cpp
        tests/unit/histogram_test.cpp
        tests/unit/clock_test.cpp
    )
    target_link_libraries(unit_tests PRIVATE mdfeed_core GTest::gtest_main)
    gtest_discover_tests(unit_tests DISCOVERY_TIMEOUT 30)

    add_executable(integration_tests
        tests/integration/loopback_test.cpp
    )
    target_link_libraries(integration_tests PRIVATE mdfeed_core GTest::gtest_main)
    gtest_discover_tests(integration_tests DISCOVERY_TIMEOUT 60 PROPERTIES TIMEOUT 60)
endif()

# -------- Fuzz (clang-only) --------
if(MDFEED_BUILD_FUZZ)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
       AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        message(FATAL_ERROR "MDFEED_BUILD_FUZZ requires clang")
    endif()
    add_executable(normalize_a_fuzz tests/fuzz/normalize_a_fuzz.cpp
        src/core/normalize_a.cpp)
    target_include_directories(normalize_a_fuzz PRIVATE src)
    target_compile_options(normalize_a_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined -g)
    target_link_options(normalize_a_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined)

    add_executable(normalize_b_fuzz tests/fuzz/normalize_b_fuzz.cpp
        src/core/normalize_b.cpp)
    target_include_directories(normalize_b_fuzz PRIVATE src)
    target_compile_options(normalize_b_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined -g)
    target_link_options(normalize_b_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined)
endif()
