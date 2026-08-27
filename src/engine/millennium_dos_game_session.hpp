#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace eon {

// Host-side observation of the original loop's *already polled* AL byte.  It
// deliberately does not invoke native handlers or mutate the original save.
class MillenniumDosGameSession {
public:
    explicit MillenniumDosGameSession(MillenniumDosGameFlow flow);

    // Returns a table index only for the exact inclusive action-byte range
    // that the original code normalizes by subtracting function_key_first_action.
    [[nodiscard]] std::optional<std::size_t> observe_action(std::uint8_t action);
    [[nodiscard]] std::optional<std::size_t> last_function_key_index() const {
        return last_function_key_index_;
    }
    [[nodiscard]] std::optional<std::uint8_t> last_special_action() const {
        return last_special_action_;
    }
    [[nodiscard]] const MillenniumDosGameFlow& flow() const { return flow_; }

private:
    MillenniumDosGameFlow flow_;
    std::optional<std::size_t> last_function_key_index_;
    std::optional<std::uint8_t> last_special_action_;
};

} // namespace eon
