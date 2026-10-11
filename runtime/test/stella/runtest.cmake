file(READ ${TEST_PARAMS} JSON_DATA)

#ARGS
string(JSON ARGS_LEN LENGTH ${JSON_DATA} args)
math(EXPR ARGS_LAST_INDEX "${ARGS_LEN} - 1")

set(TEST_ARGS "")
if(ARGS_LEN GREATER 0)
    foreach(i RANGE ${ARGS_LAST_INDEX})
        string(JSON ELEM GET ${JSON_DATA} args ${i})
        list(APPEND TEST_ARGS "${ELEM}")
    endforeach()
endif()


execute_process(COMMAND echo ${TEST_ARGS})
execute_process(
    COMMAND ${TEST_EXECUTABLE} ${TEST_ARGS}
    RESULT_VARIABLE status
)

if(status)
    MESSAGE(FATAL_ERROR "Test executing status: ${status}")
endif()
