#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>

#include "core/feed_handler.h"
#include "obs/clock.h"
#include "sim/feed_simulator.h"

namespace {

// Honest output: a single-line JSON object the README + CI can read.
// The CI smoke run trims duration/count so we don't burn minutes; the
// committed bench/results/bench_local.json is from a longer local run.
struct BenchOpts {
    std::string out_path = "bench/results/bench_run.json";
    std::size_t count = 200'000;
    uint32_t rate = 50'000;
    uint16_t port = 51717;
    int num_symbols = 8;
};

void emit_json(const BenchOpts& opts, const mdfeed::FeedHandler& h, double seconds, uint64_t sent) {
    using mdfeed::obs::Histogram;
    const Histogram& wn = h.wire_to_normalized();
    const Histogram& vr = h.venue_to_recv();
    const auto& s = h.stats();
    double rate_obs = (seconds > 0.0) ? static_cast<double>(s.messages_total) / seconds : 0.0;
    double drop_rate =
        (sent == 0) ? 0.0
                    : 1.0 - (static_cast<double>(s.messages_total) / static_cast<double>(sent));
    std::FILE* f = std::fopen(opts.out_path.c_str(), "w");
    if (f == nullptr) {
        std::perror("fopen");
        return;
    }
    std::fprintf(f,
                 "{\n"
                 "  \"messages_sent\": %llu,\n"
                 "  \"messages_received\": %llu,\n"
                 "  \"venue_a\": %llu,\n"
                 "  \"venue_b\": %llu,\n"
                 "  \"parse_errors\": %llu,\n"
                 "  \"duration_sec\": %.6f,\n"
                 "  \"observed_rate_hz\": %.2f,\n"
                 "  \"drop_rate\": %.6f,\n"
                 "  \"symbols_tracked\": %zu,\n"
                 "  \"wire_to_normalized_ns\": {\n"
                 "    \"count\": %llu,\n"
                 "    \"p50\": %llu,\n"
                 "    \"p95\": %llu,\n"
                 "    \"p99\": %llu,\n"
                 "    \"p99_9\": %llu,\n"
                 "    \"p99_99\": %llu,\n"
                 "    \"max\": %llu\n"
                 "  },\n"
                 "  \"venue_to_recv_ns\": {\n"
                 "    \"count\": %llu,\n"
                 "    \"p50\": %llu,\n"
                 "    \"p95\": %llu,\n"
                 "    \"p99\": %llu,\n"
                 "    \"p99_9\": %llu,\n"
                 "    \"p99_99\": %llu,\n"
                 "    \"max\": %llu\n"
                 "  }\n"
                 "}\n",
                 static_cast<unsigned long long>(sent),
                 static_cast<unsigned long long>(s.messages_total),
                 static_cast<unsigned long long>(s.messages_venue_a),
                 static_cast<unsigned long long>(s.messages_venue_b),
                 static_cast<unsigned long long>(s.parse_errors), seconds, rate_obs, drop_rate,
                 h.book().size(), static_cast<unsigned long long>(wn.count()),
                 static_cast<unsigned long long>(wn.value_at_percentile(50.0)),
                 static_cast<unsigned long long>(wn.value_at_percentile(95.0)),
                 static_cast<unsigned long long>(wn.value_at_percentile(99.0)),
                 static_cast<unsigned long long>(wn.value_at_percentile(99.9)),
                 static_cast<unsigned long long>(wn.value_at_percentile(99.99)),
                 static_cast<unsigned long long>(wn.value_at_percentile(100.0)),
                 static_cast<unsigned long long>(vr.count()),
                 static_cast<unsigned long long>(vr.value_at_percentile(50.0)),
                 static_cast<unsigned long long>(vr.value_at_percentile(95.0)),
                 static_cast<unsigned long long>(vr.value_at_percentile(99.0)),
                 static_cast<unsigned long long>(vr.value_at_percentile(99.9)),
                 static_cast<unsigned long long>(vr.value_at_percentile(99.99)),
                 static_cast<unsigned long long>(vr.value_at_percentile(100.0)));
    std::fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    BenchOpts opts;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        auto eq = a.find('=');
        if (eq == std::string_view::npos) continue;
        std::string_view k = a.substr(0, eq);
        std::string v(a.substr(eq + 1));
        if (k == "--out")
            opts.out_path = v;
        else if (k == "--count")
            opts.count = std::strtoull(v.c_str(), nullptr, 10);
        else if (k == "--rate")
            opts.rate = static_cast<uint32_t>(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--port")
            opts.port = static_cast<uint16_t>(std::atoi(v.c_str()));
        else if (k == "--symbols")
            opts.num_symbols = std::atoi(v.c_str());
    }

    mdfeed::FeedHandlerConfig hcfg;
    hcfg.bind_host = "127.0.0.1";
    hcfg.bind_port = opts.port;
    hcfg.max_messages = opts.count;
    hcfg.record_latency = true;
    hcfg.recv_buf_bytes = 16 * 1024 * 1024;

    mdfeed::FeedHandler handler(hcfg);
    std::atomic<bool> stop{false};

    std::thread h([&]() { handler.run(&stop); });
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    mdfeed::sim::SimConfig scfg;
    scfg.dest_host = hcfg.bind_host;
    scfg.dest_port = hcfg.bind_port;
    scfg.total_messages = opts.count;
    scfg.target_rate_hz = opts.rate;
    scfg.seed = 0xC0FFEE1234567890ull;
    scfg.num_symbols = opts.num_symbols;

    const auto t0 = std::chrono::steady_clock::now();
    int rc = mdfeed::sim::run(scfg);
    if (rc != 0) {
        std::fprintf(stderr, "simulator failed rc=%d\n", rc);
        stop.store(true);
        h.join();
        return rc;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (handler.stats().messages_total < hcfg.max_messages &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    stop.store(true);
    h.join();
    const auto t1 = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(t1 - t0).count();

    emit_json(opts, handler, seconds, opts.count);
    std::fprintf(stdout, "bench done count=%zu seconds=%.3f out=%s\n", opts.count, seconds,
                 opts.out_path.c_str());
    return 0;
}
