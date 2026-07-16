#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include "thread_pool.h"

#ifdef _WIN32
#include <windows.h>
#endif

static std::mutex cout_mutex;
#define LOCKED_LOG(msg) do { \
    std::lock_guard<std::mutex> lock(cout_mutex); \
    std::cout << msg; \
} while(0)

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "=== 普通任务队列演示 ===" << std::endl;
    {
        ThreadPool pool(false);
        pool.start();

        for (int i = 0; i < 8; ++i) {
            pool.submit([i] {
                LOCKED_LOG("任务 " << i << " 由线程 " << std::this_thread::get_id()
                           << " 执行" << std::endl);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        pool.stop();
    }

    std::cout << "\n=== 优先队列演示 (数字越大优先级越高) ===" << std::endl;
    {
        ThreadPool pool(true);
        pool.start();

        pool.submit([] { LOCKED_LOG("低优先级任务 [0]" << std::endl); }, 0);
        pool.submit([] { LOCKED_LOG("中优先级任务 [5]" << std::endl); }, 5);
        pool.submit([] { LOCKED_LOG("高优先级任务 [10]" << std::endl); }, 10);
        pool.submit([] { LOCKED_LOG("超低优先级任务 [-5]" << std::endl); }, -5);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pool.stop();
    }

    std::cout << "\n=== 性能测试 ===" << std::endl;
    {
        ThreadPool pool(false);
        pool.start();

        auto start = std::chrono::high_resolution_clock::now();
        const int TASK_COUNT = 1000;

        for (int i = 0; i < TASK_COUNT; ++i) {
            pool.submit([i] {
                volatile int sum = 0;
                for (int j = 0; j < 1000; ++j) {
                    sum += j;
                }
            });
        }

        while (pool.getQueueSize() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "完成 " << TASK_COUNT << " 个任务，耗时 " << duration.count() << " ms" << std::endl;
        pool.stop();
    }

    std::cout << "\n演示结束" << std::endl;
    return 0;
}
