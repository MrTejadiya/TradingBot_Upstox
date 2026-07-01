#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace tradingbot::runtime {

class WorkerGroup {
public:
    explicit WorkerGroup(std::size_t worker_count);
    ~WorkerGroup();

    WorkerGroup(const WorkerGroup&) = delete;
    WorkerGroup& operator=(const WorkerGroup&) = delete;

    bool submit(std::function<void()> task);
    void drain();
    void stop();

    std::size_t pending() const;
    std::vector<std::string> errors() const;

private:
    void worker_loop();

    mutable std::mutex mutex_;
    std::condition_variable has_work_;
    std::condition_variable drained_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    std::vector<std::string> errors_;
    bool stopping_{false};
    std::size_t active_tasks_{0};
};

}  // namespace tradingbot::runtime

