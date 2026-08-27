#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace eon {

using Sha256Digest = std::array<std::uint8_t, 32>;

[[nodiscard]] Sha256Digest sha256(std::span<const std::uint8_t> data);
[[nodiscard]] Sha256Digest sha256_file(const std::filesystem::path& path);
[[nodiscard]] std::string to_hex(const Sha256Digest& digest);

} // namespace eon

