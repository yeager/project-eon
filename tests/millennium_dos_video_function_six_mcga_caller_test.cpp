#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_video_function_six_mcga_caller_session.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::uint8_t> read_mcga(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Missing direct MCGA.BIN media");
    std::vector<std::uint8_t> bytes(4'366);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("MCGA.BIN changed during bounded read");
    }
    char extra = 0;
    if (stream.get(extra) || !stream.eof() || stream.bad()) {
        throw std::runtime_error("MCGA.BIN exceeds its admitted size or is unreadable");
    }
    static_cast<void>(eon::parse_millennium_dos_video_driver(bytes, eon::MillenniumDosVideoDriverKind::mcga));
    return bytes;
}

std::vector<std::uint8_t> find_mcga(const std::filesystem::path& root) {
    std::error_code ec;
    std::size_t count = 0;
    std::filesystem::recursive_directory_iterator it(root,
        std::filesystem::directory_options::skip_permission_denied, ec), end;
    if (ec) throw std::runtime_error("Cannot scan direct media directory");
    for (; it != end; it.increment(ec)) {
        if (ec || ++count > 10'000) throw std::runtime_error("Direct media scan failed or exceeded bound");
        const auto status = it->symlink_status(ec);
        if (ec) throw std::runtime_error("Cannot inspect direct media entry");
        if (std::filesystem::is_symlink(status)) {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            if (ec) throw std::runtime_error("Cannot inspect direct media symlink");
            continue;
        }
        if (!std::filesystem::is_regular_file(status) || it->path().filename() != "MCGA.BIN") continue;
        if (it->file_size(ec) != 4'366 || ec) { ec.clear(); continue; }
        try { return read_mcga(it->path()); } catch (const std::exception&) { }
    }
    throw std::runtime_error("No hash-admitted direct English MCGA.BIN found");
}

eon::MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome setup(
    const std::vector<std::uint8_t>& driver, const std::uint16_t width = 4) {
    using Session = eon::MillenniumDosVideoFunctionSixMcgaRegisterSetupSession;
    const eon::MillenniumDosVideoFunctionSixMcgaRegisterSnapshot initial{
        2,0x0200,0x3333,0x4444,0x1000,0x2222,0x1111,0x3333,0x4444,0x0100};
    Session session(driver, initial);
    const auto bx = initial.bx;
    session.observe_word_read({1,0x073d,initial.es,static_cast<std::uint16_t>(bx+0x0a),2});
    session.observe_word_read({2,0x0741,initial.es,static_cast<std::uint16_t>(bx+0x0c),0});
    session.observe_byte_read({3,0x0747,initial.es,static_cast<std::uint16_t>(bx+0x06),0x10});
    session.observe_word_read({4,0x0755,initial.es,static_cast<std::uint16_t>(bx+0x08),0});
    session.observe_word_read({5,0x0759,initial.es,static_cast<std::uint16_t>(bx+0x10),width});
    session.observe_word_read({6,0x0764,initial.es,static_cast<std::uint16_t>(bx+0x0e),2});
    session.observe_byte_read({7,0x076b,initial.es,static_cast<std::uint16_t>(bx+0x04),0x02});
    assert(session.outcome());
    return *session.outcome();
}

eon::MillenniumDosVideoFunctionSixMcgaHelperOutcome helper(
    const std::vector<std::uint8_t>& driver,
    const eon::MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome& setup) {
    using Session = eon::MillenniumDosVideoFunctionSixMcgaHelperSession;
    const auto& r = setup.final_registers;
    const eon::MillenniumDosVideoDataSegment ds{r.ds};
    const eon::MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss{r.ss};
    const auto call_sp = static_cast<std::uint16_t>(r.sp - 2U);
    Session session(driver,r.ax,r.bx,r.dx,ds,ss,call_sp);
    // AL=$02 is below the observed threshold. Signed word arithmetic yields
    // table offset $0088. Returned AX/DX are explicit un-interpreted words.
    session.observe_threshold({1,0x0666,ds,0x00ac,0x80});
    session.observe_table_word({2,0x0678,ds,0x0088,4});
    session.observe_table_word({3,0x067a,ds,0x008a,0x5555});
    session.observe_stack_word({4,0x067d,ss,static_cast<std::uint16_t>(call_sp-2U),r.bx});
    session.observe_stack_word({5,0x067e,ss,call_sp,0x0772});
    assert(session.outcome());
    return *session.outcome();
}

eon::MillenniumDosVideoFunctionSixMcgaHelperOutcome helper_carry(
    const std::vector<std::uint8_t>& driver,
    const eon::MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome& setup) {
    using Session = eon::MillenniumDosVideoFunctionSixMcgaHelperSession;
    const auto& r = setup.final_registers;
    const eon::MillenniumDosVideoDataSegment ds{r.ds};
    const eon::MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss{r.ss};
    const auto call_sp = static_cast<std::uint16_t>(r.sp - 2U);
    Session session(driver,r.ax,r.bx,r.dx,ds,ss,call_sp);
    session.observe_threshold({1,0x0666,ds,0x00ac,0x02});
    session.observe_stack_word({2,0x0680,ss,call_sp,0x0772});
    assert(session.outcome() && session.outcome()->carry);
    return *session.outcome();
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) return 2;
    const auto driver = find_mcga(argv[1]);
    const auto prepared = setup(driver);
    const auto returned = helper(driver,prepared);
    using Caller = eon::MillenniumDosVideoFunctionSixMcgaCallerSession;
    using StackRead = eon::MillenniumDosVideoFunctionSixMcgaCallerStackRead;
    using State = eon::MillenniumDosVideoFunctionSixMcgaCallerState;
    Caller caller(driver,prepared,returned,false,16);
    assert(caller.state() == State::awaiting_stack_restore);
    bool rejected_out_of_order_restore = false;
    try {
        caller.observe_stack_read({1,0x0779,{prepared.initial_registers.ss},
            prepared.stack_writes[4].offset,prepared.stack_writes[4].value});
    } catch (const std::runtime_error&) { rejected_out_of_order_restore = true; }
    assert(rejected_out_of_order_restore && caller.stack_reads().empty()
        && caller.state() == State::awaiting_stack_restore);
    for (std::size_t i = 0; i < 5; ++i) {
        const auto& write = prepared.stack_writes[4-i];
        constexpr std::uint16_t addresses[] = {0x0778,0x0779,0x077a,0x077b,0x077c};
        caller.observe_stack_read({i+1,addresses[i],{write.ss},write.offset,write.value});
    }
    caller.observe_mode_byte({6,0x077f,0x7777,0x07b9,0x88},driver);
    assert(caller.state() == State::awaiting_copy_return);
    assert(caller.outcome() == std::nullopt);
    // Width is four, so the caller dispatches to the word-aligned REP path.
    // The loop has two rows and RET consumes the caller's original SS:SP word.
    caller.observe_stack_read({7,0x079c,{prepared.initial_registers.ss},
        prepared.initial_registers.sp,0xbeef});
    assert(caller.state() == State::complete && caller.outcome());
    assert(caller.outcome()->instruction_boundary == 0x079c
        && caller.outcome()->caller_return_ip == 0xbeef
        && caller.outcome()->final_sp == static_cast<std::uint16_t>(prepared.initial_registers.sp+2U)
        && caller.outcome()->copy_loop
        && caller.outcome()->copy_loop->kind == eon::MillenniumDosVideoFunctionSixMcgaCopyLoopKind::word_aligned
        && caller.outcome()->copy_loop->rows.size() == 2
        && caller.outcome()->call_stack_write.offset == static_cast<std::uint16_t>(prepared.final_registers.sp-2U)
        && caller.outcome()->call_stack_write.value == 0x0772);

    // RET observations are allowed to supply the actual caller destination,
    // but only from the exact instruction and stack slot reached by the
    // hash-admitted driver path. Reject malformed observations before state
    // advances so the valid event can still be consumed afterward.
    auto wrong_ret_instruction = Caller(driver,prepared,returned,false,16);
    for (std::size_t i = 0; i < 5; ++i) {
        const auto& write = prepared.stack_writes[4-i];
        constexpr std::uint16_t addresses[] = {0x0778,0x0779,0x077a,0x077b,0x077c};
        wrong_ret_instruction.observe_stack_read({i+1,addresses[i],{write.ss},write.offset,write.value});
    }
    wrong_ret_instruction.observe_mode_byte({6,0x077f,0x7777,0x07b9,0x88},driver);
    bool rejected_wrong_ret_instruction = false;
    try {
        wrong_ret_instruction.observe_stack_read({7,0x079b,{prepared.initial_registers.ss},
            prepared.initial_registers.sp,0xbeef});
    } catch (const std::runtime_error&) { rejected_wrong_ret_instruction = true; }
    assert(rejected_wrong_ret_instruction
        && wrong_ret_instruction.state() == State::awaiting_copy_return
        && !wrong_ret_instruction.outcome());
    wrong_ret_instruction.observe_stack_read({7,0x079c,{prepared.initial_registers.ss},
        prepared.initial_registers.sp,0xbeef});
    assert(wrong_ret_instruction.state() == State::complete);

    auto wrong_ret_stack = Caller(driver,prepared,returned,false,16);
    for (std::size_t i = 0; i < 5; ++i) {
        const auto& write = prepared.stack_writes[4-i];
        constexpr std::uint16_t addresses[] = {0x0778,0x0779,0x077a,0x077b,0x077c};
        wrong_ret_stack.observe_stack_read({i+1,addresses[i],{write.ss},write.offset,write.value});
    }
    wrong_ret_stack.observe_mode_byte({6,0x077f,0x7777,0x07b9,0x88},driver);
    bool rejected_wrong_ret_stack = false;
    try {
        wrong_ret_stack.observe_stack_read({7,0x079c,{prepared.initial_registers.ss},
            static_cast<std::uint16_t>(prepared.initial_registers.sp+2U),0xbeef});
    } catch (const std::runtime_error&) { rejected_wrong_ret_stack = true; }
    assert(rejected_wrong_ret_stack
        && wrong_ret_stack.state() == State::awaiting_copy_return
        && !wrong_ret_stack.outcome());

    // The completed caller result is consumed directly by the memory
    // executor. Fixture bytes supply mechanics only; the loop and dispatch
    // outcome come from the hash-admitted original MCGA driver.
    eon::NativeRuntimeMemory caller_memory;
    std::map<std::uint64_t, std::uint8_t> caller_source_bytes;
    for (const auto& row : caller.outcome()->copy_loop->rows) {
        for (const auto& phase : row.phases) {
            auto offset = phase.source_start;
            const auto step = phase.direction_flag
                ? static_cast<std::uint16_t>(0U - phase.element_size)
                : static_cast<std::uint16_t>(phase.element_size);
            for (std::uint32_t element = 0; element < phase.element_count; ++element) {
                for (std::uint8_t byte = 0; byte < phase.element_size; ++byte) {
                    const auto address = static_cast<std::uint64_t>(phase.ds) * 16U + offset + byte;
                    caller_source_bytes.try_emplace(address,
                        static_cast<std::uint8_t>(address & 0xffU));
                }
                offset = static_cast<std::uint16_t>(offset + step);
            }
        }
    }
    eon::NativeRuntimeEffectBatch caller_seed{"caller-mcga-source", true, {}};
    for (const auto& [address, value] : caller_source_bytes) {
        caller_seed.effects.push_back({caller_seed.effects.size() + 1,
            {eon::NativeRuntimeAddressSpace::linear, std::nullopt, address},
            eon::MemoryTransferElementWidth::byte,
            eon::NativeRuntimeByteOrder::little_endian, value});
    }
    assert(!caller_seed.effects.empty() && caller_memory.apply(caller_seed).accepted);
    const auto caller_copy = eon::execute_millennium_dos_video_function_six_mcga_copy_loop(
        driver, *caller.outcome(), eon::MillenniumDosRealModeAddressMapping::unwrapped_21_bit,
        caller_memory, "caller-mcga-copy");
    assert(caller_copy.accepted && caller_copy.memory_applied
        && caller_copy.byte_write_count > 0);

    const auto odd_prepared = setup(driver,3);
    const auto odd_helper = helper(driver,odd_prepared);
    Caller odd_width(driver,odd_prepared,odd_helper,false,16);
    for (std::size_t i = 0; i < 5; ++i) {
        const auto& write = odd_prepared.stack_writes[4-i];
        constexpr std::uint16_t addresses[] = {0x0778,0x0779,0x077a,0x077b,0x077c};
        odd_width.observe_stack_read({i+1,addresses[i],{write.ss},write.offset,write.value});
    }
    odd_width.observe_mode_byte({6,0x077f,0x7777,0x07b9,0x88},driver);
    assert(odd_width.state() == State::awaiting_copy_return
        && odd_width.stack_reads().size() == 5);
    odd_width.observe_stack_read({7,0x07b4,{odd_prepared.initial_registers.ss},
        odd_prepared.initial_registers.sp,0xface});
    assert(odd_width.state() == State::complete
        && odd_width.outcome()->copy_loop
        && odd_width.outcome()->copy_loop->kind == eon::MillenniumDosVideoFunctionSixMcgaCopyLoopKind::byte_aligned
        && odd_width.outcome()->caller_return_ip == 0xface);

    auto other_mode = Caller(driver,prepared,returned,false,16);
    for (std::size_t i = 0; i < 5; ++i) {
        const auto& write = prepared.stack_writes[4-i];
        constexpr std::uint16_t addresses[] = {0x0778,0x0779,0x077a,0x077b,0x077c};
        other_mode.observe_stack_read({i+1,addresses[i],{write.ss},write.offset,write.value});
    }
    other_mode.observe_mode_byte({6,0x077f,0x7777,0x07b9,0x87},driver);
    assert(other_mode.state() == State::before_overlay_branch
        && other_mode.outcome()->instruction_boundary == 0x07b5
        && !other_mode.outcome()->caller_return_ip
        && other_mode.outcome()->overlay_registers
        && other_mode.outcome()->overlay_registers->ss == prepared.initial_registers.ss
        && other_mode.outcome()->overlay_registers->sp == prepared.initial_registers.sp
        && other_mode.outcome()->overlay_registers->ax == prepared.final_registers.cx
        && other_mode.outcome()->overlay_registers->ds == prepared.initial_registers.ds
        && other_mode.outcome()->overlay_registers->es == returned.final_dx
        && other_mode.outcome()->overlay_registers->cx == other_mode.stack_reads()[3].value
        && other_mode.outcome()->overlay_registers->bx == static_cast<std::uint16_t>(
            other_mode.stack_reads()[1].value - other_mode.stack_reads()[3].value)
        && other_mode.outcome()->overlay_registers->dx == other_mode.stack_reads()[0].value
        && other_mode.outcome()->overlay_registers->si == prepared.final_registers.si
        && other_mode.outcome()->overlay_registers->di == static_cast<std::uint16_t>(
            prepared.final_registers.di + returned.final_ax)
        && other_mode.outcome()->overlay_registers->bp == other_mode.stack_reads()[2].value
        && !other_mode.outcome()->overlay_registers->direction_flag);

    const auto helper_error = helper_carry(driver,prepared);
    Caller failed_helper(driver,prepared,helper_error,false,16);
    assert(failed_helper.state() == State::awaiting_caller_return);
    failed_helper.observe_stack_read({1,0x07a0,{prepared.initial_registers.ss},
        prepared.initial_registers.sp,0xcaFE});
    assert(failed_helper.state() == State::complete
        && failed_helper.outcome()->instruction_boundary == 0x07a0
        && failed_helper.outcome()->caller_return_ip == 0xcafe
        && failed_helper.outcome()->final_sp == static_cast<std::uint16_t>(prepared.initial_registers.sp+2U));
    auto failed_helper_bad_ret = Caller(driver,prepared,helper_error,false,16);
    bool rejected_failed_helper_ret = false;
    try {
        failed_helper_bad_ret.observe_stack_read({1,0x07a1,{prepared.initial_registers.ss},
            prepared.initial_registers.sp,0xcafe});
    } catch (const std::runtime_error&) { rejected_failed_helper_ret = true; }
    assert(rejected_failed_helper_ret
        && failed_helper_bad_ret.state() == State::awaiting_caller_return
        && !failed_helper_bad_ret.outcome());
    return 0;
}
