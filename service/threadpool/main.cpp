#include <iostream>
#include <chrono>
#include <atomic>
#include "thread_pool.h"

int main() {
    constexpr int TASK_COUNT = 1000;
    constexpr int THREAD_COUNT = 4;

    ThreadPool pool(false);
    pool.start();

    std::atomic<uint64_t> checksum{0};
    std::atomic<int> completed{0};

    auto t0 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < TASK_COUNT; ++i) {
        pool.submit([i, &checksum, &completed] {
            // 每个任务根据自身 ID 做不同数量的计算，模拟真实负载
            // 任务 i: 迭代 i * 50 + 100 次，计算累加
            uint64_t local = 0;
            int loops = i * 50 + 100;
            for (int k = 0; k < loops; ++k) {
                local += k * k;
            }
            checksum.fetch_add(local, std::memory_order_relaxed);
            completed.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // stop() 会排空所有剩余任务
    pool.stop();

    auto t1 = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

    std::cout << "=== ThreadPool 基准测试 ===" << std::endl;
    std::cout << "线程数:     " << THREAD_COUNT << std::endl;
    std::cout << "任务数:     " << TASK_COUNT << std::endl;
    std::cout << "完成数:     " << completed.load() << std::endl;
    std::cout << "总耗时:     " << elapsed.count() << " μs ("
              << elapsed.count() / 1000.0 << " ms)" << std::endl;
    std::cout << "吞吐量:     " << TASK_COUNT * 1'000'000 / elapsed.count()
              << " 任务/秒" << std::endl;
    std::cout << "校验和:     " << checksum.load() << std::endl;

    return 0;
}
