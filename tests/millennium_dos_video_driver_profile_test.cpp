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
    const eon::MillenniumDosVideoFunctionZeroCacheWrite ega_cache_write{0x1d8,0x8c,0x0e};
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

    const auto mcga_bytes = read_driver(root / "MCGA.BIN");
    Session mcga_unknown(mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga, 0xff);
    assert(mcga_unknown.boundary()->instruction_address == 0x1f4);
    mcga_unknown.observe_bios_result(Result{1,0x1f4,0x10,0x0013,0,0,0,0});
    const eon::MillenniumDosVideoFunctionZeroCacheWrite mcga_cache_write{0x1f6,0xae,0x13};
    assert(mcga_unknown.cache_write() == mcga_cache_write);
    mcga_unknown.observe_bios_result(Result{2,0x1fc,0x10,0x0013,0,0,0,0});
    mcga_unknown.observe_bios_result(Result{3,0x200,0x10,0x8813,0,0,0,0});
    const auto mcga_match = mcga_unknown.outcome();
    assert(mcga_match && mcga_match->endpoint == Endpoint::mode_match_continuation
        && mcga_match->instruction_address == 0x209
        && mcga_match->verify_result.ax == 0x8813 && !mcga_match->ax_is_routine_return);

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
