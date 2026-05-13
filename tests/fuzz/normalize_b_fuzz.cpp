#include <cstddef>
#include <cstdint>
#include <string_view>

#include "core/normalize_b.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
    std::string_view sv(reinterpret_cast<const char*>(data), size);
    auto m = mdfeed::venue_b::parse(sv, 0, 0);
    (void)m;
    return 0;
}
