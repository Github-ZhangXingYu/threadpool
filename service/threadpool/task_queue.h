#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

class TaskQueue {
public:
    using Task = std::function<void()>;

    TaskQueue(bool usePriority = false);
    ~TaskQueue() = default;

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    void push(Task task, int priority = 0);
    bool tryPop(Task& task);
    bool waitPop(Task& task);   // returns false if queue was shut down
    void shutdown();             // wake all waiters, stop waitPop
    bool empty() const;
    size_t size() const;

private:
    bool usePriority_;

    std::queue<Task> normalQueue_;

    struct PriorityCmp {
        bool operator()(const std::pair<int, Task>& a,
                        const std::pair<int, Task>& b) const {
            return a.first < b.first;  // higher int = higher priority
        }
    };
    std::priority_queue<std::pair<int, Task>,
                        std::vector<std::pair<int, Task>>,
                        PriorityCmp> priorityQueue_;

    mutable std::mutex mutex_;
    std::condition_variable cond_;
    bool shutdown_ = false;
};

#endif
