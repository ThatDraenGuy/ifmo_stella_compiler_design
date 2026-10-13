cmake_minimum_required(VERSION 3.27)
project(stellac C)

set(CMAKE_C_STANDARD 11)

string(TOUPPER "${CMAKE_BUILD_TYPE}" uppercase_CMAKE_BUILD_TYPE)
if (uppercase_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    message(STATUS "Configuring a Debug build...")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

add_subdirectory(runtime)

include(CTest) 
