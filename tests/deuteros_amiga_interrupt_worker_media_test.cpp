#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_interrupt_worker.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/sha256.hpp"
#include "data/zip_archive.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view system_disk_sha256 =
    "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
constexpr std::string_view archive_sha256 =
    "7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533";
constexpr std::uintmax_t archive_size = 449'666;
constexpr std::string_view worker_sha256 =
    "661854d6976ab520b0398e2545003d3fe59692fc0de54f0f810f379cf25ccaf8";
constexpr std::uint32_t memory_base = 0x22000;

std::vector<std::uint8_t> read_bounded_archive(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    if (size != archive_size
        || size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Original archive no longer matches its pinned size");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open supplied original archive");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Supplied original archive changed while being read");
    }
    return bytes;
}

struct AdmittedSystemDisk {
    std::string archive_sha256;
    std::vector<std::uint8_t> bytes;
};

AdmittedSystemDisk find_system_disk(const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error("Original-media directory does not exist");
    }
    std::vector<std::filesystem::path> archives;
    for (const auto& item : std::filesystem::directory_iterator(directory)) {
        const auto extension = item.path().extension();
        if (std::filesystem::is_symlink(item.symlink_status()) || !item.is_regular_file()
            || (extension != ".zip" && extension != ".ZIP" && extension != ".Zip")) continue;
        if (item.file_size() != archive_size) continue;
        archives.push_back(item.path());
        if (archives.size() > 512) {
            throw std::runtime_error("Original-media ZIP candidate count exceeds the test bound");
        }
    }
    std::sort(archives.begin(), archives.end());
    for (const auto& path : archives) {
        auto archive_bytes = read_bounded_archive(path);
        const auto observed_archive_sha256 = eon::to_hex(eon::sha256(archive_bytes));
        if (observed_archive_sha256 != archive_sha256) continue;
        const eon::ZipArchive archive(std::move(archive_bytes));
        auto disk = archive.extract_asset_by_sha256(system_disk_sha256);
        if (!disk) throw std::runtime_error("Pinned outer archive lacks the expected system ADF");
        return {observed_archive_sha256, std::move(*disk)};
    }
    throw std::runtime_error("Supplied media lacks the hash-admitted Deuteros archive and ADF");
}

void set_word(std::vector<std::uint8_t>& memory, const std::uint32_t address,
    const std::uint16_t value) {
    const auto offset = static_cast<std::size_t>(address - memory_base);
    memory[offset] = static_cast<std::uint8_t>(value >> 8U);
    memory[offset + 1] = static_cast<std::uint8_t>(value);
}

void set_long(std::vector<std::uint8_t>& memory, const std::uint32_t address,
    const std::uint32_t value) {
    set_word(memory, address, static_cast<std::uint16_t>(value >> 16U));
    set_word(memory, address + 2, static_cast<std::uint16_t>(value));
}

} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto admitted = find_system_disk(argv[1]);
    const eon::AmigaAdf disk{std::span<const std::uint8_t>(admitted.bytes)};
    assert(disk.kind() == eon::AmigaDiskKind::dos);

    const auto plan = eon::parse_deuteros_amiga_load_plan(disk);
    const auto prefix = eon::parse_deuteros_amiga_installed_interrupt_prefix(disk, plan);
    const auto worker = eon::parse_deuteros_amiga_installed_interrupt_worker(
        disk, plan, prefix);
    assert(worker.raw_sha256 == worker_sha256);

    // This selected-channel state is an explicit evaluator input, not a
    // captured or recovered interrupt state from the original media.
    std::vector<std::uint8_t> scenario(0x2000, 0);
    scenario[0x229e8 - memory_base] = 0x01;
    set_long(scenario, 0x229f2, 0x23000);
    set_long(scenario, 0x23008, 0x12345678);
    set_word(scenario, 0x2300c, 0x9abc);
    const auto result = eon::evaluate_deuteros_amiga_installed_interrupt_worker(
        worker, memory_base, scenario);

    assert(result.register_writes.size() == 11);
    assert(result.register_writes[4].address == 0xdff0a0);
    assert(result.register_writes[4].value == 0x12345678);
    assert(result.register_writes[5].address == 0xdff0a4);
    assert(result.register_writes[5].value == 0x9abc);
    assert(result.register_writes[6].address == 0xdff096);
    assert(result.register_writes[6].value == 0x8000);
    assert(result.memory[0x229e8 - memory_base] == 0);
    assert(result.memory[0x229f2 - memory_base] == 0);
    assert(result.memory[0x229f3 - memory_base] == 0);
    assert(result.memory[0x229f4 - memory_base] == 0);
    assert(result.memory[0x229f5 - memory_base] == 0);

    std::cout << "Deuteros interrupt worker media regression passed; archive SHA-256 "
              << admitted.archive_sha256 << ", ADF SHA-256 " << system_disk_sha256 << '\n';
}
