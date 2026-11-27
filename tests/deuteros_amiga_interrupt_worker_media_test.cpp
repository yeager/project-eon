#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_interrupt_worker.hpp"
#include "data/deuteros_amiga_vblank_callback.hpp"
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
#include <map>
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

void set_word(std::map<std::uint32_t,std::uint8_t>& memory,
    const std::uint32_t address,const std::uint16_t value){
    memory[address]=static_cast<std::uint8_t>(value>>8U);
    memory[address+1]=static_cast<std::uint8_t>(value);
}

void set_long(std::map<std::uint32_t,std::uint8_t>& memory,
    const std::uint32_t address,const std::uint32_t value){
    set_word(memory,address,static_cast<std::uint16_t>(value>>16U));
    set_word(memory,address+2,static_cast<std::uint16_t>(value));
}

std::uint16_t get_word(const std::map<std::uint32_t,std::uint8_t>& memory,
    const std::uint32_t address){
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(memory.at(address))<<8U)
        |memory.at(address+1));
}

std::uint32_t get_long(const std::map<std::uint32_t,std::uint8_t>& memory,
    const std::uint32_t address){
    return (static_cast<std::uint32_t>(get_word(memory,address))<<16U)
        |get_word(memory,address+2);
}

using OwnedMemory=std::map<std::uint32_t,std::uint8_t>;

OwnedMemory make_vblank_memory(const eon::AmigaAdf& disk,
    const eon::DeuterosAmigaLoadPlan& plan){
    OwnedMemory memory;
    const auto stage=disk.bytes(plan.main_stage.disk_offset,plan.main_stage.length);
    for(std::size_t index=0;index<stage.size();++index)
        memory.emplace(plan.main_stage.destination+static_cast<std::uint32_t>(index),stage[index]);
    set_long(memory,0x2197a,0x30000);
    set_long(memory,0x2197e,0x30100);
    for(const auto base:{0x30000U,0x30100U}){
        for(std::uint32_t offset=0;offset<0x30;offset+=2)
            set_word(memory,base+offset,0xa000U+static_cast<std::uint16_t>(offset));
    }
    return memory;
}

eon::DeuterosAmigaVblankCallbackResult run_vblank(const OwnedMemory& memory){
    return eon::evaluate_deuteros_amiga_vblank_callback(
        [&memory](const std::uint32_t address)->std::optional<std::uint8_t>{
            const auto found=memory.find(address);
            if(found==memory.end())return std::nullopt;
            return found->second;
        });
}

void apply_vblank_writes(OwnedMemory& memory,
    const std::vector<eon::DeuterosAmigaVblankCallbackMemoryWrite>& writes){
    for(std::size_t index=0;index<writes.size();++index){
        assert(writes[index].order==index+1);
        memory[writes[index].address]=writes[index].value;
    }
}

void verify_vblank_callback(const eon::AmigaAdf& disk,
    const eon::DeuterosAmigaLoadPlan& plan){
    constexpr std::uint32_t first_root=0x30000;
    constexpr std::uint32_t second_root=0x30100;
    // The callback span is sourced from the original load plan, not a fixture
    // byte array. State and display pointers below are explicit test inputs.
    auto wrap=make_vblank_memory(disk,plan);
    set_long(wrap,0x2079e,0xfffffffc);
    set_word(wrap,0x207b0,0);
    wrap[0x207ea]=0;
    wrap[0x207eb]=0;
    const auto wrap_before=wrap;
    const auto wrapped=run_vblank(wrap);
    assert(wrapped.counter_before==0xfffffffc&&wrapped.counter_after==0);
    assert(!wrapped.palette_flag&&!wrapped.audio_dispatch_requested);
    assert(wrap==wrap_before);
    auto wrapped_after=wrap;
    apply_vblank_writes(wrapped_after,wrapped.memory_writes);
    assert(get_long(wrapped_after,0x2079e)==0);

    for(std::uint16_t start_index=0;start_index<4;++start_index){
        auto palette=make_vblank_memory(disk,plan);
        set_long(palette,0x2079e,0x12345670);
        set_word(palette,0x207b0,start_index);
        palette[0x207ea]=1;
        palette[0x207eb]=0;
        const auto row=static_cast<std::uint16_t>((start_index+1U)&3U);
        const auto source=0x207b2U+static_cast<std::uint32_t>(row)*10U;
        const auto first=static_cast<std::uint16_t>(0x1100U+row);
        const auto second=static_cast<std::uint16_t>(0x2200U+row);
        set_word(palette,source,first);set_word(palette,source+2,second);
        const auto before=palette;
        const auto result=run_vblank(palette);
        assert(result.palette_flag&&result.palette_row==row
            &&result.palette_index_after==static_cast<std::uint16_t>(start_index+1U)
            &&result.first_palette_word==first
            &&result.second_palette_word==second);
        assert(palette==before);
        auto after=palette;
        apply_vblank_writes(after,result.memory_writes);
        assert(get_word(after,first_root+0x2a)==first
            &&get_word(after,first_root+0x2e)==second
            &&get_word(after,second_root+0x2a)==first
            &&get_word(after,second_root+0x2e)==second);
        assert(get_word(after,first_root+0x2c)==get_word(before,first_root+0x2c)
            &&get_word(after,second_root+0x2c)==get_word(before,second_root+0x2c));
        assert(get_long(after,0x2079e)==0x12345674
            &&get_word(after,0x207b0)==static_cast<std::uint16_t>(start_index+1U));
    }

    auto audio=make_vblank_memory(disk,plan);
    set_long(audio,0x2079e,0x100);
    audio[0x207ea]=0;
    audio[0x207eb]=1;
    const auto audio_result=run_vblank(audio);
    assert(audio_result.audio_dispatch_requested
        &&audio_result.audio_dispatch_address==0x22bea
        &&audio_result.stop_before_address==0x20860
        &&audio_result.continuation_address==0);

    auto missing=make_vblank_memory(disk,plan);
    set_long(missing,0x2079e,0x80);
    set_word(missing,0x207b0,0xffff);
    missing[0x207ea]=1;
    missing.erase(0x207b2);
    const auto missing_before=missing;
    bool rejected_missing=false;
    try{static_cast<void>(run_vblank(missing));}
    catch(const std::runtime_error&){rejected_missing=true;}
    assert(rejected_missing&&missing==missing_before);

    auto invalid_pointer=make_vblank_memory(disk,plan);
    set_long(invalid_pointer,0x2079e,0x84);
    invalid_pointer[0x207ea]=1;
    set_long(invalid_pointer,0x2197a,0x00ffffe0);
    const auto invalid_before=invalid_pointer;
    bool rejected_pointer=false;
    try{static_cast<void>(run_vblank(invalid_pointer));}
    catch(const std::runtime_error&){rejected_pointer=true;}
    assert(rejected_pointer&&invalid_pointer==invalid_before);

    auto altered_code=make_vblank_memory(disk,plan);
    altered_code[0x207fe]^=1;
    const auto altered_before=altered_code;
    bool rejected_hash=false;
    try{static_cast<void>(run_vblank(altered_code));}
    catch(const std::runtime_error&){rejected_hash=true;}
    assert(rejected_hash&&altered_code==altered_before);
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
    verify_vblank_callback(disk,plan);

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
