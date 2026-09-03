#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

struct MillenniumDosLibEntry {
    std::string name;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

// Millennium's DOS resource libraries use 16-bit offsets plus an explicit
// 64-KiB bank byte. The class borrows the genuine library bytes, so its
// caller must keep the source media alive for every entry view it returns.
// This avoids turning a read-only original library into a second host copy.
class MillenniumDosLib {
public:
    explicit MillenniumDosLib(std::span<const std::uint8_t> bytes);

    [[nodiscard]] std::uint32_t directory_offset() const { return directory_offset_; }
    // Container identity is retained separately from generic directory
    // parsing so a recovery path can require an exact original leaf without
    // making the format reader itself a cross-edition admission policy.
    [[nodiscard]] const std::string& source_sha256() const { return source_sha256_; }
    [[nodiscard]] const std::vector<MillenniumDosLibEntry>& entries() const { return entries_; }
    [[nodiscard]] const MillenniumDosLibEntry* find(std::string_view name) const;
    [[nodiscard]] std::span<const std::uint8_t> read(const MillenniumDosLibEntry& entry) const;

private:
    std::span<const std::uint8_t> bytes_;
    std::string source_sha256_;
    std::uint32_t directory_offset_ = 0;
    std::vector<MillenniumDosLibEntry> entries_;
};

} // namespace eon
