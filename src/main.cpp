#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/feed_handler.h"
#include "obs/clock.h"

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int /*signo*/) {
    g_stop.store(true, std::memory_order_relaxed);
}

void print_percentiles(const char* label, const mdfeed::obs::Histogram& h) {
    std::fprintf(
        stdout,
        "%s: n=%llu p50=%lluns p95=%lluns p99=%lluns p99.9=%lluns p99.99=%lluns max=%lluns\n",
        label, static_cast<unsigned long long>(h.count()),
        static_cast<unsigned long long>(h.value_at_percentile(50.0)),
        static_cast<unsigned long long>(h.value_at_percentile(95.0)),
        static_cast<unsigned long long>(h.value_at_percentile(99.0)),
        static_cast<unsigned long long>(h.value_at_percentile(99.9)),
        static_cast<unsigned long long>(h.value_at_percentile(99.99)),
        static_cast<unsigned long long>(h.value_at_percentile(100.0)));
}

}  // namespace

int main(int argc, char** argv) {
    mdfeed::FeedHandlerConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        auto eq = a.find('=');
        if (eq == std::string_view::npos) continue;
        std::string_view k = a.substr(0, eq);
        std::string_view v = a.substr(eq + 1);
        std::string vstr(v);
        if (k == "--host")
            cfg.bind_host = vstr;
        else if (k == "--port")
            cfg.bind_port = static_cast<uint16_t>(std::atoi(vstr.c_str()));
        else if (k == "--max")
            cfg.max_messages = static_cast<std::size_t>(std::strtoull(vstr.c_str(), nullptr, 10));
        else if (k == "--rcvbuf")
            cfg.recv_buf_bytes = std::atoi(vstr.c_str());
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    mdfeed::FeedHandler h(cfg);
    int rc = h.run(&g_stop);

    const auto& s = h.stats();
    std::fprintf(stdout,
                 "messages_total=%llu venue_a=%llu venue_b=%llu parse_errors=%llu gaps=%llu "
                 "top_changes=%llu eroded=%llu symbols=%zu\n",
                 static_cast<unsigned long long>(s.messages_total),
                 static_cast<unsigned long long>(s.messages_venue_a),
                 static_cast<unsigned long long>(s.messages_venue_b),
                 static_cast<unsigned long long>(s.parse_errors),
                 static_cast<unsigned long long>(s.gaps_detected),
                 static_cast<unsigned long long>(s.bbo_top_changes),
                 static_cast<unsigned long long>(s.bbo_eroded), h.book().size());
    print_percentiles("wire_to_normalized_ns", h.wire_to_normalized());
    print_percentiles("venue_to_recv_ns", h.venue_to_recv());
    return rc;
}
