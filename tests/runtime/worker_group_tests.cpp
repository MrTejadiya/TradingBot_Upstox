#include "tradingbot/runtime/worker_group.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void executes_submitted_tasks_and_drains() {
    tradingbot::runtime::WorkerGroup workers(2);
    std::atomic<int> count{0};

    require(workers.submit([&count] { ++count; }), "first task should submit");
    require(workers.submit([&count] { ++count; }), "second task should submit");
    workers.drain();

    require(count == 2, "both tasks should execute");
    require(workers.pending() == 0, "drain should leave no pending work");
}

void captures_task_exceptions_without_stopping_group() {
    tradingbot::runtime::WorkerGroup workers(1);
    std::atomic<int> count{0};

    workers.submit([] { throw std::runtime_error("task failed"); });
    workers.submit([&count] { ++count; });
    workers.drain();

    const auto errors = workers.errors();
    require(errors.size() == 1, "one error should be captured");
    require(errors.front() == "task failed", "exception message should be retained");
    require(count == 1, "worker should continue after exception");
}

void stop_rejects_new_tasks() {
    tradingbot::runtime::WorkerGroup workers(1);
    workers.stop();

    require(!workers.submit([] {}), "stopped worker group should reject new tasks");
}

void zero_workers_defaults_to_one_worker() {
    tradingbot::runtime::WorkerGroup workers(0);
    std::atomic<int> count{0};

    workers.submit([&count] { ++count; });
    workers.drain();

    require(count == 1, "zero worker constructor should still execute tasks");
}

}  // namespace

int main() {
    executes_submitted_tasks_and_drains();
    captures_task_exceptions_without_stopping_group();
    stop_rejects_new_tasks();
    zero_workers_defaults_to_one_worker();
    return 0;
}

