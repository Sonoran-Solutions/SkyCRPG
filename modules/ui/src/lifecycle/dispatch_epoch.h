// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>

namespace skycrpg::ui::lifecycle {
// A queued press belongs to the focus session in which it was observed.
class DispatchEpoch {
public:
    std::uint64_t Capture() const { return value.load(std::memory_order_acquire); }
    void Invalidate() { value.fetch_add(1, std::memory_order_acq_rel); }
    bool Current(std::uint64_t captured) const { return Capture() == captured; }
private:
    std::atomic<std::uint64_t> value{};
};
}
