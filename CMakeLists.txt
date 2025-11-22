cmake_minimum_required(VERSION 4.2)
project(Onyx VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(LIB_SOURCES
    src/Transpiler.cpp
)

set(SOURCES
    main.cpp
)

add_library(ox STATIC ${LIB_SOURCES})

add_executable(oxc ${SOURCES})
target_link_libraries(oxc PRIVATE ox)

# --- CTest Integration ---
enable_testing()

file(GLOB ONYX_TESTS "tests/*.ox")

foreach(TEST_FILE ${ONYX_TESTS})
    # Get the filename without the extension to use as the test name
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    add_test(
        NAME ${TEST_NAME}
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh ${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endforeach()