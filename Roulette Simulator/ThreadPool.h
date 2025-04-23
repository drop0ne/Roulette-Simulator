#pragma once

#include <thread>           // std::jthread, std::stop_token
#include <future>           // std::future, std::packaged_task
#include <functional>       // std::invoke
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stdexcept>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    // Enqueue any callable with args, returns future<R>
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

private:
    void workerLoop(std::stop_token st);

    std::vector<std::jthread>            workers;
    std::queue<std::function<void()>>    tasks;
    std::mutex                           mutex;
    std::condition_variable              cv;
    bool                                 stopFlag = false;
};

// Implementation

inline ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(
            &ThreadPool::workerLoop, this,
            std::placeholders::_1
        );
    }
}

inline ThreadPool::~ThreadPool() {
    {
        std::scoped_lock lock(mutex);
        stopFlag = true;
    }
    cv.notify_all();
    // jthread destructors will request stop and join
}

inline void ThreadPool::workerLoop(std::stop_token st) {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex);
            cv.wait(lock, [&] {
                return stopFlag || !tasks.empty() || st.stop_requested();
                });
            if ((stopFlag && tasks.empty()) || st.stop_requested())
                return;
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}

// Note: template definition must be visible in header

template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<std::invoke_result_t<F, Args...>>
{
    using ReturnType = std::invoke_result_t<F, Args...>;

    // Wrap the callable + args into a packaged_task<ReturnType()>
    auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
        [func = std::forward<F>(f),
        ... pack = std::forward<Args>(args)]() mutable {
            return std::invoke(std::move(func), std::move(pack)...);
        }
    );

    std::future<ReturnType> fut = taskPtr->get_future();

    {
        std::scoped_lock lock(mutex);
        if (stopFlag) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks.emplace([taskPtr]() { (*taskPtr)(); });
    }
    cv.notify_one();
    return fut;
}
