// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
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
// Skyrim reference Euler rotation: clockwise yaw; columns map OBND to world.
inline Bounds referenceBounds(Point position, Point angles, double scale, Point low, Point high) {
    Bounds b{position,multiply(low,scale),multiply(high,scale)};
    const auto sx = std::sin(angles.x), cx = std::cos(angles.x);
    const auto sy = std::sin(angles.y), cy = std::cos(angles.y);
    const auto sz = std::sin(angles.z), cz = std::cos(angles.z);
    b.axes = {Point{cy*cz,sx*sy*cz-cx*sz,cx*sy*cz+sx*sz},
        Point{cy*sz,sx*sy*sz+cx*cz,cx*sy*sz-sx*cz},Point{-sy,sx*cy,cx*cy}};
    b.available = finite(angles) && std::isfinite(scale) && scale > 0;
    return b;
}
// Native canPick's type rule; name/state checks still decide eligibility.
constexpr bool interactiveType(std::uint32_t type, bool carryLight = false) {
    switch (type) {
    case 0x22: case 0x23: return false; // STAT, SCOL
    case 0x1f: return carryLight;
    default: return true; // includes SCRL (0x17) and PROJ (0x32)
    }
}
// CAS returns the observed old word. Retry reader churn, never a writer; keep
// the native low-28-bit reader count and reject overflow without signed UB.
template<class CAS> bool tryReadAcquire(std::int32_t old, CAS compare, std::uint64_t& retries) {
    for (unsigned attempt = 0; attempt < 8 && old >= 0; ++attempt) {
        if ((old & 0x0fffffff) == 0x0fffffff) return false;
        const auto observed = compare(old,old+1);
        if (observed == old) return true;
        ++retries; old = observed;
    }
    return false;
}
inline Point closestPoint(const Bounds& b, Point p) {
    const auto offset = subtract(p,b.origin);
    const std::array low{b.low.x,b.low.y,b.low.z}, high{b.high.x,b.high.y,b.high.z};
    auto result = b.origin;
    for (std::size_t i = 0; i < 3; ++i)
        result = add(result,multiply(b.axes[i],std::clamp(dot(offset,b.axes[i]),low[i],high[i])));
    return result;
}
inline Point localToWorld(const Bounds& b, Point local) {
    return add(add(add(b.origin,multiply(b.axes[0],local.x)),multiply(b.axes[1],local.y)),multiply(b.axes[2],local.z));
}
inline Point boundsCentre(const Bounds& b) { return localToWorld(b,multiply(add(b.low,b.high),.5)); }
inline bool inBroadPhase(Point player, Point origin, double radius, double padding) {
    return finite(origin) && distanceSquared(player,origin) <= (radius+padding)*(radius+padding);
}
struct Settings {
    int schema = 1;
    bool enabled = true, allowExternal = false;
    double radius = 0; // zero reads native fActivatePickLength:Interface; never extends it
    double refreshSeconds = .1, latchSeconds = 10, moveThreshold = 64;
    std::uint32_t inspectLimit = 4096, rayLimit = 48, visitLimit = 65536;
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
inline std::array<Point,3> losPoints(const Candidate& c, Point body) {
    if (!validBounds(c.bounds)) return {c.position,c.position,c.position};
    const auto& b = c.bounds;
    const auto centre = multiply(add(b.low,b.high),.5);
    return {closestPoint(b,body),
        localToWorld(b,{centre.x,centre.y,b.low.z+(b.high.z-b.low.z)*.75}),
        boundsCentre(b)};
}
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
// A bounded FIFO of physical edges. One exchange consumes a batch exactly once;
// opposing presses retain order. Producers never wait on the Pick job.
class CycleRequests {
public:
    bool post(int direction) {
        if (!direction) return false;
        auto old = pending_.load();
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            unsigned shift = 0;
            while (shift < 32 && ((old >> shift) & 3u)) shift += 2;
            if (shift == 32) return false;
            const auto next = old | ((direction > 0 ? 1u : 2u) << shift);
            if (pending_.compare_exchange_strong(old,next)) return true;
        }
        return false;
    }
    std::uint32_t take() { return pending_.exchange(0); }
    static std::uint32_t count(std::uint32_t batch) {
        std::uint32_t result = 0;
        while (batch) { ++result; batch >>= 2; }
        return result;
    }
    static int pop(std::uint32_t& batch) {
        const auto code = batch & 3u; batch >>= 2;
        return code == 1 ? 1 : code == 2 ? -1 : 0;
    }
private:
    std::atomic<std::uint32_t> pending_{0};
};
enum class Validation { valid, invalid, unknown, exhausted };
struct LazyResult {
    std::vector<Candidate> accepted;
    std::vector<std::uint32_t> evaluated;
    bool unknown = false, exhausted = false;
};
// Input is cheap geometry only. Expensive native predicates run nearest outward.
// An unknown result prevents a strict-nearest claim and preserves prior state.
template<class Validate> LazyResult lazyValidate(std::span<const Candidate> candidates,
    Point player, double radius, bool sneaking, std::uint32_t limit, Validate validateCandidate) {
    LazyResult result;
    const auto sorted = rank(candidates,player,radius,sneaking);
    for (const auto& entry : sorted) {
        if (result.evaluated.size() == limit) { result.exhausted = true; break; }
        const auto it = std::find_if(candidates.begin(),candidates.end(),[&](const auto& c) { return c.id == entry.id; });
        result.evaluated.push_back(entry.id);
        const auto status = validateCandidate(*it);
        if (status == Validation::unknown) { result.unknown = true; break; }
        if (status == Validation::exhausted) {
            result.evaluated.pop_back(); result.exhausted = true; break;
        }
        if (status == Validation::valid) result.accepted.push_back(*it);
    }
    return result;
}
inline bool nativeIsNew(std::uint32_t id, std::span<const std::uint32_t> scanned, bool latched) {
    return id && !latched && std::find(scanned.begin(),scanned.end(),id) == scanned.end();
}
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
        expire(player,now,settings);
        if (latched_ && !contains(latched_)) clearLatch();
        if (latched_) {
            // Freeze order/membership for this latch, pruning invalid neighbours only.
            std::erase_if(frozen_,[&](auto id) { return !contains(id); });
        }
        current_ = latched_ ? latched_ : sorted_.front().id;
    }
    bool acceptScan(Context context, const LazyResult& checked, Point player,
        double radius, double now, const Settings& settings, bool complete) {
        if (!complete || checked.unknown) return false;
        const bool validLatch = latched_ && std::any_of(checked.accepted.begin(),checked.accepted.end(),
            [&](const auto& value) { return value.id == latched_; });
        const auto inPrefix = [&](std::uint32_t id) {
            return std::find(checked.evaluated.begin(),checked.evaluated.end(),id) != checked.evaluated.end();
        };
        const bool prefixAccepted = std::any_of(checked.accepted.begin(),checked.accepted.end(),
            [&](const auto& value) { return inPrefix(value.id); });
        if (checked.exhausted && !prefixAccepted && !validLatch) return false;
        auto candidates = checked.accepted;
        if (!validLatch) std::erase_if(candidates,[&](const auto& value) { return !inPrefix(value.id); });
        refresh(context,candidates,player,radius,now,settings);
        return reason_ == Pass::none;
    }
    bool cycle(int direction, Point player, double now) {
        if (reason_ != Pass::none || sorted_.empty() || !finite(player) || !std::isfinite(now) || direction == 0) return false;
        if (!latched_) {
            frozen_.clear();
            for (const auto& c : sorted_) {
                if (frozen_.size() == 8) break;
                frozen_.push_back(c.id);
            }
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
        auto next = frozen_;
        if (!latched_) {
            next.clear();
            for (const auto& c : sorted_) {
                if (next.size() == 8) break;
                next.push_back(c.id);
            }
        }
        if (std::find(next.begin(),next.end(),id) == next.end()) return false;
        frozen_ = std::move(next);
        current_ = latched_ = id; latchOrigin_ = player; latchStart_ = now;
        return true;
    }
    std::string_view expire(Point player, double now, const Settings& settings) {
        std::string_view reason;
        if (latched_) {
            if (now < latchStart_ || now-latchStart_ >= settings.latchSeconds) reason = "timeout";
            else if (distanceSquared(player,latchOrigin_) > settings.moveThreshold*settings.moveThreshold) reason = "movement";
        }
        if (!reason.empty()) release();
        return reason;
    }
    void invalidate(std::uint32_t id) {
        std::erase_if(sorted_,[&](const auto& c) { return c.id == id; });
        std::erase(frozen_,id);
        if (id == latched_) clearLatch();
        current_ = latched_ ? latched_ : sorted_.empty() ? 0 : sorted_.front().id;
        reason_ = current_ ? Pass::none : Pass::noCandidate;
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
    bool enter() { return !active_.test_and_set(std::memory_order_acquire); }
    void leave() { active_.clear(std::memory_order_release); }
private:
    std::atomic_flag active_ = ATOMIC_FLAG_INIT;
};
constexpr std::string_view passName(Pass reason) {
    switch (reason) {
    case Pass::none: return "none";
    case Pass::disabled: return "disabled";
    case Pass::unsupported: return "unsupported";
    case Pass::readFailure: return "read_failure";
    case Pass::conflict: return "conflict";
    case Pass::loading: return "loading";
    case Pass::invalidWorld: return "invalid_world";
    case Pass::dead: return "dead";
    case Pass::menu: return "menu";
    case Pass::dialogue: return "dialogue";
    case Pass::firstPerson: return "first_person";
    case Pass::special: return "special";
    case Pass::incompleteScan: return "incomplete_scan";
    case Pass::noCandidate: return "no_candidate";
    case Pass::armed: return "armed";
    case Pass::combat: return "combat";
    case Pass::telekinesis: return "telekinesis";
    case Pass::nativeSneakActor: return "native_sneak_actor";
    }
    return "unknown";
}
enum class EventKind { choose, cycle, pick, latch };
enum class EventSource { nearest, cycle, native, external };
enum class EventEmission { none, row, dropped };
constexpr std::string_view sourceName(EventSource source) {
    switch (source) {
    case EventSource::nearest: return "nearest";
    case EventSource::cycle: return "cycle";
    case EventSource::native: return "native";
    case EventSource::external: return "external";
    }
    return "unknown";
}
struct SelectionEvent {
    EventKind kind = EventKind::choose;
    EventSource source = EventSource::nearest;
    std::uint32_t id = 0, nativeID = 0;
    double distance = -1;
    bool bounds = false;
    std::string_view reason = "none";
    std::int64_t cycleIndex = -1;
    std::size_t cycleCount = 0;
};
// Process-wide budget; world/latch resets must not replenish it. Reasons are
// static literals, so remembering a pick decision never allocates or owns text.
class EventLog {
public:
    static constexpr std::uint32_t limit = 2000;
    EventEmission observe(EventKind kind, std::uint32_t id, EventSource source, std::string_view reason,
        std::uint32_t nativeID = 0) {
        // Every accepted physical cycle and every change of final pick pair
        // must remain observable, including A -> B -> A and single-entry wrap.
        bool duplicate = false;
        if (kind == EventKind::pick) {
            duplicate = havePick_ && lastPick_.id == id && lastPick_.nativeID == nativeID &&
                lastPick_.source == source && lastPick_.reason == reason;
            lastPick_ = {kind,id,source,reason,nativeID}; havePick_ = true;
        } else if (kind != EventKind::cycle) {
            for (std::uint32_t i = 0; i < keyCount_; ++i) {
                const auto& key = keys_[i];
                if (key.kind == kind && key.id == id && key.source == source && key.reason == reason) {
                    duplicate = true; break;
                }
            }
        }
        if (duplicate) { ++suppressed_; return EventEmission::none; }
        if (used_ == limit) {
            ++suppressed_;
            if (dropped_) return EventEmission::none;
            dropped_ = true; ++sequence_; return EventEmission::dropped;
        }
        if (kind != EventKind::pick && kind != EventKind::cycle) keys_[keyCount_++] = {kind,id,source,reason,nativeID};
        ++used_; ++sequence_; return EventEmission::row;
    }
    [[nodiscard]] std::uint32_t sequence() const { return sequence_; }
    [[nodiscard]] std::uint64_t suppressed() const { return suppressed_; }
private:
    struct Key {
        EventKind kind{};
        std::uint32_t id = 0;
        EventSource source{};
        std::string_view reason;
        std::uint32_t nativeID = 0;
    };
    std::array<Key,limit> keys_{};
    Key lastPick_{};
    std::uint32_t used_ = 0, keyCount_ = 0, sequence_ = 0;
    bool havePick_ = false;
    std::uint64_t suppressed_ = 0;
    bool dropped_ = false;
};
inline std::string formatEvent(const SelectionEvent& event, EventEmission emission,
    std::uint32_t sequence, std::uint64_t milliseconds) {
    if (emission == EventEmission::none) return {};
    std::ostringstream out;
    out.imbue(std::locale::classic());
    if (emission == EventEmission::dropped) {
        out << "selection event=dropped seq=" << sequence << " ms=" << milliseconds
            << " reason=budget limit=" << EventLog::limit;
        return out.str();
    }
    const auto kind = event.kind == EventKind::choose ? "choose" : event.kind == EventKind::cycle ? "cycle" : event.kind == EventKind::latch ? "latch" : "pick";
    out << "selection event=" << kind << " seq=" << sequence << " ms=" << milliseconds
        << " form_id=" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << event.id
        << std::dec << " distance_units=";
    if (std::isfinite(event.distance) && event.distance >= 0) out << std::fixed << std::setprecision(2) << event.distance;
    else out << "unavailable";
    out << " distance_source=" << (event.bounds ? "bounds" : "origin")
        << " source=" << sourceName(event.source) << " native_form_id="
        << std::hex << std::setw(8) << event.nativeID << std::dec << " reason=" << event.reason
        << " cycle_index=" << event.cycleIndex << " cycle_count=" << event.cycleCount;
    return out.str();
}
} // namespace skycrpg::selection
