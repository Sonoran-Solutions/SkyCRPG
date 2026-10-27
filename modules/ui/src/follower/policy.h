#pragma once
#include <cmath>
#include <cstddef>

namespace skycrpg::ui::follower {
enum class State { none, live, unknown, dead, unsupported };
struct Observation {
    bool available{};
    std::size_t count{};
    bool humanoid{};
    bool dead{};
    bool loaded{};
    bool named{};
    float current{};
    float maximum{};
};
inline State Classify(const Observation& value)
{
    if (!value.available) return State::unknown;
    if (value.count > 1) return State::unsupported;
    if (value.count == 0) return State::none;
    if (value.dead) return State::dead;
    if (!value.humanoid) return State::unsupported;
    if (!value.loaded || !value.named || !std::isfinite(value.current) ||
        !std::isfinite(value.maximum) || value.maximum <= 0.0F) return State::unknown;
    return State::live;
}
}
