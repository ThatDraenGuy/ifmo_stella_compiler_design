execute_process(COMMAND ${TEST_EXECUTABLE} RESULT_VARIABLE status)

if(status)
    MESSAGE(FATAL_ERROR "Test executing status: ${status}")
endif()
