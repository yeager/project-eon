#pragma once

#include "data/amiga_adf.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

// Read-only AmigaDOS OFS/FFS view.  It deliberately does not repair corrupt
// directory chains or guess block ownership: a malformed on-disk reference is
// an error rather than an opportunity to manufacture data.
class AmigaOfs {
public:
    struct Entry {
        std::string path;
        std::uint32_t header_block = 0;
        std::uint32_t byte_size = 0;
        bool directory = false;
    };

    explicit AmigaOfs(const AmigaAdf& disk);

    [[nodiscard]] const std::string& volume_name() const { return volume_name_; }
    [[nodiscard]] std::uint32_t root_block() const { return root_block_; }
    [[nodiscard]] const std::vector<Entry>& entries() const { return entries_; }
    [[nodiscard]] const Entry* find(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> read_file(std::string_view path) const;

private:
    [[nodiscard]] std::span<const std::uint8_t, AmigaAdf::sector_size> block(
        std::uint32_t number) const;
    void scan_directory(std::uint32_t directory_block, const std::string& prefix,
        std::vector<std::uint32_t>& ancestry);

    const AmigaAdf& disk_;
    std::uint32_t root_block_ = 0;
    std::string volume_name_;
    std::vector<Entry> entries_;
};

} // namespace eon
