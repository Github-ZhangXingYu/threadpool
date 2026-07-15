#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "task_queue.h"
#include <thread>
#include <vector>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(bool usePriorityQueue = false);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void start();
    void stop();             // drain remaining tasks, then join
    void shutdown();          // wake all threads (called by dtor / stop)
    void submit(TaskQueue::Task task, int priority = 0);

    size_t getThreadCount() const { return threads_.size(); }
    size_t getQueueSize() const { return taskQueue_.size(); }

private:
    void threadLoop();

    std::vector<std::thread> threads_;
    TaskQueue taskQueue_;
    std::atomic<bool> running_;
    static constexpr size_t THREAD_COUNT = 4;
};

#endif
