#pragma once
#include <cmath>

namespace skycrpg::ui {
struct ResourceValue { float current; float maximum; };
// Negative current health can occur at death; display clamps to zero. Current
// may transiently exceed maximum after modifiers expire; clamp only rendering.
inline bool IsValidResource(ResourceValue value)
{
    return std::isfinite(value.current) && std::isfinite(value.maximum) && value.maximum > 0.0F;
}
}
