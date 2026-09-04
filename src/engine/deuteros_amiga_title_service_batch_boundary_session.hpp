#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/sha256.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>

namespace eon {

struct DeuterosAmigaObservedGraphicsVectorReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t library_base_source_address = 0;
    std::uint32_t observed_library_base = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleServiceBatchLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t source_address = 0;
    std::uint32_t destination_address = 0;
    std::uint32_t count_d0 = 0;
    std::uint32_t graphics_helper_rts_address = 0;
    std::uint32_t second_call_address = 0;
    std::uint32_t second_call_target = 0;
    std::uint32_t zero_word_destination = 0;
    std::uint32_t literal_word_destination = 0;
    std::uint16_t literal_word_value = 0;
    std::uint32_t literal_long_destination = 0;
    std::uint32_t literal_long_value = 0;
    std::uint32_t unresolved_read_address = 0;
    std::uint32_t unresolved_read_source = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedServiceWordRead {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint16_t observed_value = 0;
};

struct DeuterosAmigaTitlePostServiceWordLocalPlan {
    DeuterosAmigaObservedServiceWordRead observation;
    std::uint32_t destination_address = 0;
    std::uint16_t destination_value = 0;
    std::uint32_t routine_rts_address = 0;
    std::uint32_t batch_next_call_address = 0;
    std::uint32_t batch_next_call_target = 0;
    std::uint32_t nested_call_address = 0;
    std::uint32_t nested_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleGraphicsServiceFirstLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t local_d0_value = 0;
    std::uint32_t descriptor_a0_value = 0;
    std::uint32_t next_library_base_source_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t next_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleGraphicsServiceSecondLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t result_byte_destination = 0;
    std::uint8_t result_low_byte = 0;
    std::uint32_t pointer_destination = 0;
    std::uint32_t pointer_value = 0;
    std::uint32_t descriptor_address = 0;
    std::array<std::uint16_t, 3> descriptor_offsets{};
    std::array<std::uint16_t, 3> descriptor_values{};
    std::uint32_t next_a0_value = 0;
    std::uint32_t next_a1_value = 0;
    std::uint32_t next_a2_pointer_cell = 0;
    std::uint32_t next_library_base_source_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t next_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

// The third graphics return completes `$20094`. The original caller then
// installs its literal local A6 value, tail-jumps to `$201d2`, and enters the
// first local setup wrapper. Keep pointer cells as addresses only: their
// runtime contents and the next graphics result are external observations.
struct DeuterosAmigaTitleGraphicsServiceThirdLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t service_rts_address = 0;
    std::uint32_t dispatcher_return_address = 0;
    std::uint32_t dispatcher_a6_value = 0;
    std::uint32_t dispatcher_jump_address = 0;
    std::uint32_t tail_entry_address = 0;
    std::uint32_t first_call_address = 0;
    std::uint32_t first_call_target = 0;
    std::uint32_t next_a0_value = 0;
    std::uint32_t next_a1_value = 0;
    std::uint32_t next_a2_pointer_cell = 0;
    std::uint32_t next_library_base_source_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t next_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

class DeuterosAmigaTitleServiceBatchBoundarySession {
public:
    DeuterosAmigaTitleServiceBatchBoundarySession(
        const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan) {
        const auto at = [&](std::uint32_t address, std::size_t length) {
            return disk.bytes(plan.title_stage.disk_offset
                + address - plan.title_stage.destination, length);
        };
        const auto batch = parse_deuteros_amiga_title_post_exec_service_batch_profile(
            disk, plan);
        third_service_ = parse_deuteros_amiga_title_post_exec_third_service_profile(
            disk, plan);
        tail_dispatch_ = parse_deuteros_amiga_title_post_exec_tail_dispatch_profile(
            disk, plan);
        tail_first_callee_ =
            parse_deuteros_amiga_title_post_exec_tail_first_callee_profile(disk, plan);
        if (to_hex(sha256(at(0x403c8, 30)))
                != "3f9cf2302a4078faddd0796fc05268386d46c4be64f294b8082ba085b9609f5f"
            || to_hex(sha256(at(0x20510, 38)))
                != "60ee2fcb4a18f62cd2066aba2429e760a64f14cd3f07f3cfe8467972030008bc"
            || to_hex(sha256(at(0x1f37a, 24)))
                != "9dd36cbd04608b7381526479275f1846284f87886b725bf581ade813f70d10f5"
            || batch.call_site_address != 0x404ce || batch.callee_address != 0x403f4
            || batch.direct_callee_addresses[0] != 0x403c8
            || batch.direct_callee_addresses[1] != 0x20510
            || batch.direct_callee_addresses[2] != 0x1f37a) {
            throw std::runtime_error("Unsupported Deuteros service batch prefix");
        }
    }

    void enter(std::uint64_t preceding_sequence, std::uint32_t graphics_library_base) {
        if (armed_ || preceding_sequence == 0 || graphics_library_base == 0) {
            throw std::runtime_error("Invalid Deuteros service batch entry");
        }
        armed_ = true;
        preceding_sequence_ = preceding_sequence;
        graphics_library_base_ = graphics_library_base;
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleServiceBatchLocalPlan>
    observe_graphics_return(const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!armed_ || observed_) return std::nullopt;
        if (observation.trace_sequence <= preceding_sequence_
            || observation.library_base_source_address != 0x12fec
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x403e0 || observation.vector != -0xc0
            || observation.return_address != 0x403e4) {
            throw std::runtime_error("Deuteros graphics return does not match service batch");
        }
        observed_ = observation;
        return DeuterosAmigaTitleServiceBatchLocalPlan{observation,
            0x1ed24, 0x12e12, 0x14, 0x403e4, 0x403fa, 0x20510,
            0x202c4, 0x2027e, 0xf690, 0x20280, 1,
            0x2052a, 0x20276, 0x2052a};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostServiceWordLocalPlan>
    observe_runtime_word(const DeuterosAmigaObservedServiceWordRead& observation) {
        if (!observed_ || observed_word_) return std::nullopt;
        if (observation.trace_sequence <= observed_->trace_sequence
            || observation.instruction_address != 0x2052a
            || observation.source_address != 0x20276) {
            throw std::runtime_error("Deuteros service word observation does not match boundary");
        }
        observed_word_ = observation;
        return DeuterosAmigaTitlePostServiceWordLocalPlan{observation,
            0x2027c, observation.observed_value, 0x20534,
            0x40400, 0x1f37a, 0x1f37a, 0x20094, 0x1f37a};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceFirstLocalPlan>
    observe_graphics_service_first_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_word_ || observed_graphics_service_first_) return std::nullopt;
        if (observation.trace_sequence <= observed_word_->trace_sequence
            || observation.library_base_source_address != third_service_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x2009c
            || observation.vector != third_service_.graphics_library_vectors[0]
            || observation.return_address != 0x200a0) {
            throw std::runtime_error("Deuteros first graphics-service return does not match boundary");
        }
        observed_graphics_service_first_ = observation;
        return DeuterosAmigaTitleGraphicsServiceFirstLocalPlan{observation,
            0xffffffff, third_service_.descriptor_address,
            third_service_.graphics_library_base_address,
            0x200b0, third_service_.graphics_library_vectors[1], 0x200b4, 0x200b0};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceSecondLocalPlan>
    observe_graphics_service_second_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_graphics_service_first_ || observed_graphics_service_second_) {
            return std::nullopt;
        }
        if (observation.trace_sequence
                <= observed_graphics_service_first_->trace_sequence
            || observation.library_base_source_address
                != third_service_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x200b0
            || observation.vector != third_service_.graphics_library_vectors[1]
            || observation.return_address != 0x200b4) {
            throw std::runtime_error("Deuteros second graphics-service return does not match boundary");
        }
        observed_graphics_service_second_ = observation;
        return DeuterosAmigaTitleGraphicsServiceSecondLocalPlan{observation,
            third_service_.status_byte_address,
            static_cast<std::uint8_t>(observation.result_d0 & 0xffU),
            third_service_.destination_pointer_cell_address,
            third_service_.destination_pointer_literal, third_service_.descriptor_address,
            third_service_.descriptor_offsets, third_service_.descriptor_values,
            0x12e12, third_service_.descriptor_address,
            third_service_.destination_pointer_cell_address,
            third_service_.graphics_library_base_address,
            0x200f4, third_service_.graphics_library_vectors[2], 0x200f8, 0x200f4};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceThirdLocalPlan>
    observe_graphics_service_third_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_graphics_service_second_ || observed_graphics_service_third_) {
            return std::nullopt;
        }
        if (observation.trace_sequence
                <= observed_graphics_service_second_->trace_sequence
            || observation.library_base_source_address
                != third_service_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x200f4
            || observation.vector != third_service_.graphics_library_vectors[2]
            || observation.return_address != 0x200f8) {
            throw std::runtime_error("Deuteros third graphics-service return does not match boundary");
        }
        observed_graphics_service_third_ = observation;
        return DeuterosAmigaTitleGraphicsServiceThirdLocalPlan{observation,
            0x200f8, 0x1f380, third_service_.dispatcher_a6_literal,
            third_service_.dispatcher_tail_jump_address, tail_dispatch_.entry_address,
            tail_first_callee_.caller_address, tail_first_callee_.entry_address,
            tail_first_callee_.a0_literal, tail_first_callee_.a1_literal,
            tail_first_callee_.a2_pointer_cell_address,
            tail_first_callee_.graphics_library_base_address,
            0x20112, tail_first_callee_.graphics_library_vector,
            tail_first_callee_.vector_return_address, 0x20112};
    }

private:
    bool armed_ = false;
    std::uint64_t preceding_sequence_ = 0;
    std::uint32_t graphics_library_base_ = 0;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn> observed_;
    std::optional<DeuterosAmigaObservedServiceWordRead> observed_word_;
    DeuterosAmigaTitlePostExecThirdServiceProfile third_service_;
    DeuterosAmigaTitlePostExecTailDispatchProfile tail_dispatch_;
    DeuterosAmigaTitlePostExecTailFirstCalleeProfile tail_first_callee_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_graphics_service_first_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_graphics_service_second_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_graphics_service_third_;
};

} // namespace eon
