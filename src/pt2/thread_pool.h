#pragma once

#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <functional>
#include <queue>
#include <atomic>

#include <pt2/structs.h>

class ThreadPool
{
public:
    explicit ThreadPool(uint8_t thread_count);

    ~ThreadPool();

    void add_tasks(const std::vector<std::function<void()>> &tasks);

    void clear_tasks();

    void start();

    void stop();

private:
    void _thread_wait();

    [[nodiscard]] std::optional<std::function<void()>> _get_task();

    uint16_t _thread_count;
    std::mutex              _work_mutex;
    std::atomic<bool>       _should_work;
    std::condition_variable _work_condition_variable;

    std::mutex                        _task_mutex;
    std::queue<std::function<void()>> _tasks;

    std::vector<std::thread> _threads;
};
