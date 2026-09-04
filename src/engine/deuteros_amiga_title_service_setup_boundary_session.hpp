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

class DeuterosAmigaTitleServiceSetupBoundarySession {
public:
    DeuterosAmigaTitleServiceSetupBoundarySession(
        const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan) {
        constexpr std::uint32_t address = 0x206d4;
        constexpr std::size_t length = 52;
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
            || bytes[0] != 0x22 || bytes[1] != 0x7c
            || bytes[6] != 0x2c || bytes[7] != 0x78 || bytes[9] != 0x04
            || bytes[10] != 0x4e || bytes[11] != 0xae
            || bytes[12] != 0xfe || bytes[13] != 0xda) {
            throw std::runtime_error("Unsupported Deuteros service setup prefix");
        }
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
};

} // namespace eon
