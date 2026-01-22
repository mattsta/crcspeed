cmake_minimum_required(VERSION 3.16)
project(crcspeed C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT MSVC)
    # Compiler flags
    add_compile_options(-O2 -Wall -Wextra)

    # Enable SIMD instructions on x86
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64|i386|i686")
        add_compile_options(-mpclmul -msse4.1)
    endif()

    # Enable crypto extensions on ARM64
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        # Check if using GCC or Clang
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            # GCC requires explicit crypto extension flag on all ARM64 platforms
            add_compile_options(-march=armv8-a+crypto)
        elseif(APPLE)
            # AppleClang on Apple Silicon - use native CPU features
            add_compile_options(-mcpu=apple-m1)
        else()
            # Other Clang on ARM64 Linux
            add_compile_options(-march=armv8-a+crypto)
        endif()
    endif()
endif()

# Core library sources
set(CRC_SOURCES
    crcspeed.c
    crc64speed.c
    crc16speed.c
    crc_simd.c
)

# Original benchmark executable
add_executable(crcspeed ${CRC_SOURCES} main.c)

# Individual CRC test executables
add_executable(crc64speed_test crcspeed.c crc64speed.c)
target_compile_definitions(crc64speed_test PRIVATE CRCSPEED_TEST_MAIN)

add_executable(crc16speed_test crcspeed.c crc16speed.c)
target_compile_definitions(crc16speed_test PRIVATE CRCSPEED_TEST_MAIN)

# Comprehensive test suite
add_executable(crc_test ${CRC_SOURCES} crc_test.c)

# Benchmark suite
add_executable(crc_bench ${CRC_SOURCES} crc_bench.c)

# Constant generator (for development)
add_executable(gen_constants gen_constants.c)

# Enable testing
enable_testing()

# Legacy individual implementation tests
add_test(NAME crc64_basic_test COMMAND crc64speed_test)
add_test(NAME crc16_basic_test COMMAND crc16speed_test)

# Comprehensive test suite - run all tests
add_test(NAME crc_all COMMAND crc_test all)

# CRC64 test groups
add_test(NAME crc64_vectors COMMAND crc_test crc64)
add_test(NAME crc64_sizes COMMAND crc_test crc64_sizes)
add_test(NAME crc64_alignment COMMAND crc_test crc64_align)

# CRC16 test groups
add_test(NAME crc16_vectors COMMAND crc_test crc16)
add_test(NAME crc16_sizes COMMAND crc_test crc16_sizes)
add_test(NAME crc16_alignment COMMAND crc_test crc16_align)
add_test(NAME crc16_initial_crc COMMAND crc_test crc16_init)

# Cross-implementation tests
add_test(NAME consistency COMMAND crc_test consistency)

# Combined test groups
add_test(NAME test_vectors COMMAND crc_test vectors)
add_test(NAME simd_tests COMMAND crc_test simd)

# Custom target to run benchmarks
add_custom_target(benchmark
    COMMAND crc_bench 1048576 100
    DEPENDS crc_bench
    COMMENT "Running CRC benchmarks..."
)
