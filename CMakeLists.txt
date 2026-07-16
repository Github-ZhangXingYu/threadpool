cmake_minimum_required(VERSION 3.24)
project(threadpool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 输出目录（仿 FST 惯例）----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/product/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/product/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/product/lib)
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

# ---- 覆盖率选项（仿 FST ENABLE_COVERAGE）----
option(ENABLE_COVERAGE "Enable code coverage instrumentation (gcov/lcov)" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
    endif()
    message(STATUS "Coverage instrumentation enabled")
endif()

# ---- 源码 ----
add_library(threadpool_lib STATIC
    service/threadpool/task_queue.cpp
    service/threadpool/thread_pool.cpp
)

target_include_directories(threadpool_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/service/threadpool)

# ---- 主程序（demo）----
add_executable(threadpool_demo service/threadpool/main.cpp)
target_link_libraries(threadpool_demo threadpool_lib)

# ---- Google Test ----
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/googletest-src
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BUILD_GMOCK OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
enable_testing()

# ---- 测试目标（输出到 product/bin/unittest/）----
add_executable(test_task_queue
    tests/threadpool/unit/test_task_queue.cpp
)
target_link_libraries(test_task_queue GTest::gtest_main threadpool_lib)
set_target_properties(test_task_queue PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/product/bin/unittest
)
gtest_discover_tests(test_task_queue)

add_executable(test_thread_pool
    tests/threadpool/unit/test_thread_pool.cpp
)
target_link_libraries(test_thread_pool GTest::gtest_main threadpool_lib)
set_target_properties(test_thread_pool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/product/bin/unittest
)
gtest_discover_tests(test_thread_pool)
