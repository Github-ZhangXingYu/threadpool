#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>
#include "thread_pool.h"

// ============================================================
// 基础生命周期测试
// ============================================================
class ThreadPoolLifecycleTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Ensure any pool is stopped before next test
    }
};

TEST_F(ThreadPoolLifecycleTest, StartThenStop_NoTasks) {
    ThreadPool pool(false);
    pool.start();
    EXPECT_EQ(pool.getThreadCount(), 4u);
    pool.stop();
    // No crash = pass
}

TEST_F(ThreadPoolLifecycleTest, DestructorStopsPool) {
    {
        ThreadPool pool(false);
        pool.start();
    }
    // Destructor called — should not hang or crash
    SUCCEED();
}

TEST_F(ThreadPoolLifecycleTest, DoubleStop_IsSafe) {
    ThreadPool pool(false);
    pool.start();
    pool.stop();
    pool.stop(); // second stop should be no-op
    SUCCEED();
}

TEST_F(ThreadPoolLifecycleTest, StopWithoutStart_IsSafe) {
    ThreadPool pool(false);
    pool.stop(); // should not crash
    SUCCEED();
}

TEST_F(ThreadPoolLifecycleTest, SubmitToUnstarted_QueueNotEmpty) {
    ThreadPool pool(false);
    std::atomic<bool> executed{false};
    pool.submit([&executed] { executed = true; });
    EXPECT_EQ(pool.getQueueSize(), 1u);
    // Pool was never started — stop() drains tasks inline
    pool.stop();
    EXPECT_EQ(pool.getQueueSize(), 0u);
    EXPECT_TRUE(executed); // drained by stop()
}

// ============================================================
// 任务执行测试
// ============================================================
class ThreadPoolExecutionTest : public ::testing::Test {};

TEST_F(ThreadPoolExecutionTest, SingleTask_Executes) {
    ThreadPool pool(false);
    pool.start();

    std::atomic<bool> executed{false};
    pool.submit([&executed] { executed = true; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.stop();
    EXPECT_TRUE(executed);
}

TEST_F(ThreadPoolExecutionTest, MultipleTasks_AllExecute) {
    ThreadPool pool(false);
    pool.start();

    const int TASK_COUNT = 50;
    std::atomic<int> counter{0};

    for (int i = 0; i < TASK_COUNT; ++i) {
        pool.submit([&counter] { counter.fetch_add(1); });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    pool.stop();

    EXPECT_EQ(counter.load(), TASK_COUNT);
}

TEST_F(ThreadPoolExecutionTest, TasksRunOnMultipleThreads) {
    ThreadPool pool(false);
    pool.start();

    std::mutex mtx;
    std::set<std::thread::id> threadIds;

    for (int i = 0; i < 200; ++i) {
        pool.submit([&mtx, &threadIds] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // ensure tasks spread across all threads
            std::lock_guard<std::mutex> lock(mtx);
            threadIds.insert(std::this_thread::get_id());
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    pool.stop();

    // With 4 worker threads and 20 tasks, we should see >1 unique thread
    EXPECT_GT(threadIds.size(), 1u);
    std::cout << "  Tasks executed on " << threadIds.size() << " distinct threads" << std::endl;
}

TEST_F(ThreadPoolExecutionTest, StopDrainsRemainingTasks) {
    ThreadPool pool(false);
    pool.start();

    const int TASK_COUNT = 200;
    std::atomic<int> counter{0};
    std::atomic<bool> allSubmitted{false};

    for (int i = 0; i < TASK_COUNT; ++i) {
        pool.submit([&counter] { counter.fetch_add(1); });
    }
    allSubmitted = true;

    // Immediately stop — stop() should drain all pending tasks
    pool.stop();

    EXPECT_EQ(counter.load(), TASK_COUNT)
        << "stop() should drain all remaining tasks in the queue";
}

TEST_F(ThreadPoolExecutionTest, TaskReturnValue_SharedVariable) {
    ThreadPool pool(false);
    pool.start();

    const int TASK_COUNT = 30;
    std::vector<int> results(TASK_COUNT);
    std::mutex resultMutex;

    for (int i = 0; i < TASK_COUNT; ++i) {
        pool.submit([i, &results, &resultMutex] {
            int squared = i * i;
            std::lock_guard<std::mutex> lock(resultMutex);
            results[i] = squared;
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    pool.stop();

    for (int i = 0; i < TASK_COUNT; ++i) {
        EXPECT_EQ(results[i], i * i);
    }
}

// ============================================================
// 优先级模式测试
// ============================================================
class ThreadPoolPriorityTest : public ::testing::Test {};

TEST_F(ThreadPoolPriorityTest, HighPriorityExecutesBeforeLow) {
    ThreadPool pool(true); // priority queue mode
    pool.start();

    std::vector<int> order;
    std::mutex orderMutex;

    // Submit tasks with mixed priorities
    pool.submit([&order, &orderMutex] {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(0);
    }, 0);

    pool.submit([&order, &orderMutex] {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(10);
    }, 10);

    pool.submit([&order, &orderMutex] {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(5);
    }, 5);

    pool.submit([&order, &orderMutex] {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(7);
    }, 7);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    pool.stop();

    ASSERT_EQ(order.size(), 4u);
    // Higher priorities (10, 7) should execute before lower (5, 0)
    // With 4 threads, exact ordering is non-deterministic, but the trend should hold
    EXPECT_GT(order[0], order[3]) << "Higher priority values should execute before lower ones";
    std::cout << "  Priority execution order: ";
    for (int v : order) std::cout << v << " ";
    std::cout << std::endl;
}

// ============================================================
// 并发和压力测试
// ============================================================
class ThreadPoolStressTest : public ::testing::Test {};

TEST_F(ThreadPoolStressTest, ManyShortTasks) {
    ThreadPool pool(false);
    pool.start();

    const int TASK_COUNT = 5000;
    std::atomic<int> counter{0};

    for (int i = 0; i < TASK_COUNT; ++i) {
        pool.submit([&counter] { counter.fetch_add(1); });
    }

    // Wait for queue to drain
    while (pool.getQueueSize() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    pool.stop();

    EXPECT_EQ(counter.load(), TASK_COUNT);
}

TEST_F(ThreadPoolStressTest, ConcurrentSubmitFromMultipleThreads) {
    ThreadPool pool(false);
    pool.start();

    const int THREADS = 4;
    const int TASKS_PER_THREAD = 200;
    std::atomic<int> counter{0};
    std::atomic<bool> startFlag{false};

    std::vector<std::thread> submitters;
    for (int t = 0; t < THREADS; ++t) {
        submitters.emplace_back([this, &pool, &counter, &startFlag] {
            while (!startFlag) { std::this_thread::yield(); }
            for (int i = 0; i < TASKS_PER_THREAD; ++i) {
                pool.submit([&counter] { counter.fetch_add(1); });
            }
        });
    }

    // Start all submitters at roughly the same time
    startFlag = true;

    for (auto& th : submitters) th.join();

    while (pool.getQueueSize() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    pool.stop();

    EXPECT_EQ(counter.load(), THREADS * TASKS_PER_THREAD);
}

TEST_F(ThreadPoolStressTest, TasksWithVaryingDurations) {
    ThreadPool pool(false);
    pool.start();

    std::atomic<int> counter{0};
    const int TASK_COUNT = 100;

    for (int i = 0; i < TASK_COUNT; ++i) {
        pool.submit([&counter, i] {
            // Varying work: some slow, some fast
            if (i % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            counter.fetch_add(1);
        });
    }

    while (pool.getQueueSize() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    pool.stop();

    EXPECT_EQ(counter.load(), TASK_COUNT);
}

// ============================================================
// 边界条件测试
// ============================================================
class ThreadPoolEdgeCaseTest : public ::testing::Test {};

TEST_F(ThreadPoolEdgeCaseTest, ThreadCount_AtStartAndStop) {
    ThreadPool pool(false);
    // Before start(): threads_ vector is empty
    EXPECT_EQ(pool.getThreadCount(), 0u);
    pool.start();
    EXPECT_EQ(pool.getThreadCount(), 4u); // THREAD_COUNT == 4
    pool.stop();
    EXPECT_EQ(pool.getThreadCount(), 0u); // threads_ cleared after stop
}

TEST_F(ThreadPoolEdgeCaseTest, QueueSizeAfterSubmit) {
    ThreadPool pool(false);
    pool.start();

    EXPECT_EQ(pool.getQueueSize(), 0u);
    pool.submit([] {});
    // May be 0 or 1 depending on if workers grabbed it already
    auto sz = pool.getQueueSize();
    EXPECT_LE(sz, 1u);

    pool.stop();
    EXPECT_EQ(pool.getQueueSize(), 0u);
}

TEST_F(ThreadPoolEdgeCaseTest, NullTaskSubmitted_NoCrash) {
    ThreadPool pool(false);
    pool.start();

    // Submit a null std::function
    pool.submit(TaskQueue::Task());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    pool.stop();
    // Should not crash — threadLoop checks if (task) before calling
    SUCCEED();
}

TEST_F(ThreadPoolEdgeCaseTest, PriorityMode_SubmitPriorityZeroByDefault) {
    ThreadPool pool(true);
    pool.start();

    std::atomic<int> called{0};
    pool.submit([&called] { called = 1; }); // default priority 0

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.stop();
    EXPECT_EQ(called, 1);
}

// ============================================================
// 线程 ID 验证
// ============================================================
TEST_F(ThreadPoolExecutionTest, WorkerThreadsAreDistinct) {
    ThreadPool pool(false);
    pool.start();
    EXPECT_EQ(pool.getThreadCount(), 4u);

    std::mutex mtx;
    std::set<std::thread::id> ids;

    for (int i = 0; i < 200; ++i) {
        pool.submit([&mtx, &ids] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // ensure tasks spread across threads
            std::lock_guard<std::mutex> lock(mtx);
            ids.insert(std::this_thread::get_id());
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    pool.stop();

    // Workers are 4 threads; some tasks may also be drained by stop() in main thread
    EXPECT_GE(ids.size(), 2u) << "Expected at least 2 distinct threads for multi-threaded execution";
    EXPECT_LE(ids.size(), 5u) << "Expected at most 5 (4 workers + 1 main/stop drain)";
}
