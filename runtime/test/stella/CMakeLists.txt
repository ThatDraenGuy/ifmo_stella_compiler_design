set(PROJECT_TEST ${PROJECT_NAME}_test)

file(GLOB_RECURSE TEST_ST_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/**.stella)
file(GLOB_RECURSE TEST_ST_PARAMS ${CMAKE_CURRENT_LIST_DIR}/params/**.json)

find_program(DOCKER docker REQUIRED)

foreach(TEST_ST_SOURCE ${TEST_ST_SOURCES})
    get_filename_component(TEST_FILE_NAME ${TEST_ST_SOURCE} NAME_WE)
    set(TEST_C_NAME ${TEST_FILE_NAME}.stella.c)

    set(TEST_ID ${PROJECT_TEST}_${TEST_FILE_NAME})
    set(TEST_C_SOURCE ${TESTS_WORKING_DIR}/${TEST_C_NAME})

    execute_process(
        COMMAND docker run -i  fizruk/stella compile /dev/stdin -o /dev/stdout
        WORKING_DIRECTORY ${TESTS_WORKING_DIR}
        INPUT_FILE ${TEST_ST_SOURCE}
        OUTPUT_FILE ${TEST_C_SOURCE}
    )

    add_executable(${TEST_ID} ${TEST_C_SOURCE} ${RUNTIME_SOURCES})
    target_include_directories(${TEST_ID} PRIVATE ${STELLA_HEADERS})
    target_link_libraries(${TEST_ID} PRIVATE ${GC_LIB})
endforeach()


foreach(TEST_ST_PARAM ${TEST_ST_PARAMS})
    get_filename_component(TEST_NAME ${TEST_ST_PARAM} NAME_WE)

    file(READ ${TEST_ST_PARAM} JSON_DATA)
    string(JSON TEST_EXEC GET ${JSON_DATA} program)
    set(TEST_ID ${PROJECT_TEST}_${TEST_EXEC})

    #ENV
    string(JSON ENV_LEN LENGTH ${JSON_DATA} env)
    math(EXPR ENV_LAST_INDEX "${ENV_LEN} - 1")

    set(TEST_ENV "")
    if(ENV_LEN GREATER 0)
        foreach(i RANGE ${ENV_LAST_INDEX})
            string(JSON ELEM GET ${JSON_DATA} env ${i})
            list(APPEND TEST_ENV "${ELEM}")
        endforeach()
    endif()
    list(JOIN TEST_ENV "\;" TEST_ENV_STR)

    add_test(
        NAME ${TEST_NAME}
        COMMAND ${CMAKE_COMMAND} -DTEST_EXECUTABLE=$<TARGET_FILE:${TEST_ID}> -DTEST_WORKING_DIR=${TESTS_WORKING_DIR} -DTEST_PARAMS=${TEST_ST_PARAM} -P ${CMAKE_CURRENT_SOURCE_DIR}/runtest.cmake
        WORKING_DIRECTORY ${TESTS_WORKING_DIR}
    )
    set_tests_properties(${TEST_NAME} PROPERTIES
        ENVIRONMENT ${TEST_ENV_STR}
    )
endforeach()

