#pragma once

namespace eon {

// A renderer-space rectangle. This is intentionally independent from SDL so
// the aspect contract can be tested without a window, texture, or game asset.
struct DisplayViewport {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

// Fit a requested display aspect into a presentation region without cropping
// or independent width/height stretching. Invalid renderer inputs are a
// caller error rather than an opportunity to produce NaN viewport geometry.
[[nodiscard]] DisplayViewport fit_display_aspect_viewport(float x, float y,
    float maximum_width, float maximum_height, float display_aspect_ratio);

} // namespace eon
