#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumAmigaBootstrapRelocatorState {
    awaiting_overread_byte,
    awaiting_terminal_jump,
    transferred,
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

    void observe_overread_byte(std::uint32_t instruction_address,
        std::uint32_t source_address, std::uint8_t value);
    void observe_terminal_jump(std::uint32_t instruction_address,
        std::uint32_t target_address);

private:
    MillenniumAmigaBootstrapRelocatorState state_ =
        MillenniumAmigaBootstrapRelocatorState::awaiting_overread_byte;
    MillenniumAmigaBootstrapCustomChipEffect custom_chip_effect_;
    std::vector<MillenniumAmigaBootstrapRelocationByteEffect> copy_effects_;
    std::uint32_t final_a3_ = 0;
    std::uint32_t final_a5_ = 0;
    std::uint32_t final_d1_ = 0;
};

} // namespace eon
