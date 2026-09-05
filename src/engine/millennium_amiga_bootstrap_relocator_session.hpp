#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eon {

enum class MillenniumAmigaBootstrapRelocatorState {
    awaiting_overread_byte,
    awaiting_terminal_jump,
    awaiting_setup_call_return,
    awaiting_first_read_return,
    awaiting_opaque_first_stage,
    awaiting_first_stage_illegal_exception,
};

struct MillenniumAmigaBootstrapRelocatorBoundary {
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t target_address = 0;
    constexpr bool operator==(const MillenniumAmigaBootstrapRelocatorBoundary&) const = default;
};

struct MillenniumAmigaBootstrapRelocationByteEffect {
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t destination_address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumAmigaBootstrapRelocationByteEffect&) const = default;
};

struct MillenniumAmigaBootstrapCustomChipEffect {
    std::uint32_t instruction_address = 0;
    std::uint32_t address = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumAmigaBootstrapCustomChipEffect&) const = default;
};

struct MillenniumAmigaFirstStageRegisterObservation {
    std::uint32_t instruction_address = 0;
    std::array<std::uint32_t, 8> data{};
    std::array<std::uint32_t, 7> address{};
    std::uint32_t stack_pointer = 0;
    std::uint32_t exception_vector_10 = 0;
};

struct MillenniumAmigaFirstStageEntryExecution {
    std::uint32_t branch_target = 0;
    std::uint32_t snapshot_address = 0;
    std::array<std::uint32_t, 16> snapshot{};
    std::uint32_t transient_stack_address = 0;
    std::uint32_t original_a6 = 0;
    std::uint32_t installed_vector_address = 0;
    std::uint32_t installed_vector_value = 0;
    std::uint32_t resulting_d0 = 0;
    std::uint32_t resulting_a6 = 0;
    std::uint32_t resulting_stack_pointer = 0;
    std::uint32_t illegal_instruction_address = 0;
};

// Manual recompilation of the exact, direct Defjam bootstrap relocator at
// $70000..$70041. The original DBRA reads one byte beyond the authenticated
// $400-byte load. That byte remains an explicit observation boundary.
class MillenniumAmigaBootstrapRelocatorSession {
public:
    explicit MillenniumAmigaBootstrapRelocatorSession(
        std::span<const std::uint8_t> disk_image);

    [[nodiscard]] MillenniumAmigaBootstrapRelocatorState state() const { return state_; }
    [[nodiscard]] MillenniumAmigaBootstrapRelocatorBoundary boundary() const;
    [[nodiscard]] const std::vector<MillenniumAmigaBootstrapRelocationByteEffect>&
    copy_effects() const { return copy_effects_; }
    [[nodiscard]] const MillenniumAmigaBootstrapCustomChipEffect&
    custom_chip_effect() const { return custom_chip_effect_; }
    [[nodiscard]] std::uint32_t final_a3() const { return final_a3_; }
    [[nodiscard]] std::uint32_t final_a5() const { return final_a5_; }
    [[nodiscard]] std::uint32_t final_d1() const { return final_d1_; }
    [[nodiscard]] std::span<const std::uint8_t> first_stage_bytes() const {
        return first_stage_bytes_;
    }
    [[nodiscard]] const std::string& first_stage_sha256() const {
        return first_stage_sha256_;
    }

    void observe_overread_byte(std::uint32_t instruction_address,
        std::uint32_t source_address, std::uint8_t value);
    void observe_terminal_jump(std::uint32_t instruction_address,
        std::uint32_t target_address);
    void observe_setup_call_return(std::uint32_t instruction_address,
        std::uint32_t target_address);
    void observe_first_read_return(std::uint32_t instruction_address,
        std::uint32_t target_address, std::uint8_t io_error);
    [[nodiscard]] MillenniumAmigaFirstStageEntryExecution
    execute_first_stage_entry(const MillenniumAmigaFirstStageRegisterObservation&);
    [[nodiscard]] const std::optional<MillenniumAmigaFirstStageEntryExecution>&
    first_stage_entry_execution() const { return first_stage_entry_execution_; }

private:
    MillenniumAmigaBootstrapRelocatorState state_ =
        MillenniumAmigaBootstrapRelocatorState::awaiting_overread_byte;
    MillenniumAmigaBootstrapCustomChipEffect custom_chip_effect_;
    std::vector<MillenniumAmigaBootstrapRelocationByteEffect> copy_effects_;
    std::uint32_t final_a3_ = 0;
    std::uint32_t final_a5_ = 0;
    std::uint32_t final_d1_ = 0;
    std::vector<std::uint8_t> first_stage_bytes_;
    std::string first_stage_sha256_;
    std::optional<MillenniumAmigaFirstStageEntryExecution> first_stage_entry_execution_;
};

} // namespace eon
