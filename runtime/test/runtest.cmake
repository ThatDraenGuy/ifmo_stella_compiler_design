file(GLOB_RECURSE FILES ${TEST_WORKING_DIR}/**)
if (NOT "${FILES}" STREQUAL "")
    file(REMOVE ${FILES})
endif ()

execute_process(COMMAND ${TEST_EXECUTABLE} RESULT_VARIABLE status)

file(GLOB_RECURSE FILES ${TEST_WORKING_DIR}/**)
if (NOT "${FILES}" STREQUAL "")
    file(REMOVE ${FILES})
endif ()
if(status)
    MESSAGE(FATAL_ERROR "Test executing status: ${status}")
endif()
