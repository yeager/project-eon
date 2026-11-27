#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace eon {

struct DeuterosAmigaVblankCallbackMemoryWrite {
    std::size_t order = 0;
    std::uint32_t address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const DeuterosAmigaVblankCallbackMemoryWrite&) const = default;
};

struct DeuterosAmigaVblankCallbackResult {
    std::uint32_t counter_before = 0;
    std::uint32_t counter_after = 0;
    bool palette_flag = false;
    std::uint16_t palette_index_before = 0;
    std::uint16_t palette_index_after = 0;
    std::uint8_t palette_row = 0;
    std::uint16_t first_palette_word = 0;
    std::uint16_t second_palette_word = 0;
    bool audio_dispatch_requested = false;
    std::uint32_t audio_dispatch_address = 0;
    std::uint32_t continuation_address = 0x20866;
    std::uint32_t stop_before_address = 0;
    std::vector<DeuterosAmigaVblankCallbackMemoryWrite> memory_writes;
};

// Executes one deterministic callback from explicitly owned 24-bit memory.
// The callback bytes at runtime $207fe must match the pinned main-stage span
// before any state reads are accepted. The returned writes are atomic output;
// callers apply them only after this function returns successfully.
[[nodiscard]] DeuterosAmigaVblankCallbackResult evaluate_deuteros_amiga_vblank_callback(
    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_owned_byte);

} // namespace eon
