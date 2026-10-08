project(stella_compiler_runtime C)

set(USER_SRC "examples/double.c" STRING "User's Stella program (trnslated into C)")

set(RUNTIME_APP ${PROJECT_NAME}_app)
set(RUNTIME_APP ${RUNTIME_APP} PARENT_SCOPE)

include(gc.cmake)

set(STELLA_HEADERS ${CMAKE_CURRENT_LIST_DIR}/include)

file(GLOB_RECURSE RUNTIME_SOURCES ${CMAKE_CURRENT_LIST_DIR}/runtime/**.c)
file(GLOB USER_SOURCES ${USER_SRC})

add_executable(${RUNTIME_APP} ${RUNTIME_SOURCES} ${USER_SOURCES})
target_include_directories(${RUNTIME_APP} PRIVATE ${STELLA_HEADERS})
target_link_libraries(${RUNTIME_APP} PRIVATE ${GC_LIB})
