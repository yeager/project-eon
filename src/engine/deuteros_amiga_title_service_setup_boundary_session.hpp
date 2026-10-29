#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/sha256.hpp"
#include "engine/deuteros_amiga_exec_compatibility.hpp"

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
    std::uint32_t message_port_address = 0;
    std::uint16_t message_port_name_offset = 0;
    std::uint32_t message_port_name_value = 0;
    std::uint16_t message_port_priority = 0;
    std::uint16_t message_port_node_type = 0;
    std::uint16_t message_port_signal_bit = 0;
    std::uint16_t signal_task_offset = 0;
    std::uint32_t signal_task_value = 0;
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
    std::uint16_t reply_port_offset = 0;
    std::uint32_t reply_port_address = 0;
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
    std::uint32_t global_request_pointer_address = 0;
    std::uint32_t global_request_pointer_value = 0;
    std::uint16_t request_count_offset = 0;
    std::uint32_t request_count_value = 0;
    std::uint16_t request_flags_offset = 0;
    std::uint8_t request_flags_value = 0;
    std::uint32_t restored_first_result_d0 = 0;
    std::uint32_t second_message_port_address = 0;
    std::uint16_t signal_task_offset = 0;
    std::uint32_t signal_task_value = 0;
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
    std::uint16_t reply_port_offset = 0;
    std::uint32_t reply_port_address = 0;
    std::uint32_t local_d0_value = 0;
    std::uint32_t local_d1_value = 0;
    std::uint32_t next_exec_base_read_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t stop_before_address = 0;
};

enum class DeuterosAmigaTitleFifthServiceOutcome {
    nonzero_fallback_to_first_request,
    zero_initialize_second_request,
};

struct DeuterosAmigaTitleFifthServiceLocalPlan {
    DeuterosAmigaObservedServiceSetupExecReturn observed_return;
    DeuterosAmigaTitleFifthServiceOutcome outcome =
        DeuterosAmigaTitleFifthServiceOutcome::nonzero_fallback_to_first_request;
    std::uint32_t first_pointer_source_address = 0;
    std::uint32_t second_pointer_destination_address = 0;
    std::uint32_t fallback_request_pointer_source_address = 0;
    std::uint16_t request_count_offset = 0;
    std::uint32_t request_count_value = 0;
    std::uint16_t request_flags_offset = 0;
    std::uint8_t request_flags_value = 0;
    std::uint32_t routine_rts_address = 0;
    std::uint32_t caller_return_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleControllerPointerSeedPlan {
    std::uint32_t helper_entry_address = 0;
    std::uint32_t pointer_source_address = 0;
    std::uint32_t pointer_value = 0;
    std::uint32_t pointer_destination_address = 0;
    std::uint32_t helper_rts_address = 0;
    std::uint32_t caller_d1_value = 0;
    std::uint32_t seed_call_address = 0;
    std::uint32_t seed_call_target = 0;
    std::uint32_t seed_value = 0;
    std::uint32_t seed_destination_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t stop_before_address = 0;
    std::uint64_t preceding_trace_sequence = 0;
    std::uint64_t eon_native_service_sequence = 0;
};

struct DeuterosAmigaTitleNativeServiceSetupReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint64_t find_task_sequence = 0;
    std::uint64_t first_add_port_sequence = 0;
    std::uint64_t first_open_device_sequence = 0;
    std::uint64_t second_add_port_sequence = 0;
    std::uint64_t second_open_device_sequence = 0;
    std::uint32_t task_address = 0;
    DeuterosAmigaExecCompatibilityOrigin origin =
        DeuterosAmigaExecCompatibilityOrigin::eon_native_compatibility;
    std::uint32_t setup_entry_address = 0x206d4;
    // Virtual stack-bank contract. UserState($7fff0) installs the supervisor
    // stack while returning to the saved user stack. These are not claims that
    // an original Amiga stack allocation was observed or owned by Eon.
    std::uint32_t supervisor_stack_argument = 0x7fff0;
    std::uint32_t user_stack_pointer = 0x40b62;
    std::uint32_t caller_return_word_address = 0x40b5e;
    std::uint32_t saved_task_word_address = 0x40b5a;
    std::uint32_t service_call_low_water_address = 0x40b56;
    std::uint32_t saved_task_value = 0;
    std::uint32_t final_stack_pointer = 0x40b62;
    std::uint32_t final_pc = 0x404bc;
    bool assumes_balanced_intervening_calls = true;
    bool assumes_userstate_bank_switch_succeeds = true;
    std::uint32_t stop_before_address = 0x404bc;
};

class DeuterosAmigaTitleServiceSetupBoundarySession {
public:
    DeuterosAmigaTitleServiceSetupBoundarySession(
        const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan) {
        constexpr std::uint32_t address = 0x206d4;
        constexpr std::size_t length = 248;
        if (address < plan.title_stage.destination
            || address - plan.title_stage.destination > plan.title_stage.length
            || length > plan.title_stage.length - (address - plan.title_stage.destination)) {
            throw std::runtime_error("Deuteros service setup lies outside original stage");
        }
        const auto bytes = disk.bytes(plan.title_stage.disk_offset
            + address - plan.title_stage.destination, length);
        const auto pointer_helper = disk.bytes(plan.title_stage.disk_offset
            + 0x206be - plan.title_stage.destination, 22);
        const auto stage = parse_deuteros_amiga_title_stage(disk, plan);
        pointer_seed_ = parse_deuteros_amiga_title_post_exec_pointer_seed_profile(disk, plan);
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
            || to_hex(sha256(bytes.subspan(200, 8)))
                != "0a982fb16e92100a04d3528d727297363de61d99ac61f8a193c4ee6c55ac4888"
            || to_hex(sha256(bytes.subspan(208, 40)))
                != "cdf3332e5b071d102231d45ecf0b05a87728df49a7f08ff27ffc2e92f055e416"
            || to_hex(sha256(pointer_helper))
                != "cdcda125af5c05d4d88e7d486f15f50bd87c4641a38e9c8a4e29a9394152317a"
            || stage.initialization_internal_calls[5] != 0x206be
            || stage.initialization_internal_calls[6] != 0x403e6
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

    [[nodiscard]] std::optional<DeuterosAmigaTitleFifthServiceLocalPlan>
    observe_fifth_exec_return(
        const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        if (!observed_fourth_ || observed_fifth_) return std::nullopt;
        if (observation.trace_sequence <= observed_fourth_->trace_sequence
            || observation.exec_base_source_address != 4
            || observation.call_address != 0x207a0 || observation.vector != -0x1bc
            || observation.return_address != 0x207a4) {
            throw std::runtime_error("Deuteros fifth service Exec return does not match boundary");
        }
        observed_fifth_ = observation;
        if (observation.result_d0 != 0) {
            return DeuterosAmigaTitleFifthServiceLocalPlan{observation,
                DeuterosAmigaTitleFifthServiceOutcome::nonzero_fallback_to_first_request,
                0x20698, 0x2069c, 0, 0, 0, 0, 0,
                0x207ca, 0x404bc, 0x404bc, 0x206be, 0x404bc};
        }
        return DeuterosAmigaTitleFifthServiceLocalPlan{observation,
            DeuterosAmigaTitleFifthServiceOutcome::zero_initialize_second_request,
            0, 0, 0x2069c, 0x30, 0xffffffff, 0x1e, 0,
            0x207ca, 0x404bc, 0x404bc, 0x206be, 0x404bc};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleControllerPointerSeedPlan>
    advance_controller_pointer_seed() {
        if ((!observed_fifth_ && !native_service_setup_plan_)
            || controller_pointer_seed_advanced_) return std::nullopt;
        controller_pointer_seed_advanced_ = true;
        const auto preceding_sequence = observed_fifth_
            ? observed_fifth_->trace_sequence
            : native_service_setup_plan_->second_open_device_sequence;
        auto plan = DeuterosAmigaTitleControllerPointerSeedPlan{
            0x206be, 0x20698, 0x205e4, 0x206a0, 0x206d2,
            pointer_seed_.caller_d1_literal, pointer_seed_.call_site_address,
            pointer_seed_.callee_address, pointer_seed_.literal_value,
            pointer_seed_.destination_address, 0x404ce, 0x403f4, 0x404ce,
            preceding_sequence,
            native_service_setup_plan_ ? native_service_setup_plan_->second_open_device_sequence : 0};
        controller_pointer_seed_plan_ = plan;
        return plan;
    }

    [[nodiscard]] const std::optional<DeuterosAmigaTitleControllerPointerSeedPlan>&
    controller_pointer_seed_plan() const noexcept { return controller_pointer_seed_plan_; }

    [[nodiscard]] std::optional<DeuterosAmigaTitleNativeServiceSetupReceipt>
    accept_native_exec_service_setup(
        const DeuterosAmigaExecCompatibilityGraph& graph,
        const DeuterosAmigaExecCompatibilityTaskResult& task,
        const DeuterosAmigaExecCompatibilityPortResult& first_port,
        const DeuterosAmigaExecCompatibilityOpenResult& first_open,
        const DeuterosAmigaExecCompatibilityPortResult& second_port,
        const DeuterosAmigaExecCompatibilityOpenResult& second_open) {
        if (!armed_ || observed_ || native_service_setup_plan_
            || graph.origin != DeuterosAmigaExecCompatibilityOrigin::eon_native_compatibility
            || task.origin != graph.origin || first_port.origin != graph.origin
            || first_open.origin != graph.origin || second_port.origin != graph.origin
            || second_open.origin != graph.origin
            || graph.runtime_generation == 0 || task.runtime_generation != graph.runtime_generation
            || first_port.runtime_generation != graph.runtime_generation
            || first_open.runtime_generation != graph.runtime_generation
            || second_port.runtime_generation != graph.runtime_generation
            || second_open.runtime_generation != graph.runtime_generation
            || task.call_address != 0x206de || task.task_address != graph.task_address
            || first_port.call_address != 0x2070c || first_port.port_address != 0x2061c
            || first_port.task_address != task.task_address
            || first_open.call_address != 0x20732 || first_open.request_address != 0x205e4
            || first_open.unit != 0 || first_open.service_generated_io_error != 0
            || second_port.call_address != 0x2077a || second_port.port_address != 0x20676
            || second_port.task_address != task.task_address
            || second_open.call_address != 0x207a0 || second_open.request_address != 0x2063e
            || second_open.unit != 1 || second_open.service_generated_io_error != 0
            || !(task.service_sequence < first_port.service_sequence
                && first_port.service_sequence < first_open.service_sequence
                && first_open.service_sequence < second_port.service_sequence
                && second_port.service_sequence < second_open.service_sequence)) {
            return std::nullopt;
        }
        native_service_setup_plan_ = DeuterosAmigaTitleNativeServiceSetupReceipt{
            graph.runtime_generation, task.service_sequence, first_port.service_sequence,
            first_open.service_sequence, second_port.service_sequence,
            second_open.service_sequence, task.task_address,
            DeuterosAmigaExecCompatibilityOrigin::eon_native_compatibility,
            0x206d4, 0x7fff0, 0x40b62, 0x40b5e, 0x40b5a, 0x40b56,
            task.task_address, 0x40b62, 0x404bc, true, true, 0x404bc};
        return native_service_setup_plan_;
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

    [[nodiscard]] const std::optional<DeuterosAmigaTitleNativeServiceSetupReceipt>&
    native_service_setup_plan() const noexcept { return native_service_setup_plan_; }

private:
    bool armed_ = false;
    std::uint64_t preceding_sequence_ = 0;
    std::optional<DeuterosAmigaObservedServiceSetupExecReturn> observed_;
    std::optional<DeuterosAmigaObservedServiceSetupExecReturn> observed_second_;
    std::optional<DeuterosAmigaObservedServiceSetupExecReturn> observed_third_;
    std::optional<DeuterosAmigaObservedServiceSetupExecReturn> observed_fourth_;
    std::optional<DeuterosAmigaObservedServiceSetupExecReturn> observed_fifth_;
    DeuterosAmigaTitlePostExecPointerSeedProfile pointer_seed_;
    bool controller_pointer_seed_advanced_ = false;
    std::optional<DeuterosAmigaTitleNativeServiceSetupReceipt> native_service_setup_plan_;
    std::optional<DeuterosAmigaTitleControllerPointerSeedPlan> controller_pointer_seed_plan_;
};

} // namespace eon
