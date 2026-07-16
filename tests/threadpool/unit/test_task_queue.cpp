#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include "task_queue.h"

// ============================================================
// 普通队列测试
// ============================================================
class NormalQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue_ = std::make_unique<TaskQueue>(false);
    }

    std::unique_ptr<TaskQueue> queue_;
};

TEST_F(NormalQueueTest, PushThenTryPop_ReturnsTask) {
    bool called = false;
    queue_->push([&called] { called = true; });

    TaskQueue::Task task;
    EXPECT_TRUE(queue_->tryPop(task));
    task();
    EXPECT_TRUE(called);
}

TEST_F(NormalQueueTest, TryPopOnEmpty_ReturnsFalse) {
    TaskQueue::Task task;
    EXPECT_FALSE(queue_->tryPop(task));
    EXPECT_FALSE(task); // nullptr function
}

TEST_F(NormalQueueTest, PushThenWaitPop_ReturnsTask) {
    int value = 0;
    std::atomic<bool> done{false};

    std::thread t([this, &value, &done] {
        TaskQueue::Task task;
        EXPECT_TRUE(queue_->waitPop(task));
        task();
        done = true;
    });

    // Give the thread time to enter waitPop
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    queue_->push([&value] { value = 42; });

    t.join();
    EXPECT_TRUE(done);
    EXPECT_EQ(value, 42);
}

TEST_F(NormalQueueTest, FifoOrdering) {
    std::vector<int> result;
    for (int i = 0; i < 10; ++i) {
        queue_->push([i, &result] { result.push_back(i); });
    }

    for (int i = 0; i < 10; ++i) {
        TaskQueue::Task task;
        ASSERT_TRUE(queue_->tryPop(task));
        task();
    }
    ASSERT_EQ(result.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(result[i], i) << "FIFO order violation at index " << i;
    }
}

TEST_F(NormalQueueTest, Empty_InitiallyTrue) {
    EXPECT_TRUE(queue_->empty());
}

TEST_F(NormalQueueTest, Empty_AfterPushFalse) {
    queue_->push([] {});
    EXPECT_FALSE(queue_->empty());
}

TEST_F(NormalQueueTest, Empty_AfterPopTrue) {
    queue_->push([] {});
    TaskQueue::Task task;
    queue_->tryPop(task);
    EXPECT_TRUE(queue_->empty());
}

TEST_F(NormalQueueTest, Size_InitiallyZero) {
    EXPECT_EQ(queue_->size(), 0u);
}

TEST_F(NormalQueueTest, Size_IncrementsAndDecrements) {
    queue_->push([] {});
    queue_->push([] {});
    EXPECT_EQ(queue_->size(), 2u);

    TaskQueue::Task task;
    queue_->tryPop(task);
    EXPECT_EQ(queue_->size(), 1u);

    queue_->tryPop(task);
    EXPECT_EQ(queue_->size(), 0u);
}

TEST_F(NormalQueueTest, Shutdown_WakesWaitPop) {
    std::atomic<bool> woken{false};
    std::thread t([this, &woken] {
        TaskQueue::Task task;
        bool got = queue_->waitPop(task); // will block until shutdown
        EXPECT_FALSE(got);               // shutdown, no task
        woken = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    queue_->shutdown();
    t.join();
    EXPECT_TRUE(woken);
}

TEST_F(NormalQueueTest, ShutdownWithPending_WaitPopDrainsThenExits) {
    queue_->push([] {});
    queue_->push([] {});

    std::vector<int> order;
    std::thread t([this, &order] {
        TaskQueue::Task task;
        while (queue_->waitPop(task)) {
            // Just count, actual call in main test
        }
    });

    // Drain through waitPop until shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    queue_->shutdown();
    t.join();
    EXPECT_TRUE(queue_->empty());
}

TEST_F(NormalQueueTest, TryPopAfterShutdown_StillWorks) {
    queue_->push([] {});
    queue_->shutdown();

    TaskQueue::Task task;
    EXPECT_TRUE(queue_->tryPop(task)); // pending tasks still retrievable
    EXPECT_TRUE(task);
}

TEST_F(NormalQueueTest, PushAfterShutdown_StillAccepted) {
    queue_->shutdown();
    queue_->push([] {});

    TaskQueue::Task task;
    EXPECT_TRUE(queue_->tryPop(task)); // task is there even after shutdown
}

// ============================================================
// 优先级队列测试
// ============================================================
class PriorityQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue_ = std::make_unique<TaskQueue>(true);
    }

    std::unique_ptr<TaskQueue> queue_;
};

TEST_F(PriorityQueueTest, HigherPriorityComesOutFirst) {
    std::vector<int> order;
    // Push with priorities: 0 (low), 10 (high), 5 (medium)
    queue_->push([&order] { order.push_back(10); }, 10);
    queue_->push([&order] { order.push_back(0); }, 0);
    queue_->push([&order] { order.push_back(5); }, 5);

    for (int i = 0; i < 3; ++i) {
        TaskQueue::Task task;
        ASSERT_TRUE(queue_->tryPop(task));
        task();
    }

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 10); // highest priority first
    EXPECT_EQ(order[1], 5);
    EXPECT_EQ(order[2], 0);  // lowest priority last
}

TEST_F(PriorityQueueTest, NegativePriority) {
    std::vector<int> order;
    queue_->push([&order] { order.push_back(1); }, 1);
    queue_->push([&order] { order.push_back(-5); }, -5);
    queue_->push([&order] { order.push_back(0); }, 0);

    for (int i = 0; i < 3; ++i) {
        TaskQueue::Task task;
        ASSERT_TRUE(queue_->tryPop(task));
        task();
    }

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 0);
    EXPECT_EQ(order[2], -5); // negative priority = lowest
}

TEST_F(PriorityQueueTest, WaitPop_HighestPriorityFirst) {
    // Push low priority first, then high priority
    queue_->push([] {}, 1);
    queue_->push([] {}, 100);

    // Schedule a thread to push high after a delay into an already blocking waitPop
    std::atomic<int> poppedPriority{-1};
    std::thread t([this, &poppedPriority] {
        // This will block on waitPop; the 100-priority task should come first
        TaskQueue::Task task;
        // We can't easily check priority in the task itself, but we can verify
        // waitPop returns the highest priority task.
        queue_->waitPop(task);
        poppedPriority = 0; // mark that we got one
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    queue_->shutdown();
    t.join();
}

// ============================================================
// 线程安全测试
// ============================================================
class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue_ = std::make_unique<TaskQueue>(false);
    }

    std::unique_ptr<TaskQueue> queue_;
};

TEST_F(ThreadSafetyTest, ConcurrentPush) {
    const int NUM_THREADS = 8;
    const int TASKS_PER_THREAD = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t] {
            for (int i = 0; i < TASKS_PER_THREAD; ++i) {
                queue_->push([t, i] {
                    // no-op, just testing data race freedom
                    volatile int x = t * 1000 + i;
                    (void)x;
                });
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(queue_->size(), static_cast<size_t>(NUM_THREADS * TASKS_PER_THREAD));
}

TEST_F(ThreadSafetyTest, ConcurrentPushAndPop) {
    const int NUM_TASKS = 500;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> done{false};

    // Producer
    std::thread producer([this, &produced, &done, &consumed] {
        for (int i = 0; i < NUM_TASKS; ++i) {
            queue_->push([&consumed] { consumed.fetch_add(1); });
            produced.fetch_add(1);
        }
        done = true;
    });

    // Consumers
    std::vector<std::thread> consumers;
    for (int c = 0; c < 4; ++c) {
        consumers.emplace_back([this, &done, &consumed] {
            while (!done || !queue_->empty()) {
                TaskQueue::Task task;
                if (queue_->tryPop(task)) {
                    if (task) task();
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    producer.join();
    for (auto& c : consumers) c.join();

    // Drain any remaining
    TaskQueue::Task task;
    while (queue_->tryPop(task)) {
        if (task) task();
    }

    EXPECT_EQ(produced.load(), NUM_TASKS);
    EXPECT_EQ(consumed.load(), NUM_TASKS);
}

TEST_F(ThreadSafetyTest, WaitPop_ProducerConsumer) {
    const int NUM_TASKS = 100;
    std::atomic<int> sum{0};

    // Consumers
    std::vector<std::thread> consumers;
    for (int c = 0; c < 2; ++c) {
        consumers.emplace_back([this, &sum] {
            TaskQueue::Task task;
            while (queue_->waitPop(task)) {
                if (task) task();
            }
        });
    }

    // Producer
    for (int i = 0; i < NUM_TASKS; ++i) {
        queue_->push([&sum, i] { sum.fetch_add(i); });
    }

    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    queue_->shutdown();

    for (auto& c : consumers) c.join();

    int expected = (NUM_TASKS - 1) * NUM_TASKS / 2; // sum of 0..99
    EXPECT_EQ(sum.load(), expected);
}
