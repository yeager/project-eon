#pragma once

#include "engine/native_runtime_memory.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace eon {

enum class DeuterosAmigaServiceOrigin : std::uint8_t {
    native_compatibility,
    external_observation,
};

// MakeVPort's ABI and caller registers are admitted independently from its
// ROM effects. A trace may establish the return without proving Copper-list
// writes, allocations, or memory contents; this value intentionally carries
// no such effects.
struct DeuterosAmigaObservedMakeVPortReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t a0_view_address = 0;
    std::uint32_t a1_viewport_address = 0;
    std::uint32_t a6_graphics_library_base = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaMakeVPortReturnReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    DeuterosAmigaObservedMakeVPortReturn observation;
    DeuterosAmigaServiceOrigin origin = DeuterosAmigaServiceOrigin::external_observation;
    std::uint32_t modeled_memory_effect_count = 0;
};

struct DeuterosAmigaMakeVPortPrefixReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t return_address = 0;
    std::uint32_t view_address = 0;
    std::uint32_t viewport_address = 0;
    std::uint32_t workspace_address = 0;
    std::uint32_t copper_storage_address = 0;
    std::uint32_t next_copper_instruction_address = 0;
    std::uint32_t workspace_bytes = 0x22;
    std::uint32_t workspace_flags = 0x10000;
    std::uint32_t copper_storage_bytes = 0xf0;
    std::uint32_t copper_storage_flags = 0;
    std::uint32_t copper_instructions_emitted = 1;
    std::uint32_t resume_address = 0xfcc7b0;
    std::string_view rom_function_sha256 =
        "a4411cc0b426c8f77509751e7f12e77cde2b98844b344ae769a289d749c01ef";
    std::string_view workspace_helper_sha256 =
        "e82f11e8dd66eece16e902874d9c7f5af3253302f1ce313c7cb728a8f10a167a";
    std::string_view copper_node_helper_sha256 =
        "31c75b5db29ae12d9a29b3b816f790fec90b2df2170e1ad38a8ebf6ea01d6a44";
    DeuterosAmigaServiceOrigin origin = DeuterosAmigaServiceOrigin::native_compatibility;
};

struct DeuterosAmigaCopperAppendReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint32_t current_node_address = 0;
    std::uint32_t next_instruction_address = 0;
    std::uint16_t current_node_instruction_count = 0;
    std::uint16_t current_node_capacity = 0;
    std::uint32_t linked_node_count = 0;
    bool allocated_next_node = false;
    std::uint32_t new_node_bytes = 0;
    std::uint32_t new_node_flags = 0;
    std::uint32_t new_node_storage_bytes = 0;
    std::uint32_t new_node_storage_flags = 0;
};

struct DeuterosAmigaMakeVPortColorRangeReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint32_t first_copper_instruction_address = 0;
    std::uint32_t next_copper_instruction_address = 0;
    std::uint32_t low_color_instruction_count = 0;
    std::uint32_t high_color_instruction_count = 0;
    std::uint32_t total_copper_instruction_count = 0;
    std::uint32_t resume_address = 0xfcc934;
    std::uint16_t bitmap_depth = 4;
    std::uint16_t color_map_entry_count = 20;
};

struct DeuterosAmigaMakeVPortGeometryPrefixReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint32_t first_instruction_address = 0xfcd010;
    std::uint32_t next_instruction_address = 0xfcd2a4;
    std::uint32_t source_viewport_address = 0x12e12;
    std::uint32_t symbolic_stack_frame_width_slot = 0xffa8;
    std::uint32_t symbolic_stack_frame_height_slot = 0xffaa;
    std::uint32_t symbolic_stack_frame_rasinfo_word_slot = 0xfffe;
    std::uint32_t symbolic_stack_frame_row_bytes_slot = 0xfff6;
    std::uint32_t symbolic_stack_frame_vertical_offset_slot = 0xfffc;
    std::uint16_t copied_width = 320;
    std::uint16_t copied_height = 0;
    std::uint16_t mode_mask = 0x4000;
    std::uint16_t line_mask = 0x000f;
    std::uint32_t opaque_graphics_library_token = 0;
    std::uint16_t graphics_max_display_column = 0x01d1;
    std::uint16_t graphics_min_display_column = 0x0030;
    std::uint16_t computed_display_height = 0x0081;
    std::uint16_t bitmap_bytes_per_row = 40;
    std::int16_t computed_horizontal_delta = -32;
    std::uint16_t symbolic_result_vertical_position = 56;
    std::uint16_t symbolic_result_horizontal_position = 208;
    std::uint16_t symbolic_result_width = 320;
    std::uint16_t symbolic_result_height = 0;
    std::uint16_t symbolic_result_rasinfo_offset = 0;
    std::uint16_t symbolic_result_row_bytes = 40;
    std::uint32_t geometry_source_begin = 0xfcd010;
    std::uint32_t geometry_source_end = 0xfcd2a4;
    std::string_view geometry_source_sha256 =
        "1c8e89a6e9edddad0f7ac00468f8dc37bd914da57f1a8ba32cf56341ab870961";
    std::uint32_t graphics_init_source_begin = 0xfcaea4;
    std::uint32_t graphics_init_source_end = 0xfcaeb0;
    std::string_view graphics_init_slice_sha256 =
        "d7f03feed5d8382a8bebc41c0543a72d3f8e26d816cb156e5442e62dc70183f3";
    bool stack_frame_address_materialized = false;
    std::string_view rom_slice_sha256 =
        "fbb257909f9200f2b603df5a058d92d26c5d4ccc7160dc6c3bdb87ecb4e3f32c";
};

struct DeuterosAmigaMakeVPortCallerPrefixReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint32_t first_instruction_address = 0xfcc946;
    // The third word remains deliberately uninitialized.  The following
    // instruction at $fcc9a2 reads GraphicsBase+$a4, whose timing-derived
    // initializer has no admitted Eon-native value.
    std::uint32_t next_instruction_address = 0xfcc9a2;
    std::uint32_t copper_instruction_address = 0x00d200a0;
    std::uint32_t next_copper_instruction_address = 0x00d200a6;
    std::uint16_t partial_copper_first_word = 0;
    std::uint16_t partial_copper_second_word = 0x2f00;
    std::uint32_t custom_chip_base = 0x00dff000;
    std::uint16_t geometry_height = 0;
    std::uint16_t copper_register = 0xf08e;
    std::uint16_t copper_value = 0x0500;
    std::uint32_t source_begin = 0xfcc946;
    std::uint32_t source_end = 0xfcc9a2;
    std::string_view source_sha256 =
        "ce1bfc1a50073999d629997883ac22674a869daae77aab0d310c5eab59456087";
};

// The graphics initialization helper can place either of these words at
// GraphicsBase+$a4.  The caller uses the word only as a component of the next
// Copper instruction.  Preserve that finite uncertainty rather than choosing a
// monitor phase.  The slot deliberately stays unmaterialized in private
// runtime memory, so no renderer can mistake either candidate for evidence.
struct DeuterosAmigaMakeVPortSymbolicCopperWordReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint32_t instruction_address = 0xfcc9a2;
    std::uint32_t copper_word_address = 0x00d200aa;
    std::array<std::uint16_t, 2> possible_values{{0x0200, 0x0302}};
    std::uint32_t next_instruction_address = 0xfcc9c0;
    std::uint32_t next_copper_instruction_address = 0x00d200ac;
    std::uint16_t current_node_instruction_count = 23;
    std::uint32_t source_begin = 0xfcc9a2;
    std::uint32_t source_end = 0xfcc9c0;
    std::string_view source_sha256 =
        "f2f2fe1db329a1ad48ede990e77f70c0212161d4de54ac6b2ee9843d23cb6fd0";
    std::uint32_t copper_node_helper_begin = 0xfca358;
    std::uint32_t copper_node_helper_end = 0xfca3ee;
    std::string_view copper_node_helper_sha256 =
        "31c75b5db29ae12d9a29b3b816f790fec90b2df2170e1ad38a8ebf6ea01d6a44";
};

// `$fcc9c0..$fcca40` carries the same unresolved graphics timing value through
// an A5-relative scratch word, but does not read that scratch word again.  It
// can therefore emit the following Copper record from the exact ViewPort
// fields without selecting a graphics timing result or materializing a stack.
struct DeuterosAmigaMakeVPortSymbolicModePrefixReceipt {
    std::uint64_t runtime_generation = 0;
    std::array<std::uint16_t, 2> possible_graphics_timing_modes{{0x0200, 0x0302}};
    std::uint8_t viewport_sprite_priority = 0x24;
    std::uint32_t copper_instruction_address = 0x00d200ac;
    std::uint16_t copper_first_word = 0;
    std::uint16_t copper_second_word = 0xf104;
    std::uint16_t copper_third_word = 0x0024;
    std::uint16_t current_node_instruction_count = 24;
    std::uint32_t next_instruction_address = 0xfcca42;
    std::uint32_t source_begin = 0xfcc9c0;
    std::uint32_t source_end = 0xfcca42;
    std::string_view source_sha256 =
        "af563f6c58eae2b4e8b8e31e2fd7474e916a87e7f3e576c1f75fc2f34dc60e03";
};

// Eon-owned translation of the admitted MakeVPort prologue and its
// fca358 Copper-node helper. It stops at the next unimplemented instruction
// in the Kickstart body; it never fabricates the whole vector return.
class DeuterosAmigaMakeVPortPrefixSession {
public:
    explicit DeuterosAmigaMakeVPortPrefixSession(
        const std::uint64_t runtime_generation) noexcept
        : runtime_generation_(runtime_generation) {}

    [[nodiscard]] std::optional<DeuterosAmigaMakeVPortPrefixReceipt>
    begin_bootstrap_call(NativeRuntimeMemory& memory,
        const std::uint32_t call_address, const std::int16_t vector,
        const std::uint32_t return_address, const std::uint32_t a0_view,
        const std::uint32_t a1_viewport, const std::uint32_t a6_graphics_base,
        const std::uint32_t graphics_library_token, const std::uint64_t service_sequence) {
        if (runtime_generation_ == 0 || receipt_ || service_sequence != 6
            || call_address != 0x13188U || vector != -0x0d8
            || return_address != 0x1318cU || a0_view != 0x12e00U
            || a1_viewport != 0x12e12U || graphics_library_token == 0
            || a6_graphics_base != graphics_library_token) {
            return std::nullopt;
        }
        const auto word = [&memory](const std::uint32_t address)
            -> std::optional<std::uint16_t> {
            const auto bytes = memory.read_linear_range(address, 2);
            if (!bytes) return std::nullopt;
            return static_cast<std::uint16_t>(((*bytes)[0] << 8U) | (*bytes)[1]);
        };
        const auto longword = [&memory](const std::uint32_t address)
            -> std::optional<std::uint32_t> {
            const auto bytes = memory.read_linear_range(address, 4);
            if (!bytes) return std::nullopt;
            return (static_cast<std::uint32_t>((*bytes)[0]) << 24U)
                | (static_cast<std::uint32_t>((*bytes)[1]) << 16U)
                | (static_cast<std::uint32_t>((*bytes)[2]) << 8U) | (*bytes)[3];
        };
        if (longword(0x12e1aU) != std::optional<std::uint32_t>{0}
            || longword(0x12e16U) != std::optional<std::uint32_t>{0x12ec4U}
            || word(0x12e32U) != std::optional<std::uint16_t>{0x4000U}
            || word(0x12ec6U) != std::optional<std::uint16_t>{20U}
            || longword(0x12ec8U) != std::optional<std::uint32_t>{0x12eccU}
            || word(0x12e2aU) != std::optional<std::uint16_t>{320U}
            || word(0x12e2cU) != std::optional<std::uint16_t>{200U}) {
            return std::nullopt;
        }
        // The ROM reads BitMap.Depth through ViewPort.RasInfo->BitMap; it
        // does not use a ColorMap byte as the display depth.
        if (longword(0x12e36U) != std::optional<std::uint32_t>{0x12e62U}
            || longword(0x12e66U) != std::optional<std::uint32_t>{0x12e3aU}) {
            return std::nullopt;
        }
        const auto bitmap_depth = memory.read_byte(
            {NativeRuntimeAddressSpace::linear, std::nullopt, 0x12e3fU});
        const auto palette = memory.read_linear_range(0x12eccU, 40);
        if (!palette || palette->size() != 40 || !bitmap_depth || *bitmap_depth != 4
            || !std::all_of(palette->begin(), palette->end(),
                [](const std::uint8_t value) { return value == 0; })) {
            return std::nullopt;
        }
        constexpr std::uint32_t root = 0x00d20000U;
        constexpr std::uint32_t storage = root + 0x22U;
        constexpr std::uint32_t arena_end = 0x00d30000U;
        if (storage + 0xf0U > arena_end || !range_is_uninitialized(memory, root, 0x22U)
            || !range_is_uninitialized(memory, storage, 0xf0U)) {
            return std::nullopt;
        }

        // fcc452 stack order is size=$22, flags=$10000 (MEMF_CLEAR),
        // followed by size=6*$28, flags=0 for the Copper instruction buffer.
        std::map<std::uint32_t, std::uint8_t> staged;
        for (std::uint32_t offset = 0; offset < 0x22U; ++offset)
            staged[root + offset] = 0;
        write_be(staged, root + 0x0cU, 4, storage);
        write_be(staged, root + 0x10U, 4, storage);
        write_be(staged, root + 0x1cU, 2, 0);
        write_be(staged, root + 0x1eU, 2, 0x28);

        // The first source instruction is WAIT ($0001,$00ff,$0000). BitMap
        // depth 4 follows the <=4 branch; the subsequent test ANDs that depth
        // with $4000 (zero), so D2 remains $ff. The following fca358 increments
        // root+1c from zero to one and returns the next instruction pointer
        // because one is below the root's 40-instruction capacity.
        write_be(staged, storage, 2, 0x0001);
        write_be(staged, storage + 2U, 2, 0xffff);
        write_be(staged, storage + 4U, 2, 0);
        write_be(staged, a1_viewport + 8U, 4, root);
        write_be(staged, root + 0x1cU, 2, 1);
        const auto applied = apply_staged(memory, staged,
            "deuteros-amiga-makevport-prefix-" + std::to_string(runtime_generation_));
        if (!applied) return std::nullopt;

        root_address_ = root;
        graphics_library_token_ = graphics_library_token;
        current_node_address_ = root;
        current_storage_address_ = storage;
        allocation_cursor_ = storage + 0xf0U;
        current_count_ = 1;
        current_capacity_ = 0x28;
        started_ = true;
        receipt_ = DeuterosAmigaMakeVPortPrefixReceipt{
            runtime_generation_, service_sequence, call_address, return_address,
            a0_view, a1_viewport, root, storage, storage + 6U};
        return receipt_;
    }

    [[nodiscard]] std::optional<DeuterosAmigaMakeVPortColorRangeReceipt>
    translate_bootstrap_color_ranges(NativeRuntimeMemory& memory) {
        if (!started_ || !receipt_ || color_ranges_translated_
            || runtime_generation_ == 0 || current_count_ != 1
            || current_capacity_ != 0x28 || linked_node_count_ != 0) {
            return std::nullopt;
        }
        const auto word = [&memory](const std::uint32_t address)
            -> std::optional<std::uint16_t> {
            const auto bytes = memory.read_linear_range(address, 2);
            if (!bytes) return std::nullopt;
            return static_cast<std::uint16_t>(((*bytes)[0] << 8U) | (*bytes)[1]);
        };
        const auto longword = [&memory](const std::uint32_t address)
            -> std::optional<std::uint32_t> {
            const auto bytes = memory.read_linear_range(address, 4);
            if (!bytes) return std::nullopt;
            return (static_cast<std::uint32_t>((*bytes)[0]) << 24U)
                | (static_cast<std::uint32_t>((*bytes)[1]) << 16U)
                | (static_cast<std::uint32_t>((*bytes)[2]) << 8U) | (*bytes)[3];
        };
        // These are the exact loaded ViewPort / ColorMap / BitMap values read
        // by fcc7b0..fcc934 for this bootstrap profile.
        if (word(0x12e32U) != std::optional<std::uint16_t>{0x4000U}
            || longword(0x12e16U) != std::optional<std::uint32_t>{0x12ec4U}
            || word(0x12ec6U) != std::optional<std::uint16_t>{20U}
            || longword(0x12ec8U) != std::optional<std::uint32_t>{0x12eccU}
            || longword(0x12e36U) != std::optional<std::uint32_t>{0x12e62U}
            || longword(0x12e66U) != std::optional<std::uint32_t>{0x12e3aU}) {
            return std::nullopt;
        }
        const auto depth = memory.read_byte(
            {NativeRuntimeAddressSpace::linear, std::nullopt, 0x12e3fU});
        const auto color_count = word(0x12ec6U);
        const auto table_pointer = longword(0x12ec8U);
        if (!depth || *depth != 4U || !color_count || !table_pointer
            || *color_count > 32U || *table_pointer != 0x12eccU) {
            return std::nullopt;
        }

        // fcc7b0 computes 2^Depth=16. The first low-bank loop appends indices
        // 0..14 in one batch; the following loop appends index 15 through
        // fca358. Together they yield sixteen low-bank entries. fcc8d0 adds
        // indices 16..ColorMap.Count-1 when Modes&$4000 and Depth<5 (four).
        constexpr std::uint16_t low_count = 16;
        constexpr std::uint16_t high_first = 16;
        const auto high_count = static_cast<std::uint16_t>(
            *color_count > high_first ? *color_count - high_first : 0U);
        if (high_count != 4U || current_storage_address_ != 0x00d20022U
            || current_node_address_ != 0x00d20000U
            || current_count_ != 1U || current_capacity_ != 0x28U) {
            return std::nullopt;
        }
        auto staged_memory = memory;
        auto staged_session = *this;
        const auto first_instruction = staged_session.current_storage_address_
            + static_cast<std::uint32_t>(staged_session.current_count_) * 6U;
        for (std::uint16_t index = 0; index < low_count; ++index) {
            const auto palette_word = word(*table_pointer + 2U * index);
            if (!palette_word || !staged_session.append_copper_instruction(
                    staged_memory, 0x0000U,
                    static_cast<std::uint16_t>(0xf180U + 2U * index), *palette_word)) {
                return std::nullopt;
            }
        }
        for (std::uint16_t index = high_first;
             index < static_cast<std::uint16_t>(*color_count); ++index) {
            const auto palette_word = word(*table_pointer + 2U * index);
            if (!palette_word || !staged_session.append_copper_instruction(
                    staged_memory, 0x0000U,
                    static_cast<std::uint16_t>(0xf180U + 2U * index), *palette_word)) {
                return std::nullopt;
            }
        }
        const auto final_count = static_cast<std::uint32_t>(staged_session.current_count_);
        const auto next_instruction = staged_session.current_storage_address_
            + final_count * 6U;
        const auto root_count = word_from(staged_memory, 0x00d2001cU);
        if (!root_count || *root_count != 21U || final_count != 21U
            || next_instruction != 0x00d200a0U) {
            return std::nullopt;
        }
        memory = std::move(staged_memory);
        *this = std::move(staged_session);
        color_ranges_translated_ = true;
        receipt_->copper_instructions_emitted = 20;
        receipt_->next_copper_instruction_address = next_instruction;
        receipt_->resume_address = 0xfcc934U;
        return DeuterosAmigaMakeVPortColorRangeReceipt{
            runtime_generation_, first_instruction, next_instruction,
            low_count, high_count, final_count, 0xfcc934U, 4U, *color_count};
    }

    // Translate the reachable fcd010..fcd2a4 geometry path. The two GraphicsBase
    // bounds are taken from the hash-pinned Kickstart 1.3 initializer that
    // created this generation's opaque Eon token. A5-relative state remains
    // symbolic; this does not materialize historical Amiga addresses.
    [[nodiscard]] std::optional<DeuterosAmigaMakeVPortGeometryPrefixReceipt>
    translate_bootstrap_geometry_prefix(const NativeRuntimeMemory& memory) {
        if (!started_ || !receipt_ || !color_ranges_translated_
            || geometry_prefix_translated_ || runtime_generation_ == 0
            || current_count_ != 21 || current_capacity_ != 0x28
            || linked_node_count_ != 0 || current_node_address_ != 0x00d20000U
            || current_storage_address_ != 0x00d20022U) {
            return std::nullopt;
        }
        const auto word = [&memory](const std::uint32_t address)
            -> std::optional<std::uint16_t> {
            const auto bytes = memory.read_linear_range(address, 2);
            if (!bytes) return std::nullopt;
            return static_cast<std::uint16_t>(((*bytes)[0] << 8U) | (*bytes)[1]);
        };
        if (graphics_library_token_ == 0
            || word(0x12e2aU) != std::optional<std::uint16_t>{320U}
            || word(0x12e2eU) != std::optional<std::uint16_t>{0U}
            || word(0x12e32U) != std::optional<std::uint16_t>{0x4000U}
            || word(0x12e6aU) != std::optional<std::uint16_t>{0U}
            || word(0x12e6cU) != std::optional<std::uint16_t>{0U}
            || word(0x12e0eU) != std::optional<std::uint16_t>{0x0081U}
            || word(0x12e18U) != std::optional<std::uint16_t>{0x12ec4U}
            || word(0x12e3aU) != std::optional<std::uint16_t>{40U}
            || word(0x00d2001cU) != std::optional<std::uint16_t>{21U}) {
            return std::nullopt;
        }
        geometry_prefix_translated_ = true;
        auto result = DeuterosAmigaMakeVPortGeometryPrefixReceipt{runtime_generation_};
        result.opaque_graphics_library_token = graphics_library_token_;
        // fcd0b8 loads MaxDisplayColumn (465). The exact initialized View
        // bounds yield 320 + 129 = 449, so the correction branch is not taken.
        // Later the $30 MinDisplayColumn value, halved for this non-interlaced
        // mode, combines with the source-derived 56-pixel local position to
        // give -32 and skips the clipping loop. The complete symbolic A5 output
        // is (56,208,320,0,0,40,0), reaching the internal RTS at fcd2a4.
        return result;
    }

    [[nodiscard]] std::optional<DeuterosAmigaMakeVPortCallerPrefixReceipt>
    translate_bootstrap_caller_prefix(NativeRuntimeMemory& memory,
        const DeuterosAmigaMakeVPortGeometryPrefixReceipt& geometry) {
        if (!started_ || !receipt_ || !geometry_prefix_translated_
            || caller_prefix_translated_ || runtime_generation_ == 0
            || geometry.runtime_generation != runtime_generation_
            || geometry.next_instruction_address != 0xfcd2a4U
            || geometry.symbolic_result_height != 0U
            || geometry.opaque_graphics_library_token != graphics_library_token_
            || current_count_ != 21U || current_capacity_ != 0x28U
            || current_node_address_ != 0x00d20000U
            || current_storage_address_ != 0x00d20022U
            || current_storage_address_ + current_count_ * 6U != 0x00d200a0U) {
            return std::nullopt;
        }
        auto staged_memory = memory;
        auto staged_session = *this;
        const auto appended = staged_session.append_copper_instruction(
            staged_memory, 0x0000U, 0xf08eU, 0x0500U);
        if (!appended || appended->runtime_generation != runtime_generation_
            || appended->current_node_address != 0x00d20000U
            || appended->next_instruction_address != 0x00d200a6U
            || appended->current_node_instruction_count != 22U
            || appended->allocated_next_node) {
            return std::nullopt;
        }
        // $fcc97c..$fcc9a1 creates the first two words of the following
        // Copper record.  It has not yet read GraphicsBase+$a4, so committing
        // these four source-defined bytes does not choose a video-timing
        // branch or fabricate the record's third word.
        const auto partial = staged_session.current_storage_address_
            + static_cast<std::uint32_t>(staged_session.current_count_) * 6U;
        if (partial + 6U > staged_session.current_storage_address_
                + storage_bytes(staged_session.current_capacity_)
            || !range_is_uninitialized(staged_memory, partial, 6U)) return std::nullopt;
        auto partial_writes = std::map<std::uint32_t, std::uint8_t>{};
        write_be(partial_writes, partial, 2, 0U);
        write_be(partial_writes, partial + 2U, 2,
            staged_session.receipt_->view_address + 0x100U);
        if (!apply_staged(staged_memory, partial_writes,
                "deuteros-amiga-makevport-copper-prefix-"
                    + std::to_string(runtime_generation_))) return std::nullopt;
        memory = std::move(staged_memory);
        *this = std::move(staged_session);
        caller_prefix_translated_ = true;
        DeuterosAmigaMakeVPortCallerPrefixReceipt result;
        result.runtime_generation = runtime_generation_;
        return result;
    }

    [[nodiscard]] std::optional<DeuterosAmigaMakeVPortSymbolicCopperWordReceipt>
    translate_bootstrap_symbolic_timing_word(NativeRuntimeMemory& memory) {
        if (!started_ || !receipt_ || !caller_prefix_translated_
            || symbolic_timing_word_translated_ || runtime_generation_ == 0
            || current_count_ != 22U || current_capacity_ != 0x28U
            || current_node_address_ != 0x00d20000U
            || current_storage_address_ != 0x00d20022U) {
            return std::nullopt;
        }
        const auto slot = current_storage_address_
            + static_cast<std::uint32_t>(current_count_) * 6U + 4U;
        if (slot != 0x00d200aaU
            || !range_is_uninitialized(memory, slot, 2U)) {
            return std::nullopt;
        }
        // `$fcc9a2..$fcc9be` writes the timing-dependent word to the third
        // slot, advances D3 and calls fca358. Its node count is still below
        // the 40-entry capacity, so fca358 reads only the owned node fields,
        // increments root+$1c, and returns D3. It never reads this record's
        // third word on that branch. Commit that source-defined control
        // effect but leave the value itself symbolic. The following static
        // instructions do not branch on it.
        std::map<std::uint32_t, std::uint8_t> staged;
        write_be(staged, current_node_address_ + 0x1cU, 2, 23U);
        if (!apply_staged(memory, staged,
                "deuteros-amiga-makevport-symbolic-timing-word-"
                    + std::to_string(runtime_generation_))) {
            return std::nullopt;
        }
        current_count_ = 23U;
        ++emitted_instruction_count_;
        symbolic_timing_word_translated_ = true;
        return DeuterosAmigaMakeVPortSymbolicCopperWordReceipt{runtime_generation_};
    }

    [[nodiscard]] std::optional<DeuterosAmigaMakeVPortSymbolicModePrefixReceipt>
    translate_bootstrap_symbolic_mode_prefix(NativeRuntimeMemory& memory) {
        if (!started_ || !receipt_ || !symbolic_timing_word_translated_
            || symbolic_mode_prefix_translated_ || runtime_generation_ == 0
            || current_count_ != 23U || current_capacity_ != 0x28U
            || current_node_address_ != 0x00d20000U
            || current_storage_address_ != 0x00d20022U) {
            return std::nullopt;
        }
        const auto sprite_priority = memory.read_byte(
            {NativeRuntimeAddressSpace::linear, std::nullopt, 0x12e34U});
        if (!sprite_priority) return std::nullopt;
        // The GraphicsBase+$a4 result is retained as the two symbolic values
        // above. It is ORed with $0200 and stored only in an A5-relative
        // scratch word. No following instruction through fcca40 reads that
        // scratch slot. The next Copper record instead derives entirely from
        // the ViewPort's owned byte at +$22.
        const auto appended = append_copper_instruction(memory, 0U, 0xf104U,
            static_cast<std::uint16_t>(*sprite_priority));
        if (!appended || appended->runtime_generation != runtime_generation_
            || appended->current_node_address != 0x00d20000U
            || appended->next_instruction_address != 0x00d200b2U
            || appended->current_node_instruction_count != 24U
            || appended->allocated_next_node) {
            return std::nullopt;
        }
        symbolic_mode_prefix_translated_ = true;
        auto result = DeuterosAmigaMakeVPortSymbolicModePrefixReceipt{runtime_generation_};
        result.viewport_sprite_priority = *sprite_priority;
        result.copper_third_word = *sprite_priority;
        return result;
    }

    [[nodiscard]] std::optional<DeuterosAmigaCopperAppendReceipt>
    append_copper_instruction(NativeRuntimeMemory& memory, const std::uint16_t first,
        const std::uint16_t second, const std::uint16_t third) {
        if (!started_ || current_count_ == 0 || current_count_ > current_capacity_
            || emitted_instruction_count_ >= maximum_instructions) return std::nullopt;
        auto staged = std::map<std::uint32_t, std::uint8_t>{};
        const auto instruction_address = current_storage_address_
            + static_cast<std::uint32_t>(current_count_) * 6U;
        if (current_count_ >= current_capacity_ || instruction_address + 6U
            > current_storage_address_ + storage_bytes(current_capacity_)) {
            return std::nullopt;
        }
        write_be(staged, instruction_address, 2, first);
        write_be(staged, instruction_address + 2U, 2, second);
        write_be(staged, instruction_address + 4U, 2, third);
        auto next_count = static_cast<std::uint16_t>(current_count_ + 1U);
        auto next_node = current_node_address_;
        auto next_storage = current_storage_address_;
        auto next_capacity = current_capacity_;
        auto next_cursor = allocation_cursor_;
        auto next_linked_nodes = linked_node_count_;
        bool allocated = false;
        std::uint32_t allocated_node_bytes = 0;
        std::uint32_t allocated_node_storage_bytes = 0;

        if (next_count >= current_capacity_) {
            constexpr std::uint32_t node_size = 0x22U;
            constexpr std::uint32_t node_storage_size = 0x60U;
            if (next_cursor > arena_end - node_size - node_storage_size
                || !range_is_uninitialized(memory, next_cursor, node_size)
                || !range_is_uninitialized(memory, next_cursor + node_size,
                    node_storage_size)) return std::nullopt;
            const auto node = next_cursor;
            const auto node_storage = node + node_size;
            // fca358 uses AllocMem($22, MEMF_PUBLIC) and AllocMem($60,
            // MEMF_PUBLIC); only the fields it writes become initialized.
            write_be(staged, node + 0x0cU, 4, node_storage);
            write_be(staged, node + 0x1eU, 2, 0x10);
            write_be(staged, node, 4, 0);
            write_be(staged, node + 0x1cU, 2, 1);
            write_be(staged, current_node_address_, 4, node);
            const auto tail = current_storage_address_
                + static_cast<std::uint32_t>(current_capacity_ - 1U) * 6U;
            for (std::uint32_t i = 0; i < 6; ++i) {
                const auto pending = staged.find(tail + i);
                const auto previous = pending == staged.end()
                    ? memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                        tail + i})
                    : std::optional<std::uint8_t>{pending->second};
                if (!previous) return std::nullopt;
                staged[node_storage + i] = *previous;
            }
            write_be(staged, tail, 2, 2);
            write_be(staged, tail + 2U, 4, node);
            next_node = node;
            next_storage = node_storage;
            next_capacity = 0x10;
            next_count = 1;
            next_cursor = node_storage + node_storage_size;
            ++next_linked_nodes;
            allocated = true;
            allocated_node_bytes = node_size;
            allocated_node_storage_bytes = node_storage_size;
        } else {
            write_be(staged, current_node_address_ + 0x1cU, 2, next_count);
        }
        const auto applied = apply_staged(memory, staged,
            "deuteros-amiga-makevport-copper-"
                + std::to_string(runtime_generation_) + "-"
                + std::to_string(emitted_instruction_count_ + 1U));
        if (!applied) return std::nullopt;
        current_node_address_ = next_node;
        current_storage_address_ = next_storage;
        current_capacity_ = next_capacity;
        current_count_ = next_count;
        allocation_cursor_ = next_cursor;
        linked_node_count_ = next_linked_nodes;
        ++emitted_instruction_count_;
        return DeuterosAmigaCopperAppendReceipt{runtime_generation_, current_node_address_,
            current_storage_address_ + static_cast<std::uint32_t>(current_count_) * 6U,
            current_count_, current_capacity_, linked_node_count_, allocated,
            allocated_node_bytes, allocated ? 1U : 0U,
            allocated_node_storage_bytes, allocated ? 1U : 0U};
    }

    [[nodiscard]] bool started() const noexcept { return started_; }
    [[nodiscard]] std::uint64_t runtime_generation() const noexcept {
        return runtime_generation_;
    }

private:
    static constexpr std::uint32_t arena_end = 0x00d30000U;
    static constexpr std::uint32_t maximum_instructions = 256;

    static std::uint32_t storage_bytes(const std::uint16_t capacity) noexcept {
        return static_cast<std::uint32_t>(capacity) * 6U;
    }
    static bool range_is_uninitialized(const NativeRuntimeMemory& memory,
        const std::uint32_t address, const std::uint32_t size) {
        if (address > arena_end || size > arena_end - address) return false;
        for (std::uint32_t offset = 0; offset < size; ++offset) {
            if (memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                    address + offset})) return false;
        }
        return true;
    }
    static std::optional<std::uint16_t> word_from(
        const NativeRuntimeMemory& memory, const std::uint32_t address) {
        const auto bytes = memory.read_linear_range(address, 2);
        if (!bytes) return std::nullopt;
        return static_cast<std::uint16_t>(((*bytes)[0] << 8U) | (*bytes)[1]);
    }
    static void write_be(std::map<std::uint32_t, std::uint8_t>& bytes,
        const std::uint32_t address, const std::uint32_t width,
        const std::uint32_t value) {
        for (std::uint32_t offset = 0; offset < width; ++offset) {
            bytes[address + offset] = static_cast<std::uint8_t>(
                value >> ((width - offset - 1U) * 8U));
        }
    }
    static bool apply_staged(NativeRuntimeMemory& memory,
        const std::map<std::uint32_t, std::uint8_t>& staged, std::string id) {
        if (staged.empty()) return false;
        NativeRuntimeEffectBatch batch{std::move(id), true, {}};
        batch.effects.reserve(staged.size());
        std::size_t order = 1;
        for (const auto& [address, value] : staged) {
            batch.effects.push_back({order++,
                {NativeRuntimeAddressSpace::linear, std::nullopt, address},
                MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::big_endian, value});
        }
        return memory.apply(batch).accepted;
    }

    std::uint64_t runtime_generation_ = 0;
    std::uint32_t graphics_library_token_ = 0;
    std::uint32_t root_address_ = 0;
    std::uint32_t current_node_address_ = 0;
    std::uint32_t current_storage_address_ = 0;
    std::uint32_t allocation_cursor_ = 0;
    std::uint16_t current_count_ = 0;
    std::uint16_t current_capacity_ = 0;
    std::uint32_t emitted_instruction_count_ = 0;
    std::uint32_t linked_node_count_ = 0;
    bool started_ = false;
    bool color_ranges_translated_ = false;
    bool geometry_prefix_translated_ = false;
    bool caller_prefix_translated_ = false;
    bool symbolic_timing_word_translated_ = false;
    bool symbolic_mode_prefix_translated_ = false;
    std::optional<DeuterosAmigaMakeVPortPrefixReceipt> receipt_;
};

// Eon's opaque service token is generation-bound and is not an Amiga address,
// an OS allocation result, or a captured OpenLibrary return value.
struct DeuterosAmigaNativeOpenLibraryServiceReturn {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t library_name_address = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::string_view library_name;
    std::uint16_t minimum_version = 0;
    std::uint32_t native_service_token = 0;
    DeuterosAmigaServiceOrigin origin = DeuterosAmigaServiceOrigin::native_compatibility;
};

struct DeuterosAmigaNativeGraphicsInitReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t object_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t effect_count = 0;
    std::string_view rom_sha256 =
        "ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53";
    std::string_view rom_function_sha256;
    DeuterosAmigaServiceOrigin origin = DeuterosAmigaServiceOrigin::native_compatibility;
    std::string_view resource_sha256{};
    std::uint32_t native_resource_address = 0;
};

// Implements Eon's availability contract for the exact title-stage request
// and the instruction-verified Kickstart 1.3 initialization primitives below.
// Copper-list generation and hardware display effects remain separate.
class DeuterosAmigaGraphicsLibraryCompatibilitySession {
public:
    explicit DeuterosAmigaGraphicsLibraryCompatibilitySession(
        const std::uint64_t runtime_generation) noexcept
        : runtime_generation_(runtime_generation) {}

    [[nodiscard]] std::optional<DeuterosAmigaNativeOpenLibraryServiceReturn>
    open_title_graphics_library(const std::uint32_t entry_address,
        const std::uint32_t library_name_address,
        const std::uint32_t exec_base_source_address,
        const std::uint32_t call_address, const std::int16_t vector,
        const std::uint32_t return_address, const std::string_view library_name,
        const std::uint16_t minimum_version) noexcept {
        constexpr std::uint64_t maximum_generation = 0x0fffffffULL;
        if (runtime_generation_ == 0 || runtime_generation_ > maximum_generation
            || service_opened_ || entry_address != 0x1ed80
            || library_name_address != 0x1ed02 || exec_base_source_address != 4
            || call_address != 0x1ed8c || vector != -0x228
            || return_address != 0x1ed90 || library_name != "graphics.library"
            || minimum_version != 0) {
            return std::nullopt;
        }
        service_opened_ = true;
        next_service_sequence_ = 2;
        graphics_library_service_token_ = static_cast<std::uint32_t>(
            0xe0000000U | runtime_generation_);
        return DeuterosAmigaNativeOpenLibraryServiceReturn{
            runtime_generation_, 1, entry_address, library_name_address,
            exec_base_source_address, call_address, vector, return_address,
            library_name, minimum_version,
            graphics_library_service_token_,
            DeuterosAmigaServiceOrigin::native_compatibility};
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeOpenLibraryServiceReturn>
    open_bootstrap_graphics_library(const std::uint32_t entry_address,
        const std::uint32_t library_name_address,
        const std::uint32_t exec_base_source_address,
        const std::uint32_t call_address, const std::int16_t vector,
        const std::uint32_t return_address, const std::string_view library_name,
        const std::uint16_t minimum_version) noexcept {
        constexpr std::uint64_t maximum_generation = 0x0fffffffULL;
        if (runtime_generation_ == 0 || runtime_generation_ > maximum_generation
            || service_opened_ || entry_address != 0x13000
            || library_name_address != 0x12c28 || exec_base_source_address != 4
            || call_address != 0x1300c || vector != -0x228
            || return_address != 0x13010 || library_name != "graphics.library"
            || minimum_version != 0) {
            return std::nullopt;
        }
        service_opened_ = true;
        next_service_sequence_ = 2;
        graphics_library_service_token_ = static_cast<std::uint32_t>(
            0xe0000000U | runtime_generation_);
        return DeuterosAmigaNativeOpenLibraryServiceReturn{
            runtime_generation_, 1, entry_address, library_name_address,
            exec_base_source_address, call_address, vector, return_address,
            library_name, minimum_version,
            graphics_library_service_token_,
            DeuterosAmigaServiceOrigin::native_compatibility};
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeGraphicsInitReceipt>
    initialize_view(NativeRuntimeMemory& memory, const std::uint32_t call_address,
        const std::int16_t vector, const std::uint32_t return_address,
        const std::uint32_t a1_view_address) {
        constexpr std::string_view function_sha256 =
            "f60b92b04fc8989cf2ffdc7057a60bbd38ba1dcc8735095a818daa1263638a45";
        const bool first = call_address == 0x13058U && return_address == 0x1305cU
            && a1_view_address == 0x12e00U;
        const bool second = call_address == 0x131bcU && return_address == 0x131c0U
            && a1_view_address == 0x12f00U;
        if (!service_opened_ || (first == second) || vector != -0x168
            || next_service_sequence_ == 0) return std::nullopt;

        NativeRuntimeEffectBatch batch{"deuteros-amiga-kickstart-initview-"
            + std::to_string(next_service_sequence_), true, {}};
        batch.effects.reserve(16);
        for (std::uint32_t offset = 0; offset < 0x0cU; ++offset)
            add_byte(batch, a1_view_address + offset, 0);
        add_word(batch, a1_view_address + 0x0cU, 0x002cU);
        add_word(batch, a1_view_address + 0x0eU, 0x0081U);
        add_byte(batch, a1_view_address + 0x10U, 0);
        add_byte(batch, a1_view_address + 0x11U, 0);
        if (!memory.apply(batch).accepted) return std::nullopt;
        return DeuterosAmigaNativeGraphicsInitReceipt{runtime_generation_,
            next_service_sequence_++, call_address, a1_view_address, vector,
            return_address, static_cast<std::uint32_t>(batch.effects.size()),
            "ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53",
            function_sha256};
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeGraphicsInitReceipt>
    initialize_viewport(NativeRuntimeMemory& memory, const std::uint32_t call_address,
        const std::int16_t vector, const std::uint32_t return_address,
        const std::uint32_t a0_viewport_address) {
        constexpr std::string_view function_sha256 =
            "d8d73d35a78e46f0ac7e977642afd6f4259f9d92a36fe78f298879a111f88d4c";
        const bool first = call_address == 0x13068U && return_address == 0x1306cU
            && a0_viewport_address == 0x12e12U;
        const bool second = call_address == 0x131ccU && return_address == 0x131d0U
            && a0_viewport_address == 0x12f12U;
        if (!service_opened_ || (first == second) || vector != -0x0cc
            || next_service_sequence_ == 0) return std::nullopt;

        NativeRuntimeEffectBatch batch{"deuteros-amiga-kickstart-initvport-"
            + std::to_string(next_service_sequence_), true, {}};
        batch.effects.reserve(40);
        for (std::uint32_t offset = 0; offset < 0x28U; ++offset) {
            if (offset == 0x22U) continue;
            add_byte(batch, a0_viewport_address + offset, 0);
        }
        add_byte(batch, a0_viewport_address + 0x22U, 0x24U);
        if (!memory.apply(batch).accepted) return std::nullopt;
        return DeuterosAmigaNativeGraphicsInitReceipt{runtime_generation_,
            next_service_sequence_++, call_address, a0_viewport_address, vector,
            return_address, static_cast<std::uint32_t>(batch.effects.size()),
            "ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53",
            function_sha256};
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeGraphicsInitReceipt>
    initialize_bitmap(NativeRuntimeMemory& memory, const std::uint32_t call_address,
        const std::int16_t vector, const std::uint32_t return_address,
        const std::uint32_t a0_bitmap_address, const std::uint16_t d0_depth,
        const std::uint16_t d1_width, const std::uint16_t d2_rows) {
        constexpr std::string_view function_sha256 =
            "16a1a45ddbedc1012a6e126024cbb624fcda26fb6249b1179272f85aba716f0b";
        const bool first = call_address == 0x1309eU && return_address == 0x130a2U
            && a0_bitmap_address == 0x12e3aU;
        const bool second = call_address == 0x13202U && return_address == 0x13206U
            && a0_bitmap_address == 0x12f3aU;
        if (!service_opened_ || (first == second) || vector != -0x186
            || d0_depth != 4U || d1_width != 320U || d2_rows != 200U
            || next_service_sequence_ == 0) return std::nullopt;

        // Kickstart 1.3 InitBitMap stores Rows, Depth, and BytesPerRow in
        // this order. It does not initialize the four plane pointers.
        NativeRuntimeEffectBatch batch{"deuteros-amiga-kickstart-initbitmap-"
            + std::to_string(next_service_sequence_), true, {}};
        batch.effects = {
            {1, {NativeRuntimeAddressSpace::linear, std::nullopt,
                a0_bitmap_address + 2U}, MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::big_endian, d2_rows},
            {2, {NativeRuntimeAddressSpace::linear, std::nullopt,
                a0_bitmap_address + 5U}, MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::big_endian, d0_depth},
            {3, {NativeRuntimeAddressSpace::linear, std::nullopt,
                a0_bitmap_address}, MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::big_endian,
                static_cast<std::uint16_t>(((d1_width + 15U) >> 4U) << 1U)},
        };
        if (!memory.apply(batch).accepted) return std::nullopt;
        return DeuterosAmigaNativeGraphicsInitReceipt{runtime_generation_,
            next_service_sequence_++, call_address, a0_bitmap_address, vector,
            return_address, 3U,
            "ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53",
            function_sha256};
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeGraphicsInitReceipt>
    initialize_rastport(NativeRuntimeMemory& memory, const std::uint32_t call_address,
        const std::int16_t vector, const std::uint32_t return_address,
        const std::uint32_t a1_rastport_address) {
        constexpr std::string_view function_sha256 =
            "9b4a1c7598c3874da943123e2dd0ae310443c2c1ee553fa8988279e15c07fe16";
        constexpr std::string_view descriptor_sha256 =
            "4d40f5902b91ed9857845f7f584c715de0ad5d0f4031b52201bfd7c694c56223";
        constexpr std::uint32_t eon_system_font_address = 0x00d10000U;
        const bool first = call_address == 0x13154U && return_address == 0x13158U
            && a1_rastport_address == 0x12e6eU;
        if (!service_opened_ || !first || vector != -0x0c6
            || next_service_sequence_ != 5 || system_font_address_ != 0
            || call_address == 0 || runtime_generation_ == 0) {
            return std::nullopt;
        }

        // Kickstart's AllocMem result is dynamic and cannot be recovered from
        // the ROM descriptor address. Eon maps the proven 52-byte font object
        // into its own Exec-compatible address range. Only the three words
        // read by the admitted SetFont body are initialized here; later reads
        // of other TextFont fields remain an explicit boundary.
        for (std::uint32_t offset = 0; offset < 0x34U; ++offset) {
            if (memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                    eon_system_font_address + offset})) {
                return std::nullopt;
            }
        }

        NativeRuntimeEffectBatch batch{"deuteros-amiga-kickstart-initrastport-"
            + std::to_string(runtime_generation_), true, {}};
        batch.effects.reserve(84);
        std::array<std::uint8_t, 0x4e> rastport{};
        rastport[0x18] = 0xff;
        rastport[0x19] = 0xff;
        rastport[0x1b] = 0xff;
        rastport[0x1c] = 0x01;
        rastport[0x22] = 0xff;
        rastport[0x23] = 0xff;
        for (std::size_t offset = 0x28; offset <= 0x2f; ++offset) {
            rastport[offset] = 0xca;
        }
        rastport[0x34] = static_cast<std::uint8_t>(eon_system_font_address >> 24U);
        rastport[0x35] = static_cast<std::uint8_t>(eon_system_font_address >> 16U);
        rastport[0x36] = static_cast<std::uint8_t>(eon_system_font_address >> 8U);
        rastport[0x37] = static_cast<std::uint8_t>(eon_system_font_address);
        rastport[0x3a] = 0x99;
        rastport[0x3b] = 0x20;
        rastport[0x3c] = 0x00;
        rastport[0x3d] = 0xfc;
        rastport[0x3e] = 0x85;
        rastport[0x3f] = 0x12;
        for (std::size_t offset = 0; offset < rastport.size(); ++offset) {
            batch.effects.push_back({batch.effects.size() + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt,
                    a1_rastport_address + offset},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                rastport[offset]});
        }
        const auto add_font_word = [&](const std::uint32_t address,
            const std::uint16_t value) {
            batch.effects.push_back({batch.effects.size() + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt, address},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, value});
        };
        add_font_word(eon_system_font_address + 0x14U, 0x9920U);
        add_font_word(eon_system_font_address + 0x18U, 0x00fcU);
        add_font_word(eon_system_font_address + 0x1aU, 0x8512U);
        if (!memory.apply(batch).accepted) return std::nullopt;

        system_font_address_ = eon_system_font_address;
        const auto sequence = next_service_sequence_++;
        return DeuterosAmigaNativeGraphicsInitReceipt{runtime_generation_, sequence,
            call_address, a1_rastport_address, vector, return_address,
            static_cast<std::uint32_t>(batch.effects.size()),
            "ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53",
            function_sha256, DeuterosAmigaServiceOrigin::native_compatibility,
            descriptor_sha256, eon_system_font_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaMakeVPortReturnReceipt>
    observe_bootstrap_makevport_return(
        const DeuterosAmigaObservedMakeVPortReturn& observation) noexcept {
        if (!service_opened_ || runtime_generation_ == 0
            || next_service_sequence_ != 6 || makevport_return_observed_
            || observation.trace_sequence == 0
            || observation.call_address != 0x13188U || observation.vector != -0x0d8
            || observation.return_address != 0x1318cU
            || observation.a0_view_address != 0x12e00U
            || observation.a1_viewport_address != 0x12e12U
            || observation.a6_graphics_library_base != graphics_library_service_token_
            || system_font_address_ != 0x00d10000U) {
            return std::nullopt;
        }
        makevport_return_observed_ = true;
        return DeuterosAmigaMakeVPortReturnReceipt{
            runtime_generation_, next_service_sequence_++, observation,
            DeuterosAmigaServiceOrigin::external_observation, 0};
    }

    void reset() noexcept {
        service_opened_ = false;
        next_service_sequence_ = 1;
        system_font_address_ = 0;
        graphics_library_service_token_ = 0;
        makevport_return_observed_ = false;
    }
    [[nodiscard]] bool service_opened() const noexcept { return service_opened_; }
    [[nodiscard]] std::uint64_t runtime_generation() const noexcept {
        return runtime_generation_;
    }
    [[nodiscard]] std::uint32_t system_font_address() const noexcept {
        return system_font_address_;
    }

private:
    static void add_byte(NativeRuntimeEffectBatch& batch,
        const std::uint32_t address, const std::uint8_t value) {
        batch.effects.push_back({batch.effects.size() + 1U,
            {NativeRuntimeAddressSpace::linear, std::nullopt, address},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, value});
    }

    static void add_word(NativeRuntimeEffectBatch& batch,
        const std::uint32_t address, const std::uint16_t value) {
        batch.effects.push_back({batch.effects.size() + 1U,
            {NativeRuntimeAddressSpace::linear, std::nullopt, address},
            MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, value});
    }

    std::uint64_t runtime_generation_ = 0;
    std::uint64_t next_service_sequence_ = 1;
    bool service_opened_ = false;
    std::uint32_t system_font_address_ = 0;
    std::uint32_t graphics_library_service_token_ = 0;
    bool makevport_return_observed_ = false;
};

} // namespace eon
