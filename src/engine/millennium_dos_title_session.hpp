#pragma once

#include "data/millennium_dos_title_flow.hpp"

#include <cstdint>

namespace eon {

// A deliberately narrow reconstruction of the verified TITLES.EXE boundary.
// The title executable polls DOS INT 21h/AH=06h/DL=FFh and, when a character
// is available, takes its local exit path. The process exit, MILL.COM return,
// and 2200ad.exe request all require unmodelled DOS results. This class records
// only the observed input-availability boundary; it does not emulate the
// transition renderer, return chain, or game executable.
class MillenniumDosTitleSession {
public:
    explicit MillenniumDosTitleSession(MillenniumDosTitleFlow flow);
    explicit MillenniumDosTitleSession(MillenniumDosSpanishTitleBoundary flow);

    // `character_available` is the DOS non-blocking console-poll result, not
    // an interpretation of a particular SDL key.  Returns true only once,
    // when the original title process would yield to its launcher.
    [[nodiscard]] bool poll_console(bool character_available);

    [[nodiscard]] bool handed_off() const { return handed_off_; }
    [[nodiscard]] std::uint16_t title_resource_index() const { return flow_.title_resource_index; }
    [[nodiscard]] const MillenniumDosTitleFlow& flow() const { return flow_; }

private:
    struct InputBoundary {
        std::uint8_t interrupt = 0;
        std::uint8_t service = 0;
        std::uint8_t parameter = 0;
        std::uint16_t nonzero_exit_address = 0;
    };

    MillenniumDosTitleFlow flow_;
    InputBoundary input_boundary_;
    bool handed_off_ = false;
};

} // namespace eon
