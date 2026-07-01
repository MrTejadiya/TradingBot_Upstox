#include "tradingbot/scan/instrument_partitioner.hpp"

#include <cstdint>
#include <thread>

namespace tradingbot::scan {

std::size_t owner_partition(const std::string& instrument_key, std::size_t partition_count) {
    if (partition_count == 0) {
        return 0;
    }

    auto hash = std::uint64_t{14695981039346656037ULL};
    for (const auto ch : instrument_key) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(hash % partition_count);
}

std::size_t available_worker_count() {
    const auto detected = std::thread::hardware_concurrency();
    return detected == 0 ? std::size_t{1} : static_cast<std::size_t>(detected);
}

}  // namespace tradingbot::scan
