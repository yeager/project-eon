#include "engine/millennium_dos_video_function_six_mcga_copy_loop_session.hpp"
#include "engine/millennium_dos_video_function_six_mcga_caller_session.hpp"

#include <array>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace eon {

MillenniumDosVideoFunctionSixMcgaCopyMemoryResult
execute_millennium_dos_video_function_six_mcga_copy_loop(
    const std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaCopyLoopOutcome& observed_loop,
    const MillenniumDosRealModeAddressMapping address_mapping,
    NativeRuntimeMemory& memory,
    std::string batch_id,
    const std::uint32_t max_rows,
    const std::uint64_t max_total_bytes) {
    const auto reject = [](std::string reason) {
        return MillenniumDosVideoFunctionSixMcgaCopyMemoryResult{
            false, false, std::move(reason), 0, 0};
    };
    if (batch_id.empty() || max_total_bytes == 0 || max_total_bytes > 1'048'576) {
        return reject("MCGA copy memory execution requires a batch id and a bounded byte limit");
    }
    if (address_mapping != MillenniumDosRealModeAddressMapping::a20_wrapped_20_bit
        && address_mapping != MillenniumDosRealModeAddressMapping::unwrapped_21_bit) {
        return reject("Unknown DOS real-mode address mapping");
    }

    MillenniumDosVideoFunctionSixMcgaCopyLoopOutcome expected_loop;
    try {
        expected_loop = MillenniumDosVideoFunctionSixMcgaCopyLoopSession(
            english_mcga_driver, observed_loop.kind,
            observed_loop.initial_registers, max_rows).outcome();
    } catch (const std::exception& error) {
        return reject(error.what());
    }
    if (expected_loop != observed_loop) {
        return reject("MCGA caller loop outcome does not match the hash-bound register model");
    }

    std::uint64_t requested_bytes = 0;
    for (const auto& row : observed_loop.rows) {
        for (const auto& copy_phase : row.phases) {
            const auto bytes = static_cast<std::uint64_t>(copy_phase.element_count)
                * copy_phase.element_size;
            if (bytes > max_total_bytes - requested_bytes) {
                return reject("MCGA copy memory execution exceeds its explicit byte limit");
            }
            requested_bytes += bytes;
        }
    }

    const auto physical_address = [address_mapping](const std::uint16_t segment,
        const std::uint16_t offset, const std::uint8_t byte_in_element)
        -> std::optional<std::uint64_t> {
        auto address = static_cast<std::uint64_t>(segment) * 16U + offset + byte_in_element;
        if (address_mapping == MillenniumDosRealModeAddressMapping::a20_wrapped_20_bit) {
            address &= 0xfffffU;
        } else if (address > 0x10fff0U) {
            return std::nullopt;
        }
        return address;
    };
    const auto location = [](const std::uint64_t address) {
        return NativeRuntimeLocation{NativeRuntimeAddressSpace::linear, std::nullopt, address};
    };

    // Later MOVS elements observe earlier writes, including aliases where
    // different segment:offset pairs resolve to the same selected bus byte.
    // The final atomic batch has unique addresses because NativeRuntimeMemory
    // rejects duplicate effects in a batch.
    std::map<std::uint64_t, std::uint8_t> pending_bytes;
    std::uint64_t byte_write_count = 0;
    for (const auto& row : observed_loop.rows) {
        for (const auto& copy_phase : row.phases) {
            auto source = copy_phase.source_start;
            auto destination = copy_phase.destination_start;
            const auto step = copy_phase.direction_flag
                ? static_cast<std::uint16_t>(0U - copy_phase.element_size)
                : static_cast<std::uint16_t>(copy_phase.element_size);
            for (std::uint32_t element = 0; element < copy_phase.element_count; ++element) {
                std::array<std::uint64_t, 2> destination_addresses{};
                std::array<std::uint8_t, 2> values{};
                for (std::uint8_t byte = 0; byte < copy_phase.element_size; ++byte) {
                    const auto source_address = physical_address(copy_phase.ds, source, byte);
                    const auto destination_address = physical_address(copy_phase.es, destination, byte);
                    if (!source_address || !destination_address) {
                        return reject("MCGA MOVS address is outside the selected real-mode bus");
                    }
                    destination_addresses[byte] = *destination_address;
                    const auto pending = pending_bytes.find(*source_address);
                    const auto value = pending == pending_bytes.end()
                        ? memory.read_byte(location(*source_address))
                        : std::optional<std::uint8_t>(pending->second);
                    if (!value) {
                        return reject("MCGA MOVS source byte is not initialized in runtime memory");
                    }
                    values[byte] = *value;
                }
                // MOVSW reads the full word before writing either byte.
                for (std::uint8_t byte = 0; byte < copy_phase.element_size; ++byte) {
                    pending_bytes.insert_or_assign(destination_addresses[byte], values[byte]);
                    ++byte_write_count;
                }
                source = static_cast<std::uint16_t>(source + step);
                destination = static_cast<std::uint16_t>(destination + step);
            }
        }
    }

    if (byte_write_count == 0) return {true, false, {}, 0, 0};
    NativeRuntimeEffectBatch batch{std::move(batch_id), true, {}};
    batch.effects.reserve(pending_bytes.size());
    for (const auto& [address, value] : pending_bytes) {
        batch.effects.push_back({batch.effects.size() + 1, location(address),
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::little_endian, value});
    }
    const auto applied = memory.apply(batch);
    if (!applied.accepted) return reject(applied.error);
    return {true, true, {}, byte_write_count, pending_bytes.size()};
}

MillenniumDosVideoFunctionSixMcgaCopyMemoryResult
execute_millennium_dos_video_function_six_mcga_copy_loop(
    const std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaCopyLoopKind kind,
    const MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters initial_registers,
    const MillenniumDosRealModeAddressMapping address_mapping,
    NativeRuntimeMemory& memory,
    std::string batch_id,
    const std::uint32_t max_rows,
    const std::uint64_t max_total_bytes) {
    try {
        const auto loop = MillenniumDosVideoFunctionSixMcgaCopyLoopSession(
            english_mcga_driver, kind, initial_registers, max_rows).outcome();
        return execute_millennium_dos_video_function_six_mcga_copy_loop(
            english_mcga_driver, loop, address_mapping, memory, std::move(batch_id),
            max_rows, max_total_bytes);
    } catch (const std::exception& error) {
        return {false, false, error.what(), 0, 0};
    }
}

MillenniumDosVideoFunctionSixMcgaCopyMemoryResult
execute_millennium_dos_video_function_six_mcga_copy_loop(
    const std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaCallerOutcome& caller,
    const MillenniumDosRealModeAddressMapping address_mapping,
    NativeRuntimeMemory& memory,
    std::string batch_id,
    const std::uint64_t max_total_bytes) {
    const auto reject = [](std::string reason) {
        return MillenniumDosVideoFunctionSixMcgaCopyMemoryResult{
            false, false, std::move(reason), 0, 0};
    };
    const auto is_copy_return = caller.instruction_boundary == 0x079c
        || caller.instruction_boundary == 0x07b4;
    if (!is_copy_return || !caller.caller_return_ip || !caller.copy_loop
        || !caller.mode_read || caller.mode_read->instruction_address != 0x077f
        || caller.mode_read->value != 0x88 || caller.stack_reads.size() != 6
        || caller.copy_loop->next_instruction != caller.instruction_boundary
        || caller.call_stack_write.instruction_address != 0x076f
        || caller.call_stack_write.value != 0x0772
        || caller.stack_reads.back().instruction_address != caller.instruction_boundary
        || caller.stack_reads.back().value != *caller.caller_return_ip
        || static_cast<std::uint16_t>(caller.stack_reads.back().offset + 2U) != caller.final_sp) {
        return reject("MCGA copy memory execution requires a completed caller copy-loop return");
    }
    return execute_millennium_dos_video_function_six_mcga_copy_loop(
        english_mcga_driver, *caller.copy_loop, address_mapping, memory,
        std::move(batch_id), 4096, max_total_bytes);
}

} // namespace eon
