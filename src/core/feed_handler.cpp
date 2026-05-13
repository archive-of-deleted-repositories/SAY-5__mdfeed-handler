#include "core/feed_handler.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <string_view>

#include "core/normalize_a.h"
#include "core/normalize_b.h"
#include "obs/clock.h"

namespace mdfeed {

FeedHandler::FeedHandler(FeedHandlerConfig cfg) : cfg_(std::move(cfg)) {}

int FeedHandler::run(std::atomic<bool>* stop_flag) {
    net::UdpSocket sock;
    if (sock.fd() < 0) return -1;
    int rc = sock.bind(cfg_.bind_host, cfg_.bind_port);
    if (rc != 0) return rc;
    sock.set_recv_buffer(cfg_.recv_buf_bytes);

    std::array<uint8_t, 2048> buf{};
    uint64_t seq_counter = 0;

    while (true) {
        if (stop_flag && stop_flag->load(std::memory_order_relaxed)) break;
        if (cfg_.max_messages > 0 && stats_.messages_total >= cfg_.max_messages) break;

        ssize_t n = sock.recv(buf.data(), buf.size());
        const TsNs recv_ns = obs::now_ns();

        if (n < 0) {
            if (n == -EINTR) break;  // signal during recvfrom == shutdown
            stats_.parse_errors += 1;
            continue;
        }
        if (n == 0) continue;

        ++seq_counter;
        std::optional<MdMessage> parsed;

        // Disambiguate venue from the first byte. Venue A is binary and
        // starts with a low msg-type byte (1 or 2, possibly | 0x80). Venue B
        // is ASCII and starts with 'V' (0x56). A single byte is enough.
        if (buf[0] == static_cast<uint8_t>('V')) {
            std::string_view sv(reinterpret_cast<const char*>(buf.data()),
                                static_cast<std::size_t>(n));
            parsed = venue_b::parse(sv, recv_ns, seq_counter);
            if (parsed) stats_.messages_venue_b += 1;
        } else {
            parsed = venue_a::parse(buf.data(), static_cast<std::size_t>(n), recv_ns, seq_counter);
            if (parsed) stats_.messages_venue_a += 1;
        }

        if (!parsed) {
            stats_.parse_errors += 1;
            continue;
        }

        // Per-venue gap detection. Sequence numbers come from the wire in
        // a real venue feed; here the simulator embeds them in the seq
        // field of the unified message via the parser.
        if (parsed->venue_id == kVenueA) {
            if (have_seq_a_ && parsed->seq != last_seq_a_ + 1) {
                stats_.gaps_detected += 1;
            }
            last_seq_a_ = parsed->seq;
            have_seq_a_ = true;
        } else if (parsed->venue_id == kVenueB) {
            if (have_seq_b_ && parsed->seq != last_seq_b_ + 1) {
                stats_.gaps_detected += 1;
            }
            last_seq_b_ = parsed->seq;
            have_seq_b_ = true;
        }

        auto upd = book_.apply(*parsed);
        if (upd.top_changed) stats_.bbo_top_changes += 1;
        if (upd.eroded) stats_.bbo_eroded += 1;
        stats_.messages_total += 1;

        if (cfg_.record_latency) {
            const TsNs post_norm = obs::now_ns();
            uint64_t wire_ns = post_norm - recv_ns;
            wire_to_norm_.record(wire_ns == 0 ? 1 : wire_ns);

            if (parsed->ts_ns_venue > 0 && parsed->ts_ns_recv >= parsed->ts_ns_venue) {
                uint64_t v_lat = parsed->ts_ns_recv - parsed->ts_ns_venue;
                // Note: venue-to-recv uses CLOCK_MONOTONIC_RAW on this host
                // and the simulator's CLOCK_MONOTONIC_RAW; they share the
                // same clock domain over loopback so this is a meaningful
                // measurement here. On a real venue feed it would require
                // PTP sync and is best-effort.
                venue_to_recv_.record(v_lat == 0 ? 1 : v_lat);
            }
        }
    }

    return 0;
}

}  // namespace mdfeed
