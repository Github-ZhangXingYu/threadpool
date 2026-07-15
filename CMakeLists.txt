cmake_minimum_required(VERSION 3.22)
project(threadpool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 源码 ----
add_library(threadpool_lib STATIC
    task_queue.cpp
    thread_pool.cpp
)

target_include_directories(threadpool_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---- 主程序 ----
add_executable(threadpool_demo main.cpp)
target_link_libraries(threadpool_demo threadpool_lib)

# ---- Google Test ----
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/googletest-src
)
set(BUILD_GMOCK OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(googletest)

enable_testing()

# ---- 测试目标 ----
add_executable(test_task_queue
    tests/test_task_queue.cpp
)
target_link_libraries(test_task_queue GTest::gtest_main threadpool_lib)
gtest_discover_tests(test_task_queue)

add_executable(test_thread_pool
    tests/test_thread_pool.cpp
)
target_link_libraries(test_thread_pool GTest::gtest_main threadpool_lib)
gtest_discover_tests(test_thread_pool)
