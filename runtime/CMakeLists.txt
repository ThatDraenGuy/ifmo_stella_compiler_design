include(gc.cmake)


project(stellac_runtime C)

if (DEFINED USER_SRC_ST)
    set(USER_SRC_ST_IS_ABS false)
    cmake_path(IS_ABSOLUTE USER_SRC_ST USER_SRC_ST_IS_ABS)
    if(${USER_SRC_ST_IS_ABS})
        set(USER_SRC_ST_PATH ${USER_SRC_ST})
    else()
        cmake_path(SET USER_SRC_ST_PATH NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/${USER_SRC_ST}")
    endif()

    get_filename_component(USER_FILE_NAME ${USER_SRC_ST_PATH} NAME_WE)
    set(USER_SRC ${CMAKE_CURRENT_BINARY_DIR}/${USER_FILE_NAME}.stella.c)

    find_program(DOCKER docker REQUIRED)
    if (NOT DOCKER)
        message(FATAL_ERROR "Docker was not found on your system!")
    endif()
    execute_process(
        COMMAND ${DOCKER} run -i  fizruk/stella compile /dev/stdin -o /dev/stdout
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        INPUT_FILE ${USER_SRC_ST_PATH}
        OUTPUT_FILE ${USER_SRC}
    )
    message(STATUS "INFO: Using user Stella program \"${USER_SRC_ST_PATH}\".")
elseif(DEFINED USER_SRC_C)
    set(USER_SRC ${USER_SRC_C})
    message(STATUS "INFO: Using user C program \"${USER_SRC_C}\".")
else()
    message(STATUS "WARNING: No user program specified, not compiling executable")
endif()

set(STELLA_HEADERS ${CMAKE_CURRENT_LIST_DIR}/include)
file(GLOB_RECURSE RUNTIME_SOURCES ${CMAKE_CURRENT_LIST_DIR}/runtime/**.c)

if(DEFINED USER_SRC)
    set(RUNTIME_APP ${PROJECT_NAME}_app)
    set(RUNTIME_APP ${RUNTIME_APP} PARENT_SCOPE)
    file(GLOB USER_SOURCES ${USER_SRC})

    add_executable(${RUNTIME_APP} ${RUNTIME_SOURCES} ${USER_SOURCES})
    target_include_directories(${RUNTIME_APP} PRIVATE ${STELLA_HEADERS})
    target_link_libraries(${RUNTIME_APP} PRIVATE ${GC_LIB})
endif()

enable_testing()
add_subdirectory(test)