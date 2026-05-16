#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

class ThreadPool{
public:
    using Task=std::function<void()>;
    explicit ThreadPool(std::size_t threadCount)
        : stopRequested_{false}
    {
        if(threadCount==0) threadCount=1;
        workers_.reserve(threadCount);
        for(std::size_t i=0;i<threadCount;i++)
        {
            workers_.emplace_back([this]{
                workerLoop();
            });
        }
    }
    ThreadPool(const ThreadPool&)=delete;
    ThreadPool& operator=(const ThreadPool&)=delete;
    ~ThreadPool(){
        shutdown();
    }

    void submit(Task task){{
            //lock queue
            std::unique_lock<std::mutex> lock(queueMutex_);
            //put task on queue
            tasks_.push(std::move(task));
        }
        condition_.notify_one();
    }

    void shutdown()
    {
        bool expected = false;
        if (!stopRequested_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
        condition_.notify_all();
        for(auto& worker:workers_){
            if(worker.joinable()) worker.join();
        }
    }

private:
    void workerLoop(){
        while(true){
            Task task;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                condition_.wait(lock, [this] {
                    return !tasks_.empty() || stopRequested_.load(std::memory_order_acquire);
                });
                if (stopRequested_.load(std::memory_order_acquire) && tasks_.empty())
                {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::atomic<bool> stopRequested_;
    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
};