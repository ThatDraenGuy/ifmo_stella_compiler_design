set(TESTS_WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)
file(MAKE_DIRECTORY ${TESTS_WORKING_DIR})

add_subdirectory(c)
add_subdirectory(stella)
