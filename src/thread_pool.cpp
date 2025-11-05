#include "thread_pool.h"

ThreadPool::ThreadPool(size_t thread_count): stop(false) {
    for (size_t i = 0; i < thread_count; ++i) {
        workers.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->mtx);
                    this->cv.wait(lock, [this]() { return stop || !tasks.empty(); });
                    if (stop && tasks.empty()) return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mtx);
        tasks.emplace(task);
    }
    cv.notify_one();
}

ThreadPool::~ThreadPool() {
    stop = true;
    cv.notify_all();
    for (auto &worker : workers)
        worker.join();
}
