#pragma once

#include "platform/game_data.hpp"

#include <string_view>

namespace eon {

// This is deliberately distinct from media admission. It states the furthest
// verified native path for a game/platform pair, never a parity claim and
// never permission to substitute a different release.
enum class PlatformCoverage { recovered_startup, recovered_opening, bootstrap_only };

[[nodiscard]] PlatformCoverage platform_coverage(Game game, Platform platform);
[[nodiscard]] std::string_view name(PlatformCoverage coverage);

} // namespace eon
