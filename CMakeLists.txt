cmake_minimum_required(VERSION 3.0)
project(xvc_server C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-Wall")

add_executable(xvc_server main.c xilinx_bit.c)