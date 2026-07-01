#pragma once

#include <cstddef>
#include <string>

namespace tradingbot::scan {

std::size_t owner_partition(const std::string& instrument_key, std::size_t partition_count);
std::size_t available_worker_count();

}  // namespace tradingbot::scan
