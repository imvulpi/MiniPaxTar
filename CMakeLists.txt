cmake_minimum_required(VERSION 3.14)
project(minipaxtar C)

# Set C standard (e.g., C99)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# an INTERFACE library for single-header usage
add_library(minipaxtar_header INTERFACE)
target_include_directories(minipaxtar_header INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dist>
    $<INSTALL_INTERFACE:include>
)

# a STATIC library for standard two-file usage
add_library(minipaxtar STATIC minipaxtar.c)
target_include_directories(minipaxtar PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)