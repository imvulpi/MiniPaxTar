cmake_minimum_required(VERSION 3.14)
project(minipaxtar C)

# Default options
option(MPTAR_SUPPORT_EXTRA_TIMES "Enable PAX atime and ctime support" ON)
option(MPTAR_SUPPORT_SPECIAL     "Enable special device nodes (devmajor/devminor)" ON)

# Set C standard
set(CMAKE_C_STANDARD 89)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

if(MPTAR_SUPPORT_SPECIAL)
    target_compile_definitions(minipaxtar PUBLIC MPTAR_SUPPORT_SPECIAL)
    target_compile_definitions(minipaxtar_header INTERFACE MPTAR_SUPPORT_SPECIAL)
endif()

if(MPTAR_SUPPORT_EXTRA_TIMES)
    target_compile_definitions(minipaxtar PUBLIC MPTAR_SUPPORT_EXTRA_TIMES)
    target_compile_definitions(minipaxtar_header INTERFACE MPTAR_SUPPORT_EXTRA_TIMES)
endif()

# ALIASES for clean usage
add_library(minipaxtar::minipaxtar ALIAS minipaxtar)
add_library(minipaxtar::minipaxtar_header ALIAS minipaxtar_header)