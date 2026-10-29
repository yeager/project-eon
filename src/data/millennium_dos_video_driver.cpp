#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace eon {
namespace {

std::uint16_t little16(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Millennium DOS video-driver dispatch table");
    }
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U));
}

} // namespace

namespace {

MillenniumDosVideoDriverProfile parse_driver_profile(
    const std::span<const std::uint8_t> bytes, const MillenniumDosVideoDriverKind kind,
    const bool spanish) {
    const bool ega = kind == MillenniumDosVideoDriverKind::ega640;
    const auto dispatch = static_cast<std::uint16_t>(ega ? 0x20 : 0x32);
    const auto english_size = static_cast<std::size_t>(ega ? 4632 : 4366);
    const auto spanish_size = static_cast<std::size_t>(ega ? 4630 : 4346);
    if (bytes.size() != (spanish ? spanish_size : english_size)) {
        throw std::runtime_error("Unsupported Millennium DOS video-driver size");
    }
    // A matching dispatch table is not enough to make a foreign or altered
    // driver executable evidence. English drivers are admitted by their full
    // supplied leaf identity just like the Spanish FAT12 leaves below.
    if (!spanish) {
        const auto expected = ega
            ? "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a"
            : "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
        if (to_hex(sha256(bytes)) != expected) {
            throw std::runtime_error("Unsupported Millennium English DOS video driver");
        }
    }
    const auto function_zero = little16(bytes, dispatch);
    const auto function_four = little16(bytes, dispatch + 8);
    const auto function_six = little16(bytes, dispatch + 12);
    const auto function_thirteen = little16(bytes, dispatch + 0x26);
    const auto function_thirty_one = little16(bytes, dispatch + 0x3e);
    const auto function_nineteen = little16(bytes, dispatch + 2U * 0x19U);
    const auto expected_zero = static_cast<std::uint16_t>(ega ? 0x1c8 : 0x1e6);
    const auto expected_four = static_cast<std::uint16_t>(ega ? 0xc17 : 0x815);
    const auto expected_six = static_cast<std::uint16_t>(ega ? 0x8a6 : 0x705);
    const auto expected_thirteen = static_cast<std::uint16_t>(ega ? 0xd37 : 0x905);
    const auto expected_thirty_one = static_cast<std::uint16_t>(ega ? 0x235 : 0x24c);
    const auto expected_nineteen = static_cast<std::uint16_t>(ega ? 0x234 : 0x9ee);
    const auto mode = static_cast<std::uint8_t>(ega ? 0x0e : 0x13);
    const auto state = static_cast<std::uint16_t>(ega ? 0x8d : 0xaf);
    if (function_zero != expected_zero || function_four != expected_four || function_six != expected_six
        || function_thirteen != expected_thirteen
        || function_thirty_one != expected_thirty_one
        || function_nineteen != expected_nineteen) {
        throw std::runtime_error("Unsupported Millennium DOS video-driver dispatch targets");
    }
    if (!spanish) {
        // The exact English leaves are hash-bound above. Keep the two short
        // relative branches explicit as well so their published destinations
        // cannot drift into an instruction operand while changing profile
        // arithmetic. JNE skips the cached-mode query to the mode setup; JZ
        // reaches the successful mode-match continuation.
        const auto jne = static_cast<std::size_t>(function_zero) + 0x0a;
        const auto jz = static_cast<std::size_t>(function_zero) + 0x1e;
        const auto known_target = static_cast<std::size_t>(function_zero) + 0x13;
        const auto match_target = static_cast<std::size_t>(function_zero) + 0x23;
        const auto match_opcode = static_cast<std::uint8_t>(ega ? 0xc6 : 0xbf);
        const auto cache = static_cast<std::size_t>(ega ? 0x8c : 0xae);
        if (bytes[jne] != 0x75 || bytes[jne + 1] != 0x07
            || bytes[known_target] != 0xb8
            || bytes[jz] != 0x74 || bytes[jz + 1] != 0x03
            || bytes[match_target] != match_opcode
            || bytes[static_cast<std::size_t>(function_zero) + 0x0e] != 0xcd
            || bytes[static_cast<std::size_t>(function_zero) + 0x0f] != 0x10
            || bytes[static_cast<std::size_t>(function_zero) + 0x10] != 0xa2
            || bytes[static_cast<std::size_t>(function_zero) + 0x11] != cache
            || bytes[static_cast<std::size_t>(function_zero) + 0x12] != 0
            || bytes[static_cast<std::size_t>(function_zero) + 0x13] != 0xb8
            || bytes[static_cast<std::size_t>(function_zero) + 0x14] != mode
            || bytes[static_cast<std::size_t>(function_zero) + 0x15] != 0
            || bytes[static_cast<std::size_t>(function_zero) + 0x16] != 0xcd
            || bytes[static_cast<std::size_t>(function_zero) + 0x17] != 0x10
            || bytes[static_cast<std::size_t>(function_zero) + 0x18] != 0xb4
            || bytes[static_cast<std::size_t>(function_zero) + 0x19] != 0x0f
            || bytes[static_cast<std::size_t>(function_zero) + 0x1a] != 0xcd
            || bytes[static_cast<std::size_t>(function_zero) + 0x1b] != 0x10
            || bytes[static_cast<std::size_t>(function_zero) + 0x1c] != 0x3c
            || bytes[static_cast<std::size_t>(function_zero) + 0x1d] != mode
            || bytes[static_cast<std::size_t>(function_zero) + 0x20] != 0x33
            || bytes[static_cast<std::size_t>(function_zero) + 0x21] != 0xc0
            || bytes[static_cast<std::size_t>(function_zero) + 0x22] != 0xc3) {
            throw std::runtime_error("Unsupported Millennium English function-zero control flow");
        }
    }
    constexpr auto ega_six_source_pointer_load_offset = 0x33U;
    constexpr auto mcga_six_source_pointer_load_offset = 0x29U;
    return {
        .kind = kind,
        .byte_size = bytes.size(),
        .dispatch_table_address = dispatch,
        .function_zero_address = function_zero,
        .function_zero_input_offset = 0,
        .function_zero_cached_mode_address = static_cast<std::uint16_t>(ega ? 0x8c : 0xae),
        .function_zero_cached_mode_unknown_sentinel = 0xff,
        .function_zero_cached_mode_query_interrupt_site = static_cast<std::uint16_t>(
            spanish ? 0 : function_zero + 0x0e),
        .function_zero_cached_mode_store_instruction = static_cast<std::uint16_t>(function_zero + 0x10),
        .function_zero_cached_mode_known_branch_target = static_cast<std::uint16_t>(function_zero + 0x13),
        .function_four_address = function_four,
        .function_zero_video_mode = mode,
        .function_zero_set_mode_interrupt_site = static_cast<std::uint16_t>(function_zero + 0x16),
        .function_zero_verify_mode_interrupt_site = static_cast<std::uint16_t>(
            spanish ? 0 : function_zero + 0x1a),
        .function_zero_mode_match_branch_target = static_cast<std::uint16_t>(function_zero + 0x23),
        .function_zero_mode_mismatch_return = static_cast<std::uint16_t>(function_zero + 0x22),
        .function_zero_mode_mismatch_zero_ax_instruction = static_cast<std::uint16_t>(function_zero + 0x20),
        .function_four_input_offset = 0,
        .function_four_input_mask = 3,
        .function_four_state_address = state,
        .function_six_address = function_six,
        .function_six_source_pointer_offset = 0,
        .function_six_source_pointer_load_address = static_cast<std::uint16_t>(
            function_six + (ega ? ega_six_source_pointer_load_offset : mcga_six_source_pointer_load_offset)),
        .function_six_source_word_zero_read_address = static_cast<std::uint16_t>(
            ega ? function_six + 0x45 : 0),
        .function_six_source_word_two_read_address = static_cast<std::uint16_t>(
            ega ? function_six + 0x39 : function_six + 0x2c),
        .function_six_source_word_four_read_address = static_cast<std::uint16_t>(
            ega ? function_six + 0x36 : 0),
        .function_six_source_nested_pointer_load_address = static_cast<std::uint16_t>(
            ega ? 0 : function_six + 0x31),
        .function_six_screen_width = 0x140,
        .function_six_horizontal_offset = 8,
        .function_six_width_offset = 0x10,
        .function_thirteen_address = function_thirteen,
        .function_thirteen_status_port = 0x03da,
        .function_thirteen_retrace_mask = 0x08,
        .function_thirteen_first_poll_address = static_cast<std::uint16_t>(function_thirteen + 3),
        .function_thirteen_second_poll_address = static_cast<std::uint16_t>(function_thirteen + 8),
        .function_thirty_one_address = function_thirty_one,
        .function_thirty_one_state_address = static_cast<std::uint16_t>(ega ? 0x8a : 0xac),
        .function_thirty_one_return_ah = static_cast<std::uint8_t>(ega ? 0x04 : 0x01),
        .function_nineteen_address = function_nineteen,
    };
}

} // namespace

MillenniumDosVideoDriverProfile parse_millennium_dos_video_driver(
    const std::span<const std::uint8_t> bytes, const MillenniumDosVideoDriverKind kind) {
    return parse_driver_profile(bytes, kind, false);
}

MillenniumDosVideoDriverProfile parse_millennium_dos_spanish_video_driver(
    const std::span<const std::uint8_t> bytes, const MillenniumDosVideoDriverKind kind) {
    const auto expected = kind == MillenniumDosVideoDriverKind::ega640
        ? "ef031b0b6e720ab2dafc1eb6373ddb76e0ff15f7b59ac785265c5136be153daf"
        : "3fb76b2ccccffc304b0525cd410b940bbb61e3d1a7a90340d72e5683d7f0211d";
    if (to_hex(sha256(bytes)) != expected) {
        throw std::runtime_error("Unsupported Millennium Spanish DOS video driver");
    }
    return parse_driver_profile(bytes, kind, true);
}

std::optional<MillenniumDosEgaFunctionNineteenReturn>
execute_millennium_dos_ega_function_nineteen(
    const std::span<const std::uint8_t> exact_driver,
    const std::span<const std::uint8_t> loaded_driver,
    const std::uint16_t input_ax) {
    constexpr std::string_view exact_sha =
        "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a";
    constexpr std::uint16_t dispatch_prefix_size = 0x13;
    constexpr std::uint16_t function_table = 0x20;
    constexpr std::uint16_t function_entry = 0x234;
    if (input_ax != 0x0019U || exact_driver.size() != 4'632U
        || loaded_driver.size() != exact_driver.size()
        || to_hex(sha256(exact_driver)) != exact_sha) {
        return std::nullopt;
    }
    const auto profile = parse_millennium_dos_video_driver(
        exact_driver, MillenniumDosVideoDriverKind::ega640);
    if (profile.dispatch_table_address != function_table
        || profile.function_nineteen_address != function_entry
        || exact_driver[function_entry] != 0xc3U
        || loaded_driver[function_entry] != 0xc3U
        || !std::equal(exact_driver.begin(),
            exact_driver.begin() + dispatch_prefix_size, loaded_driver.begin())
        || little16(loaded_driver,
            function_table + 2U * static_cast<std::size_t>(0x19U)) != function_entry) {
        return std::nullopt;
    }

    // Handler entry receives AX=$19. Its exact `SHL AX,1` makes the function
    // table byte offset $32. The table points to a single RET, so AX remains
    // $32. IRET restores the caller's saved FLAGS; their value is unavailable.
    return MillenniumDosEgaFunctionNineteenReturn{
        function_entry, 0x0032U, 0x0032U, true};
}

std::optional<MillenniumDosMcgaFunctionNineteenReturn>
execute_millennium_dos_mcga_function_nineteen(
    const std::span<const std::uint8_t> exact_driver,
    const std::span<const std::uint8_t> loaded_driver,
    const std::span<const std::uint8_t> exact_titles,
    const std::span<const std::uint8_t> call_record,
    const std::span<const std::uint8_t> loaded_palette_source,
    const std::uint16_t child_code_segment,
    const std::uint16_t entry_stack_pointer,
    const std::uint16_t input_ax,
    const std::uint8_t live_compare_flag_09c7,
    const std::uint8_t live_dispatch_flag_01e5) {
    constexpr std::string_view driver_sha =
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
    constexpr std::string_view titles_sha =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    constexpr std::string_view handler_sha =
        "4e967bbe0be7b21d0611a45bcc4481f6f4dec896c8bdac9b7f21b83b0a17c612";
    constexpr std::size_t handler_entry = 0x09ee;
    constexpr std::size_t handler_end = 0x0b5b;
    constexpr std::size_t palette_bytes = 0x02fe;
    constexpr std::size_t record_bytes = 10;
    constexpr std::uint16_t function_table = 0x0032;
    constexpr std::uint16_t entry_stack_pointer_expected = 0xd9ea;
    if (input_ax != 0x0019U || child_code_segment == 0
        || entry_stack_pointer != entry_stack_pointer_expected
        || live_compare_flag_09c7 != 0 || live_dispatch_flag_01e5 != 0
        || exact_driver.size() != 4'366U || loaded_driver.size() != exact_driver.size()
        || exact_titles.size() != 7'022U || call_record.size() != record_bytes
        || loaded_palette_source.size() != palette_bytes
        || to_hex(sha256(exact_driver)) != driver_sha
        || to_hex(sha256(exact_titles)) != titles_sha
        || to_hex(sha256(exact_driver.subspan(handler_entry,
            handler_end - handler_entry))) != handler_sha) {
        return std::nullopt;
    }
    const auto profile = parse_millennium_dos_video_driver(
        exact_driver, MillenniumDosVideoDriverKind::mcga);
    if (profile.dispatch_table_address != function_table
        || profile.function_nineteen_address != handler_entry
        || exact_driver[0x09c7] != 0
        || exact_driver[0x01e5] != 0
        || !std::equal(exact_driver.begin(), exact_driver.begin() + function_table,
            loaded_driver.begin())
        || !std::equal(exact_driver.begin() + handler_entry,
            exact_driver.begin() + handler_end, loaded_driver.begin() + handler_entry)
        || little16(loaded_driver,
            function_table + 2U * static_cast<std::size_t>(0x19U)) != handler_entry) {
        return std::nullopt;
    }

    const std::array<std::uint8_t, record_bytes> expected_record{
        0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x4c, 0x01,
        static_cast<std::uint8_t>(child_code_segment),
        static_cast<std::uint8_t>(child_code_segment >> 8U)};
    if (!std::equal(call_record.begin(), call_record.end(), expected_record.begin())) {
        return std::nullopt;
    }

    // The exact TITLES record has DL=DH=0 and count=$00ff. The MCGA handler
    // sets BH/AH to one, observes live CS:$09c7=0, and takes its $0b28 path.
    // That path writes 255 black RGB triples starting at DAC index zero.
    // TITLE.LIB setup has already replaced the original TITLES bytes at
    // CS:$014c with an owned 768-byte palette. The handler's final DAC path
    // depends on the descriptor and driver flags, not those palette values;
    // the caller validates that the live 766-byte source range is initialized
    // by its hash-bound TITLE.LIB copy before calling this adapter. Retrace
    // polling changes only timing; the result carries an explicit Eon
    // timing-abstraction marker and no beam-position observation.
    static_cast<void>(loaded_palette_source);
    return MillenniumDosMcgaFunctionNineteenReturn{};
}

std::optional<MillenniumDosMcgaFunctionTwentySixReturn>
execute_millennium_dos_mcga_function_twenty_six(
    const std::span<const std::uint8_t> exact_driver,
    const std::span<const std::uint8_t> loaded_driver,
    const std::span<const std::uint8_t> call_record,
    const std::span<const std::uint8_t> loaded_palette_source,
    const std::uint16_t child_code_segment,
    const std::uint8_t live_copy_flag_09c7) {
    constexpr std::string_view driver_sha =
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
    constexpr std::string_view table_sha =
        "7d0e024ea8f57445a74997544ae2ff06e3b80a7409c33ab6731406d23d3f9b3a";
    constexpr std::string_view handler_sha =
        "3d4e4739180dd45e984a7c4e974ed6b4312752b1c1ea3c2aad028851163e2ec1";
    constexpr std::size_t table_start = 0x0032;
    constexpr std::size_t table_end = 0x006a;
    constexpr std::size_t handler_start = 0x0b67;
    constexpr std::size_t handler_end = 0x0c88;
    constexpr std::size_t record_size = 10;
    constexpr std::size_t component_count = 0x00ff * 3U;
    constexpr std::size_t scratch_count = component_count + 1U;
    if (child_code_segment == 0 || exact_driver.size() != 4366U
        || loaded_driver.size() != exact_driver.size() || call_record.size() != record_size
        || loaded_palette_source.size() != scratch_count
        || to_hex(sha256(exact_driver)) != driver_sha
        || to_hex(sha256(exact_driver.subspan(table_start, table_end - table_start))) != table_sha
        || to_hex(sha256(exact_driver.subspan(handler_start, handler_end - handler_start)))
            != handler_sha
        || !std::equal(exact_driver.begin() + table_start, exact_driver.begin() + table_end,
            loaded_driver.begin() + table_start)
        || !std::equal(exact_driver.begin() + handler_start, exact_driver.begin() + handler_end,
            loaded_driver.begin() + handler_start)
        || little16(loaded_driver, table_start + 2U * 0x1aU) != handler_start
        || loaded_driver[0x09c7] != live_copy_flag_09c7 || live_copy_flag_09c7 != 0) {
        return std::nullopt;
    }
    const std::array<std::uint8_t, record_size> expected_record{
        0x00, 0x00, 0xff, 0x00, 0x19, 0x00, 0x4c, 0x01,
        static_cast<std::uint8_t>(child_code_segment),
        static_cast<std::uint8_t>(child_code_segment >> 8U)};
    if (!std::equal(call_record.begin(), call_record.end(), expected_record.begin())
        || std::any_of(loaded_palette_source.begin(),
            loaded_palette_source.begin() + component_count,
            [](const std::uint8_t component) { return component > 63U; })) {
        return std::nullopt;
    }

    // Literal translation of $0bbd..$0bee. The function first emits the
    // quotient buffer (all zero), then updates both buffers between passes.
    // Its `CMP AL,BH` is strictly greater-than, hence the deliberately
    // nonstandard remainder interval 0..25 instead of a `% 25` shorthand.
    std::array<std::uint8_t, scratch_count> remainder{};
    std::array<std::uint8_t, scratch_count> quotient{};
    MillenniumDosMcgaFunctionTwentySixReturn result;
    result.dac_writes.reserve(25U * 0x00ffU);
    const auto emit = [&result](const std::uint8_t phase,
                          const std::array<std::uint8_t, scratch_count>& source) {
        for (std::uint16_t index = 0; index < 0x00ff; ++index) {
            const auto component = static_cast<std::size_t>(index) * 3U;
            result.dac_writes.push_back({phase, static_cast<std::uint8_t>(index),
                source[component], source[component + 1U], source[component + 2U]});
        }
    };
    const auto advance = [&]() {
        for (std::size_t index = 0; index < component_count; ++index) {
            auto value = static_cast<unsigned>(remainder[index]) + loaded_palette_source[index];
            while (value > 25U) {
                ++quotient[index];
                value -= 25U;
            }
            remainder[index] = static_cast<std::uint8_t>(value);
        }
    };
    for (std::uint8_t phase = 0; phase < 24U; ++phase) {
        emit(phase, quotient);
        advance();
    }
    emit(24U, remainder);
    return result;
}

MillenniumDosMcgaVideoProbe evaluate_millennium_dos_mcga_video_probe(
    const std::span<const std::uint8_t> exact_mcga_driver, const char startup_input) {
    if (startup_input != '0' && startup_input != '1' && startup_input != '2') {
        throw std::runtime_error("Millennium MCGA diagnostic probe requires startup input 0, 1, or 2");
    }
    const auto profile = parse_millennium_dos_video_driver(
        exact_mcga_driver, MillenniumDosVideoDriverKind::mcga);
    if (profile.function_zero_cached_mode_query_interrupt_site != 0x01f4
        || profile.function_zero_cached_mode_store_instruction != 0x01f6
        || profile.function_zero_cached_mode_address != 0x00ae
        || profile.function_zero_set_mode_interrupt_site != 0x01fc
        || profile.function_zero_video_mode != 0x13) {
        throw std::runtime_error("Millennium MCGA diagnostic probe differs from the recovered function-zero path");
    }

    // The caller deliberately supplies no BIOS data. This one explicit
    // diagnostic fixture stands in for AL only; the verified caller stores
    // it and then requests the source literal MCGA mode at $01fc.
    constexpr std::uint8_t synthetic_eon_current_mode = 0x13;
    return {startup_input, profile.function_zero_cached_mode_query_interrupt_site,
        0x10, 0x0f, synthetic_eon_current_mode,
        profile.function_zero_cached_mode_store_instruction,
        profile.function_zero_cached_mode_address,
        profile.function_zero_set_mode_interrupt_site, 0x10,
        static_cast<std::uint16_t>(profile.function_zero_video_mode), 0xffff};
}

} // namespace eon
