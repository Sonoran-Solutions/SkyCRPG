// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace skycrpg::selection {
struct Point { double x = 0, y = 0, z = 0; };
inline bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline double distanceSquared(Point a, Point b) {
    const double x = a.x-b.x, y = a.y-b.y, z = a.z-b.z;
    return x*x+y*y+z*z;
}
inline Point add(Point a, Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Point subtract(Point a, Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Point multiply(Point a, double b) { return {a.x*b,a.y*b,a.z*b}; }
inline double dot(Point a, Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
struct Bounds {
    Point origin, low, high;
    std::array<Point,3> axes{Point{1,0,0},Point{0,1,0},Point{0,0,1}};
    bool available = false;
};
inline bool validBounds(const Bounds& b) {
    if (!b.available || !finite(b.origin) || !finite(b.low) || !finite(b.high) ||
        b.low.x > b.high.x || b.low.y > b.high.y || b.low.z > b.high.z ||
        distanceSquared(b.low,b.high) == 0) return false;
    for (std::size_t i = 0; i < 3; ++i) {
        if (!finite(b.axes[i]) || std::abs(dot(b.axes[i],b.axes[i])-1) > .001) return false;
        for (std::size_t j = 0; j < i; ++j) if (std::abs(dot(b.axes[i],b.axes[j])) > .001) return false;
    }
    return true;
}
inline Point closestPoint(const Bounds& b, Point p) {
    const auto offset = subtract(p,b.origin);
    const std::array low{b.low.x,b.low.y,b.low.z}, high{b.high.x,b.high.y,b.high.z};
    auto result = b.origin;
    for (std::size_t i = 0; i < 3; ++i)
        result = add(result,multiply(b.axes[i],std::clamp(dot(offset,b.axes[i]),low[i],high[i])));
    return result;
}
inline bool inBroadPhase(Point player, Point origin, double radius, double padding) {
    return finite(origin) && distanceSquared(player,origin) <= (radius+padding)*(radius+padding);
}
struct Settings {
    int schema = 1;
    bool enabled = true, allowExternal = false;
    double radius = 0; // zero reads native fActivatePickLength:Interface; never extends it
    double refreshSeconds = .1, latchSeconds = 10, moveThreshold = 64;
    std::uint32_t inspectLimit = 4096, rayLimit = 192, visitLimit = 65536;
    double boundsPadding = 2048;
    int nextKey = 26, previousKey = 27; // DIK_LBRACKET / DIK_RBRACKET, unapproved test defaults
    int nextGamepad = -1, previousGamepad = -1;
};
inline double bounded(double v, double fallback, double low, double high) {
    return std::clamp(std::isfinite(v) ? v : fallback,low,high);
}
inline Settings validate(Settings s) {
    if (s.schema != 1) { s.enabled = false; return s; }
    s.radius = bounded(s.radius,0,0,4096);
    s.refreshSeconds = bounded(s.refreshSeconds,.1,.1,1);
    s.latchSeconds = bounded(s.latchSeconds,10,.5,60);
    s.moveThreshold = bounded(s.moveThreshold,64,1,256);
    s.inspectLimit = std::clamp(s.inspectLimit,64u,16384u);
    s.rayLimit = std::clamp(s.rayLimit,3u,1536u);
    s.visitLimit = std::clamp(s.visitLimit,4096u,262144u);
    s.boundsPadding = bounded(s.boundsPadding,2048,256,8192);
    auto key = [](int k) {
        // Camera recovery/recenter, minimap and hotbar reservations.
        return k == -1 || (k > 0 && k <= 255 && k != 64 && k != 65 && k != 66 &&
            k != 37 && k != 38 && !(k >= 2 && k <= 9));
    };
    auto pad = [](int k) { return k == -1 || (k > 0 && k <= 65535 && (k & (k-1)) == 0); };
    if (!key(s.nextKey)) s.nextKey = -1;
    if (!key(s.previousKey) || s.previousKey == s.nextKey) s.previousKey = -1;
    if (!pad(s.nextGamepad)) s.nextGamepad = -1;
    if (!pad(s.previousGamepad) || s.previousGamepad == s.nextGamepad) s.previousGamepad = -1;
    return s;
}
inline double searchRadius(double nativeRadius, const Settings& s) {
    if (!std::isfinite(nativeRadius) || nativeRadius <= 0 || nativeRadius > 4096) return 0;
    return s.radius > 0 ? std::min(s.radius,nativeRadius) : nativeRadius;
}
struct Context {
    bool enabled = false, runtime = false, reads = false, world = false;
    bool loading = true, dead = true, menu = true, dialogue = true;
    bool firstPerson = true, thirdPerson = false, special = true, conflict = false;
    bool armed = false, combat = false, telekinesis = false, sneaking = false, nativeLiveActor = false;
};
enum class Pass : std::uint32_t {
    none, disabled, unsupported, readFailure, conflict, loading, invalidWorld,
    dead, menu, dialogue, firstPerson, special, incompleteScan, noCandidate, armed, combat, telekinesis, nativeSneakActor
};
constexpr Pass passthrough(Context c) {
    if (!c.enabled) return Pass::disabled;
    if (!c.runtime) return Pass::unsupported;
    if (!c.reads) return Pass::readFailure;
    if (c.conflict) return Pass::conflict;
    if (c.loading) return Pass::loading;
    if (!c.world) return Pass::invalidWorld;
    if (c.dead) return Pass::dead;
    if (c.menu) return Pass::menu;
    if (c.dialogue) return Pass::dialogue;
    if (c.firstPerson) return Pass::firstPerson;
    if (!c.thirdPerson || c.special) return Pass::special;
    if (c.armed) return Pass::armed;
    if (c.combat) return Pass::combat;
    if (c.telekinesis) return Pass::telekinesis;
    if (c.sneaking && c.nativeLiveActor) return Pass::nativeSneakActor;
    return Pass::none;
}
struct Candidate {
    std::uint32_t id = 0;
    Point position;
    bool player = false, disabled = false, deleted = false;
    bool loaded = true, activatable = true, lineOfSight = true;
    Bounds bounds{};
    bool liveActor = false;
};
inline double surfaceDistanceSquared(const Candidate& c, Point player) {
    return distanceSquared(player,validBounds(c.bounds) ? closestPoint(c.bounds,player) : c.position);
}
inline bool rankable(const Candidate& c, bool sneaking) { return !(sneaking && c.liveActor); }
struct PickChanges {
    bool target, grab, clearCollider;
};
constexpr PickChanges pickChanges(bool differentTarget, bool grabbable) {
    return {differentTarget,differentTarget && grabbable,differentTarget};
}
inline bool eligible(const Candidate& c, Point player, double radius) {
    return c.id && !c.player && !c.disabled && !c.deleted && c.loaded && c.activatable &&
        c.lineOfSight && finite(c.position) && finite(player) && std::isfinite(radius) && radius > 0 &&
        surfaceDistanceSquared(c,player) <= radius*radius;
}
struct Ranked { std::uint32_t id; double distance; };
inline std::vector<Ranked> rank(std::span<const Candidate> candidates, Point player, double radius, bool sneaking = false) {
    std::vector<Ranked> result;
    for (const auto& c : candidates) if (rankable(c,sneaking) && eligible(c,player,radius))
        result.push_back({c.id,surfaceDistanceSquared(c,player)});
    std::sort(result.begin(),result.end(),[](const auto& a, const auto& b) {
        return a.distance < b.distance || (a.distance == b.distance && a.id < b.id);
    });
    result.erase(std::unique(result.begin(),result.end(),[](const auto& a, const auto& b) {
        return a.id == b.id;
    }),result.end());
    return result;
}
struct FreshPress {
    bool down = false, armed = true;
    void suspend() { armed = !down; }
    bool update(bool pressed, bool physicalEdge, bool allowed) {
        if (!pressed) { down = false; armed = true; return false; }
        const bool step = allowed && armed && !down && physicalEdge;
        down = true;
        if (!allowed) armed = false;
        return step;
    }
};
class Policy {
public:
    void reset() { sorted_.clear(); clearLatch(); current_ = 0; reason_ = Pass::noCandidate; }
    void refresh(Context context, std::span<const Candidate> candidates, Point player,
        double radius, double now, const Settings& settings, bool complete = true) {
        reason_ = passthrough(context);
        if (reason_ == Pass::none && (!finite(player) || !std::isfinite(now) || radius <= 0 || !std::isfinite(radius)))
            reason_ = Pass::readFailure;
        if (reason_ == Pass::none && !complete) reason_ = Pass::incompleteScan;
        if (reason_ != Pass::none) { sorted_.clear(); clearLatch(); current_ = 0; return; }
        sorted_ = rank(candidates,player,radius,context.sneaking);
        if (sorted_.empty()) { clearLatch(); current_ = 0; reason_ = Pass::noCandidate; return; }
        if (latched_ && (now < latchStart_ || now-latchStart_ >= settings.latchSeconds ||
            distanceSquared(player,latchOrigin_) > settings.moveThreshold*settings.moveThreshold || !contains(latched_))) clearLatch();
        if (latched_) {
            // Freeze order/membership for this latch, pruning invalid neighbours only.
            std::erase_if(frozen_,[&](auto id) { return !contains(id); });
        }
        current_ = latched_ ? latched_ : sorted_.front().id;
    }
    bool cycle(int direction, Point player, double now) {
        if (reason_ != Pass::none || sorted_.empty() || !finite(player) || !std::isfinite(now) || direction == 0) return false;
        if (!latched_) {
            frozen_.clear();
            for (const auto& c : sorted_) frozen_.push_back(c.id);
            latchOrigin_ = player;
        }
        const auto it = std::find(frozen_.begin(),frozen_.end(),current_);
        const auto size = frozen_.size();
        if (!size) return false;
        const auto index = it == frozen_.end() ? 0u : static_cast<std::size_t>(it-frozen_.begin());
        current_ = frozen_[(index+(direction > 0 ? 1 : size-1))%size];
        latched_ = current_; latchStart_ = now;
        return true;
    }
    bool push(std::uint32_t id, Point player, double now, bool allowed) {
        if (!allowed || reason_ != Pass::none || !contains(id) || !finite(player) || !std::isfinite(now)) return false;
        frozen_.clear();
        for (const auto& c : sorted_) frozen_.push_back(c.id);
        current_ = latched_ = id; latchOrigin_ = player; latchStart_ = now;
        return true;
    }
    void release() {
        clearLatch(); current_ = sorted_.empty() ? 0 : sorted_.front().id;
    }
    [[nodiscard]] std::uint32_t current() const { return current_; }
    [[nodiscard]] bool latched() const { return latched_ != 0; }
    [[nodiscard]] Pass reason() const { return reason_; }
    [[nodiscard]] std::vector<std::uint32_t> order() const {
        if (latched_) return frozen_;
        std::vector<std::uint32_t> ids;
        for (const auto& c : sorted_) ids.push_back(c.id);
        return ids;
    }
private:
    bool contains(std::uint32_t id) const {
        return id && std::any_of(sorted_.begin(),sorted_.end(),[&](const auto& c) { return c.id == id; });
    }
    void clearLatch() { latched_ = 0; frozen_.clear(); latchOrigin_ = {}; latchStart_ = 0; }
    std::vector<Ranked> sorted_;
    std::vector<std::uint32_t> frozen_;
    Point latchOrigin_;
    double latchStart_ = 0;
    std::uint32_t current_ = 0, latched_ = 0;
    Pass reason_ = Pass::noCandidate;
};
// Independent limits for cheap visits, nearby inspections and physics rays.
// A capped scan refuses its partial list instead of claiming strict nearest.
struct ScanBudget {
    std::uint32_t limit, inspected = 0;
    bool complete = true;
    bool inspect() {
        if (inspected >= limit) { complete = false; return false; }
        ++inspected; return true;
    }
};
struct RefreshGate {
    double last = -1;
    void reset() { last = -1; }
    bool due(double now, double interval) const { return last < 0 || now < last || now-last >= interval; }
};
class AccessGate {
public:
    void bind(std::uint32_t ownerThread) { thread_.store(ownerThread); }
    [[nodiscard]] std::uint32_t thread() const { return thread_.load(); }
    bool enter(std::uint32_t callerThread) {
        return callerThread && callerThread == thread_.load() && !active_.test_and_set();
    }
    void leave() { active_.clear(); }
private:
    std::atomic<std::uint32_t> thread_{0};
    std::atomic_flag active_ = ATOMIC_FLAG_INIT;
};
} // namespace skycrpg::selection
