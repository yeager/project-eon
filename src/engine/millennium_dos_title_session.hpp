#pragma once

#include "data/millennium_dos_title_flow.hpp"

#include <cstdint>

namespace eon {

// A deliberately narrow reconstruction of the verified TITLES.EXE boundary.
// The title executable polls DOS INT 21h/AH=06h/DL=FFh and, when a character
// is available, returns to MILL.COM.  The launcher then executes 2200ad.exe.
// This class records that exact observable hand-off; it does not claim to
// emulate TITLES.EXE's transition renderer or the game executable.
class MillenniumDosTitleSession {
public:
    explicit MillenniumDosTitleSession(MillenniumDosTitleFlow flow);

    // `character_available` is the DOS non-blocking console-poll result, not
    // an interpretation of a particular SDL key.  Returns true only once,
    // when the original title process would yield to its launcher.
    [[nodiscard]] bool poll_console(bool character_available);

    [[nodiscard]] bool handed_off() const { return handed_off_; }
    [[nodiscard]] std::uint16_t title_resource_index() const { return flow_.title_resource_index; }
    [[nodiscard]] const MillenniumDosTitleFlow& flow() const { return flow_; }

private:
    MillenniumDosTitleFlow flow_;
    bool handed_off_ = false;
};

} // namespace eon
