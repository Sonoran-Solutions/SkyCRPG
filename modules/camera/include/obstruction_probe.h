// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace skycrpg::camera {
// Diagnostic values only. They do not classify collision or request camera changes.
struct ProbePoint { float x, y, z; };
inline bool finite(ProbePoint p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
struct ProbeSample {
    std::uint64_t frame, generation;
    double seconds;
    ProbePoint root, providerLast;
    bool eligible;
};
struct ProbePair {
    std::uint64_t rootFrame, providerFrame, generation;
    double rootSeconds, providerSeconds;
    ProbePoint previousRoot, providerLast;
};

// Caller supplies consecutive samples before the native camera update. Source
// inspection suggests the provider's last pre-collision position corresponds
// to the previous sample's root. Actual runtime alignment is NOT established.
// Reset on every state/ownership transition and on any missed hook. Limit the
// output of each explicitly started diagnostic session to 128 pairs.
class ObstructionProbe {
public:
    static constexpr std::uint32_t pairLimit = 128;
    void start() { previous_.reset(); emitted_ = 0; }
    void suspend() { previous_.reset(); }
    [[nodiscard]] std::uint32_t emitted() const { return emitted_; }
    [[nodiscard]] std::optional<ProbePair> observe(const ProbeSample& s) {
        if (!s.eligible || !std::isfinite(s.seconds) || !finite(s.root) || !finite(s.providerLast)) {
            suspend();
            return std::nullopt;
        }
        if (emitted_ >= pairLimit) { suspend(); return std::nullopt; }
        std::optional<ProbePair> result;
        if (previous_ && s.generation == previous_->generation &&
            s.frame > previous_->frame && s.frame - previous_->frame == 1 &&
            s.seconds > previous_->seconds && s.seconds - previous_->seconds <= 0.1) {
            result = ProbePair{previous_->frame, s.frame, s.generation,
                previous_->seconds, s.seconds, previous_->root, s.providerLast};
            ++emitted_;
        }
        previous_ = s;
        return result;
    }
private:
    std::optional<ProbeSample> previous_;
    std::uint32_t emitted_ = 0;
};
} // namespace skycrpg::camera
