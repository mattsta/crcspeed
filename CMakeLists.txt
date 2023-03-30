cmake_minimum_required(VERSION 3.1)
project(crcspeed C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-O2 -Wall -Wextra)
add_executable(crcspeed crcspeed.c crc64speed.c crc16speed.c main.c)

add_executable(crc64speed crcspeed.c crc64speed.c)
target_compile_definitions(crc64speed PRIVATE CRCSPEED_TEST_MAIN)

add_executable(crc16speed crcspeed.c crc16speed.c)
target_compile_definitions(crc16speed PRIVATE CRCSPEED_TEST_MAIN)
