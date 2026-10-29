#include "data/millennium_dos_sound_driver.hpp"

#include "data/millennium_dos_game_flow.hpp"
#include "data/sha256.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace eon {
namespace {

template <std::size_t Size>
struct ExecutableByteAnchor {
    std::string_view sha256;
    static constexpr std::size_t size() { return Size; }
    bool matches(const std::span<const std::uint8_t> bytes) const {
        return bytes.size() == Size && to_hex(eon::sha256(bytes)) == sha256;
    }
};

template <std::size_t Size>
bool has_bytes(const std::span<const std::uint8_t> bytes, const std::size_t offset,
    const ExecutableByteAnchor<Size>& expected) {
    return offset <= bytes.size() && Size <= bytes.size() - offset
        && expected.matches(bytes.subspan(offset, Size));
}

} // namespace

MillenniumDosSoundSelectionEvidence parse_millennium_dos_sound_selection(
    const std::span<const std::uint8_t> mill_com) {
    constexpr auto launcher_sha256 =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    if (mill_com.size() != 1445 || to_hex(sha256(mill_com)) != launcher_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS sound-selection launcher");
    }
    constexpr ExecutableByteAnchor<100> selector{"f9e63fc4c7c590fc57abef4a0154a2399f714951c787f98d2f7d64eee86a7434"};
    constexpr ExecutableByteAnchor<54> filenames{
        "a5a3260fdf7a7018df0f34b0e9ba6f74a03e157f6d97cfb8f2f70407d8791185"};
    constexpr ExecutableByteAnchor<14> selection_table{
        "c49071bf0db7a712437ca74d2e9effe9222665f2ab154db1f5d748f540e10ef8"};
    // File offsets are loaded COM addresses minus $0100.
    if (!has_bytes(mill_com, 0x411, selector) || !has_bytes(mill_com, 0x52a, filenames)
        || !has_bytes(mill_com, 0x56e, selection_table)) {
        throw std::runtime_error("Unsupported Millennium DOS sound-selection bytes");
    }
    return {
        .launcher_sha256 = launcher_sha256,
        .selector_entry_address = 0x511,
        .selector_byte_count = selector.size(),
        .selector_sha256 = "f9e63fc4c7c590fc57abef4a0154a2399f714951c787f98d2f7d64eee86a7434",
        .prompt_address = 0x407,
        .prompt_byte_count = 155,
        .prompt_sha256 = "d84297ee58abeaa4ca09d60a533fe0b05ea4b805af46629d32c031b11700cad0",
        .filename_table_address = 0x62a,
        .filename_table_byte_count = filenames.size(),
        .filename_table_sha256 = "a5a3260fdf7a7018df0f34b0e9ba6f74a03e157f6d97cfb8f2f70407d8791185",
        .selection_table_address = 0x66e,
        .selection_table_byte_count = selection_table.size(),
        .selection_table_sha256 = "c49071bf0db7a712437ca74d2e9effe9222665f2ab154db1f5d748f540e10ef8",
        .ibm_speaker_table_slot = 0,
        .sound_blaster_table_slot = 3,
        .covox_table_slot = 4,
        .ibm_speaker_filename = "sibm.drv",
        .sound_blaster_filename = "ssbl.drv",
        .covox_filename = "scvx.drv",
        .missing_srol_table_slot = 2,
        .missing_srol_filename = "srol.drv",
    };
}

std::string extract_millennium_dos_sound_selection_prompt(
    const std::span<const std::uint8_t> mill_com,
    const MillenniumDosSoundSelectionEvidence& evidence) {
    // Reparse, rather than trusting caller-provided offsets, before exposing
    // any bytes as original text.  The COM load address is $0100.
    const auto verified = parse_millennium_dos_sound_selection(mill_com);
    if (evidence.launcher_sha256 != verified.launcher_sha256
        || evidence.prompt_address != verified.prompt_address
        || evidence.prompt_byte_count != verified.prompt_byte_count
        || evidence.prompt_sha256 != verified.prompt_sha256) {
        throw std::runtime_error("Mismatched Millennium DOS sound-selection prompt evidence");
    }
    constexpr std::size_t com_load_address = 0x100;
    const auto offset = static_cast<std::size_t>(verified.prompt_address) - com_load_address;
    if (offset > mill_com.size() || verified.prompt_byte_count > mill_com.size() - offset) {
        throw std::runtime_error("Truncated Millennium DOS sound-selection prompt");
    }
    const auto prompt = mill_com.subspan(offset, verified.prompt_byte_count);
    if (prompt.empty() || prompt.back() != '$' || to_hex(sha256(prompt)) != verified.prompt_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS sound-selection prompt bytes");
    }
    return {reinterpret_cast<const char*>(prompt.data()), prompt.size() - 1U};
}

MillenniumDosSoundDriverLeaf admit_millennium_dos_sound_driver_leaf(
    const std::span<const std::uint8_t> bytes) {
    const auto digest = to_hex(sha256(bytes));
    if (bytes.size() == 2871
        && digest == "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1") {
        return {MillenniumDosSoundDriverKind::ibm_speaker, "sibm.drv", digest, bytes.size()};
    }
    if (bytes.size() == 9194
        && digest == "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72") {
        return {MillenniumDosSoundDriverKind::sound_blaster, "ssbl.drv", digest, bytes.size()};
    }
    if (bytes.size() == 4053
        && digest == "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4") {
        return {MillenniumDosSoundDriverKind::covox_sound_master, "scvx.drv", digest, bytes.size()};
    }
    throw std::runtime_error("Unsupported Millennium DOS sound-driver leaf");
}

MillenniumDosSibmStartupProfile parse_millennium_dos_sibm_startup_profile(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> sibm_driver) {
    constexpr std::string_view sibm_sha256 =
        "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
    constexpr std::string_view startup_helper_sha256 =
        "201bdc778db37bcef39005e1eb8d4b314a6822b95d60f9dbcf3559719d32e519";
    constexpr std::array<std::pair<std::size_t, std::size_t>, 5> anchored_spans{{
        {0x20, 34}, {0x4a8, 32}, {0x9e7, 64}, {0xa27, 118}, {0x184, 24}}};
    constexpr std::array<std::string_view, 5> span_hashes{{
        "46598959f558b112e7331f904fa3d289aba73dcb284d8e5e768245c079c3af81",
        "fa8089e4e17500d3b1ee5b66b882fcd1437f10772ec80fc992fe59e1f82147bf",
        "e7059c363caa3ed2746199de05a525037daa6fd63dcd69f92cd017bb45e70047",
        "af3ef2f085bffc200b4e411f94523e3d3e5d2f9bb05f9217cafb85facd6f55f3",
        "4b31b7773504cc549b06cfd10aeaa3343bc7229e30dd7c16df62c48c0c370d29"}};
    const auto leaf = admit_millennium_dos_sound_driver_leaf(sibm_driver);
    if (leaf.kind != MillenniumDosSoundDriverKind::ibm_speaker
        || leaf.sha256 != sibm_sha256 || sibm_driver.size() != 2'871) {
        throw std::runtime_error("Millennium SIBM startup requires its exact admitted driver leaf");
    }
    for (std::size_t index = 0; index < span_hashes.size(); ++index) {
        const auto [offset, size] = anchored_spans[index];
        if (offset > sibm_driver.size() || size > sibm_driver.size() - offset
            || to_hex(sha256(sibm_driver.subspan(offset, size))) != span_hashes[index]) {
            throw std::runtime_error("Millennium SIBM dispatch or startup handler bytes changed");
        }
    }

    constexpr std::array<std::pair<std::uint16_t, std::pair<std::size_t, std::string_view>>, 6>
        game_spans{{
            {0xd07a, {77, startup_helper_sha256}},
            {0xcf8f, {16, "e5c033ac945f574e791bfff61eae9ea686bc81c99ab7842374290759a02b7f2c"}},
            {0xcf9f, {14, "6e9f903942e5c69c19c02c80c80daa6efdb308ae6a872f5122f7651080f8e3da"}},
            {0xcfad, {12, "5b4f97399ba035cb1af91303b00ab81d6934e13868192e7f45e24d85dafbba71"}},
            {0xcfb9, {15, "80065db3a9330eac4f83e53c101e08bb8712aa2746588b2cf8b52e4ebebcb4ac"}},
            {0xd05c, {28, "b741567f5563ed48fd2050146acad4018020b122da50537493bbc2b896ad06ed"}}}};
    const auto continuation = parse_millennium_dos_startup_zero_continuation_boundary(
        game_executable);
    constexpr std::size_t com_load_bias = 0x100;
    for (const auto& [address, span] : game_spans) {
        const auto [size, expected_hash] = span;
        const auto offset = static_cast<std::size_t>(address) - com_load_bias;
        if (continuation.first_local_call_target != 0xd07a
            || offset > game_executable.size() || size > game_executable.size() - offset
            || to_hex(sha256(game_executable.subspan(offset, size))) != expected_hash) {
            throw std::runtime_error("Millennium startup-to-SIBM wrapper source bytes changed");
        }
    }
    constexpr std::size_t table_offset = 0x20;
    std::array<std::uint16_t, 17> dispatch_targets{};
    for (std::size_t index = 0; index < dispatch_targets.size(); ++index) {
        const auto offset = table_offset + index * 2U;
        dispatch_targets[index] = static_cast<std::uint16_t>(sibm_driver[offset]
            | (static_cast<std::uint16_t>(sibm_driver[offset + 1U]) << 8U));
    }
    if (dispatch_targets[0] != 0x04a8 || dispatch_targets[11] != 0x09e7
        || dispatch_targets[12] != 0x0a27) {
        throw std::runtime_error("Millennium SIBM startup dispatch targets changed");
    }
    const auto sound_names = parse_millennium_dos_sound_effect_name_table_evidence(
        game_executable);
    constexpr std::uint16_t argument_table_address = 0xd05c;
    constexpr std::size_t load_bias = 0x100;
    std::array<std::uint16_t, 14> function_eleven_name_addresses{};
    std::array<std::string, 14> function_eleven_filenames{};
    const auto argument_table_offset = static_cast<std::size_t>(argument_table_address)
        - load_bias;
    for (std::size_t index = 0; index < function_eleven_name_addresses.size(); ++index) {
        const auto entry_offset = argument_table_offset + index * 2U;
        const auto name_address = static_cast<std::uint16_t>(game_executable[entry_offset]
            | (static_cast<std::uint16_t>(game_executable[entry_offset + 1U]) << 8U));
        const auto expected_name_address = static_cast<std::uint16_t>(
            sound_names.table_address + index * 9U);
        const auto name_offset = static_cast<std::size_t>(name_address) - load_bias;
        const auto& expected_name = sound_names.filenames[index];
        if (name_address != expected_name_address || name_offset > game_executable.size()
            || game_executable.size() - name_offset < 9U
            || expected_name.size() != 8U || game_executable[name_offset + 8U] != 0
            || !std::equal(expected_name.begin(), expected_name.end(),
                game_executable.begin() + static_cast<std::ptrdiff_t>(name_offset))) {
            throw std::runtime_error("SIBM function-$0b caller no longer names the exact SFX leaves");
        }
        function_eleven_name_addresses[index] = name_address;
        function_eleven_filenames[index] = expected_name;
    }
    return {
        .dispatch_targets = dispatch_targets,
        .function_eleven_name_addresses = function_eleven_name_addresses,
        .function_eleven_filenames = function_eleven_filenames,
        .function_zero_target = dispatch_targets[0],
        .function_eleven_target = dispatch_targets[11],
        .function_twelve_target = dispatch_targets[12],
        .startup_helper_address = 0xd07a,
        .first_private_interrupt_address = 0xcf98,
        .function_zero_wrapper_address = 0xcf9f,
        .function_twelve_wrapper_address = 0xcfad,
        .function_eleven_wrapper_address = 0xcfb9,
        .function_eleven_argument_table = 0xd05c,
        .function_twelve_requested_bytes = 15,
        .function_eleven_first_argument = 1,
        .function_eleven_last_argument = 14,
        .function_eleven_call_count = 14,
        .function_zero_input_for_mode_one = 0x0380,
        .function_zero_input_for_other_modes = 0x0220,
        .driver_sha256 = std::string(sibm_sha256),
        .startup_helper_sha256 = std::string(startup_helper_sha256),
        .function_eleven_argument_table_sha256 =
            "b741567f5563ed48fd2050146acad4018020b122da50537493bbc2b896ad06ed",
    };
}

} // namespace eon
