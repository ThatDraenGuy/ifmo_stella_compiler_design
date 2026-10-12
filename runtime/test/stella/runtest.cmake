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


execute_process(
    COMMAND ${TEST_EXECUTABLE} ${TEST_ARGS}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE stats
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

MESSAGE(STATUS "Test stdout: ${output}\nTest stderr: ${stats}\n")

if(status)
    MESSAGE(FATAL_ERROR "Test executing status: ${status}")
endif()

string(JSON ASSERTIONS_LEN LENGTH "${JSON_DATA}" assertions)
math(EXPR ASSERTIONS_LAST_INDEX "${ASSERTIONS_LEN} - 1")
if(ASSERTIONS_LEN GREATER 0)
    foreach(i RANGE ${ASSERTIONS_LAST_INDEX})
        string(JSON KEY MEMBER ${JSON_DATA} assertions ${i})
        string(JSON CHECK GET "${JSON_DATA}" assertions "${KEY}" check)
        string(JSON EXPECTED_VALUE GET "${JSON_DATA}" assertions "${KEY}" value)
        string(JSON ACTUAL_VALUE GET "${stats}" "${KEY}")

        if (CHECK STREQUAL "=")
            if (NOT ACTUAL_VALUE STREQUAL EXPECTED_VALUE)
                MESSAGE(FATAL_ERROR "Test assertion failed for key \"${KEY}\". Expected: ${EXPECTED_VALUE}; Actual: ${ACTUAL_VALUE}")
            endif()
        elseif(CHECK STREQUAL "<")
            if (NOT ACTUAL_VALUE LESS EXPECTED_VALUE)
                MESSAGE(FATAL_ERROR "Test assertion failed for key \"${KEY}\". Expected: less than ${EXPECTED_VALUE}; Actual: ${ACTUAL_VALUE}")
            endif()
        elseif(CHECK STREQUAL ">")
            if (NOT ACTUAL_VALUE GREATER EXPECTED_VALUE)
                MESSAGE(FATAL_ERROR "Test assertion failed for key \"${KEY}\". Expected: greater than ${EXPECTED_VALUE}; Actual: ${ACTUAL_VALUE}")
            endif()
        else()
            MESSAGE(FATAL_ERROR "Test assertion failed for key \"${KEY}\". Invalid CHECK: \"${CHECK}\"")
        endif()
    endforeach()
endif()
