#pragma once

namespace skycrpg::ui::minimap
{
struct Capability
{
    bool enabled;
    bool present;
    bool compatible;
    bool ready;
    bool gameplay;
    bool transition;
};

constexpr bool ShouldSuppress(const Capability& capability)
{
    return capability.enabled && capability.present && capability.compatible &&
        capability.ready && capability.gameplay && !capability.transition;
}

// No public DEM contract proves that a fresh map frame was rendered.
constexpr bool FunctionalReadinessProven()
{
    return false;
}
}
