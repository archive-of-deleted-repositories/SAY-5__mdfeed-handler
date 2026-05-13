#include "core/normalize_b.h"

#include <charconv>
#include <cstdio>
#include <cstring>

namespace mdfeed::venue_b {

namespace {

constexpr std::string_view kPrefix = "V|VENUE_B|";

std::string_view next_field(std::string_view& rest) {
    auto pos = rest.find('|');
    if (pos == std::string_view::npos) {
        auto out = rest;
        rest = {};
        return out;
    }
    auto out = rest.substr(0, pos);
    rest.remove_prefix(pos + 1);
    return out;
}

bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool parse_u64(std::string_view s, uint64_t& out) {
    if (s.empty()) return false;
    auto* first = s.data();
    auto* last = s.data() + s.size();
    auto r = std::from_chars(first, last, out);
    return r.ec == std::errc{} && r.ptr == last;
}

bool parse_u32(std::string_view s, uint32_t& out) {
    uint64_t v;
    if (!parse_u64(s, v)) return false;
    if (v > UINT32_MAX) return false;
    out = static_cast<uint32_t>(v);
    return true;
}

// Decimal -> integer ticks at kPriceScale. Rejects non-finite,
// negative, or out-of-range inputs. Tolerates missing fractional part.
bool parse_price(std::string_view s, int64_t& out) {
    if (s.empty()) return false;
    bool negative = false;
    if (s.front() == '-') {
        negative = true;
        s.remove_prefix(1);
    }
    auto dot = s.find('.');
    std::string_view int_part = (dot == std::string_view::npos) ? s : s.substr(0, dot);
    std::string_view frac_part =
        (dot == std::string_view::npos) ? std::string_view{} : s.substr(dot + 1);
    if (int_part.empty() && frac_part.empty()) return false;

    uint64_t i_val = 0;
    if (!int_part.empty()) {
        if (!parse_u64(int_part, i_val)) return false;
    }
    // Pad/truncate fractional part to 6 digits.
    char frac_buf[7] = {'0', '0', '0', '0', '0', '0', 0};
    const std::size_t copy_len = frac_part.size() < 6 ? frac_part.size() : 6;
    for (std::size_t i = 0; i < copy_len; ++i) {
        if (frac_part[i] < '0' || frac_part[i] > '9') return false;
        frac_buf[i] = frac_part[i];
    }
    uint64_t f_val = 0;
    if (!parse_u64(std::string_view(frac_buf, 6), f_val)) return false;

    if (i_val > static_cast<uint64_t>(INT64_MAX / kPriceScale)) return false;
    int64_t ticks = static_cast<int64_t>(i_val) * kPriceScale + static_cast<int64_t>(f_val);
    out = negative ? -ticks : ticks;
    return true;
}

}  // namespace

SymbolId hash_symbol(std::string_view s) {
    uint32_t h = 2166136261u;  // FNV offset basis
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;  // FNV prime
    }
    return h;
}

std::optional<MdMessage> parse(std::string_view line, TsNs recv_ns, uint64_t seq) {
    // Trim a single trailing '\n' if present.
    if (!line.empty() && line.back() == '\n') {
        line.remove_suffix(1);
    }
    if (!starts_with(line, kPrefix)) return std::nullopt;
    auto rest = line.substr(kPrefix.size());

    std::string_view sym, side_s, px_s, qty_s, ts_s;
    sym = next_field(rest);
    side_s = next_field(rest);
    px_s = next_field(rest);
    qty_s = next_field(rest);
    ts_s = next_field(rest);
    if (sym.empty() || side_s.empty() || px_s.empty() || qty_s.empty() || ts_s.empty()) {
        return std::nullopt;
    }
    if (!starts_with(sym, "SYM=") || !starts_with(side_s, "SIDE=") || !starts_with(px_s, "PX=") ||
        !starts_with(qty_s, "QTY=") || !starts_with(ts_s, "TS=")) {
        return std::nullopt;
    }
    auto sym_val = sym.substr(4);
    auto side_val = side_s.substr(5);
    auto px_val = px_s.substr(3);
    auto qty_val = qty_s.substr(4);
    auto ts_val = ts_s.substr(3);
    if (sym_val.empty()) return std::nullopt;
    if (side_val.size() != 1 || (side_val[0] != 'B' && side_val[0] != 'A')) {
        return std::nullopt;
    }

    MdMessage m{};
    m.type = MsgType::Quote;
    m.venue_id = kVenueB;
    m.symbol_id = hash_symbol(sym_val);
    m.side = (side_val[0] == 'B') ? Side::Bid : Side::Ask;
    if (!parse_price(px_val, m.price_ticks)) return std::nullopt;
    if (!parse_u32(qty_val, m.qty)) return std::nullopt;
    uint64_t ts = 0;
    if (!parse_u64(ts_val, ts)) return std::nullopt;
    m.ts_ns_venue = ts;
    m.ts_ns_recv = recv_ns;
    m.seq = seq;
    return m;
}

std::size_t serialize(const MdMessage& msg, std::string_view sym_ticker, uint8_t* buf,
                      std::size_t cap) {
    if (buf == nullptr || cap == 0 || sym_ticker.empty()) return 0;

    int64_t ticks = msg.price_ticks;
    bool negative = ticks < 0;
    uint64_t abs_ticks = negative ? static_cast<uint64_t>(-ticks) : static_cast<uint64_t>(ticks);
    uint64_t i_part = abs_ticks / static_cast<uint64_t>(kPriceScale);
    uint64_t f_part = abs_ticks % static_cast<uint64_t>(kPriceScale);

    char tmp[256];
    int n = std::snprintf(
        tmp, sizeof(tmp), "V|VENUE_B|SYM=%.*s|SIDE=%c|PX=%s%llu.%06llu|QTY=%u|TS=%llu\n",
        static_cast<int>(sym_ticker.size()), sym_ticker.data(), msg.side == Side::Bid ? 'B' : 'A',
        negative ? "-" : "", static_cast<unsigned long long>(i_part),
        static_cast<unsigned long long>(f_part), msg.qty,
        static_cast<unsigned long long>(msg.ts_ns_venue));
    if (n <= 0 || static_cast<std::size_t>(n) >= cap) return 0;
    std::memcpy(buf, tmp, static_cast<std::size_t>(n));
    return static_cast<std::size_t>(n);
}

}  // namespace mdfeed::venue_b
