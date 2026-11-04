// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>
#include <limits>

namespace skycrpg::selection {
inline bool cursorRequestCurrent(bool active, std::uint64_t expected, std::uint64_t cycles) {
    return !active || expected == cycles;
}
template<class T> T cursorLatchSettings(T settings, bool active) {
    if (active) {
        settings.latchSeconds = std::numeric_limits<double>::infinity();
        settings.moveThreshold = std::numeric_limits<double>::infinity();
    }
    return settings;
}
// Single cursor client, any thread. A generation prevents an in-flight Pick
// from authorizing a target after exit/re-entry. Zero is explicitly empty.
class CursorOverride {
public:
    static constexpr std::uint64_t activeBit = std::uint64_t{1} << 32;
    static constexpr std::uint64_t generationStep = std::uint64_t{1} << 33;
    std::uint64_t read() const { return state_.load(); }
    static bool active(std::uint64_t value) { return (value & activeBit) != 0; }
    static std::uint32_t target(std::uint64_t value) { return static_cast<std::uint32_t>(value); }
    void begin() { change(true); }
    void end() { change(false); }
    bool publish(std::uint64_t token, std::uint32_t id) {
        if (!active(token)) return false;
        return state_.compare_exchange_strong(token,(token & ~std::uint64_t{0xffffffff}) | id);
    }
    static bool suppress(std::uint64_t value, bool injected, std::uint32_t id) {
        return active(value) && (!injected || !id || target(value) != id);
    }
private:
    void change(bool enabled) {
        const auto generation = next_.fetch_add(generationStep)+generationStep;
        state_.store(generation | (enabled ? activeBit : 0));
    }
    std::atomic<std::uint64_t> state_{0}, next_{0};
};
} // namespace skycrpg::selection
