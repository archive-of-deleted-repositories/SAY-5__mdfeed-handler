#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#include "core/feed_handler.h"
#include "sim/feed_simulator.h"

using namespace mdfeed;

namespace {

// Pick a port deterministically per test to avoid collisions inside one
// `ctest` run when tests share a runner.
uint16_t kPort = 43101;

}  // namespace

TEST(Loopback, SmallRunReceivesAndBuildsBbo) {
    FeedHandlerConfig hcfg;
    hcfg.bind_host = "127.0.0.1";
    hcfg.bind_port = kPort++;
    hcfg.max_messages = 5000;
    hcfg.record_latency = true;

    FeedHandler handler(hcfg);
    std::atomic<bool> stop{false};

    std::thread h([&]() { handler.run(&stop); });

    // Let the handler bind before the simulator starts sending.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    sim::SimConfig scfg;
    scfg.dest_host = "127.0.0.1";
    scfg.dest_port = hcfg.bind_port;
    scfg.total_messages = 5000;
    scfg.target_rate_hz = 20'000;  // a bit higher than the consumer cap
    scfg.seed = 0xDEADBEEFCAFEull;
    scfg.num_symbols = 4;

    int rc = sim::run(scfg);
    EXPECT_EQ(rc, 0);

    // Wait for consumer to drain (max_messages stops it).
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (handler.stats().messages_total < hcfg.max_messages &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    stop.store(true);
    h.join();

    const auto& s = handler.stats();
    // Loopback UDP should be lossless for this size; we accept >= 99%.
    EXPECT_GE(s.messages_total, 4'950u);
    EXPECT_GT(s.messages_venue_a + s.messages_venue_b, 0u);
    EXPECT_EQ(s.parse_errors, 0u);
    EXPECT_GT(handler.book().size(), 0u);

    // Latency histograms are finite and percentiles monotonic.
    auto p50 = handler.wire_to_normalized().value_at_percentile(50.0);
    auto p99 = handler.wire_to_normalized().value_at_percentile(99.0);
    EXPECT_GT(p50, 0u);
    EXPECT_GE(p99, p50);
}

TEST(Loopback, MalformedDatagramsDoNotCrash) {
    FeedHandlerConfig hcfg;
    hcfg.bind_host = "127.0.0.1";
    hcfg.bind_port = kPort++;
    hcfg.max_messages = 100;
    hcfg.record_latency = false;

    FeedHandler handler(hcfg);
    std::atomic<bool> stop{false};
    std::thread h([&]() { handler.run(&stop); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    net::UdpSocket s;
    ASSERT_GE(s.fd(), 0);
    ASSERT_EQ(s.connect("127.0.0.1", hcfg.bind_port), 0);

    // Mix of junk and one valid venue-B line so the handler can exit on
    // max_messages.
    const char* junk1 = "GARBAGE";
    s.send(reinterpret_cast<const uint8_t*>(junk1), 7);
    const uint8_t binshort[5] = {0xFF, 0x00, 0x00, 0x00, 0x00};
    s.send(binshort, 5);
    for (int i = 0; i < 200; ++i) {
        char line[128];
        int n = std::snprintf(line, sizeof(line), "V|VENUE_B|SYM=AAPL|SIDE=B|PX=1.00|QTY=1|TS=%d\n",
                              i + 1);
        s.send(reinterpret_cast<const uint8_t*>(line), static_cast<std::size_t>(n));
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (handler.stats().messages_total < hcfg.max_messages &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    stop.store(true);
    h.join();

    EXPECT_GE(handler.stats().parse_errors, 2u);
    EXPECT_GE(handler.stats().messages_venue_b, 1u);
}
