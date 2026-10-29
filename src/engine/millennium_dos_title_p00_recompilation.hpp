#pragma once

#include "engine/native_runtime_memory.hpp"

#include <cstdint>
#include <span>

namespace eon {

class MillenniumDosLib;

// A bounded static recompilation of the English TITLES.EXE call made at
// $1b17: AX=0, CALL $1625, which in turn calls the codec at $1290.  It models
// only the resulting 320x200 indexed source buffer.  Allocation, video-driver
// dispatch, palette programming, cadence, and input stay outside this unit.
//
// `destination_segment` is an Eon-owned DOS-compatible allocation selected by
// the caller.  It is deliberately not presented as an observed DOS segment.
[[nodiscard]] NativeRuntimeEffectBatch make_millennium_dos_title_p00_recompilation_batch(
    std::span<const std::uint8_t> titles_executable,
    const MillenniumDosLib& title_library, std::uint16_t destination_segment);

} // namespace eon
