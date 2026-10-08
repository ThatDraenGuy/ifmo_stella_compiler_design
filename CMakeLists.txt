cmake_minimum_required(VERSION 3.27)
project(stella_compiler C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
add_link_options(-fsanitize=address)

add_subdirectory(runtime)

enable_testing()