#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/types.h"

namespace mdfeed::sim {

struct SimConfig {
    std::string dest_host = "127.0.0.1";
    uint16_t dest_port = 42424;
    std::size_t total_messages = 100'000;
    uint32_t target_rate_hz = 10'000;  // messages per second
    uint64_t seed = 0x9E3779B97F4A7C15ull;
    int num_symbols = 8;
    // Pause this long between batches to honour rate.
    // If 0, send as fast as possible.
};

// Run the simulator. Returns 0 on success.
int run(const SimConfig& cfg);

}  // namespace mdfeed::sim
