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

struct DeuterosAmigaTitleTailFirstGraphicsLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t wrapper_rts_address = 0;
    std::uint32_t caller_resume_address = 0;
    std::uint32_t restored_a6_value = 0;
    std::uint32_t destination_base = 0;
    std::uint32_t source_address = 0;
    std::uint32_t copy_instruction_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTailCopyWords {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::array<std::uint16_t, 4> observed_words{};
};

struct DeuterosAmigaTitleTailCopyLocalPlan {
    DeuterosAmigaObservedTailCopyWords observation;
    std::array<std::uint32_t, 4> destination_addresses{};
    std::array<std::uint16_t, 4> destination_values{};
    std::array<std::uint32_t, 2> literal_destination_addresses{};
    std::array<std::uint16_t, 2> literal_values{};
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t unresolved_read_address = 0;
    std::uint32_t unresolved_read_source = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTailSelectionWords {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::array<std::uint32_t, 8> source_addresses{};
    std::array<std::uint16_t, 8> observed_words{};
};

struct DeuterosAmigaTitleTailSelectionLocalPlan {
    DeuterosAmigaObservedTailSelectionWords observation;
    std::array<std::uint16_t, 2> selected_words{};
    std::array<std::uint32_t, 2> destination_addresses{};
    std::uint16_t graphics_d0 = 0;
    std::uint16_t graphics_d1 = 0;
    std::uint32_t a0_value = 0;
    std::uint32_t a1_value = 0;
    std::uint32_t library_base_source_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t next_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleTailSecondGraphicsLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t register_restore_address = 0;
    std::uint32_t wrapper_rts_address = 0;
    std::uint32_t caller_resume_address = 0;
    std::array<std::uint32_t, 2> literal_destination_addresses{};
    std::array<std::uint16_t, 2> literal_values{};
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t unresolved_read_address = 0;
    std::uint32_t unresolved_read_source = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleTailRepeatedGraphicsLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t register_restore_address = 0;
    std::uint32_t wrapper_rts_address = 0;
    std::uint32_t caller_resume_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t next_a0_value = 0;
    std::uint32_t next_a1_value = 0;
    std::uint32_t next_a2_pointer_cell = 0;
    std::uint32_t next_library_base_source_address = 0;
    std::uint32_t next_graphics_call_address = 0;
    std::int16_t next_graphics_vector = 0;
    std::uint32_t next_graphics_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleTailRepeatedWrapperReturnPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t generation = 0;
    std::uint32_t wrapper_rts_address = 0;
    std::uint32_t tail_caller_resume_address = 0;
    std::uint32_t tail_rts_address = 0;
    std::uint32_t batch_third_return_address = 0;
    std::uint32_t batch_fourth_call_address = 0;
    std::uint32_t batch_fourth_call_target = 0;
    std::uint32_t batch_fourth_return_address = 0;
    std::uint32_t batch_rts_address = 0;
    std::uint32_t batch_caller_resume_address = 0;
    std::uint32_t unresolved_read_address = 0;
    std::uint32_t unresolved_read_source = 0;
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
        tail_second_callee_ =
            parse_deuteros_amiga_title_post_exec_tail_second_callee_profile(disk, plan);
        tail_fourth_callee_ =
            parse_deuteros_amiga_title_post_exec_tail_fourth_callee_profile(disk, plan);
        fourth_service_ = parse_deuteros_amiga_title_post_exec_fourth_service_profile(
            disk, plan);
        tail_return_ = parse_deuteros_amiga_title_post_exec_tail_return_profile(disk, plan);
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

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailFirstGraphicsLocalPlan>
    observe_tail_first_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_graphics_service_third_ || observed_tail_first_graphics_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_graphics_service_third_->trace_sequence
            || observation.library_base_source_address
                != tail_first_callee_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x20112
            || observation.vector != tail_first_callee_.graphics_library_vector
            || observation.return_address != tail_first_callee_.vector_return_address) {
            throw std::runtime_error("Deuteros tail first graphics return does not match boundary");
        }
        observed_tail_first_graphics_ = observation;
        return DeuterosAmigaTitleTailFirstGraphicsLocalPlan{observation,
            tail_first_callee_.vector_return_address, 0x201da,
            third_service_.dispatcher_a6_literal, 0x1ffc8, 0x1f372,
            0x201e4, 0x201e4};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailCopyLocalPlan>
    observe_tail_copy_words(const DeuterosAmigaObservedTailCopyWords& observation) {
        if (!observed_tail_first_graphics_ || observed_tail_copy_words_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_first_graphics_->trace_sequence
            || observation.instruction_address != 0x201e4
            || observation.source_address != third_service_.dispatcher_a6_literal) {
            throw std::runtime_error("Deuteros tail word-copy observation does not match boundary");
        }
        observed_tail_copy_words_ = observation;
        return DeuterosAmigaTitleTailCopyLocalPlan{observation,
            {{0x1ffca, 0x1ffcc, 0x1ffd0, 0x1ffd2}}, observation.observed_words,
            {{0x1ee12, 0x1ee10}}, {{0xffff, 0xffff}},
            tail_second_callee_.caller_address, tail_second_callee_.entry_address,
            tail_second_callee_.entry_address, 0x1ffc8, tail_second_callee_.entry_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSelectionLocalPlan>
    observe_tail_selection_words(
        const DeuterosAmigaObservedTailSelectionWords& observation) {
        constexpr std::array<std::uint32_t, 8> sources{{
            0x1ffc8, 0x1ee10, 0x1ffca, 0x1ffcc,
            0x1ffce, 0x1ee12, 0x1ffd0, 0x1ffd2}};
        if (!observed_tail_copy_words_ || observed_tail_selection_words_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_copy_words_->trace_sequence
            || observation.instruction_address != tail_second_callee_.entry_address
            || observation.source_addresses != sources) {
            throw std::runtime_error("Deuteros tail selection observation does not match boundary");
        }
        observed_tail_selection_words_ = observation;
        const auto select = [](std::uint16_t current, std::uint16_t adjustment,
                               std::uint16_t lower, std::uint16_t upper) {
            const auto sum = static_cast<std::uint16_t>(current + adjustment);
            if (static_cast<std::int16_t>(adjustment) < 0) {
                return static_cast<std::int16_t>(sum) < 0 || sum < lower ? lower : sum;
            }
            return sum < upper ? sum : upper;
        };
        const auto first = select(observation.observed_words[0], observation.observed_words[1],
                                  observation.observed_words[2], observation.observed_words[3]);
        const auto second = select(observation.observed_words[4], observation.observed_words[5],
                                   observation.observed_words[6], observation.observed_words[7]);
        return DeuterosAmigaTitleTailSelectionLocalPlan{observation, {{first, second}},
            {{0x1ffc8, 0x1ffce}},
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(first - 0x10U) >> 1U),
            static_cast<std::uint16_t>(second - 6U),
            tail_second_callee_.a0_literal, tail_second_callee_.a1_literal,
            tail_second_callee_.graphics_library_base_address,
            0x201b6, tail_second_callee_.graphics_library_vector,
            tail_second_callee_.vector_return_address, 0x201b6};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSecondGraphicsLocalPlan>
    observe_tail_second_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_tail_selection_words_ || observed_tail_second_graphics_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_selection_words_->trace_sequence
            || observation.library_base_source_address
                != tail_second_callee_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x201b6
            || observation.vector != tail_second_callee_.graphics_library_vector
            || observation.return_address != tail_second_callee_.vector_return_address) {
            throw std::runtime_error("Deuteros tail second graphics return does not match boundary");
        }
        observed_tail_second_graphics_ = observation;
        return DeuterosAmigaTitleTailSecondGraphicsLocalPlan{observation,
            0x201ba, 0x201be, tail_second_callee_.caller_continuation_address,
            {{0x1ee12, 0x1ee10}}, {{1, 1}}, 0x20212, 0x20118,
            0x20118, 0x1ffc8, 0x20118};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSelectionLocalPlan>
    observe_tail_repeated_selection_words(
        const DeuterosAmigaObservedTailSelectionWords& observation) {
        constexpr std::array<std::uint32_t, 8> sources{{
            0x1ffc8, 0x1ee10, 0x1ffca, 0x1ffcc,
            0x1ffce, 0x1ee12, 0x1ffd0, 0x1ffd2}};
        if (!observed_tail_second_graphics_ || observed_tail_repeated_selection_words_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_second_graphics_->trace_sequence
            || observation.instruction_address != tail_second_callee_.entry_address
            || observation.source_addresses != sources) {
            throw std::runtime_error("Deuteros repeated tail selection does not match boundary");
        }
        observed_tail_repeated_selection_words_ = observation;
        const auto select = [](std::uint16_t current, std::uint16_t adjustment,
                               std::uint16_t lower, std::uint16_t upper) {
            const auto sum = static_cast<std::uint16_t>(current + adjustment);
            if (static_cast<std::int16_t>(adjustment) < 0) {
                return static_cast<std::int16_t>(sum) < 0 || sum < lower ? lower : sum;
            }
            return sum < upper ? sum : upper;
        };
        const auto first = select(observation.observed_words[0], observation.observed_words[1],
                                  observation.observed_words[2], observation.observed_words[3]);
        const auto second = select(observation.observed_words[4], observation.observed_words[5],
                                   observation.observed_words[6], observation.observed_words[7]);
        return DeuterosAmigaTitleTailSelectionLocalPlan{observation, {{first, second}},
            {{0x1ffc8, 0x1ffce}},
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(first - 0x10U) >> 1U),
            static_cast<std::uint16_t>(second - 6U),
            tail_second_callee_.a0_literal, tail_second_callee_.a1_literal,
            tail_second_callee_.graphics_library_base_address,
            0x201b6, tail_second_callee_.graphics_library_vector,
            tail_second_callee_.vector_return_address, 0x201b6};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailRepeatedGraphicsLocalPlan>
    observe_tail_repeated_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_tail_repeated_selection_words_ || observed_tail_repeated_graphics_) {
            return std::nullopt;
        }
        if (observation.trace_sequence
                <= observed_tail_repeated_selection_words_->trace_sequence
            || observation.library_base_source_address
                != tail_second_callee_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x201b6
            || observation.vector != tail_second_callee_.graphics_library_vector
            || observation.return_address != tail_second_callee_.vector_return_address) {
            throw std::runtime_error("Deuteros repeated tail graphics return does not match boundary");
        }
        observed_tail_repeated_graphics_ = observation;
        return DeuterosAmigaTitleTailRepeatedGraphicsLocalPlan{observation,
            0x201ba, 0x201be, 0x20216, tail_fourth_callee_.caller_address,
            tail_fourth_callee_.entry_address, tail_fourth_callee_.a0_literal,
            tail_fourth_callee_.a1_literal, tail_fourth_callee_.a2_pointer_cell_address,
            tail_fourth_callee_.graphics_library_base_address, 0x200f4,
            tail_fourth_callee_.graphics_library_vector,
            tail_fourth_callee_.vector_return_address, 0x200f4};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailRepeatedWrapperReturnPlan>
    observe_tail_repeated_wrapper_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_tail_repeated_graphics_ || observed_tail_repeated_wrapper_graphics_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_repeated_graphics_->trace_sequence
            || observation.library_base_source_address
                != tail_fourth_callee_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x200f4
            || observation.vector != tail_fourth_callee_.graphics_library_vector
            || observation.return_address != tail_fourth_callee_.vector_return_address) {
            throw std::runtime_error("Deuteros repeated wrapper graphics return does not match boundary");
        }
        observed_tail_repeated_wrapper_graphics_ = observation;
        return DeuterosAmigaTitleTailRepeatedWrapperReturnPlan{observation, 2,
            tail_fourth_callee_.vector_return_address,
            tail_fourth_callee_.caller_continuation_address,
            tail_dispatch_.return_address - 2U, third_service_.caller_address + 6U,
            fourth_service_.caller_address, fourth_service_.callee_address,
            fourth_service_.caller_return_address, fourth_service_.caller_return_address,
            tail_return_.continuation_address, 0x404da,
            tail_return_.source_table_address, 0x404da};
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
    DeuterosAmigaTitlePostExecTailSecondCalleeProfile tail_second_callee_;
    DeuterosAmigaTitlePostExecTailFourthCalleeProfile tail_fourth_callee_;
    DeuterosAmigaTitlePostExecFourthServiceProfile fourth_service_;
    DeuterosAmigaTitlePostExecTailReturnProfile tail_return_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_graphics_service_first_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_graphics_service_second_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_graphics_service_third_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_tail_first_graphics_;
    std::optional<DeuterosAmigaObservedTailCopyWords> observed_tail_copy_words_;
    std::optional<DeuterosAmigaObservedTailSelectionWords>
        observed_tail_selection_words_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_tail_second_graphics_;
    std::optional<DeuterosAmigaObservedTailSelectionWords>
        observed_tail_repeated_selection_words_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_tail_repeated_graphics_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_tail_repeated_wrapper_graphics_;
};

} // namespace eon
