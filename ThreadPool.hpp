#pragma once

#include <condition_variable>
#include <exception>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <tuple>

#include <fmt/format.h>

namespace ThreadPool {
    
namespace details {    

    class TaskBase
    {
    public:
        virtual ~TaskBase() {}
        virtual void run_me() = 0;
    };

    template <class Result>
    class Task : public TaskBase
    {
    public:
        template <class Func, class... Args>
        Task(Func&& function, Args&&... arguments) : 
            m_task(
                [f = std::forward<Func>(function), 
                args = std::tuple(std::forward<Args>(arguments)...)] ()
                {
                    return std::apply(f, args);
                }
            )
        {}
        
        Task(const Task&) = delete;
        Task(Task&&) = default;
        Task& operator=(const Task&) = delete;
        Task& operator=(Task&&) = default;
        
        auto get_future()
        {
            return m_task.get_future();
        }
        
        void run_me() override
        {
            m_task();
        }
    private:
        std::packaged_task<Result()> m_task;
    };

    template <class Func, class... Args>
    auto make_task(Func&& func, Args&&... args)
    {
        using Result = std::invoke_result_t<Func, Args...>;
        return std::make_unique<Task<Result>>(std::forward<Func>(func),
                                              std::forward<Args>(args)...);
    }

} /* namespace details */

class ThreadPoolException : public std::exception
{
public:
    ThreadPoolException(const char* msg) : m_msg(msg)
    {}
    const char* what() const noexcept
    {
        return m_msg;
    }
private:
    const char* m_msg;
};

class ThreadPool
{
public:
    explicit ThreadPool(unsigned num_threads) : m_done(false)
    {
        try {
            m_threads.reserve(num_threads);
            for (unsigned i = 0; i < num_threads; ++i)
                m_threads.push_back(
                    std::thread(&ThreadPool::wait_for_work, this)
                );
        } catch(...) {
            throw ThreadPoolException(constructor_failed);
        }
    }
    
    template <class Func, class... Args>
    auto submit(Func&& func, Args&&... args)
    {
        try {
            auto task = details::make_task(
                std::forward<Func>(func), std::forward<Args>(args)...
            );
            auto task_future = task->get_future();
            add_task(std::move(task));
            return task_future;
        } catch(...) {
            throw ThreadPoolException(adding_task_failed);
        }
    }
    
    ~ThreadPool()
    {
        destroy();
    }
    
private:
    using task_ptr = std::unique_ptr<details::TaskBase>;
    
    std::atomic_bool m_done;
    std::queue<task_ptr, std::list<task_ptr>> m_queue;
    std::vector<std::thread> m_threads;
    std::condition_variable m_condition;
    std::mutex m_mutex;
    
    void wait_for_work()
    {
        while (!m_done) {
            auto task = try_pop();
            if (task) {
                try {
                    task.value()->run_me();
                } catch(std::exception e) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    fmt::print("Caught exception from ThreadPool task:\n"
                               "exc.what() = {}\n", e.what());
                }
            }
        }
    }
    
    std::optional<task_ptr> try_pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [&]() 
            { 
                return m_done || !m_queue.empty(); 
            });
        
        if (m_done)
            return std::nullopt;
        
        auto ret = std::optional(std::move(m_queue.front()));
        m_queue.pop();
        return ret;
    }
    
    template <class Task>
    void add_task(std::unique_ptr<Task>&& task)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(task));
        m_condition.notify_one();
    }
    
    void destroy()
    {
        m_done = true;
        m_condition.notify_all();
        for (auto& thread: m_threads)
            if (thread.joinable())
                thread.join();
    }
    
    constexpr static const char* const constructor_failed = 
    "Failed to allocate or create threads in ThreadPool constructor";
    
    constexpr static const char* const adding_task_failed = 
    "ThreadPool::submit caught exception while enqueueing task";
};
    
} /* namespace ThreadPool */
