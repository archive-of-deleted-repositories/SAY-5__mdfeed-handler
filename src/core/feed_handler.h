#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/bbo_book.h"
#include "core/types.h"
#include "net/udp_socket.h"
#include "obs/histogram.h"

namespace mdfeed {

struct FeedHandlerConfig {
    std::string bind_host = "127.0.0.1";
    uint16_t bind_port = 42424;
    int recv_buf_bytes = 4 * 1024 * 1024;
    std::size_t max_messages = 0;  // 0 = run until stop_flag
    bool record_latency = true;
    std::size_t print_every = 0;  // 0 = never print mid-run
};

struct FeedHandlerStats {
    uint64_t messages_total = 0;
    uint64_t messages_venue_a = 0;
    uint64_t messages_venue_b = 0;
    uint64_t parse_errors = 0;
    uint64_t gaps_detected = 0;
    uint64_t bbo_top_changes = 0;
    uint64_t bbo_eroded = 0;
};

class FeedHandler {
public:
    explicit FeedHandler(FeedHandlerConfig cfg);

    // Run the receive loop. Returns 0 on graceful shutdown. The caller
    // signals shutdown by flipping stop_flag to true (e.g. from a signal
    // handler) or by setting cfg.max_messages > 0.
    int run(std::atomic<bool>* stop_flag);

    const FeedHandlerStats& stats() const { return stats_; }
    const obs::Histogram& wire_to_normalized() const { return wire_to_norm_; }
    const obs::Histogram& venue_to_recv() const { return venue_to_recv_; }
    const BboBook& book() const { return book_; }

private:
    FeedHandlerConfig cfg_;
    BboBook book_;
    obs::Histogram wire_to_norm_;
    obs::Histogram venue_to_recv_;
    FeedHandlerStats stats_{};

    // Per-venue last-seen sequence for gap detection. We don't ship a
    // gap-fill protocol (out of scope); we log gap counts.
    uint64_t last_seq_a_ = 0;
    bool have_seq_a_ = false;
    uint64_t last_seq_b_ = 0;
    bool have_seq_b_ = false;
};

}  // namespace mdfeed
