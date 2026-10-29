#include "engine/deuteros_amiga_owned_outer_loop.hpp"
#include "engine/deuteros_amiga_host_input.hpp"

#include <cassert>
#include <cstdint>

int main() {
    eon::DeuterosAmigaHostInput input;
    input.set_right_mouse(true);
    input.set_keyboard_fire(eon::DeuterosAmigaHostInput::KeyboardFireKey::space, true);
    input.set_gamepad_fire(true);
    assert(input.fire_held() && input.right_mouse_held());
    input.set_keyboard_fire(eon::DeuterosAmigaHostInput::KeyboardFireKey::space, false);
    assert(input.fire_held() && input.right_mouse_held());
    input.set_gamepad_fire(false);
    assert(!input.fire_held() && input.right_mouse_held());
    input.set_keyboard_fire(eon::DeuterosAmigaHostInput::KeyboardFireKey::space, true);
    input.set_keyboard_fire(eon::DeuterosAmigaHostInput::KeyboardFireKey::enter, true);
    input.set_keyboard_fire(eon::DeuterosAmigaHostInput::KeyboardFireKey::space, false);
    assert(input.fire_held()); // Releasing one key must not release the other.
    input.set_keyboard_fire(eon::DeuterosAmigaHostInput::KeyboardFireKey::enter, false);
    assert(!input.fire_held());
    input.set_left_mouse_fire(true);
    assert(input.fire_held() && input.right_mouse_held());
    input.clear();
    assert(!input.fire_held() && !input.right_mouse_held());

    // SDL right-button state is translated to the active-low POTINP byte at
    // the exact recovered $dff016 polling boundary.
    assert(eon::deuteros_amiga_potinp_right_mouse_sample(false) == 0x04);
    assert(eon::deuteros_amiga_potinp_right_mouse_sample(true) == 0x00);
    assert(eon::deuteros_amiga_ciaa_fire_sample(false) == 0x40);
    assert(eon::deuteros_amiga_ciaa_fire_sample(true) == 0x00);

    // RAWMOUSE motion carries the relative-motion qualifier independently of
    // the left/right button qualifiers.
    assert(eon::deuteros_amiga_raw_mouse_motion_qualifier(false, false) == 0x8000);
    assert(eon::deuteros_amiga_raw_mouse_motion_qualifier(true, false) == 0xc000);
    assert(eon::deuteros_amiga_raw_mouse_motion_qualifier(false, true) == 0xa000);
    assert(eon::deuteros_amiga_raw_mouse_motion_qualifier(true, true) == 0xe000);

    // The first scheduler POTINP sample is stored as the next instruction;
    // later samples are represented by the pending-read field.
    assert(eon::deuteros_amiga_outer_input_candidate(0, 0x21822) == 0x21822);
    assert(eon::deuteros_amiga_outer_input_candidate(0x2185e, 0x21822) == 0x2185e);
    assert(eon::deuteros_amiga_outer_input_candidate(0, 0) == 0);
}
