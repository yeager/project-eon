#pragma once

#include "data/deuteros_amiga_title_stage.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace eon {

struct DeuterosAmigaObservedCustomChipWrite {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t base_address = 0;
    std::uint16_t register_offset = 0;
    std::uint16_t value = 0;
};

struct DeuterosAmigaDeferredCallbackExecCall {
    std::uint32_t exec_base_read_address = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
};

struct DeuterosAmigaTitleCallbackRegistrationLocalPlan {
    std::array<DeuterosAmigaObservedCustomChipWrite, 4> observed_writes{};
    std::uint32_t call_address = 0;
    std::uint32_t call_target = 0;
    std::uint32_t descriptor_address = 0;
    std::uint16_t descriptor_owner_offset = 0;
    std::uint32_t descriptor_owner_value = 0;
    std::uint16_t descriptor_callback_offset = 0;
    std::uint32_t descriptor_callback_value = 0;
    std::uint32_t request_address = 0;
    std::uint16_t request_command_offset = 0;
    std::uint16_t request_command_value = 0;
    std::uint16_t request_descriptor_offset = 0;
    std::uint32_t request_descriptor_value = 0;
    DeuterosAmigaDeferredCallbackExecCall deferred_exec_call;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedCallbackExecReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitlePostCallbackRegistrationAdvance {
    DeuterosAmigaObservedCallbackExecReturn observed_return;
    std::uint32_t registration_rts_address = 0;
    std::uint32_t caller_return_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

class DeuterosAmigaTitleCustomChipBoundarySession {
public:
    DeuterosAmigaTitleCustomChipBoundarySession(std::uint64_t preceding_trace_sequence,
        const DeuterosAmigaTitleStageProfile& stage,
        const DeuterosAmigaTitleCallbackRegistrationProfile& callback)
        : preceding_trace_sequence_(preceding_trace_sequence), callback_(callback) {
        if (preceding_trace_sequence == 0
            || stage.initialization_custom_base_address != 0xdff000
            || stage.initialization_custom_offsets != expected_offsets_
            || stage.initialization_custom_values != expected_values_
            || stage.initialization_internal_calls[3] != 0x1ef74
            || stage.initialization_internal_calls[4] != 0x206d4
            || callback.registration_entry_address != 0x1ef74
            || callback.descriptor_address != 0x1ef48
            || callback.descriptor_callback_offset != 0x12
            || callback.callback_address != 0x1f056
            || callback.request_address != 0x1eefa
            || callback.request_command_offset != 0x1c
            || callback.request_command_value != 9
            || callback.request_descriptor_offset != 0x28
            || callback.exec_base_address != 4 || callback.exec_vector != -0x1ce
            || callback.registration_return_address != 0x1f052
            || callback.registration_sha256
                != "f571a8e5e48c29fe3d6f493e503e2a3a0b3328ac4cafb425808eff48804c4f27") {
            throw std::runtime_error("Invalid Deuteros custom-chip boundary provenance");
        }
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCallbackRegistrationLocalPlan>
    observe_write(const DeuterosAmigaObservedCustomChipWrite& observation) {
        if (complete_) return std::nullopt;
        constexpr std::array<std::uint32_t, 4> sites{0x40498, 0x4049e, 0x404a4, 0x404aa};
        const auto previous = count_ == 0
            ? preceding_trace_sequence_ : observations_[count_ - 1].trace_sequence;
        if (count_ >= sites.size() || observation.trace_sequence <= previous
            || observation.instruction_address != sites[count_]
            || observation.base_address != 0xdff000
            || observation.register_offset != expected_offsets_[count_]
            || observation.value != expected_values_[count_]) {
            throw std::runtime_error("Deuteros custom-chip observation does not match boundary");
        }
        observations_[count_++] = observation;
        if (count_ != observations_.size()) return std::nullopt;
        complete_ = true;
        return DeuterosAmigaTitleCallbackRegistrationLocalPlan{observations_,
            0x404b0, callback_.registration_entry_address, callback_.descriptor_address,
            0x0e, 0x1ef40, callback_.descriptor_callback_offset, callback_.callback_address,
            callback_.request_address, callback_.request_command_offset,
            callback_.request_command_value, callback_.request_descriptor_offset,
            callback_.descriptor_address,
            {0x1f04a, callback_.exec_base_address, 0x1f04e,
                callback_.exec_vector, callback_.registration_return_address},
            0x1f04a};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCallbackRegistrationAdvance>
    observe_exec_return(const DeuterosAmigaObservedCallbackExecReturn& observation) {
        if (!complete_ || observed_exec_return_) return std::nullopt;
        if (observation.trace_sequence <= observations_.back().trace_sequence
            || observation.exec_base_source_address != 4
            || observation.call_address != 0x1f04e || observation.vector != -0x1ce
            || observation.return_address != 0x1f052) {
            throw std::runtime_error("Deuteros callback Exec return does not match boundary");
        }
        observed_exec_return_ = observation;
        return DeuterosAmigaTitlePostCallbackRegistrationAdvance{
            observation, 0x1f052, 0x404b6, 0x404b6, 0x206d4, 0x404b6};
    }

    [[nodiscard]] std::size_t observed_write_count() const noexcept { return count_; }
    [[nodiscard]] bool complete() const noexcept { return complete_; }
    [[nodiscard]] const std::optional<DeuterosAmigaObservedCallbackExecReturn>&
    observed_exec_return() const noexcept { return observed_exec_return_; }
    [[nodiscard]] std::uint32_t stop_before_address() const noexcept {
        return complete_ ? 0x1f04a : std::array<std::uint32_t, 4>{
            0x40498, 0x4049e, 0x404a4, 0x404aa}[count_];
    }

private:
    static constexpr std::array<std::uint16_t, 4> expected_offsets_{
        0x40, 0x42, 0x9a, 0x96};
    static constexpr std::array<std::uint16_t, 4> expected_values_{
        0x7fff, 0x7fff, 0xc000, 0x87ff};
    std::uint64_t preceding_trace_sequence_ = 0;
    DeuterosAmigaTitleCallbackRegistrationProfile callback_;
    std::array<DeuterosAmigaObservedCustomChipWrite, 4> observations_{};
    std::size_t count_ = 0;
    bool complete_ = false;
    std::optional<DeuterosAmigaObservedCallbackExecReturn> observed_exec_return_;
};

} // namespace eon
