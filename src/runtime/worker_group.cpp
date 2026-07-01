#include "tradingbot/runtime/worker_group.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace tradingbot::runtime {

WorkerGroup::WorkerGroup(std::size_t worker_count) {
    if (worker_count == 0) {
        worker_count = 1;
    }
    workers_.reserve(worker_count);
    for (std::size_t index = 0; index < worker_count; ++index) {
        workers_.emplace_back([this] {
            worker_loop();
        });
    }
}

WorkerGroup::~WorkerGroup() {
    stop();
}

bool WorkerGroup::submit(std::function<void()> task) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            return false;
        }
        tasks_.push(std::move(task));
    }
    has_work_.notify_one();
    return true;
}

void WorkerGroup::drain() {
    std::unique_lock lock(mutex_);
    drained_.wait(lock, [this] {
        return tasks_.empty() && active_tasks_ == 0;
    });
}

void WorkerGroup::stop() {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }
    has_work_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::size_t WorkerGroup::pending() const {
    std::lock_guard lock(mutex_);
    return tasks_.size() + active_tasks_;
}

std::vector<std::string> WorkerGroup::errors() const {
    std::lock_guard lock(mutex_);
    return errors_;
}

void WorkerGroup::worker_loop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            has_work_.wait(lock, [this] {
                return stopping_ || !tasks_.empty();
            });
            if (stopping_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
            ++active_tasks_;
        }

        try {
            task();
        } catch (const std::exception& ex) {
            std::lock_guard lock(mutex_);
            errors_.push_back(ex.what());
        } catch (...) {
            std::lock_guard lock(mutex_);
            errors_.push_back("unknown worker error");
        }

        {
            std::lock_guard lock(mutex_);
            --active_tasks_;
            if (tasks_.empty() && active_tasks_ == 0) {
                drained_.notify_all();
            }
        }
    }
}

}  // namespace tradingbot::runtime

