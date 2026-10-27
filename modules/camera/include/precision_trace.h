// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "policy.h"
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace skycrpg::camera {
// Diagnostic-only numeric state; never stores menu names, form IDs or input text.
enum class PrecisionTraceEvent : unsigned {
    menuOpen = 1, menuClose, retention, nativePov, togglePress, toggleRelease,
    toggleSuppressed, toggleMapped, toggleNoEdge, toggleDecision, toggleClaim
};
struct PrecisionTraceState {
    Context context{};
    bool hardSpecial{}, owned{}, armed{}, down{};
};
struct PrecisionTraceRecord {
    PrecisionTraceEvent event{};
    PrecisionTraceState before{}, after{};
    unsigned device{}; // 0=none, 1=keyboard, 2=gamepad
    bool pressed{}, edge{};
    unsigned decision{}; // ToggleRequest, or zero when not applicable
};
// An opt-in session has a hard bound; callers decide which event boundaries
// are useful and only write when take() succeeds. No per-frame logging.
class PrecisionTraceBudget {
public:
    static constexpr unsigned limit = 64;
    bool take() {
        auto current = used_.load(std::memory_order_relaxed);
        while (current < limit) {
            if (used_.compare_exchange_weak(current,current + 1,std::memory_order_relaxed)) return true;
        }
        return false;
    }
    unsigned used() const { return used_.load(std::memory_order_relaxed); }
private:
    std::atomic_uint used_{};
};
// Bit ordering is fixed for offline comparison of pre/post event states.
constexpr std::uint32_t traceBits(const PrecisionTraceState& s) {
    const auto& c = s.context;
    return (std::uint32_t(c.enabled) << 0) | (std::uint32_t(c.prerequisites) << 1) |
        (std::uint32_t(c.world) << 2) | (std::uint32_t(c.special) << 3) |
        (std::uint32_t(c.menu) << 4) | (std::uint32_t(c.firstPerson) << 5) |
        (std::uint32_t(s.hardSpecial) << 6) | (std::uint32_t(s.owned) << 7) |
        (std::uint32_t(s.armed) << 8) | (std::uint32_t(s.down) << 9);
}
// Returns false on a too-small buffer; caller must not emit truncated data.
inline bool formatPrecisionTrace(char* out, std::size_t size, const PrecisionTraceRecord& r) {
    if (!out || !size) return false;
    const int n = std::snprintf(out,size,
        "precision_trace event=%u before=0x%03x after=0x%03x device=%u pressed=%u edge=%u decision=%u",
        static_cast<unsigned>(r.event),traceBits(r.before),traceBits(r.after),r.device,
        unsigned(r.pressed),unsigned(r.edge),r.decision);
    return n >= 0 && static_cast<std::size_t>(n) < size;
}
} // namespace skycrpg::camera
