#include "engine/millennium_dos_game_function_four_dispatch.hpp"

#include "data/millennium_dos_game_flow.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::string_view game_sha256 =
    "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
constexpr std::array<std::uint8_t, 12> private_wrapper{
    0x1e, 0x56, 0x57, 0x55, 0x06, 0xcd, 0x91,
    0x07, 0x5d, 0x5f, 0x5e, 0x1f};
constexpr std::string_view private_wrapper_sha256 =
    "fbab4cdf18042f95511e0ee33ad0635a9ad0b1a836259cbf19efe84e7aaef60f";
constexpr std::array<std::uint8_t, 17> ega_function_four{
    0x26, 0x8a, 0x07, 0x25, 0x03, 0x00, 0xb1, 0x03, 0xd2,
    0xe0, 0x86, 0x06, 0x8d, 0x00, 0xd2, 0xe8, 0xc3};
constexpr std::string_view ega_function_four_sha256 =
    "3a3e253f24e1c61228b34c57a25d22892dfc62e204a041d8085bc92026ff9c49";
constexpr std::array<std::uint8_t, 11> mcga_function_four{
    0x26, 0x8a, 0x07, 0x25, 0x03, 0x00, 0x86, 0x06, 0xaf, 0x00, 0xc3};
constexpr std::string_view mcga_function_four_sha256 =
    "7fbb6ca31a1a08015162f5cb6278b0feb2be77e40c98b685d7fbeeb09aaaf842";

bool contains_bytes(const std::span<const std::uint8_t> image,
    const std::size_t offset, const std::span<const std::uint8_t> expected) {
    return offset <= image.size() && expected.size() <= image.size() - offset
        && std::equal(expected.begin(), expected.end(),
            image.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::uint16_t little16(const std::span<const std::uint8_t> image,
    const std::size_t offset) {
    if (offset > image.size() || image.size() - offset < 2U) {
        throw std::runtime_error("Truncated function-$04 dispatch table");
    }
    return static_cast<std::uint16_t>(image[offset]
        | (static_cast<std::uint16_t>(image[offset + 1U]) << 8U));
}

} // namespace

MillenniumDosGameFunctionFourDispatchResult
execute_millennium_dos_game_function_four_dispatch(
    const std::span<const std::uint8_t> exact_game_executable,
    const std::uint16_t first_function_ax,
    const std::span<const std::uint8_t> exact_driver_image,
    const std::span<const std::uint8_t> owned_loaded_driver_image,
    const MillenniumDosVideoDriverKind kind,
    const std::uint16_t child_code_segment,
    const std::uint16_t loaded_driver_segment,
    const std::uint8_t vector_interrupt,
    const std::uint16_t vector_offset,
    const std::uint16_t vector_segment) {
    MillenniumDosGameFunctionFourDispatchResult result;
    result.kind = kind;
    try {
        if (to_hex(sha256(exact_game_executable)) != game_sha256
            || exact_game_executable.size() != 54'391U
            || child_code_segment == 0 || loaded_driver_segment == 0
            || vector_interrupt != 0x91 || vector_offset != 0
            || vector_segment != loaded_driver_segment
            || exact_driver_image.size() != owned_loaded_driver_image.size()) {
            throw std::runtime_error("Game function-$04 source, segment, or INT $91 vector rejected");
        }
        const auto game_offset = [](const std::uint16_t address) {
            return static_cast<std::size_t>(address - 0x0100U);
        };
        if (!contains_bytes(exact_game_executable, game_offset(0x0124), private_wrapper)
            || to_hex(sha256(exact_game_executable.subspan(
                    game_offset(0x0124), private_wrapper.size()))) != private_wrapper_sha256) {
            throw std::runtime_error("2200AD private INT $91 wrapper bytes changed");
        }

        const auto callees = parse_millennium_dos_english_game_startup_callees(
            exact_game_executable);
        const auto startup = evaluate_millennium_dos_english_startup_prefix(
            exact_game_executable, first_function_ax);
        if (startup.outcome
                != MillenniumDosEnglishStartupPrefixOutcome::selected_private_interrupt_boundary
            || startup.private_interrupt != 0x91 || startup.boundary_address != 0x0129
            || startup.stack_pointer != 0xda00
            || startup.selected_private_call_address == 0
            || (startup.selected_entry_address != callees.equal_entry_address
                && startup.selected_entry_address != callees.other_entry_address)) {
            throw std::runtime_error("Game is not at the selected second INT $91 boundary");
        }
        const bool equal_branch =
            startup.selected_entry_address == callees.equal_entry_address;
        const auto expected_entry = equal_branch ? 0xd1a1U : 0xd1b5U;
        const auto expected_call = equal_branch ? 0xd1a9U : 0xd1bdU;
        const auto expected_wrapper_return = equal_branch ? 0xd1acU : 0xd1c0U;
        const auto expected_outer_return = equal_branch ? 0xd2e0U : 0xd2e5U;
        const auto expected_outer_call = equal_branch ? 0xd2ddU : 0xd2e2U;
        if (startup.selected_entry_address != expected_entry
            || startup.selected_private_call_address != expected_call
            || (equal_branch && callees.equal_private_target_address != 0x0124)
            || (!equal_branch && callees.other_private_target_address != 0x0124)) {
            throw std::runtime_error("Function-$04 caller does not select the exact wrapper route");
        }

        const auto profile = parse_millennium_dos_video_driver(exact_driver_image, kind);
        const auto expected_first_ah = profile.function_thirty_one_return_ah;
        const auto first_ah = static_cast<std::uint8_t>(first_function_ax >> 8U);
        const auto first_al = static_cast<std::uint8_t>(first_function_ax & 0xffU);
        if (first_ah != expected_first_ah
            || profile.function_thirty_one_state_address >= owned_loaded_driver_image.size()
            || first_al != owned_loaded_driver_image[profile.function_thirty_one_state_address]
            || profile.function_four_address == 0
            || profile.function_four_state_address >= owned_loaded_driver_image.size()) {
            throw std::runtime_error("Function-$1f return is detached from its owned driver state");
        }

        const std::array<std::uint16_t, 4> mutable_cells{
            profile.function_thirty_one_state_address,
            profile.function_zero_cached_mode_address,
            profile.function_four_state_address,
            0x01e4};
        for (std::size_t offset = 0; offset < exact_driver_image.size(); ++offset) {
            const bool mutable_cell = std::ranges::find(mutable_cells,
                static_cast<std::uint16_t>(offset)) != mutable_cells.end();
            if (!mutable_cell && owned_loaded_driver_image[offset] != exact_driver_image[offset]) {
                throw std::runtime_error("Loaded video driver contains an unowned function-$04 change");
            }
        }
        const auto dispatch_prefix_hash = kind == MillenniumDosVideoDriverKind::ega640
            ? "a7d507882cf6bfe5b51db64b2dfb3ea8dd66f9d032b52622af2edac0a5ba81aa"
            : "2892839bc9f8ed84ce14e315addef727247431c808236b413356094d2392f35c";
        if (exact_driver_image.size() < 0x13U
            || to_hex(sha256(exact_driver_image.first(0x13U))) != dispatch_prefix_hash
            || !std::equal(exact_driver_image.begin(), exact_driver_image.begin() + 0x13,
                owned_loaded_driver_image.begin())) {
            throw std::runtime_error("Video driver INT $91 dispatch prefix changed");
        }

        const auto dispatch_slot = static_cast<std::size_t>(profile.dispatch_table_address) + 8U;
        const auto dispatch_target = little16(owned_loaded_driver_image, dispatch_slot);
        if (dispatch_target != profile.function_four_address) {
            throw std::runtime_error("INT $91 function-four slot does not reach its admitted leaf");
        }
        const bool ega = kind == MillenniumDosVideoDriverKind::ega640;
        const auto leaf = ega
            ? std::span<const std::uint8_t>(ega_function_four)
            : std::span<const std::uint8_t>(mcga_function_four);
        const auto leaf_hash = ega ? ega_function_four_sha256 : mcga_function_four_sha256;
        if (to_hex(sha256(exact_driver_image.subspan(profile.function_four_address, leaf.size())))
                != leaf_hash
            || !std::equal(leaf.begin(), leaf.end(),
                exact_driver_image.begin() + profile.function_four_address)
            || !std::equal(leaf.begin(), leaf.end(),
                owned_loaded_driver_image.begin() + profile.function_four_address)) {
            throw std::runtime_error("Function-$04 leaf differs from its admitted source bytes");
        }

        const auto handler_compare = owned_loaded_driver_image[0x01e5];
        if (!ega && (handler_compare != 0 || exact_driver_image[0x01e5] != 0)) {
            throw std::runtime_error("MCGA handler does not take its source-verified IRET route");
        }

        const auto record_index = game_offset(0xd19f);
        result.entry_address = static_cast<std::uint16_t>(expected_entry);
        result.wrapper_call_address = static_cast<std::uint16_t>(expected_call);
        result.wrapper_return_address = static_cast<std::uint16_t>(expected_wrapper_return);
        result.branch_call_return_address = static_cast<std::uint16_t>(expected_outer_return);
        result.handler_function_address = dispatch_target;
        result.record_byte = exact_game_executable[record_index];
        result.masked_record_byte = static_cast<std::uint8_t>(
            result.record_byte & profile.function_four_input_mask);
        result.driver_state_before = owned_loaded_driver_image[profile.function_four_state_address];
        result.driver_state_after = ega
            ? static_cast<std::uint8_t>(result.masked_record_byte << 3U)
            : result.masked_record_byte;
        result.returned_ax = ega
            ? static_cast<std::uint16_t>(result.driver_state_before >> 3U)
            : result.driver_state_before;
        const auto continuation = evaluate_millennium_dos_english_startup_prefix(
            exact_game_executable, first_function_ax, result.returned_ax);
        if ((equal_branch
                && (continuation.outcome != MillenniumDosEnglishStartupPrefixOutcome::equal_return
                    || continuation.boundary_address != 0x0455))
            || (!equal_branch
                && (continuation.outcome
                        != MillenniumDosEnglishStartupPrefixOutcome::palette_bios_interrupt_boundary
                    || continuation.boundary_address != 0x0476
                    || !continuation.first_palette_request))) {
            throw std::runtime_error("Function-$04 result does not reach its exact startup continuation");
        }
        result.continuation_address = continuation.boundary_address;
        // The wrapper's return word has been consumed. Its caller has entered
        // the following local helper, which leaves its own return at SP.
        result.final_stack_pointer = 0xd9fc;
        result.driver_writes.push_back({profile.function_four_state_address,
            result.driver_state_before, result.driver_state_after});
        if (!ega) {
            result.driver_writes.push_back({0x01e4,
                owned_loaded_driver_image[0x01e4], 0});
        }

        // Stack bytes left behind after the wrapper RET. Saved SI/DI/BP and
        // FLAGS were pushed, but their values are not supplied by this model.
        result.stack_words = {
            {0xd9ea, std::uint16_t{0x0012}},
            {0xd9ec, std::uint16_t{0x012b}},
            {0xd9ee, child_code_segment},
            {0xd9f0, std::nullopt},
            {0xd9f2, child_code_segment},
            {0xd9f4, std::nullopt},
            {0xd9f6, std::nullopt},
            {0xd9f8, std::nullopt},
            {0xd9fa, child_code_segment},
            {0xd9fc, static_cast<std::uint16_t>(equal_branch ? 0xd1afU : 0xd1c3U)},
            {0xd9fe, static_cast<std::uint16_t>(expected_outer_call + 3U)},
        };
        if (result.branch_call_return_address != result.stack_words.back().value
            || result.final_stack_pointer
                != result.stack_words[result.stack_words.size() - 2U].offset) {
            throw std::runtime_error("Function-$04 wrapper stack route failed its return checks");
        }
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

MillenniumDosStartupAllocationRequestBoundary
advance_millennium_dos_startup_to_allocation_request(
    const std::span<const std::uint8_t> exact_game_executable,
    const MillenniumDosGameFunctionFourDispatchResult& function_four,
    const std::uint16_t child_code_segment) {
    MillenniumDosStartupAllocationRequestBoundary result;
    try {
        if (!function_four.accepted || function_four.kind != MillenniumDosVideoDriverKind::mcga
            || function_four.continuation_address != 0x0455
            || function_four.final_stack_pointer != 0xd9fc
            || child_code_segment == 0
            || exact_game_executable.size() != 54'391U
            || to_hex(sha256(exact_game_executable)) != game_sha256) {
            throw std::runtime_error("Startup allocation prefix is detached from MCGA return at $0455");
        }
        const auto callees = parse_millennium_dos_english_game_startup_callees(
            exact_game_executable);
        const auto d0455 = parse_millennium_dos_english_game_startup_followups(
            exact_game_executable, callees);
        const auto allocation = parse_millennium_dos_startup_allocation_boundary(
            exact_game_executable);
        if (d0455.equal_return_address != 0x0455
            || allocation.continuation_entry_address != 0xd2e5
            || allocation.allocator_call_address != 0xd2e8
            || allocation.allocator_entry_address != 0xd1fa
            || allocation.allocator_first_external_interrupt_site != 0xd201
            || allocation.allocator_first_external_interrupt != 0x21
            || allocation.allocator_first_external_service != 0x4a) {
            throw std::runtime_error("Startup allocation request profile changed");
        }
        const auto word_at = [&](const std::uint16_t offset)
            -> std::optional<std::uint16_t> {
            const auto found = std::ranges::find_if(function_four.stack_words,
                [offset](const auto& word) { return word.offset == offset; });
            return found == function_four.stack_words.end() ? std::nullopt : found->value;
        };
        if (word_at(0xd9fc) != 0xd1af || word_at(0xd9fe) != 0xd2e0
            || word_at(0xd9fa) != child_code_segment) {
            throw std::runtime_error("MCGA $0455 stack preimage does not match the caller RET chain");
        }
        result.instruction_address = allocation.allocator_first_external_interrupt_site;
        result.interrupt_number = allocation.allocator_first_external_interrupt;
        result.ah = allocation.allocator_first_external_service;
        result.bx = 0x1000;
        result.ds = child_code_segment;
        result.code_segment = child_code_segment;
        result.stack_pointer = 0xd9fc;
        result.stack_words = {
            {0xd9fa, child_code_segment}, // PUSH CS / POP ES leaves stale CS.
            {0xd9fc, 0xd2eb}, // CALL $d1fa return IP.
            {0xd9fe, std::nullopt}, // PUSH DX; DX is not recovered here.
        };
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

} // namespace eon
