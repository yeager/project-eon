#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_video_function_six_mcga_copy_loop_session.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::uint8_t> read_mcga(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Missing direct MCGA.BIN media");
    std::vector<std::uint8_t> bytes(4'366);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("MCGA.BIN size changed during bounded read");
    }
    char extra = 0;
    if (stream.get(extra) || !stream.eof() || stream.bad()) {
        throw std::runtime_error("MCGA.BIN is larger than its bounded size or unreadable");
    }
    static_cast<void>(eon::parse_millennium_dos_video_driver(
        bytes, eon::MillenniumDosVideoDriverKind::mcga));
    return bytes;
}

std::vector<std::uint8_t> find_mcga(const std::filesystem::path& root) {
    constexpr std::size_t max_entries = 10'000;
    std::size_t visited = 0;
    std::error_code error;
    std::filesystem::recursive_directory_iterator it(root,
        std::filesystem::directory_options::skip_permission_denied, error), end;
    if (error) throw std::runtime_error("Cannot inspect direct media directory");
    for (; it != end; it.increment(error)) {
        if (error) throw std::runtime_error("Cannot continue scanning direct media directory");
        if (++visited > max_entries) throw std::runtime_error("Direct media scan bound exceeded");
        const auto status = it->symlink_status(error);
        if (error) throw std::runtime_error("Cannot inspect direct media entry");
        if (std::filesystem::is_symlink(status)) {
            if (it->is_directory(error)) it.disable_recursion_pending();
            if (error) throw std::runtime_error("Cannot inspect direct media symlink");
            continue;
        }
        if (!std::filesystem::is_regular_file(status) || it->path().filename() != "MCGA.BIN") continue;
        if (it->file_size(error) != 4'366 || error) {
            error.clear();
            continue;
        }
        try { return read_mcga(it->path()); }
        catch (const std::exception&) { /* Continue past unrelated same-name files. */ }
    }
    if (error) throw std::runtime_error("Cannot finish direct media scan");
    throw std::runtime_error("No hash-admitted English MCGA.BIN found in direct media directory");
}

template<typename F>
void expect_rejected(F&& f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}

void seed(eon::NativeRuntimeMemory& memory, const std::string& id,
    const std::uint64_t base, const std::vector<std::uint8_t>& bytes) {
    eon::NativeRuntimeEffectBatch batch{id, true, {}};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        batch.effects.push_back({i + 1,
            {eon::NativeRuntimeAddressSpace::linear, std::nullopt, base + i},
            eon::MemoryTransferElementWidth::byte,
            eon::NativeRuntimeByteOrder::little_endian, bytes[i]});
    }
    assert(memory.apply(batch).accepted);
}

std::uint64_t physical(const std::uint16_t segment, const std::uint16_t offset) {
    return static_cast<std::uint64_t>(segment) * 16U + offset;
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) return 2;
    const auto driver = find_mcga(argv[1]);
    using Session = eon::MillenniumDosVideoFunctionSixMcgaCopyLoopSession;
    using Kind = eon::MillenniumDosVideoFunctionSixMcgaCopyLoopKind;
    using Registers = eon::MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters;

    const auto forward = Session(driver, Kind::word_aligned,
        Registers{5, 4, 0xaaaa, 0xfffe, 0xfffe, 1, 2, 0x1111, 0x2222, false}).outcome();
    assert(forward.next_instruction == 0x079c && forward.rows.size() == 2);
    assert(forward.rows[0].phases == (std::vector<eon::MillenniumDosVideoFunctionSixMcgaCopyPhase>{
        {0x078f,0x1111,0x2222,0xfffe,1,2,2,false,2,5},
        {0x0793,0x1111,0x2222,2,5,1,1,false,3,6}}));
    assert(forward.final_registers.si == 0x0010 && forward.final_registers.di == 0x0007
        && forward.final_registers.bp == 0);

    const auto backwards = Session(driver, Kind::word_aligned,
        Registers{3, 0x0010, 0xbbbb, 0x0020, 2, 1, 1, 0x3333, 0x4444, true}).outcome();
    assert(backwards.rows[0].phases == (std::vector<eon::MillenniumDosVideoFunctionSixMcgaCopyPhase>{
        {0x078f,0x3333,0x4444,2,1,1,2,true,0,0xffff},
        {0x0793,0x3333,0x4444,0,0xffff,1,1,true,0xffff,0xfffe}}));
    assert(backwards.final_registers.si == 0x000f && backwards.final_registers.di == 0x001e);

    const auto byte_aligned = Session(driver, Kind::byte_aligned,
        Registers{6, 2, 0xcccc, 3, 0xffff, 0, 1, 0x5555, 0x6666, false}).outcome();
    assert(byte_aligned.next_instruction == 0x07b4 && byte_aligned.rows.size() == 1);
    assert(byte_aligned.rows[0].phases == (std::vector<eon::MillenniumDosVideoFunctionSixMcgaCopyPhase>{
        {0x07a3,0x5555,0x6666,0xffff,0,1,1,false,0,1},
        {0x07a7,0x5555,0x6666,0,1,2,2,false,4,5},
        {0x07ab,0x5555,0x6666,4,5,1,1,false,5,6}}));
    assert(byte_aligned.final_registers.si == 7 && byte_aligned.final_registers.di == 9);

    const auto zero_width = Session(driver, Kind::byte_aligned,
        Registers{0, 0, 0xdddd, 0, 0, 0, 1, 0x7777, 0x8888, false}).outcome();
    assert(zero_width.rows[0].phases[0].element_count == 1
        && zero_width.rows[0].phases[1].element_count == 0x7fff
        && zero_width.rows[0].phases[2].element_count == 1);
    expect_rejected([&] {
        static_cast<void>(Session(driver, Kind::byte_aligned,
            Registers{1,0,0,0,0,0,0,0,0,false}));
    });
    expect_rejected([&] {
        static_cast<void>(Session(driver, Kind::word_aligned,
            Registers{1,0,0,0,0,0,0,0,0,false}, 65'536));
    });

    using Mapping = eon::MillenniumDosRealModeAddressMapping;
    eon::NativeRuntimeMemory copied;
    seed(copied, "copy-source", physical(0x1000, 0x0010), {0x12,0x34,0x56,0x78});
    const auto copied_loop = Session(driver, Kind::word_aligned,
        Registers{3,0,0,0,0x0010,0x0020,1,0x1000,0x2000,false}).outcome();
    const auto copied_result = eon::execute_millennium_dos_video_function_six_mcga_copy_loop(
        driver, copied_loop,
        Mapping::unwrapped_21_bit, copied, "copy-run");
    assert(copied_result.accepted && copied_result.memory_applied
        && copied_result.byte_write_count == 3 && copied_result.distinct_destination_bytes == 3);
    assert(copied.read_linear_range(physical(0x2000,0x0020),3)
        == std::optional<std::vector<std::uint8_t>>({0x12,0x34,0x56}));

    eon::NativeRuntimeMemory backwards_memory;
    seed(backwards_memory, "backwards-source", physical(0x3000, 0x0010), {0x11,0x22,0x33,0x44});
    const auto backwards_result = eon::execute_millennium_dos_video_function_six_mcga_copy_loop(
        driver, Kind::word_aligned,
        Registers{4,0,0,0,0x0012,0x0022,1,0x3000,0x4000,true},
        Mapping::unwrapped_21_bit, backwards_memory, "backwards-run");
    assert(backwards_result.accepted && backwards_result.byte_write_count == 4);
    assert(backwards_memory.read_linear_range(physical(0x4000,0x0020),4)
        == std::optional<std::vector<std::uint8_t>>({0x11,0x22,0x33,0x44}));

    eon::NativeRuntimeMemory overlap_memory;
    seed(overlap_memory, "overlap-source", physical(0x5000, 0x0001), {0x11,0x22,0x33,0x44,0x55});
    const auto overlap_result = eon::execute_millennium_dos_video_function_six_mcga_copy_loop(
        driver, Kind::word_aligned,
        Registers{4,0,0,0,0x0001,0x0002,1,0x5000,0x5000,false},
        Mapping::unwrapped_21_bit, overlap_memory, "overlap-run");
    assert(overlap_result.accepted && overlap_result.byte_write_count == 4
        && overlap_result.distinct_destination_bytes == 4);
    assert(overlap_memory.read_linear_range(physical(0x5000,0x0002),4)
        == std::optional<std::vector<std::uint8_t>>({0x11,0x22,0x22,0x44}));

    eon::NativeRuntimeMemory a20_memory;
    seed(a20_memory, "a20-alias-byte", 0, {0x5a});
    const auto a20_result = eon::execute_millennium_dos_video_function_six_mcga_copy_loop(
        driver, Kind::word_aligned,
        Registers{1,0,0,0,0x0010,0x0000,1,0xffff,0x0000,false},
        Mapping::a20_wrapped_20_bit, a20_memory, "a20-alias-run");
    assert(a20_result.accepted && a20_memory.read_byte(
        {eon::NativeRuntimeAddressSpace::linear,std::nullopt,0}) == 0x5a);

    eon::NativeRuntimeMemory unwrapped_memory;
    seed(unwrapped_memory, "unwrapped-top-word", 0x10ffef, {0xa5,0x5a});
    const auto unwrapped_result = eon::execute_millennium_dos_video_function_six_mcga_copy_loop(
        driver, Kind::word_aligned,
        Registers{2,0,0,0,0xffff,0x0000,1,0xffff,0x1000,false},
        Mapping::unwrapped_21_bit, unwrapped_memory, "unwrapped-top-word-run");
    assert(unwrapped_result.accepted && unwrapped_result.byte_write_count == 2);
    assert(unwrapped_memory.read_linear_range(physical(0x1000,0x0000),2)
        == std::optional<std::vector<std::uint8_t>>({0xa5,0x5a}));

    eon::NativeRuntimeMemory incomplete_memory;
    const auto incomplete_before = incomplete_memory.diagnostics();
    const auto incomplete_result = eon::execute_millennium_dos_video_function_six_mcga_copy_loop(
        driver, Kind::word_aligned,
        Registers{2,0,0,0,0x0030,0x0040,1,0x6000,0x7000,false},
        Mapping::unwrapped_21_bit, incomplete_memory, "incomplete-run");
    const auto incomplete_after = incomplete_memory.diagnostics();
    assert(!incomplete_result.accepted && !incomplete_result.memory_applied
        && incomplete_after.initialized_byte_count == incomplete_before.initialized_byte_count
        && incomplete_after.applied_batch_count == incomplete_before.applied_batch_count
        && incomplete_after.checksum == incomplete_before.checksum);
}
