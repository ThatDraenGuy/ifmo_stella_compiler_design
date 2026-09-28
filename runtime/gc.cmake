project(stella_compiler_runtime_gc C)

set(GC_LIB ${PROJECT_NAME}_lib)
set(GC_LIB ${GC_LIB} PARENT_SCOPE)

set(STELLA_HEADERS ${CMAKE_CURRENT_LIST_DIR}/include)

file(GLOB_RECURSE GC_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/**.c)

add_library(${GC_LIB} ${GC_SOURCES})
target_include_directories(${GC_LIB} PRIVATE ${STELLA_HEADERS})