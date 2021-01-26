#include "thread_pool.h"

ThreadPool::ThreadPool(uint8_t thread_count) : _thread_count(thread_count), _should_work(true)
{
    for (auto i = 0; i < thread_count; i++) _threads.emplace_back([this]() { _thread_wait(); });
}

ThreadPool::~ThreadPool()
{
    _should_work = false;
    {
        std::lock_guard lock(_work_mutex);
        _work_condition_variable.notify_all();
    }
    for (auto &thread : _threads) thread.join();
}

void ThreadPool::_thread_wait()
{
    while (_should_work)
    {
        {
            std::unique_lock lock(_work_mutex);
            _work_condition_variable.wait(lock);
        }
        auto task = _get_task();
        if (task.has_value())
            task.value()();
//        while (task.has_value())
//        {
//            task.value()();
//            task = _get_task();
//        }
    }
}

void ThreadPool::start()
{
    _threads.clear();
    _threads.shrink_to_fit();
    _should_work = true;
    for (auto i = 0; i < _thread_count; i++) _threads.emplace_back([this]() { _thread_wait(); });
}

void ThreadPool::stop()
{
    _should_work = false;
    {
        std::lock_guard lock(_task_mutex);
        while (!_tasks.empty()) _tasks.pop();
    }
    {
        std::lock_guard lock(_work_mutex);
        _work_condition_variable.notify_all();
    }
    for (auto &thread : _threads) thread.join();
}

void ThreadPool::add_tasks(const std::vector<std::function<void()>> &tasks)
{
    std::unique_lock lock(_task_mutex);
    for (const auto &task : tasks) _tasks.push(task);
    _work_condition_variable.notify_all();
}

void ThreadPool::clear_tasks()
{
    std::unique_lock lock(_task_mutex);
    while (!_tasks.empty()) _tasks.pop();
}

std::optional<std::function<void()>> ThreadPool::_get_task()
{
    std::unique_lock lock(_task_mutex);
    while (!_tasks.empty())
    {
        const auto ret = _tasks.front();
        _tasks.pop();
        return ret;
    }

    return {};
}
