#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

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

// A value observed at one exact native runtime address.  This is deliberately
// not a host-side state value: callers must preserve which original cell was
// sampled before asking a hash-bound prefix evaluator to use it.
struct MillenniumDosRuntimeByteObservation {
    std::uint16_t address = 0;
    std::uint8_t value = 0;
};

// The original action loop obtains AL through a call at one recovered site.
// Retain that site with every candidate action so a trace from another poll or
// helper cannot silently acquire gameplay-dispatch meaning.
struct MillenniumDosActionObservation {
    std::uint16_t poll_address = 0;
    std::uint8_t action = 0;
};

// Host-side observation of the original loop's *already polled* AL byte.  It
// deliberately does not invoke native handlers or mutate the original save.
class MillenniumDosGameSession {
public:
    explicit MillenniumDosGameSession(MillenniumDosGameFlow flow);
    // Keeps only a non-owning view of the verified original executable.  This
    // enables the two non-table action prefixes to be checked again at the
    // point they are observed; it neither extracts nor copies game data.
    MillenniumDosGameSession(MillenniumDosGameFlow flow,
        std::span<const std::uint8_t> game_executable);

    // Returns a table index only for the exact inclusive action-byte range
    // that the original code normalizes by subtracting function_key_first_action.
    [[nodiscard]] std::optional<std::size_t> observe_action(MillenniumDosActionObservation observation);
    [[nodiscard]] std::optional<std::size_t> last_function_key_index() const {
        return last_function_key_index_;
    }
    [[nodiscard]] std::optional<std::uint8_t> last_special_action() const {
        return last_special_action_;
    }
    // Action $0b reads the supplied native byte, toggles it before its first
    // unresolved helper, then stops.  The byte is an explicit observation of
    // original runtime state, not an SDL key mapping or a value made up by the
    // host. Requires the non-owning original-executable constructor.
    [[nodiscard]] MillenniumDosFirstSpecialActionPrefix
    observe_first_special_action(MillenniumDosRuntimeByteObservation observation);
    // Action $0c has no pre-helper write. This records whether the supplied
    // native gate blocks it or reaches its first helper boundary. Requires the
    // non-owning original-executable constructor.
    [[nodiscard]] MillenniumDosSecondSpecialActionPrefix
    observe_second_special_action(MillenniumDosRuntimeByteObservation observation);
    [[nodiscard]] std::optional<MillenniumDosFirstSpecialActionPrefix>
    last_first_special_action_trace() const { return last_first_special_action_trace_; }
    [[nodiscard]] std::optional<MillenniumDosSecondSpecialActionPrefix>
    last_second_special_action_trace() const { return last_second_special_action_trace_; }
    [[nodiscard]] std::optional<MillenniumDosRuntimeByteEffect>
    last_special_runtime_byte_effect() const { return last_special_runtime_byte_effect_; }
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
    void clear_last_observation();

    MillenniumDosGameFlow flow_;
    std::span<const std::uint8_t> game_executable_;
    std::optional<std::size_t> last_function_key_index_;
    std::optional<std::uint8_t> last_special_action_;
    std::optional<MillenniumDosFirstSpecialActionPrefix> last_first_special_action_trace_;
    std::optional<MillenniumDosSecondSpecialActionPrefix> last_second_special_action_trace_;
    std::optional<MillenniumDosRuntimeByteEffect> last_special_runtime_byte_effect_;
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
    std::optional<std::uint8_t> reconstructed_07f9_;
};

} // namespace eon
