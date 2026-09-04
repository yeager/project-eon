#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/sha256.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>

namespace eon {

struct DeuterosAmigaObservedServiceSetupExecReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleServiceSetupLocalPlan {
    DeuterosAmigaObservedServiceSetupExecReturn observed_return;
    std::uint32_t saved_stack_value = 0;
    std::uint32_t descriptor_address = 0;
    std::uint16_t pointer_offset = 0;
    std::uint32_t pointer_value = 0;
    std::uint16_t byte_9_value = 0;
    std::uint16_t byte_8_value = 0;
    std::uint16_t byte_15_value = 0;
    std::uint16_t result_offset = 0;
    std::uint32_t result_value = 0;
    std::uint32_t next_exec_base_read_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleSecondServiceLocalPlan {
    DeuterosAmigaObservedServiceSetupExecReturn observed_return;
    std::uint32_t a0_value = 0;
    std::uint32_t a1_value = 0;
    std::uint32_t global_pointer_address = 0;
    std::uint32_t global_pointer_value = 0;
    std::uint16_t descriptor_link_offset = 0;
    std::uint32_t descriptor_link_value = 0;
    std::uint32_t local_d0_value = 0;
    std::uint32_t local_d1_value = 0;
    std::uint32_t next_exec_base_read_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t stop_before_address = 0;
};

enum class DeuterosAmigaTitleThirdServiceOutcome {
    nonzero_original_loop,
    zero_local_continuation,
};

struct DeuterosAmigaTitleThirdServiceLocalPlan {
    DeuterosAmigaObservedServiceSetupExecReturn observed_return;
    DeuterosAmigaTitleThirdServiceOutcome outcome =
        DeuterosAmigaTitleThirdServiceOutcome::nonzero_original_loop;
    std::uint32_t stop_before_address = 0;
    std::uint32_t active_descriptor_pointer_address = 0;
    std::uint32_t active_descriptor_pointer_value = 0;
    std::uint16_t inactive_long_offset = 0;
    std::uint32_t inactive_long_value = 0;
    std::uint16_t inactive_byte_offset = 0;
    std::uint8_t inactive_byte_value = 0;
    std::uint32_t restored_first_result_d0 = 0;
    std::uint32_t second_descriptor_address = 0;
    std::uint16_t result_offset = 0;
    std::uint32_t result_value = 0;
    std::uint32_t next_exec_base_read_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
};

struct DeuterosAmigaTitleFourthServiceLocalPlan {
    DeuterosAmigaObservedServiceSetupExecReturn observed_return;
    std::uint32_t a0_value = 0;
    std::uint32_t a1_value = 0;
    std::uint32_t global_pointer_address = 0;
    std::uint32_t global_pointer_value = 0;
    std::uint16_t descriptor_link_offset = 0;
    std::uint32_t descriptor_link_value = 0;
    std::uint32_t local_d0_value = 0;
    std::uint32_t local_d1_value = 0;
    std::uint32_t next_exec_base_read_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t stop_before_address = 0;
};

class DeuterosAmigaTitleServiceSetupBoundarySession {
public:
    DeuterosAmigaTitleServiceSetupBoundarySession(
        const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan) {
        constexpr std::uint32_t address = 0x206d4;
        constexpr std::size_t length = 200;
        if (address < plan.title_stage.destination
            || address - plan.title_stage.destination > plan.title_stage.length
            || length > plan.title_stage.length - (address - plan.title_stage.destination)) {
            throw std::runtime_error("Deuteros service setup lies outside original stage");
        }
        const auto bytes = disk.bytes(plan.title_stage.disk_offset
            + address - plan.title_stage.destination, length);
        if (to_hex(sha256(bytes.first(14)))
                != "a5c916b3959fe074f18e12a12d0488a38b2c8b638079fb05d1ad3a0739848001"
            || to_hex(sha256(bytes.subspan(14, 38)))
                != "b1cc2be3a282d4a49fdc161f1d6b8c74a03be4a7aa5b13b8f2300f179dbb8cde"
            || to_hex(sha256(bytes.subspan(52, 8)))
                != "913043cfe14c05c8e74c79915e6922eb2ccd071169a85e1ab0b47f85925ff795"
            || to_hex(sha256(bytes.subspan(60, 30)))
                != "f4312fcc6e66dd97c124f167ac5634d69bd32071fada15f48194324bb1b29dd7"
            || to_hex(sha256(bytes.subspan(90, 8)))
                != "0a982fb16e92100a04d3528d727297363de61d99ac61f8a193c4ee6c55ac4888"
            || to_hex(sha256(bytes.subspan(98, 64)))
                != "e5f6841f53d99f63a4c4de84abc98d334f2a376cea4acbf279f5534c4e79b063"
            || to_hex(sha256(bytes.subspan(162, 8)))
                != "913043cfe14c05c8e74c79915e6922eb2ccd071169a85e1ab0b47f85925ff795"
            || to_hex(sha256(bytes.subspan(170, 30)))
                != "ee1c0c590b6037a7e59608bb83dae26c4ffc510fd53e3a8b05ccff78fab8c2c0"
            || bytes[0] != 0x22 || bytes[1] != 0x7c
            || bytes[6] != 0x2c || bytes[7] != 0x78 || bytes[9] != 0x04
            || bytes[10] != 0x4e || bytes[11] != 0xae
            || bytes[12] != 0xfe || bytes[13] != 0xda) {
            throw std::runtime_error("Unsupported Deuteros service setup prefix");
        }
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleSecondServiceLocalPlan>
    observe_second_exec_return(
        const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        if (!observed_ || observed_second_) return std::nullopt;
        if (observation.trace_sequence <= observed_->trace_sequence
            || observation.exec_base_source_address != 4
            || observation.call_address != 0x2070c || observation.vector != -0x162
            || observation.return_address != 0x20710) {
            throw std::runtime_error("Deuteros second service Exec return does not match boundary");
        }
        observed_second_ = observation;
        return DeuterosAmigaTitleSecondServiceLocalPlan{observation,
            0x206ac, 0x205e4, 0x20698, 0x205e4, 0x0e, 0x2061c,
            0, 0, 0x2072e, 0x20732, -0x1bc, 0x2072e};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleThirdServiceLocalPlan>
    observe_third_exec_return(
        const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        if (!observed_second_ || observed_third_) return std::nullopt;
        if (observation.trace_sequence <= observed_second_->trace_sequence
            || observation.exec_base_source_address != 4
            || observation.call_address != 0x20732 || observation.vector != -0x1bc
            || observation.return_address != 0x20736) {
            throw std::runtime_error("Deuteros third service Exec return does not match boundary");
        }
        observed_third_ = observation;
        if (observation.result_d0 != 0) {
            return DeuterosAmigaTitleThirdServiceLocalPlan{observation,
                DeuterosAmigaTitleThirdServiceOutcome::nonzero_original_loop, 0x2073a};
        }
        return DeuterosAmigaTitleThirdServiceLocalPlan{observation,
            DeuterosAmigaTitleThirdServiceOutcome::zero_local_continuation, 0x20776,
            0x20698, 0x205e4, 0x30, 0xffffffff, 0x1e, 0,
            observed_->result_d0, 0x20676, 0x10, observed_->result_d0,
            0x20776, 0x2077a, -0x162};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleFourthServiceLocalPlan>
    observe_fourth_exec_return(
        const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        if (!observed_third_ || observed_third_->result_d0 != 0 || observed_fourth_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_third_->trace_sequence
            || observation.exec_base_source_address != 4
            || observation.call_address != 0x2077a || observation.vector != -0x162
            || observation.return_address != 0x2077e) {
            throw std::runtime_error("Deuteros fourth service Exec return does not match boundary");
        }
        observed_fourth_ = observation;
        return DeuterosAmigaTitleFourthServiceLocalPlan{observation,
            0x206ac, 0x2063e, 0x2069c, 0x2063e, 0x0e, 0x20676,
            1, 0, 0x2079c, 0x207a0, -0x1bc, 0x2079c};
    }

    void enter(std::uint64_t preceding_sequence) {
        if (armed_ || preceding_sequence == 0) {
            throw std::runtime_error("Invalid Deuteros service setup entry");
        }
        armed_ = true;
        preceding_sequence_ = preceding_sequence;
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleServiceSetupLocalPlan>
    observe_exec_return(const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        if (!armed_ || observed_) return std::nullopt;
        if (observation.trace_sequence <= preceding_sequence_
            || observation.exec_base_source_address != 4
            || observation.call_address != 0x206de || observation.vector != -0x126
            || observation.return_address != 0x206e2) {
            throw std::runtime_error("Deuteros service setup Exec return does not match boundary");
        }
        observed_ = observation;
        return DeuterosAmigaTitleServiceSetupLocalPlan{observation, observation.result_d0,
            0x2061c, 0x0a, 0x206ac, 0x7f, 4, 1, 0x10, observation.result_d0,
            0x20708, 0x2070c, -0x162, 0x20708};
    }

private:
    bool armed_ = false;
    std::uint64_t preceding_sequence_ = 0;
    std::optional<DeuterosAmigaObservedServiceSetupExecReturn> observed_;
    std::optional<DeuterosAmigaObservedServiceSetupExecReturn> observed_second_;
    std::optional<DeuterosAmigaObservedServiceSetupExecReturn> observed_third_;
    std::optional<DeuterosAmigaObservedServiceSetupExecReturn> observed_fourth_;
};

} // namespace eon
