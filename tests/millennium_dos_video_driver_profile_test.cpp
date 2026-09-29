#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_video_function_zero_session.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_driver(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Missing real Millennium DOS video-driver leaf: " + path.filename().string());
    }
    return {std::istreambuf_iterator<char>(stream), {}};
}

template<typename Function>
void expect_rejected(Function&& function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    const auto root = std::filesystem::path(argv[1]);
    const auto ega = eon::parse_millennium_dos_video_driver(
        read_driver(root / "EGA640.BIN"), eon::MillenniumDosVideoDriverKind::ega640);
    const auto mcga = eon::parse_millennium_dos_video_driver(
        read_driver(root / "MCGA.BIN"), eon::MillenniumDosVideoDriverKind::mcga);

    assert(ega.function_zero_cached_mode_known_branch_target == 0x1db);
    assert(ega.function_zero_cached_mode_query_interrupt_site == 0x1d6);
    assert(ega.function_zero_cached_mode_store_instruction == 0x1d8);
    assert(ega.function_zero_verify_mode_interrupt_site == 0x1e2);
    assert(ega.function_zero_mode_match_branch_target == 0x1eb);
    assert(ega.function_zero_mode_mismatch_return == 0x1ea);
    assert(mcga.function_zero_cached_mode_known_branch_target == 0x1f9);
    assert(mcga.function_zero_cached_mode_query_interrupt_site == 0x1f4);
    assert(mcga.function_zero_cached_mode_store_instruction == 0x1f6);
    assert(mcga.function_zero_verify_mode_interrupt_site == 0x200);
    assert(mcga.function_zero_mode_match_branch_target == 0x209);
    assert(mcga.function_zero_mode_mismatch_return == 0x208);

    using Session = eon::MillenniumDosVideoFunctionZeroSession;
    using State = eon::MillenniumDosVideoFunctionZeroState;
    using Result = eon::MillenniumDosVideoFunctionZeroBiosResult;
    using Endpoint = eon::MillenniumDosVideoFunctionZeroEndpoint;

    const auto ega_bytes = read_driver(root / "EGA640.BIN");
    Session ega_unknown(ega_bytes, eon::MillenniumDosVideoDriverKind::ega640, 0xff);
    auto boundary = ega_unknown.boundary();
    assert(boundary && boundary->kind
        == eon::MillenniumDosVideoFunctionZeroBoundaryKind::cached_mode_query
        && boundary->instruction_address == 0x1d6
        && boundary->ax == 0x0f00 && boundary->ax_known_mask == 0xff00);
    const Result ega_query{1,0x1d6,0x10,0x560e,0x1234,0x5678,0x9abc,0x0202};
    const auto pre_rejection_state = ega_unknown.state();
    expect_rejected([&] { ega_unknown.observe_bios_result(
        Result{2,0x1d6,0x10,0x560e,0x1234,0x5678,0x9abc,0x0202}); });
    assert(ega_unknown.state() == pre_rejection_state && ega_unknown.next_sequence() == 1
        && ega_unknown.bios_results().empty() && !ega_unknown.cache_write());
    expect_rejected([&] { ega_unknown.observe_bios_result(
        Result{1,0x1d6,0x11,0x560e,0,0,0,0}); });
    ega_unknown.observe_bios_result(ega_query);
    const eon::MillenniumDosVideoFunctionZeroLocalWrite ega_cache_write{0x1d8,0x8c,0x0e};
    assert(ega_unknown.cache_write() == ega_cache_write);
    boundary = ega_unknown.boundary();
    assert(boundary && boundary->kind == eon::MillenniumDosVideoFunctionZeroBoundaryKind::set_mode
        && boundary->instruction_address == 0x1de && boundary->ax == 0x000e
        && boundary->ax_known_mask == 0xffff);
    ega_unknown.observe_bios_result(Result{2,0x1de,0x10,0xaaaa,1,2,3,4});
    boundary = ega_unknown.boundary();
    assert(boundary && boundary->kind
        == eon::MillenniumDosVideoFunctionZeroBoundaryKind::verify_mode_query
        && boundary->instruction_address == 0x1e2
        && boundary->ax == 0x0faa && boundary->ax_known_mask == 0xffff);
    const Result ega_verified{3,0x1e2,0x10,0x700e,0x1357,0x2468,0x9876,0x0202};
    ega_unknown.observe_bios_result(ega_verified);
    assert(ega_unknown.state() == State::mode_match_continuation_boundary
        && !ega_unknown.boundary());
    const auto ega_outcome = ega_unknown.outcome();
    assert(ega_outcome && ega_outcome->endpoint == Endpoint::mode_match_continuation
        && ega_outcome->instruction_address == 0x1eb
        && ega_outcome->verify_result == ega_verified
        && ega_outcome->ax_after_local_effect == ega_verified.ax
        && !ega_outcome->ax_is_routine_return);
    expect_rejected([&] { ega_unknown.observe_bios_result(ega_verified); });
    ega_unknown.advance_success_postlude_prefix();
    const auto ega_postlude = ega_unknown.postlude_prefix_outcome();
    const eon::MillenniumDosVideoFunctionZeroLocalWrite ega_postlude_write{0x1eb,0x0192,0};
    assert(ega_postlude
        && ega_postlude->endpoint
            == eon::MillenniumDosVideoFunctionZeroPostludeEndpoint::ega_pre_push
        && ega_postlude->instruction_address == 0x1fd
        && ega_postlude->ax == ega_verified.ax && ega_postlude->bx == 0x72
        && ega_postlude->cx == 4 && ega_postlude->si == 4 && !ega_postlude->di
        && ega_postlude->local_write == ega_postlude_write);
    expect_rejected([&] { ega_unknown.advance_success_postlude_prefix(); });
    ega_unknown.advance_ega_success_stack_prefix(0x2000, 0x0100);
    const auto ega_stack = ega_unknown.ega_stack_outcome();
    assert(ega_stack
        && ega_stack->endpoint
            == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_vga_out
        && ega_stack->instruction_address == 0x0207
        && ega_stack->ss == 0x2000 && ega_stack->sp_before == 0x0100
        && ega_stack->sp_after == 0x00fe && ega_stack->pushed_value == 4
        && ega_stack->si == 3 && ega_stack->ax == 0xff08
        && ega_stack->bx == 0x0072 && ega_stack->cx == 4
        && ega_stack->dx == 0x03ce && !ega_stack->zero_flag);
    expect_rejected([&] { ega_unknown.advance_ega_success_stack_prefix(0x2000, 0x0100); });

    Session ega_single_count(ega_bytes, eon::MillenniumDosVideoDriverKind::ega640, 0xff);
    ega_single_count.observe_bios_result(Result{1,0x1d6,0x10,0x560e,0,0,0,0});
    ega_single_count.observe_bios_result(Result{2,0x1de,0x10,0x000e,0,0,0,0});
    ega_single_count.observe_bios_result(Result{3,0x1e2,0x10,0x000e,0,1,0,0});
    ega_single_count.advance_success_postlude_prefix();
    ega_single_count.advance_ega_success_stack_prefix(0x1000, 1);
    const auto ega_single_stack = ega_single_count.ega_stack_outcome();
    assert(ega_single_stack
        && ega_single_stack->endpoint
            == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_single_count_pop
        && ega_single_stack->instruction_address == 0x022e
        && ega_single_stack->sp_before == 1 && ega_single_stack->sp_after == 0xffff
        && ega_single_stack->pushed_value == 1 && ega_single_stack->si == 0
        && ega_single_stack->zero_flag);
    ega_single_count.advance_ega_single_count_pop_prefix();
    const auto ega_single_pop = ega_single_count.ega_stack_outcome();
    assert(ega_single_count.state() == State::mode_success_single_pop_prefix_recorded
        && ega_single_pop
        && ega_single_pop->endpoint
            == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_local_store
        && ega_single_pop->instruction_address == 0x022f
        && ega_single_pop->sp_before == 1 && ega_single_pop->sp_after == 1
        && ega_single_pop->pushed_value == 1 && ega_single_pop->ax == 1);
    expect_rejected([&] { ega_single_count.advance_ega_single_count_pop_prefix(); });
    ega_single_count.advance_ega_single_count_store_prefix(0x4567);
    const auto ega_single_ret = ega_single_count.ega_stack_outcome();
    const eon::MillenniumDosVideoFunctionZeroLocalWrite ega_single_write{0x022f,0x008a,1};
    assert(ega_single_count.state() == State::mode_success_single_store_prefix_recorded
        && ega_single_ret
        && ega_single_ret->endpoint
            == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_ret
        && ega_single_ret->instruction_address == 0x0234
        && ega_single_ret->ax == 0x0401 && ega_single_ret->ds == 0x4567
        && ega_single_ret->local_write == ega_single_write
        && ega_single_ret->sp_after == ega_single_ret->sp_before);
    expect_rejected([&] { ega_single_count.advance_ega_single_count_store_prefix(0x4567); });

    Session ega_known(ega_bytes, eon::MillenniumDosVideoDriverKind::ega640, 0x0e);
    assert(ega_known.state() == State::awaiting_set_mode_result && !ega_known.cache_write()
        && ega_known.boundary()->instruction_address == 0x1de);
    ega_known.observe_bios_result(Result{1,0x1de,0x10,0x000e,0,0,0,0});
    ega_known.observe_bios_result(Result{2,0x1e2,0x10,0x1234,0x5678,0x9abc,0xdef0,0x0202});
    const auto ega_mismatch = ega_known.outcome();
    assert(ega_mismatch && ega_mismatch->endpoint == Endpoint::mode_mismatch_ret_boundary
        && ega_mismatch->instruction_address == 0x1ea
        && ega_mismatch->verify_result.ax == 0x1234
        && ega_mismatch->ax_after_local_effect == 0 && !ega_mismatch->ax_is_routine_return);
    expect_rejected([&] { ega_known.advance_success_postlude_prefix(); });
    expect_rejected([&] { ega_known.advance_ega_success_stack_prefix(0, 0); });

    const auto mcga_bytes = read_driver(root / "MCGA.BIN");
    Session mcga_unknown(mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga, 0xff);
    assert(mcga_unknown.boundary()->instruction_address == 0x1f4);
    mcga_unknown.observe_bios_result(Result{1,0x1f4,0x10,0x0013,0,0,0,0});
    const eon::MillenniumDosVideoFunctionZeroLocalWrite mcga_cache_write{0x1f6,0xae,0x13};
    assert(mcga_unknown.cache_write() == mcga_cache_write);
    mcga_unknown.observe_bios_result(Result{2,0x1fc,0x10,0x0013,0,0,0,0});
    mcga_unknown.observe_bios_result(Result{3,0x200,0x10,0x8813,0,0,0,0});
    const auto mcga_match = mcga_unknown.outcome();
    assert(mcga_match && mcga_match->endpoint == Endpoint::mode_match_continuation
        && mcga_match->instruction_address == 0x209
        && mcga_match->verify_result.ax == 0x8813 && !mcga_match->ax_is_routine_return);
    mcga_unknown.advance_success_postlude_prefix();
    const auto mcga_postlude = mcga_unknown.postlude_prefix_outcome();
    assert(mcga_postlude
        && mcga_postlude->endpoint
            == eon::MillenniumDosVideoFunctionZeroPostludeEndpoint::mcga_int92_request
        && mcga_postlude->instruction_address == 0x021f
        && mcga_postlude->ax == 1 && mcga_postlude->bx == 0xfa00
        && mcga_postlude->cx == 0 && mcga_postlude->si == 0xffff
        && mcga_postlude->di == 0x0084 && mcga_postlude->interrupt_number == 0x92
        && !mcga_postlude->local_write);

    Session mcga_single_count(mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga, 0x13);
    mcga_single_count.observe_bios_result(Result{1,0x1fc,0x10,0x0013,0,1,0,0});
    mcga_single_count.observe_bios_result(Result{2,0x0200,0x10,0x0013,0,1,0,0});
    mcga_single_count.advance_success_postlude_prefix();
    const auto mcga_single_postlude = mcga_single_count.postlude_prefix_outcome();
    assert(mcga_single_postlude
        && mcga_single_postlude->endpoint
            == eon::MillenniumDosVideoFunctionZeroPostludeEndpoint::mcga_single_count_branch
        && mcga_single_postlude->instruction_address == 0x023d
        && mcga_single_postlude->ax == 0x0013 && mcga_single_postlude->cx == 1
        && mcga_single_postlude->si == 0 && mcga_single_postlude->di == 0x0084
        && mcga_single_postlude->interrupt_number == 0);

    Session mcga_known(mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga, 0x13);
    mcga_known.observe_bios_result(Result{1,0x1fc,0x10,0xbeef,0,0,0,0});
    mcga_known.observe_bios_result(Result{2,0x200,0x10,0xabcd,0,0,0,0});
    const auto mcga_mismatch = mcga_known.outcome();
    assert(mcga_mismatch && mcga_mismatch->endpoint == Endpoint::mode_mismatch_ret_boundary
        && mcga_mismatch->instruction_address == 0x208
        && mcga_mismatch->verify_result.ax == 0xabcd
        && mcga_mismatch->ax_after_local_effect == 0 && !mcga_mismatch->ax_is_routine_return);

    expect_rejected([&] {
        Session unsupported(mcga_bytes, eon::MillenniumDosVideoDriverKind::ega640, 0xff);
    });
}
