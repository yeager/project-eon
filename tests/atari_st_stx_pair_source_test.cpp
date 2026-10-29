#include "data/atari_st_stx.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void write_le16(std::span<std::uint8_t> bytes, const std::size_t offset,
    const std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

void write_le32(std::span<std::uint8_t> bytes, const std::size_t offset,
    const std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 16U);
    bytes[offset + 3U] = static_cast<std::uint8_t>(value >> 24U);
}

void set_fat12(std::span<std::uint8_t> fat, const std::uint16_t cluster,
    const std::uint16_t value) {
    const auto offset = static_cast<std::size_t>(cluster) + cluster / 2U;
    if ((cluster & 1U) != 0) {
        fat[offset] = static_cast<std::uint8_t>((fat[offset] & 0x0fU) | ((value << 4U) & 0xf0U));
        fat[offset + 1U] = static_cast<std::uint8_t>(value >> 4U);
    } else {
        fat[offset] = static_cast<std::uint8_t>(value);
        fat[offset + 1U] = static_cast<std::uint8_t>(
            (fat[offset + 1U] & 0xf0U) | ((value >> 8U) & 0x0fU));
    }
}

std::vector<std::uint8_t> make_stx(const std::string_view filename,
    const std::array<std::uint8_t, 4> payload) {
    constexpr std::size_t sector_size = 512;
    std::array<std::array<std::uint8_t, sector_size>, 20> sectors{};
    auto& boot = sectors[0];
    write_le16(boot, 11, 512);
    boot[13] = 1;
    write_le16(boot, 14, 1);
    boot[16] = 2;
    write_le16(boot, 17, 16);
    write_le16(boot, 19, 20);
    write_le16(boot, 22, 1);
    write_le16(boot, 24, 10);
    write_le16(boot, 26, 1);

    std::array<std::uint8_t, sector_size> fat{};
    fat[0] = 0xf0;
    fat[1] = 0xff;
    fat[2] = 0xff;
    set_fat12(fat, 2, 0x0fff);
    sectors[1] = fat;
    sectors[2] = fat;

    const auto dot = filename.find('.');
    assert(dot != std::string_view::npos && dot <= 8 && filename.size() - dot - 1U <= 3);
    auto& entry = sectors[3];
    for (std::size_t index = 0; index < 8; ++index) entry[index] = ' ';
    for (std::size_t index = 0; index < 3; ++index) entry[8U + index] = ' ';
    for (std::size_t index = 0; index < dot; ++index) {
        entry[index] = static_cast<std::uint8_t>(filename[index]);
    }
    for (std::size_t index = dot + 1U; index < filename.size(); ++index) {
        entry[8U + index - dot - 1U] = static_cast<std::uint8_t>(filename[index]);
    }
    entry[11] = 0x20;
    write_le16(entry, 26, 2);
    write_le32(entry, 28, static_cast<std::uint32_t>(payload.size()));
    std::copy(payload.begin(), payload.end(), sectors[4].begin());

    std::vector<std::uint8_t> image(16, 0);
    image[0] = 'R'; image[1] = 'S'; image[2] = 'Y';
    write_le16(image, 4, 3);
    image[10] = 2;
    for (std::uint8_t track = 0; track < 2; ++track) {
        constexpr std::size_t descriptor_bytes = 16;
        constexpr std::size_t descriptor_count = 10;
        constexpr std::size_t track_header = 16;
        constexpr std::size_t track_bytes = track_header + descriptor_count * descriptor_bytes
            + descriptor_count * sector_size;
        const auto track_offset = image.size();
        image.resize(track_offset + track_bytes, 0);
        auto track_header_span = std::span(image).subspan(track_offset, track_header);
        write_le32(track_header_span, 0, static_cast<std::uint32_t>(track_bytes));
        write_le16(track_header_span, 8, static_cast<std::uint16_t>(descriptor_count));
        track_header_span[14] = track;
        const auto payload_base = track_offset + track_header + descriptor_count * descriptor_bytes;
        for (std::size_t index = 0; index < descriptor_count; ++index) {
            const auto descriptor_offset = track_offset + track_header + index * descriptor_bytes;
            auto descriptor = std::span(image).subspan(descriptor_offset, descriptor_bytes);
            write_le32(descriptor, 0, static_cast<std::uint32_t>(index * sector_size));
            descriptor[8] = track;
            descriptor[9] = 0;
            descriptor[10] = static_cast<std::uint8_t>(index + 1U);
            descriptor[11] = 2;
            const auto sector_index = static_cast<std::size_t>(track) * descriptor_count + index;
            std::copy(sectors[sector_index].begin(), sectors[sector_index].end(),
                image.begin() + static_cast<std::ptrdiff_t>(payload_base + index * sector_size));
        }
    }
    return image;
}

template <typename Callback>
void expect_runtime_error(Callback&& callback) {
    bool rejected = false;
    try {
        std::forward<Callback>(callback)();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main() {
    const std::array<std::uint8_t, 4> disk1_payload{0x11, 0x22, 0x33, 0x44};
    const std::array<std::uint8_t, 4> disk2_payload{0xaa, 0xbb, 0xcc, 0xdd};
    auto disk1 = make_stx("MILL22A.INF", disk1_payload);
    auto disk2 = make_stx("MILL22E.INF", disk2_payload);
    const auto disk1_hash = eon::to_hex(eon::sha256(disk1));
    const auto disk2_hash = eon::to_hex(eon::sha256(disk2));
    const auto original_disk1 = disk1;
    const auto original_disk2 = disk2;

    const eon::AtariStStxPairedFileSource source(disk1, disk2);
    const auto first = source.read_file("mill22a.inf");
    assert(first.disk == eon::AtariStStxSourceDisk::disk1);
    assert(std::equal(first.bytes.begin(), first.bytes.end(), disk1_payload.begin(), disk1_payload.end()));
    const auto second = source.read_file("MILL22E.INF");
    assert(second.disk == eon::AtariStStxSourceDisk::disk2);
    assert(std::equal(second.bytes.begin(), second.bytes.end(), disk2_payload.begin(), disk2_payload.end()));

    expect_runtime_error([&] { static_cast<void>(source.read_file("MISSING.DAT")); });
    expect_runtime_error([&] { static_cast<void>(source.read_file("TOO-LONG-NAME.DAT")); });

    auto duplicate_disk = make_stx("MILL22A.INF", disk2_payload);
    const eon::AtariStStxPairedFileSource duplicate_source(disk1, std::move(duplicate_disk));
    expect_runtime_error([&] { static_cast<void>(duplicate_source.read_file("MILL22A.INF")); });

    assert(disk1 == original_disk1 && disk2 == original_disk2);
    assert(eon::to_hex(eon::sha256(disk1)) == disk1_hash);
    assert(eon::to_hex(eon::sha256(disk2)) == disk2_hash);
}
