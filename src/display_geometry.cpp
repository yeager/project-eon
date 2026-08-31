#include "display_geometry.hpp"

#include <cmath>
#include <stdexcept>

namespace eon {

DisplayViewport fit_display_aspect_viewport(const float x, const float y,
    const float maximum_width, const float maximum_height,
    const float display_aspect_ratio) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(maximum_width)
        || !std::isfinite(maximum_height) || !std::isfinite(display_aspect_ratio)
        || maximum_width <= 0.0F || maximum_height <= 0.0F
        || display_aspect_ratio <= 0.0F) {
        throw std::invalid_argument("display aspect viewport requires finite positive bounds and ratio");
    }

    float width = maximum_width;
    float height = width / display_aspect_ratio;
    if (height > maximum_height) {
        height = maximum_height;
        width = height * display_aspect_ratio;
    }
    return {x + (maximum_width - width) / 2.0F,
        y + (maximum_height - height) / 2.0F, width, height};
}

} // namespace eon
