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

void set_stage_word(std::vector<std::uint8_t>& memory, const std::uint32_t address,
    const std::uint16_t value) {
    const auto offset = static_cast<std::size_t>(address - 0x20000U);
    memory[offset] = static_cast<std::uint8_t>(value >> 8U);
    memory[offset + 1U] = static_cast<std::uint8_t>(value);
}

void set_stage_long(std::vector<std::uint8_t>& memory, const std::uint32_t address,
    const std::uint32_t value) {
    set_stage_word(memory, address, static_cast<std::uint16_t>(value >> 16U));
    set_stage_word(memory, address + 2U, static_cast<std::uint16_t>(value));
}

} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto admitted = find_system_disk(argv[1]);
    const eon::AmigaAdf disk{std::span<const std::uint8_t>(admitted.bytes)};
    assert(disk.kind() == eon::AmigaDiskKind::dos);

    const auto plan = eon::parse_deuteros_amiga_load_plan(disk);
    const auto vertb = eon::parse_deuteros_amiga_vertb_handler(disk, plan);
    assert(vertb.entry_address == 0x207ec && vertb.source_disk_offset == 0x5fec
        && vertb.source_length == 0x100);
    eon::DeuterosAmigaVertbSession vertb_session(vertb);
    vertb_session.observe_registration({1, 0x208b4, 0x208b8, -0xa8, 5, 0x2086a, 0x207ec});
    const auto vertb_tick = vertb_session.observe_idle_invocation(
        {2, 0x207ec, 0, 0, 0x00001234});
    assert(vertb_tick.address == 0x2079e && vertb_tick.before == 0x1234
        && vertb_tick.after == 0x1238);

    // Synthetic RAM inputs are layered over the genuine loaded main-stage
    // bytes. This exercises one explicitly observed handler event, including
    // the double-buffer word copy and the existing hash-bound $22bea model.
    const auto original_main_stage = disk.bytes(0x5800, 0x4200);
    std::vector<std::uint8_t> vertb_memory(original_main_stage.begin(), original_main_stage.end());
    set_stage_long(vertb_memory, 0x2079e, 0xfffffffcU);
    set_stage_word(vertb_memory, 0x207b0, 0);
    vertb_memory[0x207ea - 0x20000] = 1;
    vertb_memory[0x207eb - 0x20000] = 1;
    const std::array<std::uint16_t, 4> copied_words{0x1234, 0x5678, 0x9abc, 0xdef0};
    for (std::size_t i = 0; i < copied_words.size(); ++i)
        set_stage_word(vertb_memory, 0x207bc + static_cast<std::uint32_t>(i * 2U), copied_words[i]);
    set_stage_long(vertb_memory, 0x2197a, 0x23000);
    set_stage_long(vertb_memory, 0x2197e, 0x23100);
    set_stage_word(vertb_memory, 0x22a6c, 0);
    for (std::size_t i = 0; i < 56; ++i) vertb_memory[0x22a6e - 0x20000 + i] = 0;
    set_stage_word(vertb_memory, 0x22a32, 0);
    const auto owned_stage_byte = [&vertb_memory](const std::uint32_t address)
        -> std::optional<std::uint8_t> {
        if (address < 0x20000U || address - 0x20000U >= vertb_memory.size())
            return std::nullopt;
        return vertb_memory[address - 0x20000U];
    };
    eon::DeuterosAmigaVertbSession observed_vertb_session(vertb);
    observed_vertb_session.observe_registration(
        {10, 0x208b4, 0x208b8, -0xa8, 5, 0x2086a, 0x207ec});
    const auto translated_vertb = observed_vertb_session.observe_invocation(
        11, 0x207ec, owned_stage_byte);
    assert(translated_vertb.trace_sequence == 11);
    assert(translated_vertb.memory_writes.size() == 13);
    assert(translated_vertb.register_writes.size() == 14);
    const auto find_owned = [&translated_vertb](const std::uint32_t address) {
        return std::find_if(translated_vertb.memory_writes.begin(),
            translated_vertb.memory_writes.end(), [address](const auto& write) {
                return write.address == address;
            });
    };
    assert(find_owned(0x2079e) != translated_vertb.memory_writes.end());
    assert(find_owned(0x2079e)->value == 0);
    assert(find_owned(0x207b0)->value == 0);
    assert(find_owned(0x2302a)->value == 0x12);
    assert(find_owned(0x2302b)->value == 0x34);
    assert(find_owned(0x2302e)->value == 0x56);
    assert(find_owned(0x2302f)->value == 0x78);
    assert(find_owned(0x2312a)->value == 0x9a);
    assert(find_owned(0x2312b)->value == 0xbc);
    assert(find_owned(0x2312e)->value == 0xde);
    assert(find_owned(0x2312f)->value == 0xf0);
    assert(translated_vertb.register_writes[0].address == 0xdff096
        && translated_vertb.register_writes[0].value == 0);
    assert(translated_vertb.register_writes[1].address == 0xdff096
        && translated_vertb.register_writes[1].value == 0x800f);
    assert(translated_vertb.register_writes[2].address == 0xdff0a0);
    bool missing_sound_dependency_rejected = false;
    try {
        static_cast<void>(eon::evaluate_deuteros_amiga_vertb_entry(vertb, 12, 0x207ec,
            [&owned_stage_byte](const std::uint32_t address) -> std::optional<std::uint8_t> {
                if (address == 0x22a6e) return std::nullopt;
                return owned_stage_byte(address);
            }));
    } catch (const std::exception&) {
        missing_sound_dependency_rejected = true;
    }
    assert(missing_sound_dependency_rejected);

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
