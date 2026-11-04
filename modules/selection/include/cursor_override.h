// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
#include "skycrpg/SelectionAPI.h"

namespace skycrpg::selection {
template<class T> T cursorContext(T context, bool active) {
    if (active) { context.nativeLiveActor = false; context.combat = false; }
    return context;
}
constexpr bool finishedScan(bool complete, bool unknown, bool contended, bool capped, bool rayCapped) {
    return !contended && ((complete && !unknown) || capped || (complete && rayCapped));
}
constexpr api::Result legacyResult(api::Result result) {
    return result == api::Result::busy ? api::Result::unavailable : result;
}
constexpr api::Result legacyReadResult(bool acquired, bool ready = true) {
    return acquired && ready ? api::Result::ok : api::Result::unavailable;
}
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
    // Change generation even when already empty: cancel an in-flight publication.
    std::uint64_t clear() {
        auto old = read();
        const auto generation = next_.fetch_add(generationStep)+generationStep;
        while (active(old)) {
            const auto empty = generation | activeBit;
            if (state_.compare_exchange_weak(old,empty)) return empty;
        }
        return 0; // a concurrent lifecycle exit must never be resurrected
    }
    static bool permits(std::uint64_t before, std::uint64_t after, std::uint32_t id) {
        return before == after && active(after) && id && target(after) == id;
    }
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
template<class Submit> api::Result replaceCursorTarget(CursorOverride& state, std::uint32_t id, Submit submit) {
    const auto token = state.clear();
    if (!token) return api::Result::unavailable;
    return submit(token,id);
}
// Short independent mailbox/publication gate. Pick never waits for readers.
// A failed try leaves the last complete value untouched; no racy seqlock copies.
template<class T> class CursorExchange {
public:
    bool write(const T& value) {
        if (gate_.test_and_set(std::memory_order_acquire)) return false;
        value_ = value;
        gate_.clear(std::memory_order_release);
        return true;
    }
    bool read(T& value) {
        if (gate_.test_and_set(std::memory_order_acquire)) return false;
        value = value_;
        gate_.clear(std::memory_order_release);
        return true;
    }
    bool take(T& value) {
        if (gate_.test_and_set(std::memory_order_acquire)) return false;
        value = value_; value_ = {};
        gate_.clear(std::memory_order_release);
        return true;
    }
private:
    std::atomic_flag gate_ = ATOMIC_FLAG_INIT;
    T value_{};
};
struct CursorRequest { std::uint64_t token = 0, cycles = 0; std::uint32_t id = 0; };
} // namespace skycrpg::selection
