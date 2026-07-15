#!/bin/bash
# build.sh — 一键编译 threadpool
g++ -std=c++17 -Wall -Wextra -pthread \
    main.cpp task_queue.cpp thread_pool.cpp \
    -o product/threadpool.exe
