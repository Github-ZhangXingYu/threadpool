#include "task_queue.h"

TaskQueue::TaskQueue(bool usePriority) : usePriority_(usePriority) {}

void TaskQueue::push(Task task, int priority) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (usePriority_) {
            priorityQueue_.push({priority, std::move(task)});
        } else {
            normalQueue_.push(std::move(task));
        }
    }
    cond_.notify_one();
}

bool TaskQueue::tryPop(Task& task) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (usePriority_) {
        if (priorityQueue_.empty()) {
            return false;
        }
        task = std::move(priorityQueue_.top().second);
        priorityQueue_.pop();
    } else {
        if (normalQueue_.empty()) {
            return false;
        }
        task = std::move(normalQueue_.front());
        normalQueue_.pop();
    }
    return true;
}

bool TaskQueue::waitPop(Task& task) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (usePriority_) {
        cond_.wait(lock, [this] { return shutdown_ || !priorityQueue_.empty(); });
        if (shutdown_ && priorityQueue_.empty()) {
            return false;
        }
        task = std::move(priorityQueue_.top().second);
        priorityQueue_.pop();
    } else {
        cond_.wait(lock, [this] { return shutdown_ || !normalQueue_.empty(); });
        if (shutdown_ && normalQueue_.empty()) {
            return false;
        }
        task = std::move(normalQueue_.front());
        normalQueue_.pop();
    }
    return true;
}

void TaskQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    cond_.notify_all();
}

bool TaskQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (usePriority_) {
        return priorityQueue_.empty();
    }
    return normalQueue_.empty();
}

size_t TaskQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (usePriority_) {
        return priorityQueue_.size();
    }
    return normalQueue_.size();
}
