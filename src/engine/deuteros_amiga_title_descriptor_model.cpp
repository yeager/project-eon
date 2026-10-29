#include "engine/deuteros_amiga_title_descriptor_model.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace eon {
namespace {
constexpr std::uint32_t kDescriptorTableAddress = 0x3f7fa;
constexpr std::uint32_t kDescriptorStride = 14;
constexpr std::uint32_t kDestinationTableAddress = 0x3f7c2;
constexpr std::uint32_t kFlagsAddress = 0x3f7be;
constexpr std::uint16_t kSelector = 0x13;
constexpr std::uint16_t kSelectionMask = 0x0c;
constexpr std::uint32_t kCallerAddress = 0x1fc08;
constexpr std::uint32_t kCallerReturnAddress = 0x1fc0e;
constexpr std::uint32_t kOuterCallerAddress = 0x40786;
constexpr std::uint32_t kOuterCallerReturnAddress = 0x4078c;
constexpr std::uint32_t kOuterPrefixAddress = 0x3f7a8;
constexpr std::uint32_t kOuterPrefixSize = 20;
constexpr std::uint32_t kEntryAddress = 0x3fbf8;
constexpr std::uint32_t kGateAddress = 0x3f752;
constexpr std::uint32_t kDescriptorBaseAddress = 0x3f7fa;
constexpr std::uint32_t kOuterScratchAddress = 0x3f77e;
constexpr std::uint32_t kTitleStageBaseAddress = 0x13000;
constexpr std::uint32_t kTitleStageSize = 0x6ca00;

bool in_title_stage(const std::uint32_t address, const std::size_t size) {
    const auto stage_end = static_cast<std::uint64_t>(kTitleStageBaseAddress)
        + kTitleStageSize;
    return address >= kTitleStageBaseAddress
        && static_cast<std::uint64_t>(address) + size <= stage_end;
}

std::uint32_t read_be32(const std::span<const std::uint8_t> bytes) {
    return (std::uint32_t{bytes[0]} << 24U) | (std::uint32_t{bytes[1]} << 16U)
        | (std::uint32_t{bytes[2]} << 8U) | std::uint32_t{bytes[3]};
}

std::uint16_t read_be16(const std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint16_t>((std::uint16_t{bytes[0]} << 8U) | bytes[1]);
}

NativeRuntimeLocation linear(const std::uint32_t address) {
    return {NativeRuntimeAddressSpace::linear, std::nullopt, address};
}
}

const DeuterosAmigaTitleDescriptorProfile&
deuteros_amiga_title_descriptor_profile() {
    static constexpr DeuterosAmigaTitleDescriptorProfile profile{
        "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
        0x3fbf8, 0x00b8, 0x1fbe6, kDescriptorTableAddress, kDescriptorStride,
        kFlagsAddress, {{0x3f7de, 0x3f7ec}},
        "a2301bf08c6c1c5615368687c620f17293d3f148e19934092854762ca91507b2",
        "native translation at an observed call boundary; scheduler remains unwired",
        0x79000, 0x1e000, 0x9abf8,
        "3b5800403216783e327c5c1d83bae66d5da6ea9ad89b952f6b1811d2d70e4d7e"};
    return profile;
}

std::optional<std::vector<DeuterosAmigaTitleDescriptorWrite>>
evaluate_deuteros_amiga_title_descriptor(
    const DeuterosAmigaTitleDescriptorInputs& in) {
    if (in.gate_byte != 0)
        return std::nullopt;

    const auto expected_descriptor = kDescriptorTableAddress
        + std::uint32_t{in.caller_d0} * kDescriptorStride;
    if (in.descriptor_address != expected_descriptor
        || in.descriptor_marker_source_address != in.descriptor_address
        || !in_title_stage(in.descriptor_address, 14))
        return std::nullopt;

    // At $3fc0e the routine tests the descriptor marker with BPL. A negative
    // marker makes it load a source-row pointer from descriptor+4; otherwise
    // the descriptor itself is the source row. Keep both branches explicit.
    const bool pointer_row = (in.descriptor_marker & 0x80000000U) != 0;
    if (!pointer_row) {
        const std::uint32_t row_marker =
            (std::uint32_t{in.source_row[0]} << 24U)
            | (std::uint32_t{in.source_row[1]} << 16U)
            | (std::uint32_t{in.source_row[2]} << 8U)
            | std::uint32_t{in.source_row[3]};
        if (row_marker != in.descriptor_marker)
            return std::nullopt;
    } else if (in.descriptor_data_pointer_source_address
        != in.descriptor_address + 4U) {
        return std::nullopt;
    }
    const auto expected_source = pointer_row
        ? in.descriptor_data_pointer : in.descriptor_address;
    if (in.source_row_address != expected_source || !in_title_stage(expected_source, 14))
        return std::nullopt;

    std::vector<DeuterosAmigaTitleDescriptorWrite> writes;
    writes.reserve(8);
    auto current_flags = in.flags_preimage;
    auto selector = in.caller_d1;
    auto destination = kDestinationTableAddress;
    for (std::size_t index = 0; index < 4; ++index) {
        const bool copy = (selector & 1U) != 0;
        selector = static_cast<std::uint16_t>(selector >> 1U);
        if (!copy) {
            destination += kDescriptorStride;
            continue;
        }
        const auto bit = static_cast<std::uint16_t>(1U << index);
        current_flags = static_cast<std::uint16_t>(current_flags | bit);
        writes.push_back({kFlagsAddress, {
            static_cast<std::uint8_t>(current_flags >> 8),
            static_cast<std::uint8_t>(current_flags & 0xff)}});
        writes.push_back({destination, std::vector<std::uint8_t>(
            in.source_row.begin(), in.source_row.end())});
        destination += kDescriptorStride;
    }
    return writes;
}

std::optional<DeuterosAmigaTitleDescriptorNativeAdvance>
advance_deuteros_amiga_title_descriptor_native(
    const NativeRuntimeMemory& memory,
    const DeuterosAmigaTitleDescriptorCallBoundary& boundary) {
    const auto& profile = deuteros_amiga_title_descriptor_profile();
    const bool selector_origin = boundary.origin
        == DeuterosAmigaTitleDescriptorCallOrigin::selector_dispatch;
    const bool response_origin = boundary.origin
        == DeuterosAmigaTitleDescriptorCallOrigin::response_loop;
    if (!selector_origin && !response_origin) return std::nullopt;
    const auto expected_return = boundary.origin
            == DeuterosAmigaTitleDescriptorCallOrigin::selector_dispatch
        ? kCallerReturnAddress : kOuterCallerReturnAddress;
    const auto expected_d0 = boundary.origin
            == DeuterosAmigaTitleDescriptorCallOrigin::selector_dispatch
        ? kSelector : std::uint16_t{0};
    const auto expected_d1 = boundary.origin
            == DeuterosAmigaTitleDescriptorCallOrigin::selector_dispatch
        ? kSelectionMask : std::uint16_t{0x000f};
    if (boundary.trace_sequence == 0 || boundary.release_sha256 != profile.release_sha256
        || boundary.program_counter != kEntryAddress
        || boundary.return_address != expected_return
        || boundary.d0 != expected_d0 || boundary.d1 != expected_d1
        || profile.source_disk_offset != profile.source_read_disk_offset
                + (profile.routine_address - profile.source_read_runtime_address)) {
        return std::nullopt;
    }

    const auto routine = memory.read_linear_range(profile.routine_address, profile.routine_size);
    const auto caller_address = selector_origin
        ? kCallerAddress : kOuterCallerAddress;
    const auto caller_hash = selector_origin
        ? profile.caller_sha256 : std::string_view{
            "3ae9f056e47c93fdd96ea96910bcd05db63b3813a433f62b214794d272c8211c"};
    const auto caller = memory.read_linear_range(caller_address, 6);
    const auto stack_return = memory.read_linear_range(boundary.stack_pointer, 4);
    if (!routine || !caller || !stack_return
        || to_hex(sha256(*routine)) != profile.routine_sha256
        || to_hex(sha256(*caller)) != caller_hash
        || read_be32(*stack_return) != boundary.return_address) {
        return std::nullopt;
    }
    if (selector_origin) {
        if (*caller != std::vector<std::uint8_t>{0x4e, 0xb9, 0x00, 0x03, 0xfb, 0xf8})
            return std::nullopt;
    } else {
        const auto prefix = memory.read_linear_range(kOuterPrefixAddress, kOuterPrefixSize);
        const auto scratch = memory.read_linear_range(kOuterScratchAddress, 2);
        if (!prefix || !scratch
            || to_hex(sha256(*prefix))
                != "2ed49faf9d9a87986b075890b247f699e89a8e9e1b2970eca7d9830bd952a9ed"
            || *caller != std::vector<std::uint8_t>{0x4e, 0xb9, 0x00, 0x03, 0xf7, 0xa8}
            || *scratch != std::vector<std::uint8_t>{0x00, 0x00}) {
            return std::nullopt;
        }
    }

    const auto gate = memory.read_linear_range(kGateAddress, 1);
    if (!gate) return std::nullopt;
    DeuterosAmigaTitleDescriptorNativeAdvance advance{
        boundary.trace_sequence, boundary.return_address, (*gate)[0], {}};
    // The hash-bound entry performs TST.B/BNE/RTS before touching the
    // descriptor when this gate is set. Preserve that native no-op return.
    if ((*gate)[0] != 0) return advance;

    const auto descriptor_address = kDescriptorBaseAddress
        + std::uint32_t{boundary.d0} * kDescriptorStride;
    if (!in_title_stage(descriptor_address, 14)) return std::nullopt;
    const auto descriptor = memory.read_linear_range(descriptor_address, 14);
    const auto flags = memory.read_linear_range(kFlagsAddress, 2);
    if (!descriptor || !flags) return std::nullopt;
    const auto marker = read_be32(std::span<const std::uint8_t>(*descriptor).first(4));
    const auto pointer = read_be32(std::span<const std::uint8_t>(*descriptor).subspan(4, 4));
    const bool indirect = (marker & 0x80000000U) != 0;
    const auto row_address = indirect ? pointer : descriptor_address;
    if (!in_title_stage(row_address, 14)) return std::nullopt;
    const auto row = memory.read_linear_range(row_address, 14);
    if (!row) return std::nullopt;

    DeuterosAmigaTitleDescriptorInputs inputs;
    inputs.gate_byte = (*gate)[0];
    inputs.caller_d0 = boundary.d0;
    inputs.caller_d1 = boundary.d1;
    inputs.descriptor_address = descriptor_address;
    inputs.descriptor_marker_source_address = descriptor_address;
    inputs.descriptor_marker = marker;
    inputs.descriptor_data_pointer_source_address = descriptor_address + 4U;
    inputs.descriptor_data_pointer = pointer;
    inputs.source_row_address = row_address;
    std::copy(row->begin(), row->end(), inputs.source_row.begin());
    inputs.flags_preimage = read_be16(*flags);
    const auto writes = evaluate_deuteros_amiga_title_descriptor(inputs);
    if (!writes) return std::nullopt;

    advance.ordered_batches.reserve(writes->size());
    for (std::size_t index = 0; index < writes->size(); ++index) {
        const auto& write = (*writes)[index];
        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-descriptor-" + std::to_string(boundary.trace_sequence)
                + "-" + std::to_string(index + 1U), true, {}};
        if (write.bytes.size() == 2) {
            const auto value = static_cast<std::uint32_t>(
                (std::uint16_t{write.bytes[0]} << 8U) | write.bytes[1]);
            batch.effects.push_back({1, linear(write.address),
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, value});
        } else {
            batch.effects.reserve(write.bytes.size());
            for (std::size_t byte = 0; byte < write.bytes.size(); ++byte) {
                batch.effects.push_back({byte + 1U,
                    linear(write.address + static_cast<std::uint32_t>(byte)),
                    MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                    write.bytes[byte]});
            }
        }
        advance.ordered_batches.push_back(std::move(batch));
    }
    return advance;
}

} // namespace eon
