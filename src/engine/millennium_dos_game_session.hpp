#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace eon {

// A single, byte-exact runtime write reconstructed from the F8 handler.  It
// is intentionally not a claim about the byte's game meaning.  `previous`
// remains empty until this minimal overlay itself has observed a write, so no
// initial runtime value is invented from the save file or host state.
struct MillenniumDosRuntimeByteEffect {
    std::uint16_t address = 0;
    std::optional<std::uint8_t> previous;
    std::uint8_t value = 0;
};

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
    [[nodiscard]] std::optional<MillenniumDosThirdFunctionKeyTrace>
    last_third_function_key_trace() const { return last_third_function_key_trace_; }
    [[nodiscard]] std::optional<MillenniumDosFourthFunctionKeyTrace>
    last_fourth_function_key_trace() const { return last_fourth_function_key_trace_; }
    [[nodiscard]] std::optional<MillenniumDosFifthFunctionKeyTrace>
    last_fifth_function_key_trace() const { return last_fifth_function_key_trace_; }
    [[nodiscard]] std::optional<MillenniumDosSixthFunctionKeyTrace>
    last_sixth_function_key_trace() const { return last_sixth_function_key_trace_; }
    [[nodiscard]] std::optional<MillenniumDosSeventhFunctionKeyTrace>
    last_seventh_function_key_trace() const { return last_seventh_function_key_trace_; }
    [[nodiscard]] std::optional<MillenniumDosEighthFunctionKeyTrace>
    last_eighth_function_key_trace() const { return last_eighth_function_key_trace_; }
    [[nodiscard]] std::optional<MillenniumDosNinthFunctionKeyTrace>
    last_ninth_function_key_trace() const { return last_ninth_function_key_trace_; }
    [[nodiscard]] std::optional<MillenniumDosTenthFunctionKeyTrace>
    last_tenth_function_key_trace() const { return last_tenth_function_key_trace_; }
    // F8 is the only reconstructed handler path with a write that is both
    // unconditional and fully encoded in the verified bytes before a call:
    // `mov byte ptr [$da30], 0`. This reports that narrow in-memory effect.
    [[nodiscard]] std::optional<MillenniumDosRuntimeByteEffect>
    last_runtime_byte_effect() const { return last_runtime_byte_effect_; }
    // Only addresses reached by a reconstructed unconditional write can be
    // queried. Unknown means that no value was supplied by original code yet.
    [[nodiscard]] std::optional<std::uint8_t> reconstructed_runtime_byte(
        std::uint16_t address) const;
    [[nodiscard]] const MillenniumDosGameFlow& flow() const { return flow_; }

private:
    MillenniumDosGameFlow flow_;
    std::optional<std::size_t> last_function_key_index_;
    std::optional<std::uint8_t> last_special_action_;
    std::optional<MillenniumDosFirstFunctionKeyTrace> last_first_function_key_trace_;
    std::optional<MillenniumDosSecondFunctionKeyTrace> last_second_function_key_trace_;
    std::optional<MillenniumDosThirdFunctionKeyTrace> last_third_function_key_trace_;
    std::optional<MillenniumDosFourthFunctionKeyTrace> last_fourth_function_key_trace_;
    std::optional<MillenniumDosFifthFunctionKeyTrace> last_fifth_function_key_trace_;
    std::optional<MillenniumDosSixthFunctionKeyTrace> last_sixth_function_key_trace_;
    std::optional<MillenniumDosSeventhFunctionKeyTrace> last_seventh_function_key_trace_;
    std::optional<MillenniumDosEighthFunctionKeyTrace> last_eighth_function_key_trace_;
    std::optional<MillenniumDosNinthFunctionKeyTrace> last_ninth_function_key_trace_;
    std::optional<MillenniumDosTenthFunctionKeyTrace> last_tenth_function_key_trace_;
    std::optional<MillenniumDosRuntimeByteEffect> last_runtime_byte_effect_;
    std::optional<std::uint8_t> reconstructed_da30_;
};

} // namespace eon
