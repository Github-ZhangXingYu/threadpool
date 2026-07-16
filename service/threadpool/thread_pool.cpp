#include "thread_pool.h"

ThreadPool::ThreadPool(bool usePriorityQueue)
    : taskQueue_(usePriorityQueue), running_(false) {}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    running_ = true;
    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        threads_.emplace_back(&ThreadPool::threadLoop, this);
    }
}

void ThreadPool::stop() {
    // 1. Shut down the queue — wakes all blocked workers so they exit
    shutdown();

    // 2. Drain any orphaned tasks (even if pool was never started)
    TaskQueue::Task task = nullptr;
    while (taskQueue_.tryPop(task)) {
        if (task) {
            task();
        }
    }

    // 3. Join all threads (if pool was started)
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
    running_ = false;
}

void ThreadPool::shutdown() {
    taskQueue_.shutdown();
}

void ThreadPool::submit(TaskQueue::Task task, int priority) {
    taskQueue_.push(std::move(task), priority);
}

void ThreadPool::threadLoop() {
    TaskQueue::Task task = nullptr;
    while (taskQueue_.waitPop(task)) {
        if (task) {
            task();
        }
    }
    // waitPop returned false → queue was shut down → exit
}
