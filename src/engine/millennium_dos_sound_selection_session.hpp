#pragma once

#include "data/millennium_dos_sound_driver.hpp"

#include <optional>
#include <string_view>

namespace eon {

// A narrow, source-level reconstruction of MILL.COM's verified sound-effect
// chooser.  It models only the three literal ASCII selections in the locked
// routine; it neither invokes DOS, loads a driver nor converts an SDL key into
// a hardware/device policy.  A selected entry always stops at the unobserved
// driver-initialisation boundary.
class MillenniumDosSoundSelectionSession {
public:
    explicit MillenniumDosSoundSelectionSession(MillenniumDosSoundSelectionEvidence evidence);
    MillenniumDosSoundSelectionSession(MillenniumDosSoundSelectionEvidence evidence,
        std::optional<MillenniumDosSoundDriverLeaf> sound_blaster_driver,
        std::optional<MillenniumDosSoundDriverLeaf> covox_driver);

    // Return true only when `ascii_character` is one of the exact characters
    // accepted by the original routine and this session has not selected an
    // entry already.  The original uses an input byte; callers must provide
    // text/character input rather than an unproven physical-key mapping.
    [[nodiscard]] bool accept_ascii_character(char ascii_character);

    [[nodiscard]] bool awaiting_choice() const { return !choice_.has_value(); }
    [[nodiscard]] std::optional<MillenniumDosSoundEffectChoice> choice() const { return choice_; }
    [[nodiscard]] std::string_view selected_original_filename() const;
    [[nodiscard]] std::uint8_t selected_table_slot() const;
    // A supplied leaf may be admitted by its complete immutable hash while
    // its original initialization ABI remains unrecovered. IBM speaker is a
    // literal selectable table entry but has no admitted leaf in this corpus.
    [[nodiscard]] bool selected_driver_is_admitted() const;
    [[nodiscard]] std::optional<MillenniumDosSoundDriverLeaf> selected_driver() const;
    [[nodiscard]] const MillenniumDosSoundSelectionEvidence& evidence() const { return evidence_; }

private:
    MillenniumDosSoundSelectionEvidence evidence_;
    std::optional<MillenniumDosSoundDriverLeaf> sound_blaster_driver_;
    std::optional<MillenniumDosSoundDriverLeaf> covox_driver_;
    std::optional<MillenniumDosSoundEffectChoice> choice_;
};

} // namespace eon
