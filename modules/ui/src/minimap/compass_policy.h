#pragma once

namespace skycrpg::ui::minimap
{
struct Capability
{
    bool enabled;
    bool exact;
    bool gameplay;
    bool attached;
    bool visible;
    bool inBounds;
    bool hudMode;
    bool settled;
    bool cellChanged;
};

constexpr bool BoundedObservationReady(const Capability& capability)
{
    return capability.enabled && capability.exact && capability.gameplay &&
        capability.attached && capability.visible && capability.inBounds &&
        capability.hudMode && capability.settled && !capability.cellChanged;
}

constexpr bool ShouldSuppress(const Capability& capability)
{
    return BoundedObservationReady(capability);
}
}
