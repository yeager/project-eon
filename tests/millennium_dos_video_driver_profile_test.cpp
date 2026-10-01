#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_video_function_31_session.hpp"
#include "engine/millennium_dos_video_function_13_session.hpp"
#include "engine/millennium_dos_video_function_13_interrupt_session.hpp"
#include "engine/millennium_dos_video_function_zero_session.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<std::uint8_t> read_driver(const std::filesystem::path& path,
    const std::size_t expected_size) {
    if (expected_size == 0 || expected_size > 4'632) {
        throw std::runtime_error("Invalid bound for Millennium DOS video-driver leaf");
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Missing real Millennium DOS video-driver leaf: " + path.filename().string());
    }
    std::vector<std::uint8_t> bytes(expected_size);
    stream.read(reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("Millennium DOS video-driver leaf changed size while being read");
    }
    char trailing_byte = 0;
    if (stream.get(trailing_byte)) {
        throw std::runtime_error("Millennium DOS video-driver leaf exceeds its bounded size");
    }
    if (!stream.eof() || stream.bad()) {
        throw std::runtime_error("Unable to finish reading Millennium DOS video-driver leaf");
    }
    return bytes;
}

struct DriverPairPaths {
    std::filesystem::path ega;
    std::filesystem::path mcga;
};

DriverPairPaths find_hash_admitted_driver_pair(const std::filesystem::path& root) {
    constexpr std::uintmax_t ega_size = 4'632;
    constexpr std::uintmax_t mcga_size = 4'366;
    constexpr std::size_t max_entries = 10'000;
    struct Candidate {
        std::filesystem::path ega;
        std::filesystem::path mcga;
    };
    std::map<std::filesystem::path, Candidate> candidates;
    std::size_t visited = 0;
    std::error_code error;
    std::filesystem::recursive_directory_iterator entry(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    if (error) throw std::runtime_error("Unable to scan direct Millennium DOS media directory");
    for (; entry != end; entry.increment(error)) {
        if (error) throw std::runtime_error("Unable to continue scanning direct Millennium DOS media directory");
        if (++visited > max_entries) {
            throw std::runtime_error("Direct Millennium DOS media scan exceeds its entry bound");
        }
        const auto status = entry->symlink_status(error);
        if (error) throw std::runtime_error("Unable to inspect direct Millennium DOS media entry");
        if (std::filesystem::is_symlink(status)) {
            if (entry->is_directory(error)) entry.disable_recursion_pending();
            if (error) throw std::runtime_error("Unable to inspect direct Millennium DOS symlink");
            continue;
        }
        if (!std::filesystem::is_regular_file(status)) continue;
        const auto name = entry->path().filename().string();
        const bool is_ega = name == "EGA640.BIN";
        const bool is_mcga = name == "MCGA.BIN";
        if (!is_ega && !is_mcga) continue;
        const auto expected_size = is_ega ? ega_size : mcga_size;
        const auto size = entry->file_size(error);
        if (error) throw std::runtime_error("Unable to inspect direct Millennium DOS driver size");
        if (size != expected_size) continue;
        auto& candidate = candidates[entry->path().parent_path()];
        (is_ega ? candidate.ega : candidate.mcga) = entry->path();
    }
    if (error) throw std::runtime_error("Unable to finish scanning direct Millennium DOS media directory");

    for (const auto& [directory, candidate] : candidates) {
        static_cast<void>(directory);
        if (candidate.ega.empty() || candidate.mcga.empty()) continue;
        auto ega_bytes = read_driver(candidate.ega, static_cast<std::size_t>(ega_size));
        auto mcga_bytes = read_driver(candidate.mcga, static_cast<std::size_t>(mcga_size));
        try {
            static_cast<void>(eon::parse_millennium_dos_video_driver(
                ega_bytes, eon::MillenniumDosVideoDriverKind::ega640));
            static_cast<void>(eon::parse_millennium_dos_video_driver(
                mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga));
            return {candidate.ega, candidate.mcga};
        } catch (const std::exception&) {
            // A collection can include unrelated files with the same leaf
            // names. Continue until both hash-bound driver identities match.
        }
    }
    throw std::runtime_error("No hash-admitted EGA640/MCGA driver pair found in direct media directory");
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
    const auto driver_paths = find_hash_admitted_driver_pair(root);
    const auto ega_bytes = read_driver(driver_paths.ega, 4'632);
    const auto mcga_bytes = read_driver(driver_paths.mcga, 4'366);
    const auto ega = eon::parse_millennium_dos_video_driver(
        ega_bytes, eon::MillenniumDosVideoDriverKind::ega640);
    const auto mcga = eon::parse_millennium_dos_video_driver(
        mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga);

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

    using Function31Session = eon::MillenniumDosVideoFunction31Session;
    using Function31State = eon::MillenniumDosVideoFunction31State;
    Function31Session ega_function_31(ega_bytes, eon::MillenniumDosVideoDriverKind::ega640);
    assert(ega_function_31.state() == Function31State::awaiting_local_state_read
        && !ega_function_31.outcome());
    expect_rejected([&] {
        ega_function_31.observe_local_state_read({0,0x0235,0x1234,0x008a,0x37});
    });
    expect_rejected([&] {
        ega_function_31.observe_local_state_read({1,0x0235,0x1234,0x008c,0x37});
    });
    assert(ega_function_31.state() == Function31State::awaiting_local_state_read
        && !ega_function_31.outcome());
    const eon::MillenniumDosVideoFunction31LocalRead ega_function_31_read{
        1,0x0235,0x1234,0x008a,0x37};
    ega_function_31.observe_local_state_read(ega_function_31_read);
    const eon::MillenniumDosVideoFunction31Outcome ega_function_31_outcome{
        0x023a,ega_function_31_read,0x0437};
    assert(ega_function_31.state() == Function31State::ret_boundary
        && ega_function_31.outcome() == ega_function_31_outcome);
    expect_rejected([&] { ega_function_31.observe_local_state_read(ega_function_31_read); });

    Function31Session mcga_function_31(mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga);
    const eon::MillenniumDosVideoFunction31LocalRead mcga_function_31_read{
        1,0x024c,0x5678,0x00ac,0xab};
    mcga_function_31.observe_local_state_read(mcga_function_31_read);
    const eon::MillenniumDosVideoFunction31Outcome mcga_function_31_outcome{
        0x0251,mcga_function_31_read,0x01ab};
    assert(mcga_function_31.state() == Function31State::ret_boundary
        && mcga_function_31.outcome() == mcga_function_31_outcome);

    using Function13Session = eon::MillenniumDosVideoFunction13Session;
    using Function13State = eon::MillenniumDosVideoFunction13State;
    Function13Session ega_function_13(ega_bytes, eon::MillenniumDosVideoDriverKind::ega640);
    const eon::MillenniumDosVideoFunction13Boundary ega_first_poll{0x0d3a,0x03da};
    assert(ega_function_13.state() == Function13State::awaiting_retrace_clear
        && ega_function_13.next_sequence() == 1
        && ega_function_13.boundary() == ega_first_poll);
    expect_rejected([&] { ega_function_13.observe_status({1,0x0d3b,0x03da,0}); });
    expect_rejected([&] { ega_function_13.observe_status({1,0x0d3a,0x03d8,0}); });
    assert(ega_function_13.reads().empty() && ega_function_13.next_sequence() == 1);
    ega_function_13.observe_status({1,0x0d3a,0x03da,0x08});
    ega_function_13.observe_status({2,0x0d3a,0x03da,0x09});
    assert(ega_function_13.state() == Function13State::awaiting_retrace_clear
        && ega_function_13.boundary() == ega_first_poll);
    ega_function_13.observe_status({3,0x0d3a,0x03da,0x01});
    const eon::MillenniumDosVideoFunction13Boundary ega_second_poll{0x0d3f,0x03da};
    assert(ega_function_13.state() == Function13State::awaiting_retrace_set
        && ega_function_13.boundary() == ega_second_poll);
    ega_function_13.observe_status({4,0x0d3f,0x03da,0x00});
    assert(ega_function_13.state() == Function13State::awaiting_retrace_set
        && ega_function_13.boundary() == ega_second_poll);
    ega_function_13.observe_status({5,0x0d3f,0x03da,0x08});
    const auto ega_retrace = ega_function_13.outcome();
    assert(ega_function_13.state() == Function13State::ret_boundary
        && !ega_function_13.boundary() && ega_retrace
        && ega_retrace->ret_instruction_address == 0x0d44
        && ega_retrace->reads.size() == 5 && ega_function_13.next_sequence() == 6);
    expect_rejected([&] { ega_function_13.observe_status({6,0x0d3f,0x03da,0x08}); });

    Function13Session mcga_function_13(mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga);
    const eon::MillenniumDosVideoFunction13Boundary mcga_first_poll{0x0908,0x03da};
    assert(mcga_function_13.boundary() == mcga_first_poll);
    mcga_function_13.observe_status({1,0x0908,0x03da,0});
    mcga_function_13.observe_status({2,0x090d,0x03da,0x08});
    const auto mcga_retrace = mcga_function_13.outcome();
    assert(mcga_function_13.state() == Function13State::ret_boundary
        && mcga_retrace && mcga_retrace->ret_instruction_address == 0x0912
        && mcga_retrace->reads.size() == 2);

    using Function13Interrupt = eon::MillenniumDosVideoFunction13InterruptSession;
    using Function13InterruptState = eon::MillenniumDosVideoFunction13InterruptState;
    Function13Interrupt ega_interrupt(ega_bytes, eon::MillenniumDosVideoDriverKind::ega640, 0x2345);
    expect_rejected([&] { ega_interrupt.observe_interrupt_request({1,0x0127,0x0006,0x0129,0x4567,0x0202}); });
    assert(ega_interrupt.state() == Function13InterruptState::awaiting_interrupt_request
        && ega_interrupt.next_sequence() == 1);
    ega_interrupt.observe_interrupt_request({1,0x0127,0x0013,0x0129,0x4567,0x0202});
    assert(ega_interrupt.boundary() == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d3a});
    ega_interrupt.observe_port_read({2,0x0d3a,0x03da,0x00});
    ega_interrupt.observe_port_read({3,0x0d3f,0x03da,0x08});
    assert(ega_interrupt.state() == Function13InterruptState::iret_boundary
        && ega_interrupt.boundary() == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0012});
    ega_interrupt.execute_iret(4,0x0012);
    const auto ega_interrupt_outcome = ega_interrupt.outcome();
    assert(ega_interrupt.state() == Function13InterruptState::returned
        && ega_interrupt_outcome && ega_interrupt_outcome->ax == 0x0008
        && ega_interrupt_outcome->dx == 0x03da && ega_interrupt_outcome->di == 0x0026
        && ega_interrupt_outcome->ds == 0x2345
        && ega_interrupt_outcome->return_ip == 0x0129
        && ega_interrupt_outcome->return_cs == 0x4567
        && ega_interrupt_outcome->return_flags == 0x0202);

    Function13Interrupt mcga_interrupt(mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga, 0x3456);
    mcga_interrupt.observe_interrupt_request({1,0x0127,0x0013,0x0129,0x5678,0x0302});
    mcga_interrupt.observe_port_read({2,0x0908,0x03da,0x00});
    mcga_interrupt.observe_port_read({3,0x090d,0x03da,0x08});
    assert(mcga_interrupt.state() == Function13InterruptState::awaiting_mcga_postlude_byte
        && mcga_interrupt.boundary() == eon::MillenniumDosVideoFunction13InterruptBoundary{0x001a});
    expect_rejected([&] { mcga_interrupt.observe_mcga_postlude_byte({4,0x001a,0x9999,0x01e5,0}); });
    assert(mcga_interrupt.next_sequence() == 4);
    mcga_interrupt.observe_mcga_postlude_byte({4,0x001a,0x3456,0x01e5,0});
    assert(mcga_interrupt.state() == Function13InterruptState::iret_boundary
        && mcga_interrupt.boundary() == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0020});
    mcga_interrupt.execute_iret(5,0x0020);
    const auto mcga_interrupt_outcome = mcga_interrupt.outcome();
    assert(mcga_interrupt_outcome && mcga_interrupt_outcome->ax == 0x0008
        && mcga_interrupt_outcome->ds == 0x3456
        && mcga_interrupt_outcome->return_ip == 0x0129
        && mcga_interrupt_outcome->return_cs == 0x5678
        && mcga_interrupt_outcome->return_flags == 0x0302
        && mcga_interrupt_outcome->memory_effects.size() == 1
        && mcga_interrupt_outcome->memory_effects.front()
            == (eon::MillenniumDosVideoFunction13DriverByteEffect{0x0012,0x3456,0x01e4,0}));

    Function13Interrupt mcga_callback(mcga_bytes, eon::MillenniumDosVideoDriverKind::mcga, 0x3456);
    mcga_callback.observe_interrupt_request({1,0x0127,0x0013,0x0129,0x5678,0x0302});
    mcga_callback.observe_port_read({2,0x0908,0x03da,0x00});
    mcga_callback.observe_port_read({3,0x090d,0x03da,0x08});
    mcga_callback.observe_mcga_postlude_byte({4,0x001a,0x3456,0x01e5,1});
    assert(mcga_callback.state() == Function13InterruptState::callback_boundary
        && mcga_callback.boundary() == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d22}
        && !mcga_callback.outcome());
    expect_rejected([&] { mcga_callback.execute_iret(5,0x0020); });
    expect_rejected([&] { mcga_callback.observe_mcga_callback_read({5,0x0d22,0x9999,0x0c88,0,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte}); });
    expect_rejected([&] { mcga_callback.observe_mcga_callback_read({5,0x0d23,0x3456,0x0c88,0,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte}); });
    assert(mcga_callback.next_sequence() == 5 && mcga_callback.callback_reads().empty());
    mcga_callback.observe_mcga_callback_read({5,0x0d22,0x3456,0x0c88,1,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    assert(mcga_callback.state() == Function13InterruptState::callback_local_boundary
        && mcga_callback.boundary() == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0c94}
        && mcga_callback.callback_reads().size() == 1);
    expect_rejected([&] { mcga_callback.observe_mcga_callback_counter({7,0x0c9a,0x3456,0x0c92,1,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word}); });
    expect_rejected([&] { mcga_callback.observe_mcga_callback_counter({6,0x0c9a,0x3456,0x0c90,1,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word}); });
    assert(mcga_callback.next_sequence() == 6 && mcga_callback.callback_reads().size() == 1
        && mcga_callback.callback_driver_effects().empty()
        && mcga_callback.callback_driver_word_effects().empty());
    mcga_callback.observe_mcga_callback_counter({6,0x0c9a,0x3456,0x0c92,1,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback.state() == Function13InterruptState::callback_local_boundary
        && mcga_callback.boundary() == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d10}
        && mcga_callback.callback_driver_word_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13DriverWordEffect>{{0x0d11,0x3456,0x0c92,0}}));
    assert(mcga_callback.callback_driver_effects()
        == (std::vector<eon::MillenniumDosVideoFunction13DriverByteEffect>{
            {0x0c94,0x3456,0x01e4,1},
            {0x0d04,0x3456,0x01e4,0},
            {0x0d0a,0x3456,0x01e5,0}}));
    expect_rejected([&] { mcga_callback.execute_iret(7,0x0020); });
    mcga_callback.execute_mcga_callback_return(7,0x0d10);
    assert(mcga_callback.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0024});
    mcga_callback.execute_iret(8,0x0024);
    assert(mcga_callback.state() == Function13InterruptState::returned);

    Function13Interrupt mcga_callback_empty_counter(mcga_bytes,
        eon::MillenniumDosVideoDriverKind::mcga, 0x3456);
    mcga_callback_empty_counter.observe_interrupt_request({1,0x0127,0x0013,0x0129,0x5678,0x0302});
    mcga_callback_empty_counter.observe_port_read({2,0x0908,0x03da,0x00});
    mcga_callback_empty_counter.observe_port_read({3,0x090d,0x03da,0x08});
    mcga_callback_empty_counter.observe_mcga_postlude_byte({4,0x001a,0x3456,0x01e5,1});
    mcga_callback_empty_counter.observe_mcga_callback_read({5,0x0d22,0x3456,0x0c88,1,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_empty_counter.observe_mcga_callback_counter({6,0x0c9a,0x3456,0x0c92,0,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_empty_counter.state() == Function13InterruptState::callback_local_boundary
        && mcga_callback_empty_counter.boundary()
            == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0ca2}
        && mcga_callback_empty_counter.callback_driver_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13DriverByteEffect>{{0x0c94,0x3456,0x01e4,1}})
        && mcga_callback_empty_counter.callback_driver_word_effects().empty());
    Function13Interrupt mcga_callback_wrap_counter(mcga_bytes,
        eon::MillenniumDosVideoDriverKind::mcga, 0x3456);
    mcga_callback_wrap_counter.observe_interrupt_request({1,0x0127,0x0013,0x0129,0x5678,0x0302});
    mcga_callback_wrap_counter.observe_port_read({2,0x0908,0x03da,0x00});
    mcga_callback_wrap_counter.observe_port_read({3,0x090d,0x03da,0x08});
    mcga_callback_wrap_counter.observe_mcga_postlude_byte({4,0x001a,0x3456,0x01e5,1});
    mcga_callback_wrap_counter.observe_mcga_callback_read({5,0x0d22,0x3456,0x0c88,1,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_wrap_counter.observe_mcga_callback_counter({6,0x0c9a,0x3456,0x0c92,0xffff,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_wrap_counter.callback_driver_word_effects()
        == (std::vector<eon::MillenniumDosVideoFunction13DriverWordEffect>{{0x0d11,0x3456,0x0c92,0xfffe}})
        && mcga_callback_wrap_counter.boundary()
            == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d10});

    Function13Interrupt mcga_callback_zero(mcga_bytes,
        eon::MillenniumDosVideoDriverKind::mcga, 0x3456);
    mcga_callback_zero.observe_interrupt_request({1,0x0127,0x0013,0x0129,0x5678,0x0302});
    mcga_callback_zero.observe_port_read({2,0x0908,0x03da,0x00});
    mcga_callback_zero.observe_port_read({3,0x090d,0x03da,0x08});
    mcga_callback_zero.observe_mcga_postlude_byte({4,0x001a,0x3456,0x01e5,1});
    mcga_callback_zero.observe_mcga_callback_read({5,0x0d22,0x3456,0x0c88,0,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    assert(mcga_callback_zero.state() == Function13InterruptState::awaiting_mcga_callback_word
        && mcga_callback_zero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d2d});
    expect_rejected([&] {
        mcga_callback_zero.observe_mcga_callback_read({6,0x0d2d,0x3456,0x0d18,0,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    });
    assert(mcga_callback_zero.next_sequence() == 6
        && mcga_callback_zero.callback_reads().size() == 1);
    mcga_callback_zero.observe_mcga_callback_read({6,0x0d2d,0x3456,0x0d18,0,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_zero.state() == Function13InterruptState::callback_local_boundary
        && mcga_callback_zero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d10});
    mcga_callback_zero.execute_mcga_callback_return(7,0x0d10);
    mcga_callback_zero.execute_iret(8,0x0024);
    assert(mcga_callback_zero.state() == Function13InterruptState::returned);

    Function13Interrupt mcga_callback_nonzero(mcga_bytes,
        eon::MillenniumDosVideoDriverKind::mcga, 0x3456);
    mcga_callback_nonzero.observe_interrupt_request({1,0x0127,0x0013,0x0129,0x5678,0x0302});
    mcga_callback_nonzero.observe_port_read({2,0x0908,0x03da,0x00});
    mcga_callback_nonzero.observe_port_read({3,0x090d,0x03da,0x08});
    mcga_callback_nonzero.observe_mcga_postlude_byte({4,0x001a,0x3456,0x01e5,1});
    mcga_callback_nonzero.observe_mcga_callback_read({5,0x0d22,0x3456,0x0c88,0,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_nonzero.observe_mcga_callback_read({6,0x0d2d,0x3456,0x0d18,1,
        eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_nonzero.state() == Function13InterruptState::callback_local_boundary
        && mcga_callback_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d35});
    expect_rejected([&] { mcga_callback_nonzero.execute_mcga_callback_alternate_flag(7,0x0d36); });
    assert(mcga_callback_nonzero.next_sequence() == 7
        && mcga_callback_nonzero.callback_driver_effects().empty());
    mcga_callback_nonzero.execute_mcga_callback_alternate_flag(7,0x0d35);
    assert(mcga_callback_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d3b}
        && mcga_callback_nonzero.callback_driver_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13DriverByteEffect>{{0x0d35,0x3456,0x01e4,1}}));
    expect_rejected([&] { mcga_callback_nonzero.execute_mcga_callback_register_saves(
        {8,0x0d3c,0x4000,0x0008,0x1111,0x2222,0x3333,0x4444,0x5555,0x6666,0x7777,0x8888,0x9999}); });
    assert(mcga_callback_nonzero.next_sequence() == 8
        && mcga_callback_nonzero.callback_stack_effects().empty());
    mcga_callback_nonzero.execute_mcga_callback_register_saves(
        {8,0x0d3b,0x4000,0x0008,0x1111,0x2222,0x3333,0x4444,0x5555,0x6666,0x7777,0x8888,0x9999});
    assert(mcga_callback_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d44}
        && mcga_callback_nonzero.callback_stack_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13CallbackStackWordEffect>{
                {0x0d3b,0x4000,0x0006,0x1111},
                {0x0d3c,0x4000,0x0004,0x2222},
                {0x0d3d,0x4000,0x0002,0x3333},
                {0x0d3e,0x4000,0x0000,0x4444},
                {0x0d3f,0x4000,0xfffe,0x5555},
                {0x0d40,0x4000,0xfffc,0x6666},
                {0x0d41,0x4000,0xfffa,0x7777},
                {0x0d42,0x4000,0xfff8,0x8888},
                {0x0d43,0x4000,0xfff6,0x9999}}));
    expect_rejected([&] { mcga_callback_nonzero.observe_mcga_callback_alternate_cx_read(
        {9,0x0d44,0x3456,0x0d1a,1,eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word}); });
    assert(mcga_callback_nonzero.next_sequence() == 9
        && mcga_callback_nonzero.callback_reads().size() == 2
        && mcga_callback_nonzero.callback_register_effects().empty());
    auto mcga_callback_repeated_loop = mcga_callback_nonzero;
    mcga_callback_nonzero.observe_mcga_callback_alternate_cx_read(
        {9,0x0d44,0x3456,0x0d18,1,eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d49}
        && mcga_callback_nonzero.callback_register_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect>{
                {0x0d44,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,1}}));
    expect_rejected([&] { mcga_callback_nonzero.observe_mcga_callback_alternate_pointer(
        {10,0x0d49,0x3456,0x0d1c,0x2000,0xfffc}); });
    assert(mcga_callback_nonzero.next_sequence() == 10
        && mcga_callback_nonzero.callback_far_pointer_reads().empty());
    mcga_callback_nonzero.observe_mcga_callback_alternate_pointer(
        {10,0x0d49,0x3456,0x0d1a,0x2000,0xfffc});
    assert(mcga_callback_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d4e}
        && mcga_callback_nonzero.callback_register_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect>{
                {0x0d44,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,1},
                {0x0d49,eon::MillenniumDosVideoFunction13McgaCallbackRegister::ds,0x2000},
                {0x0d49,eon::MillenniumDosVideoFunction13McgaCallbackRegister::si,0xfffc}}));
    auto mcga_callback_alternate_nonzero = mcga_callback_nonzero;
    mcga_callback_repeated_loop.observe_mcga_callback_alternate_cx_read(
        {9,0x0d44,0x3456,0x0d18,2,eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    mcga_callback_repeated_loop.observe_mcga_callback_alternate_pointer(
        {10,0x0d49,0x3456,0x0d1a,0x2000,0xfffc});
    mcga_callback_repeated_loop.observe_mcga_callback_alternate_indirect_word(
        {11,0x0d4e,0x2000,0x0004,1});
    mcga_callback_repeated_loop.execute_mcga_callback_loop_iteration(12,0x0dad);
    assert(mcga_callback_repeated_loop.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d4e}
        && mcga_callback_repeated_loop.callback_memory_word_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect>{{
                0x0dad,0x2000,0x0004,0}})
        && mcga_callback_repeated_loop.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0db3,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,1}));
    mcga_callback_repeated_loop.observe_mcga_callback_alternate_indirect_word(
        {13,0x0d4e,0x2000,0x0010,0});
    assert(mcga_callback_repeated_loop.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d54});
    expect_rejected([&] { mcga_callback_nonzero.observe_mcga_callback_alternate_indirect_word(
        {11,0x0d4e,0x2000,0x0005,0}); });
    assert(mcga_callback_nonzero.next_sequence() == 11
        && mcga_callback_nonzero.callback_indirect_word_reads().empty());
    mcga_callback_nonzero.observe_mcga_callback_alternate_indirect_word(
        {11,0x0d4e,0x2000,0x0004,0});
    mcga_callback_alternate_nonzero.observe_mcga_callback_alternate_indirect_word(
        {11,0x0d4e,0x2000,0x0004,1});
    assert(mcga_callback_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d54}
        && mcga_callback_nonzero.callback_indirect_word_reads()
            == (std::vector<eon::MillenniumDosVideoFunction13McgaCallbackIndirectWordRead>{
                {11,0x0d4e,0x2000,0x0004,0}})
        && mcga_callback_alternate_nonzero.boundary()
            == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0dad});
    expect_rejected([&] { mcga_callback_nonzero.observe_mcga_callback_alternate_video_pointer(
        {12,0x0d54,0x3456,0x0d1c,0x3000,0x4100}); });
    assert(mcga_callback_nonzero.next_sequence() == 12
        && mcga_callback_nonzero.callback_register_effects().size() == 3);
    mcga_callback_nonzero.observe_mcga_callback_alternate_video_pointer(
        {12,0x0d54,0x3456,0x0d1e,0x3000,0xfffa});
    assert(mcga_callback_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d59}
        && mcga_callback_nonzero.callback_far_pointer_reads()
            == (std::vector<eon::MillenniumDosVideoFunction13McgaCallbackFarPointerRead>{
                {10,0x0d49,0x3456,0x0d1a,0x2000,0xfffc},
                {12,0x0d54,0x3456,0x0d1e,0x3000,0xfffa}})
        && mcga_callback_nonzero.callback_register_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect>{
                {0x0d44,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,1},
                {0x0d49,eon::MillenniumDosVideoFunction13McgaCallbackRegister::ds,0x2000},
                {0x0d49,eon::MillenniumDosVideoFunction13McgaCallbackRegister::si,0xfffc},
                {0x0d54,eon::MillenniumDosVideoFunction13McgaCallbackRegister::es,0x3000},
                {0x0d54,eon::MillenniumDosVideoFunction13McgaCallbackRegister::di,0xfffa}}));
    auto mcga_callback_selector_zero = mcga_callback_nonzero;
    auto mcga_callback_selector_one = mcga_callback_nonzero;
    auto mcga_callback_selector_three = mcga_callback_nonzero;
    auto mcga_callback_selector_four = mcga_callback_nonzero;
    expect_rejected([&] { mcga_callback_nonzero.observe_mcga_callback_alternate_selector_byte(
        {13,0x0d59,0x2001,0xfffc,2,eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte}); });
    assert(mcga_callback_nonzero.next_sequence() == 13
        && mcga_callback_nonzero.callback_reads().size() == 3);
    const auto observe_selector_byte = [](auto& session, const std::uint16_t value) {
        session.observe_mcga_callback_alternate_selector_byte(
            {13,0x0d59,0x2000,0xfffc,value,
                eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    };
    observe_selector_byte(mcga_callback_nonzero,2);
    observe_selector_byte(mcga_callback_selector_zero,0);
    observe_selector_byte(mcga_callback_selector_one,1);
    observe_selector_byte(mcga_callback_selector_three,3);
    observe_selector_byte(mcga_callback_selector_four,4);
    assert(mcga_callback_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0dcb}
        && mcga_callback_selector_zero.boundary()
            == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0dad}
        && mcga_callback_selector_one.boundary()
            == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0dcd}
        && mcga_callback_selector_three.boundary()
            == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0dcb}
        && mcga_callback_selector_four.boundary()
            == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0dad}
        && mcga_callback_nonzero.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0d59,eon::MillenniumDosVideoFunction13McgaCallbackRegister::al,2}));
    mcga_callback_nonzero.execute_mcga_callback_copy_route(14,0x0dcb);
    assert(mcga_callback_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e46});
    auto mcga_callback_copy = mcga_callback_selector_three;
    mcga_callback_copy.execute_mcga_callback_copy_route(14,0x0dcb);
    const auto trace_epilogue = std::getenv("EON_TRACE_MCGA_EPILOGUE");
    if (trace_epilogue != nullptr) {
        const auto start = mcga_bytes.data() + 0x0e8f;
        std::uint64_t h = 14695981039346656037ULL;
        for (std::size_t i = 0; i < 13; ++i) {
            h = (h ^ start[i]) * 1099511628211ULL;
        }
        static_cast<void>(h);
    }
    expect_rejected([&] { mcga_callback_copy.observe_mcga_callback_copy_height(
        {16,0x0e49,0x2000,0xfffe,0xff,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte}); });
    expect_rejected([&] { mcga_callback_copy.observe_mcga_callback_copy_height(
        {15,0x0e49,0x2000,0xffff,0xff,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte}); });
    assert(mcga_callback_copy.next_sequence() == 15
        && mcga_callback_copy.callback_reads().size() == 4);
    mcga_callback_copy.observe_mcga_callback_copy_height(
        {15,0x0e49,0x2000,0xfffe,0xff,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    expect_rejected([&] { mcga_callback_copy.observe_mcga_callback_copy_width(
        {16,0x0e55,0x2000,0xfffd,2,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word}); });
    mcga_callback_copy.observe_mcga_callback_copy_width(
        {16,0x0e55,0x2000,0xfffd,2,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    expect_rejected([&] { mcga_callback_copy.observe_mcga_callback_copy_limit(
        {17,0x0e62,0x2000,0x0003,0x0040,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word}); });
    mcga_callback_copy.observe_mcga_callback_copy_limit(
        {17,0x0e62,0x2000,0x0002,0x0040,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    const auto& copy_registers = mcga_callback_copy.callback_register_effects();
    assert(mcga_callback_copy.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e71}
        && mcga_callback_copy.next_sequence() == 18
        && mcga_callback_copy.callback_reads().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRead{17,0x0e62,0x2000,0x0002,0x0040,
                eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word})
        && copy_registers[copy_registers.size() - 1]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e6f,eon::MillenniumDosVideoFunction13McgaCallbackRegister::ds,0x3000})
        && copy_registers[copy_registers.size() - 2]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e6d,eon::MillenniumDosVideoFunction13McgaCallbackRegister::ax,0x3000})
        && copy_registers[copy_registers.size() - 3]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e6b,eon::MillenniumDosVideoFunction13McgaCallbackRegister::di,0x0906})
        && copy_registers[copy_registers.size() - 4]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e67,eon::MillenniumDosVideoFunction13McgaCallbackRegister::di,0x0900})
        && copy_registers[copy_registers.size() - 5]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e65,eon::MillenniumDosVideoFunction13McgaCallbackRegister::si,0x0000})
        && copy_registers[copy_registers.size() - 6]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e62,eon::MillenniumDosVideoFunction13McgaCallbackRegister::dx,0x0040})
        && copy_registers[copy_registers.size() - 7]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e60,eon::MillenniumDosVideoFunction13McgaCallbackRegister::di,0x0000})
        && copy_registers[copy_registers.size() - 8]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e5e,eon::MillenniumDosVideoFunction13McgaCallbackRegister::ax,0x0006})
        && copy_registers[copy_registers.size() - 9]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e5c,eon::MillenniumDosVideoFunction13McgaCallbackRegister::ax,0x0004})
        && copy_registers[copy_registers.size() - 10]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e5a,eon::MillenniumDosVideoFunction13McgaCallbackRegister::dx,0x0002})
        && copy_registers[copy_registers.size() - 11]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e58,eon::MillenniumDosVideoFunction13McgaCallbackRegister::ax,0x0002})
        && copy_registers[copy_registers.size() - 12]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e55,eon::MillenniumDosVideoFunction13McgaCallbackRegister::al,0x0002})
        && copy_registers[copy_registers.size() - 13]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e53,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,0x0300}));
    auto mcga_callback_copy_overflow = mcga_callback_copy;
    mcga_callback_copy_overflow.observe_mcga_callback_copy_source_byte(
        {18,0x0e71,0x3000,0x0600,2,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_copy_overflow.observe_mcga_callback_copy_destination_word(
        {19,0x0e77,0x3000,0x0906,0xffff,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_copy_overflow.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e7f}
        && mcga_callback_copy_overflow.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e77,eon::MillenniumDosVideoFunction13McgaCallbackRegister::ax,1}));

    auto mcga_callback_copy_direct = mcga_callback_copy;
    mcga_callback_copy_direct.observe_mcga_callback_copy_source_byte(
        {18,0x0e71,0x3000,0x0600,0x10,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_copy_direct.observe_mcga_callback_copy_destination_word(
        {19,0x0e77,0x3000,0x0906,0x10,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_copy_direct.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e8b});
    mcga_callback_copy_direct.execute_mcga_callback_copy_store(20,0x0e8b);
    assert(mcga_callback_copy_direct.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e71}
        && mcga_callback_copy_direct.callback_memory_word_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect{
                0x0e8b,0x3000,0x0906,0x0020})
        && mcga_callback_copy_direct.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e8d,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,0x02ff}));

    auto mcga_callback_copy_complete = mcga_callback_selector_three;
    mcga_callback_copy_complete.execute_mcga_callback_copy_route(14,0x0dcb);
    mcga_callback_copy_complete.observe_mcga_callback_copy_height(
        {15,0x0e49,0x2000,0xfffe,0,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_copy_complete.observe_mcga_callback_copy_width(
        {16,0x0e55,0x2000,0xfffd,0,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_copy_complete.observe_mcga_callback_copy_limit(
        {17,0x0e62,0x2000,0x0002,1,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    std::uint64_t copy_sequence = 18;
    std::uint16_t copy_si = 0xfffa;
    std::uint16_t copy_di = 0x08fa;
    for (std::uint16_t pixel = 0; pixel < 3; ++pixel) {
        mcga_callback_copy_complete.observe_mcga_callback_copy_source_byte(
            {copy_sequence++,0x0e71,0x3000,
                static_cast<std::uint16_t>(copy_si + 0x0600U),0,
                eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
        mcga_callback_copy_complete.observe_mcga_callback_copy_destination_word(
            {copy_sequence++,0x0e77,0x3000,copy_di,0,
                eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
        mcga_callback_copy_complete.execute_mcga_callback_copy_store(copy_sequence++,0x0e8b);
        copy_si = static_cast<std::uint16_t>(copy_si + 1U);
        copy_di = static_cast<std::uint16_t>(copy_di + 2U);
    }
    assert(mcga_callback_copy_complete.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e8f}
        && mcga_callback_copy_complete.next_sequence() == 27
        && mcga_callback_copy_complete.callback_memory_word_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect{
                0x0e8b,0x3000,0x08fe,0})
        && mcga_callback_copy_complete.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e8d,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,0}));
    auto mcga_callback_copy_epilogue_nonzero = mcga_callback_copy_complete;
    mcga_callback_copy_epilogue_nonzero.observe_mcga_callback_epilogue_stack_read(
        {27,0x0e8f,0x6000,0xfffa,0x2000});
    mcga_callback_copy_epilogue_nonzero.observe_mcga_callback_epilogue_stack_read(
        {28,0x0e90,0x6000,0xfffc,0x4000});
    mcga_callback_copy_epilogue_nonzero.observe_mcga_callback_epilogue_descriptor(
        {29,0x0e91,0x4000,0x200a,2,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_copy_epilogue_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e99}
        && mcga_callback_copy_epilogue_nonzero.callback_memory_byte_effects().empty());
    mcga_callback_copy_epilogue_nonzero.execute_mcga_callback_epilogue_jump(30,0x0e99);
    assert(mcga_callback_copy_epilogue_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d6a});
    auto mcga_callback_epilogue_wrapped_stack = mcga_callback_copy_complete;
    mcga_callback_epilogue_wrapped_stack.observe_mcga_callback_epilogue_stack_read(
        {27,0x0e8f,0x6000,0xfffe,0x2000});
    mcga_callback_epilogue_wrapped_stack.observe_mcga_callback_epilogue_stack_read(
        {28,0x0e90,0x6000,0x0000,0x4000});
    mcga_callback_epilogue_wrapped_stack.observe_mcga_callback_epilogue_descriptor(
        {29,0x0e91,0x4000,0x200a,2,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_epilogue_wrapped_stack.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e99}
        && mcga_callback_epilogue_wrapped_stack.callback_stack_effects().back()
            == (eon::MillenniumDosVideoFunction13CallbackStackWordEffect{
                0x0e90,0x6000,0x0000,0x4000}));
    expect_rejected([&] { mcga_callback_copy_complete.observe_mcga_callback_epilogue_stack_read(
        {26,0x0e8f,0x6000,0xfffa,0x2000}); });
    mcga_callback_copy_complete.observe_mcga_callback_epilogue_stack_read(
        {27,0x0e8f,0x6000,0xfffa,0x2000});
    expect_rejected([&] { mcga_callback_copy_complete.observe_mcga_callback_epilogue_stack_read(
        {28,0x0e90,0x6000,0xfffe,0x4000}); });
    mcga_callback_copy_complete.observe_mcga_callback_epilogue_stack_read(
        {28,0x0e90,0x6000,0xfffc,0x4000});
    mcga_callback_copy_complete.observe_mcga_callback_epilogue_descriptor(
        {29,0x0e91,0x4000,0x200a,1,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_copy_complete.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e96}
        && mcga_callback_copy_complete.callback_memory_word_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect{
                0x0e91,0x4000,0x200a,0}));
    mcga_callback_copy_complete.execute_mcga_callback_epilogue_clear(30,0x0e96);
    mcga_callback_copy_complete.execute_mcga_callback_epilogue_jump(31,0x0e99);
    assert(mcga_callback_copy_complete.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d6a}
        && mcga_callback_copy_complete.callback_memory_byte_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryByteEffect{
                0x0e96,0x4000,0x2000,0}));

    auto mcga_callback_palette_index = mcga_callback_copy_complete;
    mcga_callback_palette_index.observe_mcga_callback_palette_descriptor_count(
        {32,0x0d6a,0x4000,0x2002,2,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_palette_index.observe_mcga_callback_palette_descriptor_index(
        {33,0x0d6d,0x4000,0x2003,0,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_palette_index.execute_mcga_callback_palette_prefix(34,0x0d79);
    mcga_callback_palette_index.observe_mcga_callback_palette_source_index(
        {35,0x0d7c,0x4000,0x2001,7,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d82}
        && mcga_callback_palette_index.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0d7c,eon::MillenniumDosVideoFunction13McgaCallbackRegister::al,7})
        && mcga_callback_palette_index.callback_register_effects()[
            mcga_callback_palette_index.callback_register_effects().size() - 3]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0d79,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,3})
        && mcga_callback_palette_index.callback_memory_byte_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryByteEffect{
                0x0d6d,0x4000,0x2003,1})
        && mcga_callback_palette_index.callback_register_effects()[
            mcga_callback_palette_index.callback_register_effects().size() - 2]
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0d7f,eon::MillenniumDosVideoFunction13McgaCallbackRegister::dx,0x03c8}));
    expect_rejected([&] { mcga_callback_palette_index.observe_mcga_callback_palette_retrace(
        {36,0x0d94,0x03da,1}); });
    mcga_callback_palette_index.execute_mcga_callback_palette_out(36,0x0d82);
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d94}
        && mcga_callback_palette_index.callback_port_writes()
            == (std::vector<eon::MillenniumDosVideoFunction13McgaCallbackPortWrite>{
                {36,0x0d82,0x03c8,7}}));
    mcga_callback_palette_index.observe_mcga_callback_palette_retrace({37,0x0d94,0x03da,0});
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d94});
    mcga_callback_palette_index.observe_mcga_callback_palette_retrace({38,0x0d94,0x03da,1});
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d87}
        && mcga_callback_palette_index.callback_port_reads()
            == (std::vector<eon::MillenniumDosVideoFunction13PortRead>{
                {37,0x0d94,0x03da,0},{38,0x0d94,0x03da,1}}));
    expect_rejected([&] { mcga_callback_palette_index.observe_mcga_callback_palette_source_pointer(
        {39,0x0d87,0x3456,0x0d1c,0x5000,0x0100}); });
    mcga_callback_palette_index.observe_mcga_callback_palette_source_pointer(
        {39,0x0d87,0x3456,0x0d1e,0x5000,0x0100});
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d9b}
        && mcga_callback_palette_index.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0d8c,eon::MillenniumDosVideoFunction13McgaCallbackRegister::si,0x0115}));
    expect_rejected([&] { mcga_callback_palette_index.observe_mcga_callback_palette_source_byte(
        {40,0x0d9b,0x5000,0x0116,0xaa,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte}); });
    mcga_callback_palette_index.observe_mcga_callback_palette_source_byte(
        {40,0x0d9b,0x5000,0x0115,0xaa,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_palette_index.execute_mcga_callback_palette_data_out(41,0x0d9c);
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d9d}
        && mcga_callback_palette_index.callback_port_writes().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackPortWrite{
                41,0x0d9c,0x03c9,0xaa}));
    mcga_callback_palette_index.observe_mcga_callback_palette_source_byte(
        {42,0x0d9d,0x5000,0x0116,0xbb,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_palette_index.execute_mcga_callback_palette_data_out(43,0x0d9e);
    mcga_callback_palette_index.observe_mcga_callback_palette_source_byte(
        {44,0x0d9f,0x5000,0x0117,0xcc,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_palette_index.execute_mcga_callback_palette_data_out(45,0x0da0);
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0da1}
        && mcga_callback_palette_index.callback_port_writes().size() == 4);
    mcga_callback_palette_index.execute_mcga_callback_palette_loop(46,0x0da1);
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d9b}
        && mcga_callback_palette_index.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0da1,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,2}));
    std::uint64_t palette_sequence = 47;
    std::uint16_t palette_source_offset = 0x0118;
    for (int row = 0; row < 2; ++row) {
        for (int component = 0; component < 3; ++component) {
            const auto read_ip = static_cast<std::uint16_t>(0x0d9b + 2 * component);
            mcga_callback_palette_index.observe_mcga_callback_palette_source_byte(
                {palette_sequence++,read_ip,0x5000,palette_source_offset++,
                    static_cast<std::uint16_t>(0x20 + row * 3 + component),
                    eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
            mcga_callback_palette_index.execute_mcga_callback_palette_data_out(
                palette_sequence++,static_cast<std::uint16_t>(read_ip + 1));
        }
        mcga_callback_palette_index.execute_mcga_callback_palette_loop(palette_sequence++,0x0da1);
    }
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0da3});
    mcga_callback_palette_index.observe_mcga_callback_palette_stack_read(
        {palette_sequence++,0x0da3,0x7000,0x1000,0x2222});
    mcga_callback_palette_index.observe_mcga_callback_palette_stack_read(
        {palette_sequence++,0x0da4,0x7000,0x1002,0x3333});
    mcga_callback_palette_index.observe_mcga_callback_palette_descriptor_word(
        {palette_sequence++,0x0da5,0x3333,0x2226,0x0007,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    mcga_callback_palette_index.observe_mcga_callback_palette_loop_count_stack(
        {palette_sequence++,0x0dac,0x7000,0x1004,1});
    mcga_callback_palette_index.execute_mcga_callback_palette_descriptor_loop(
        palette_sequence++,0x0dad);
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0db5}
        && mcga_callback_palette_index.callback_memory_word_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect{
                0x0dad,0x3333,0x222a,7}));
    for (std::uint16_t pop = 0; pop < 9; ++pop) {
        if (pop == 0) {
            expect_rejected([&] { mcga_callback_palette_index.observe_mcga_callback_return_stack_read(
                {palette_sequence,0x0db5,0x7000,0x1008,0xa0}); });
        }
        mcga_callback_palette_index.observe_mcga_callback_return_stack_read(
            {palette_sequence++,static_cast<std::uint16_t>(0x0db5 + pop),
                0x7000,static_cast<std::uint16_t>(0x1006 + 2 * pop),
                static_cast<std::uint16_t>(0xa0 + pop)});
    }
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0dbe});
    mcga_callback_palette_index.execute_mcga_callback_return_flag_clear(palette_sequence++,0x0dbe);
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0dca}
        && mcga_callback_palette_index.callback_driver_effects().end()[-2]
            == (eon::MillenniumDosVideoFunction13DriverByteEffect{0x0dbe,0x3456,0x01e4,0})
        && mcga_callback_palette_index.callback_driver_effects().back()
            == (eon::MillenniumDosVideoFunction13DriverByteEffect{0x0dc4,0x3456,0x01e5,0}));
    expect_rejected([&] { mcga_callback_palette_index.execute_iret(palette_sequence,0x0024); });
    mcga_callback_palette_index.execute_mcga_callback_return(palette_sequence++,0x0dca);
    assert(mcga_callback_palette_index.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0024});
    mcga_callback_palette_index.execute_iret(palette_sequence++,0x0024);
    assert(mcga_callback_palette_index.state() == Function13InterruptState::returned
        && mcga_callback_palette_index.outcome()
        && mcga_callback_palette_index.outcome()->return_ip == 0x0129);

    auto mcga_callback_palette_wrap = mcga_callback_copy_complete;
    mcga_callback_palette_wrap.observe_mcga_callback_palette_descriptor_count(
        {32,0x0d6a,0x4000,0x2002,1,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_palette_wrap.observe_mcga_callback_palette_descriptor_index(
        {33,0x0d6d,0x4000,0x2003,1,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    assert(mcga_callback_palette_wrap.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d75});
    mcga_callback_palette_wrap.execute_mcga_callback_palette_descriptor_clear(34,0x0d75);
    mcga_callback_palette_wrap.execute_mcga_callback_palette_prefix(35,0x0d79);
    mcga_callback_palette_wrap.observe_mcga_callback_palette_source_index(
        {36,0x0d7c,0x4000,0x2001,9,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    assert(mcga_callback_palette_wrap.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0d82}
        && mcga_callback_palette_wrap.callback_memory_byte_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryByteEffect{
                0x0d75,0x4000,0x2003,0}));

    mcga_callback_copy.observe_mcga_callback_copy_source_byte(
        {18,0x0e71,0x3000,0x0600,0x70,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_copy.observe_mcga_callback_copy_destination_word(
        {19,0x0e77,0x3000,0x0906,0x10,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::word});
    assert(mcga_callback_copy.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e7f});
    expect_rejected([&] { mcga_callback_copy.observe_mcga_callback_copy_density_byte(
        {20,0x0e7f,0x3000,0x02ff,3,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte}); });
    assert(mcga_callback_copy.next_sequence() == 20
        && mcga_callback_copy.callback_memory_byte_effects().empty());
    mcga_callback_copy.observe_mcga_callback_copy_density_byte(
        {20,0x0e7f,0x3000,0x0300,3,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    mcga_callback_copy.observe_mcga_callback_copy_current_byte(
        {21,0x0e83,0x3000,0x0000,0xfe,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    assert(mcga_callback_copy.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e83}
        && mcga_callback_copy.callback_memory_byte_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryByteEffect{
                0x0e83,0x3000,0x0000,1}));
    mcga_callback_copy.observe_mcga_callback_copy_current_byte(
        {22,0x0e83,0x3000,0x0000,1,
            eon::MillenniumDosVideoFunction13McgaCallbackReadWidth::byte});
    assert(mcga_callback_copy.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e8b}
        && mcga_callback_copy.callback_memory_byte_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryByteEffect{
                0x0e83,0x3000,0x0000,4}));
    mcga_callback_copy.execute_mcga_callback_copy_store(23,0x0e8b);
    assert(mcga_callback_copy.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0e71}
        && mcga_callback_copy.callback_memory_word_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect{
                0x0e8b,0x3000,0x0906,0x0000})
        && mcga_callback_copy.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0e8d,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,0x02ff}));
    mcga_callback_alternate_nonzero.execute_mcga_callback_loop_iteration(12,0x0dad);
    assert(mcga_callback_alternate_nonzero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0db5}
        && mcga_callback_alternate_nonzero.callback_memory_word_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect>{{
                0x0dad,0x2000,0x0004,0}})
        && mcga_callback_alternate_nonzero.callback_register_effects().back()
            == (eon::MillenniumDosVideoFunction13McgaCallbackRegisterEffect{
                0x0db3,eon::MillenniumDosVideoFunction13McgaCallbackRegister::cx,0}));
    mcga_callback_selector_zero.execute_mcga_callback_loop_iteration(14,0x0dad);
    assert(mcga_callback_selector_zero.boundary()
        == eon::MillenniumDosVideoFunction13InterruptBoundary{0x0db5}
        && mcga_callback_selector_zero.callback_memory_word_effects()
            == (std::vector<eon::MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect>{{
                0x0dad,0x2000,0x0004,0xffff}}));

    using Session = eon::MillenniumDosVideoFunctionZeroSession;
    using State = eon::MillenniumDosVideoFunctionZeroState;
    using Result = eon::MillenniumDosVideoFunctionZeroBiosResult;
    using Endpoint = eon::MillenniumDosVideoFunctionZeroEndpoint;

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

    Session ega_multi_count(ega_bytes, eon::MillenniumDosVideoDriverKind::ega640, 0xff);
    ega_multi_count.observe_bios_result(Result{1,0x1d6,0x10,0x560e,0,0,0,0});
    ega_multi_count.observe_bios_result(Result{2,0x1de,0x10,0x000e,0,0,0,0});
    ega_multi_count.observe_bios_result(Result{3,0x1e2,0x10,0x700e,0,4,0,0});
    ega_multi_count.advance_success_postlude_prefix();
    ega_multi_count.advance_ega_success_stack_prefix(0x2000, 0x0100);
    ega_multi_count.advance_ega_multi_count_loop(0x4567);
    const auto ega_loop = ega_multi_count.ega_loop_outcome();
    assert((ega_multi_count.state() == State::mode_success_ega_loop_recorded
        && ega_loop && ega_loop->instruction_address == 0x022e
        && ega_loop->ds == 0x4567 && ega_loop->es == 0xac00
        && ega_loop->ax == 0 && ega_loop->bx == 0x007e
        && ega_loop->cx == 0 && ega_loop->dx == 0xb000 && ega_loop->si == 0
        && ega_loop->di == 0x1f40
        && ega_loop->ss == 0x2000 && ega_loop->sp_before == 0x0100
        && ega_loop->sp_after == 0x00fe
        && ega_loop->port_write
            == eon::MillenniumDosVideoFunctionZeroEgaPortWriteIntent{0x0207,0x03ce,0xff08}
        && ega_loop->writes.size() == 18
        && ega_multi_count.ega_stack_outcome()->endpoint
            == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_loop_pop
        && ega_multi_count.ega_stack_outcome()->instruction_address == 0x022e));
    assert((ega_loop->writes[0]
        == eon::MillenniumDosVideoFunctionZeroEgaWriteIntent{0x020d,0x4567,0x008e,1,0}));
    assert((ega_loop->writes[1]
        == eon::MillenniumDosVideoFunctionZeroEgaWriteIntent{0x0212,0x4567,0x0091,1,3}));
    assert((ega_loop->writes[2]
        == eon::MillenniumDosVideoFunctionZeroEgaWriteIntent{0x0218,0x4567,0x0072,2,0}));
    assert((ega_loop->writes[4]
        == eon::MillenniumDosVideoFunctionZeroEgaWriteIntent{0x021a,0x4567,0x0075,1,0xa4}));
    assert((ega_loop->writes[5]
        == eon::MillenniumDosVideoFunctionZeroEgaWriteIntent{0x0225,0xa400,0,0x1f40,0}));
    assert((ega_loop->writes[7]
        == eon::MillenniumDosVideoFunctionZeroEgaWriteIntent{0x0212,0x4567,0x0090,1,2}));
    assert((ega_loop->writes[11]
        == eon::MillenniumDosVideoFunctionZeroEgaWriteIntent{0x0225,0xa800,0,0x1f40,0}));
    assert((ega_loop->writes[13]
        == eon::MillenniumDosVideoFunctionZeroEgaWriteIntent{0x0212,0x4567,0x008f,1,1}));
    assert((ega_loop->writes[17]
        == eon::MillenniumDosVideoFunctionZeroEgaWriteIntent{0x0225,0xac00,0,0x1f40,0}));
    expect_rejected([&] { ega_multi_count.advance_ega_multi_count_loop(0x4567); });
    ega_multi_count.advance_ega_success_pop_prefix();
    const auto ega_multi_pop = ega_multi_count.ega_stack_outcome();
    assert(ega_multi_count.state() == State::mode_success_pop_prefix_recorded
        && ega_multi_pop
        && ega_multi_pop->endpoint
            == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_local_store
        && ega_multi_pop->instruction_address == 0x022f
        && ega_multi_pop->pushed_value == 4 && ega_multi_pop->ax == 4
        && ega_multi_pop->sp_before == 0x0100 && ega_multi_pop->sp_after == 0x0100);
    ega_multi_count.advance_ega_success_store_prefix(0x4567);
    const auto ega_multi_ret = ega_multi_count.ega_stack_outcome();
    assert((ega_multi_count.state() == State::mode_success_store_prefix_recorded
        && ega_multi_ret
        && ega_multi_ret->endpoint == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_ret
        && ega_multi_ret->instruction_address == 0x0234 && ega_multi_ret->ax == 0x0404
        && ega_multi_ret->ds == 0x4567
        && ega_multi_ret->local_write
            == eon::MillenniumDosVideoFunctionZeroLocalWrite{0x022f,0x008a,4}));
    expect_rejected([&] {
        ega_multi_count.observe_ega_near_return({4,0x0234,0x2000,0x00fe,0x5678});
    });
    expect_rejected([&] {
        ega_multi_count.observe_ega_near_return({5,0x0235,0x2000,0x0100,0x5678});
    });
    assert(ega_multi_count.state() == State::mode_success_store_prefix_recorded
        && ega_multi_count.next_sequence() == 4 && !ega_multi_count.ega_return_outcome());
    ega_multi_count.observe_ega_near_return({4,0x0234,0x2000,0x0100,0x5678});
    const auto ega_near_ret = ega_multi_count.ega_return_outcome();
    assert(ega_multi_count.state() == State::mode_success_returned_boundary
        && ega_multi_count.next_sequence() == 5
        && ega_near_ret
        && ega_near_ret->observation.return_ip == 0x5678
        && ega_near_ret->observation.ss == 0x2000
        && ega_near_ret->observation.sp == 0x0100
        && ega_near_ret->sp_after == 0x0102
        && ega_multi_count.ega_stack_outcome()->endpoint
            == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_returned);
    expect_rejected([&] {
        ega_multi_count.observe_ega_near_return({5,0x0234,0x2000,0x0100,0x5678});
    });

    Session ega_wrapped_ret(ega_bytes, eon::MillenniumDosVideoDriverKind::ega640, 0xff);
    ega_wrapped_ret.observe_bios_result(Result{1,0x1d6,0x10,0x560e,0,0,0,0});
    ega_wrapped_ret.observe_bios_result(Result{2,0x1de,0x10,0x000e,0,0,0,0});
    ega_wrapped_ret.observe_bios_result(Result{3,0x1e2,0x10,0x700e,0,4,0,0});
    ega_wrapped_ret.advance_success_postlude_prefix();
    ega_wrapped_ret.advance_ega_success_stack_prefix(0x2000,0xfffe);
    ega_wrapped_ret.advance_ega_multi_count_loop(0x4567);
    ega_wrapped_ret.advance_ega_success_pop_prefix();
    ega_wrapped_ret.advance_ega_success_store_prefix(0x4567);
    ega_wrapped_ret.observe_ega_near_return({4,0x0234,0x2000,0xfffe,0x5678});
    const auto ega_wrapped_near_ret = ega_wrapped_ret.ega_return_outcome();
    assert(ega_wrapped_near_ret
        && ega_wrapped_near_ret->observation.sp == 0xfffe
        && ega_wrapped_near_ret->sp_after == 0x0000
        && ega_wrapped_ret.ega_stack_outcome()->sp_after == 0x0000
        && ega_wrapped_ret.state() == State::mode_success_returned_boundary);

    Session ega_zero_count(ega_bytes, eon::MillenniumDosVideoDriverKind::ega640, 0xff);
    ega_zero_count.observe_bios_result(Result{1,0x1d6,0x10,0x560e,0,0,0,0});
    ega_zero_count.observe_bios_result(Result{2,0x1de,0x10,0x000e,0,0,0,0});
    ega_zero_count.observe_bios_result(Result{3,0x1e2,0x10,0x700e,0,0,0,0});
    ega_zero_count.advance_success_postlude_prefix();
    ega_zero_count.advance_ega_success_stack_prefix(0x2000, 0x0100);
    expect_rejected([&] { ega_zero_count.advance_ega_multi_count_loop(0x4567); });
    assert(ega_zero_count.state() == State::mode_success_stack_prefix_recorded
        && !ega_zero_count.ega_loop_outcome()
        && ega_zero_count.ega_stack_outcome()->si == 0xffff);

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
    ega_single_count.advance_ega_success_pop_prefix();
    const auto ega_single_pop = ega_single_count.ega_stack_outcome();
    assert(ega_single_count.state() == State::mode_success_pop_prefix_recorded
        && ega_single_pop
        && ega_single_pop->endpoint
            == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_local_store
        && ega_single_pop->instruction_address == 0x022f
        && ega_single_pop->sp_before == 1 && ega_single_pop->sp_after == 1
        && ega_single_pop->pushed_value == 1 && ega_single_pop->ax == 1);
    expect_rejected([&] { ega_single_count.advance_ega_success_pop_prefix(); });
    ega_single_count.advance_ega_success_store_prefix(0x4567);
    const auto ega_single_ret = ega_single_count.ega_stack_outcome();
    const eon::MillenniumDosVideoFunctionZeroLocalWrite ega_single_write{0x022f,0x008a,1};
    assert(ega_single_count.state() == State::mode_success_store_prefix_recorded
        && ega_single_ret
        && ega_single_ret->endpoint
            == eon::MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_ret
        && ega_single_ret->instruction_address == 0x0234
        && ega_single_ret->ax == 0x0401 && ega_single_ret->ds == 0x4567
        && ega_single_ret->local_write == ega_single_write
        && ega_single_ret->sp_after == ega_single_ret->sp_before);
    expect_rejected([&] { ega_single_count.advance_ega_success_store_prefix(0x4567); });

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
