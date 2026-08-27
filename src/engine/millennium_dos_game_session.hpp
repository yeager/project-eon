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
    // Present only after raw action $3b has traversed the verified first
    // table record. This is an observation of original code/data, not a
    // writable replacement for the game's runtime state.
    [[nodiscard]] std::optional<MillenniumDosFirstFunctionKeyTrace>
    last_first_function_key_trace() const { return last_first_function_key_trace_; }
    // Present only after raw action $3c has traversed the verified second
    // table record. The runtime availability byte is deliberately not supplied
    // or written by the host.
    [[nodiscard]] std::optional<MillenniumDosSecondFunctionKeyTrace>
    last_second_function_key_trace() const { return last_second_function_key_trace_; }
    [[nodiscard]] const MillenniumDosGameFlow& flow() const { return flow_; }

private:
    MillenniumDosGameFlow flow_;
    std::optional<std::size_t> last_function_key_index_;
    std::optional<std::uint8_t> last_special_action_;
    std::optional<MillenniumDosFirstFunctionKeyTrace> last_first_function_key_trace_;
    std::optional<MillenniumDosSecondFunctionKeyTrace> last_second_function_key_trace_;
};

} // namespace eon
