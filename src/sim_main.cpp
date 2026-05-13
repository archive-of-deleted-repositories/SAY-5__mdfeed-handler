#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "sim/feed_simulator.h"

int main(int argc, char** argv) {
    mdfeed::sim::SimConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        auto eq = a.find('=');
        if (eq == std::string_view::npos) continue;
        std::string_view k = a.substr(0, eq);
        std::string vstr(a.substr(eq + 1));
        if (k == "--host")
            cfg.dest_host = vstr;
        else if (k == "--port")
            cfg.dest_port = static_cast<uint16_t>(std::atoi(vstr.c_str()));
        else if (k == "--count")
            cfg.total_messages = static_cast<std::size_t>(std::strtoull(vstr.c_str(), nullptr, 10));
        else if (k == "--rate")
            cfg.target_rate_hz = static_cast<uint32_t>(std::strtoul(vstr.c_str(), nullptr, 10));
        else if (k == "--seed")
            cfg.seed = std::strtoull(vstr.c_str(), nullptr, 0);
        else if (k == "--symbols")
            cfg.num_symbols = std::atoi(vstr.c_str());
    }
    int rc = mdfeed::sim::run(cfg);
    std::fprintf(stdout, "simulator done rc=%d sent=%zu rate=%u\n", rc, cfg.total_messages,
                 cfg.target_rate_hz);
    return rc;
}
