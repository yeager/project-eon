#pragma once

namespace eon {

// Host button state for the two independently sampled Amiga input lines in
// the recovered Deuteros outer loop. Multiple host fire bindings are ORed;
// the right mouse button stays on its separate POTINP line.
class DeuterosAmigaHostInput {
public:
    enum class KeyboardFireKey { space, enter };
    void set_keyboard_fire(KeyboardFireKey key, bool held) noexcept {
        if (key == KeyboardFireKey::space) space_fire_ = held;
        else enter_fire_ = held;
    }
    void set_gamepad_fire(bool held) noexcept { gamepad_fire_ = held; }
    void set_left_mouse_fire(bool held) noexcept { left_mouse_fire_ = held; }
    void set_right_mouse(bool held) noexcept { right_mouse_ = held; }
    void clear() noexcept {
        space_fire_ = false;
        enter_fire_ = false;
        gamepad_fire_ = false;
        left_mouse_fire_ = false;
        right_mouse_ = false;
    }
    [[nodiscard]] bool fire_held() const noexcept {
        return space_fire_ || enter_fire_ || gamepad_fire_ || left_mouse_fire_;
    }
    [[nodiscard]] bool right_mouse_held() const noexcept { return right_mouse_; }

private:
    bool space_fire_ = false;
    bool enter_fire_ = false;
    bool gamepad_fire_ = false;
    bool left_mouse_fire_ = false;
    bool right_mouse_ = false;
};

// Amiga InputEvent qualifier bits used by the recovered RAWMOUSE callback.
// Relative motion is represented by nonzero X/Y deltas and must carry
// IEQUALIFIER_RELATIVEMOUSE even when neither mouse button is held.
[[nodiscard]] constexpr std::uint16_t deuteros_amiga_raw_mouse_motion_qualifier(
    const bool left_button_held, const bool right_button_held) noexcept {
    return static_cast<std::uint16_t>(0x8000U
        | (left_button_held ? 0x4000U : 0U)
        | (right_button_held ? 0x2000U : 0U));
}

} // namespace eon
