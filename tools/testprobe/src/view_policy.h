// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Portable rules for the P5.5-01 view_sample observation: request grammar,
// INI bounds, projection, pose angles, ray status folding, transmittance,
// reference treatment and JSON shape. No engine types; the Windows adapter in
// plugin.cpp feeds plain values in. Geometric clearance is diagnostic only.
#include "json_line.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace skycrpg::testprobe {
inline constexpr unsigned kViewProtocol = 1;
// Skyrim.esm COLL L_LOS (0x0008878A) is BNAM 41; its CNAM list is symmetric
// with every other COLL record and contains statics, anim statics, clutter,
// bipeds, trees, props, terrain, ground and dead bipeds, but not
// L_CHARCONTROLLER, L_INVISIBLE_WALL or the transparent layers.
inline constexpr std::uint32_t kViewRayLayer = 41;
inline constexpr unsigned kViewMaxInFlight = 4;      // outstanding samples, requested + periodic
inline constexpr unsigned kViewMaxHops = 600;        // UI -> main-queue -> UI retries for a main-thread drain
inline constexpr double kViewMaxWaitMs = 5000;       // and the wall-clock bound on the same wait
inline constexpr unsigned kViewCastBudget = 256;     // Havok casts per sample, all rays together
inline constexpr unsigned kViewPlayerSkips = 8;      // player surfaces skipped per ray
inline constexpr unsigned kViewTreatmentNodes = 256; // nodes visited per hit reference
inline constexpr unsigned kViewParentLinks = 64;     // hit object -> reference root walk
inline constexpr unsigned kViewSkeletonNodes = 1024; // player bone search
inline constexpr double kViewEndClearance = 2;       // rays stop this many units before the point
inline constexpr double kViewRecastStep = 1;         // re-cast this many units past each hit
inline constexpr double kViewPointLift = 8;          // feet and ring points sit above the player's z

// ---- Request grammar and settings ---------------------------------------
enum class ViewCommand { none, sample, invalid };
struct ViewRequest {
	ViewCommand command{ ViewCommand::none };
	std::string label;
};
inline bool ViewLabel(std::string_view label) noexcept
{
	if (label.empty() || label.size() > 40) return false;
	for (const char c : label) {
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
			c == '_' || c == '.' || c == '-')) return false;
	}
	return true;
}
// "@view <label>", no trailing newline. Only consulted while bViewSamples=1;
// otherwise the text stays an ordinary raw checkpoint tag.
inline ViewRequest ParseViewRequest(std::string_view text)
{
	if (!text.starts_with("@view ")) return {};
	text.remove_prefix(6);
	ViewRequest result;
	result.label = text;
	result.command = ViewLabel(text) ? ViewCommand::sample : ViewCommand::invalid;
	return result;
}

// Strict ASCII digits; anything else, empty or out of range gives the fallback.
inline std::uint32_t ParseBounded(std::string_view text, std::uint32_t minimum, std::uint32_t maximum,
	std::uint32_t fallback) noexcept
{
	std::uint32_t value{};
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
	if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
		value < minimum || value > maximum) return fallback;
	return value;
}
inline constexpr std::uint32_t kViewMinIntervalMs = 100;
// Missing/invalid = 0 (off). 1..99 are raised to 100 ms; the cap matches iStateIntervalMs.
inline std::uint32_t ParseViewInterval(std::string_view text) noexcept
{
	const auto value = ParseBounded(text, 0, 0x7FFFFFFFU, 0);
	return value == 0 ? 0 : (std::max)(value, kViewMinIntervalMs);
}
struct ViewSettings {
	bool enabled{};
	bool mainThreadOnly{};
	std::uint32_t intervalMs{};
	std::uint32_t ringPoints{ 8 };
	std::uint32_t ringRadius{ 128 };
	std::uint32_t maxHits{ 8 };
};
inline ViewSettings ParseViewSettings(std::string_view enabled, std::string_view interval,
	std::string_view ringPoints, std::string_view ringRadius, std::string_view maxHits,
	std::string_view mainThreadOnly = {}) noexcept
{
	ViewSettings result;
	result.enabled = enabled == "1";
	result.mainThreadOnly = mainThreadOnly == "1";
	result.intervalMs = result.enabled ? ParseViewInterval(interval) : 0;
	result.ringPoints = ParseBounded(ringPoints, 0, 16, 8);
	result.ringRadius = ParseBounded(ringRadius, 16, 1024, 128);
	result.maxHits = ParseBounded(maxHits, 1, 16, 8);
	return result;
}

// ---- Geometry ------------------------------------------------------------
struct Vec3 {
	double x{}, y{}, z{};
};
inline Vec3 operator+(Vec3 a, Vec3 b) noexcept { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 operator-(Vec3 a, Vec3 b) noexcept { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 operator*(Vec3 a, double s) noexcept { return { a.x * s, a.y * s, a.z * s }; }
inline double Dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double Length(Vec3 a) noexcept { return std::sqrt(Dot(a, a)); }
inline bool Finite(Vec3 a) noexcept { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
inline constexpr double kPi = 3.14159265358979323846;
inline double Degrees(double radians) noexcept { return radians * 180.0 / kPi; }
inline double NormalizeDegrees(double degrees) noexcept
{
	if (!std::isfinite(degrees)) return degrees;
	double result = std::fmod(degrees, 360.0);
	if (result < 0) result += 360.0;
	return result >= 360.0 ? 0.0 : result;
}

// NiRect<float> order as stored at NiCamera+0x174: left, right, top, bottom.
struct Port {
	double left{}, right{ 1 }, top{ 1 }, bottom{};
};
struct Projection {
	bool valid{};    // finite matrix/port/point
	bool inFront{};  // w > tolerance
	double x{}, y{}; // normalized, origin top-left, y down; meaningful only inFront
	double depth{};  // w
	bool onScreen{};
};
// Same arithmetic as WorldPtToScreenPt3 (ID 70640, RVA EF1180): w = row3.(p,1),
// x = row0.(p,1)/w mapped through (right-left)/2 and (left+right)/2, y likewise
// with (top-bottom). The engine divides by |w| and so mirrors points behind the
// camera; here w <= tolerance is reported as not in front instead.
inline Projection Project(const float (&m)[4][4], const Port& port, Vec3 p, double tolerance = 1e-5)
{
	Projection result;
	for (const auto& row : m)
		for (const float value : row)
			if (!std::isfinite(value)) return result;
	if (!Finite(p) || !std::isfinite(port.left) || !std::isfinite(port.right) || !std::isfinite(port.top) ||
		!std::isfinite(port.bottom) || port.right == port.left || port.top == port.bottom) return result;
	const auto dot = [&p](const float (&row)[4]) {
		return static_cast<double>(row[0]) * p.x + static_cast<double>(row[1]) * p.y +
			static_cast<double>(row[2]) * p.z + static_cast<double>(row[3]);
	};
	const double w = dot(m[3]);
	result.valid = std::isfinite(w);
	result.depth = w;
	if (!result.valid || !(w > tolerance)) return result;
	result.inFront = true;
	const double cx = dot(m[0]) / w, cy = dot(m[1]) / w;
	const double sx = cx * (port.right - port.left) * 0.5 + (port.left + port.right) * 0.5;
	const double sy = cy * (port.top - port.bottom) * 0.5 + (port.top + port.bottom) * 0.5;
	result.x = (sx - port.left) / (port.right - port.left);
	result.y = (port.top - sy) / (port.top - port.bottom);
	result.valid = std::isfinite(result.x) && std::isfinite(result.y);
	result.onScreen = result.valid && result.x >= 0 && result.x <= 1 && result.y >= 0 && result.y <= 1;
	return result;
}

// Heading convention of TESObjectREFR angle.z: 0 = +Y (north), clockwise
// toward +X. Pitch is positive when looking down, like angle.x.
struct Angles {
	double yaw{}, pitch{};
};
inline std::optional<Angles> ForwardAngles(Vec3 forward) noexcept
{
	const double length = Length(forward);
	if (!std::isfinite(length) || length < 1e-9) return std::nullopt;
	const double horizontal = std::sqrt(forward.x * forward.x + forward.y * forward.y);
	return Angles{ NormalizeDegrees(Degrees(std::atan2(forward.x, forward.y))),
		Degrees(std::atan2(-forward.z, horizontal)) };
}
inline std::optional<double> AngleBetweenDegrees(Vec3 a, Vec3 b) noexcept
{
	const double la = Length(a), lb = Length(b);
	if (!std::isfinite(la) || !std::isfinite(lb) || la < 1e-9 || lb < 1e-9) return std::nullopt;
	const double c = Dot(a, b) / (la * lb);
	return Degrees(std::acos((std::clamp)(c, -1.0, 1.0)));
}
// Perspective NiFrustum planes are tangents at unit distance (Hypothesis,
// cross-checked at run time against PlayerCamera::worldFOV).
inline std::optional<double> FrustumDegrees(double low, double high) noexcept
{
	if (!std::isfinite(low) || !std::isfinite(high) || !(high > low)) return std::nullopt;
	return Degrees(std::atan(high) - std::atan(low));
}
// Ring point i of n at the given radius, starting at the camera's heading
// (the far side of the player) and proceeding clockwise seen from above.
inline Vec3 RingPoint(Vec3 center, double radius, std::uint32_t count, std::uint32_t index,
	double startYawDegrees, double lift) noexcept
{
	const double step = count ? 360.0 / static_cast<double>(count) : 0.0;
	const double yaw = (startYawDegrees + step * static_cast<double>(index)) * kPi / 180.0;
	return { center.x + radius * std::sin(yaw), center.y + radius * std::cos(yaw), center.z + lift };
}
struct RayPlan {
	Vec3 from, to;
	double length{};
};
// Camera -> point, stopping kViewEndClearance short so the point's own
// supporting surface is not reported as an occluder.
inline std::optional<RayPlan> PlanRay(Vec3 from, Vec3 target, double clearance = kViewEndClearance) noexcept
{
	if (!Finite(from) || !Finite(target)) return std::nullopt;
	const Vec3 delta = target - from;
	const double total = Length(delta);
	if (!std::isfinite(total) || total <= clearance + kViewRecastStep) return std::nullopt;
	const double length = total - clearance;
	return RayPlan{ from, from + delta * (length / total), length };
}
// Next cast origin after a hit at `fraction` of the segment from -> to.
inline Vec3 Advance(Vec3 from, Vec3 to, double fraction, double step = kViewRecastStep) noexcept
{
	const Vec3 delta = to - from;
	const double length = Length(delta);
	if (!(length > 0)) return to;
	return from + delta * ((std::clamp)(fraction, 0.0, 1.0) + step / length);
}

// ---- Treatment and folding ----------------------------------------------
enum class Treatment { opaque, faded, hidden, unknown };
enum class RayStatus { clear, faded_only, blocked, indeterminate };
inline std::string_view TreatmentName(Treatment value) noexcept
{
	switch (value) {
	case Treatment::opaque: return "opaque";
	case Treatment::faded: return "faded";
	case Treatment::hidden: return "hidden";
	default: return "unknown";
	}
}
inline std::string_view StatusName(RayStatus value) noexcept
{
	switch (value) {
	case RayStatus::clear: return "clear";
	case RayStatus::faded_only: return "faded_only";
	case RayStatus::blocked: return "blocked";
	default: return "indeterminate";
	}
}

// One drawable part of a hit reference, as read from the scene graph.
struct PartSample {
	bool culled{};    // this geometry or an ancestor below the root is app-culled
	bool blending{};  // NiAlphaProperty present with alphaFlags bit 0 (blend enable)
	bool lighting{};  // BSShaderMaterial::GetType() == kLighting (2)
	double alpha{ 1 };// BSLightingShaderMaterialBase::materialAlpha
};
struct TreatmentResult {
	Treatment treatment{ Treatment::unknown };
	std::optional<double> alpha;
	std::string_view reason;  // set only for unknown
};
// Conservative: any visible opaque part makes the reference opaque, because
// the probe cannot tell which drawn part a collision ray corresponds to.
class TreatmentAccumulator {
	bool hidden_{}, capped_{}, opaque_{}, unknown_{};
	std::string_view unknownReason_;
	unsigned visible_{}, culled_{};
	double maxAlpha_{ -1 };
public:
	void hiddenChain() noexcept { hidden_ = true; }
	void nodeCap() noexcept { capped_ = true; }
	void part(const PartSample& part) noexcept
	{
		if (part.culled) { ++culled_; return; }
		++visible_;
		if (!part.blending) { opaque_ = true; return; }
		if (!part.lighting) { if (!unknown_) unknownReason_ = "non_lighting_blend"; unknown_ = true; return; }
		if (!std::isfinite(part.alpha) || part.alpha < 0 || part.alpha > 1) {
			if (!unknown_) unknownReason_ = "invalid_alpha";
			unknown_ = true;
			return;
		}
		if (part.alpha >= 1) { opaque_ = true; return; }
		maxAlpha_ = (std::max)(maxAlpha_, part.alpha);
	}
	TreatmentResult result() const noexcept
	{
		if (hidden_) return { Treatment::hidden, std::nullopt, {} };
		if (capped_) return { Treatment::unknown, std::nullopt, "node_cap" };
		if (opaque_) return { Treatment::opaque, std::nullopt, {} };
		if (unknown_) return { Treatment::unknown, std::nullopt, unknownReason_ };
		if (visible_) return { Treatment::faded, maxAlpha_, {} };
		if (culled_) return { Treatment::hidden, std::nullopt, {} };
		return { Treatment::unknown, std::nullopt, "no_geometry" };
	}
};

struct HitRecord {
	std::optional<std::uint32_t> ref, base;
	std::uint32_t layer{};
	double distance{};
	Treatment treatment{ Treatment::unknown };
	std::optional<double> alpha;
	std::string_view reason;
};
struct RayRecord {
	bool ran{};          // every cast needed for a complete answer ran
	bool truncated{};    // more surfaces lay before the point than maxHits
	bool worldChanged{};
	std::string_view reason;  // why the ray is indeterminate, when it is
	std::vector<HitRecord> hits;
	std::uint32_t skippedPlayer{};
	std::uint32_t casts{};
};
struct RayFold {
	RayStatus status{ RayStatus::indeterminate };
	std::optional<double> transmittance;
};
// clear only with no hits or hidden hits; faded never folds into clear;
// truncated, not run and changed worlds are indeterminate.
inline RayFold FoldRay(const RayRecord& ray) noexcept
{
	if (!ray.ran || ray.truncated || ray.worldChanged) return {};
	bool opaque = false, unknown = false, faded = false;
	double transmittance = 1;
	for (const auto& hit : ray.hits) {
		switch (hit.treatment) {
		case Treatment::opaque: opaque = true; break;
		case Treatment::hidden: break;
		case Treatment::faded:
			if (!hit.alpha || !std::isfinite(*hit.alpha) || *hit.alpha < 0 || *hit.alpha >= 1) unknown = true;
			else { faded = true; transmittance *= 1 - *hit.alpha; }
			break;
		default: unknown = true; break;
		}
	}
	if (opaque) return { RayStatus::blocked, 0.0 };
	if (unknown) return { RayStatus::blocked, std::nullopt };
	if (faded) return { RayStatus::faded_only, transmittance };
	return { RayStatus::clear, 1.0 };
}
inline std::string_view RayReason(const RayRecord& ray) noexcept
{
	if (!ray.reason.empty()) return ray.reason;
	if (ray.worldChanged) return "world_changed";
	if (ray.truncated) return "truncated";
	if (!ray.ran) return "not_run";
	return {};
}

// ---- Cadence --------------------------------------------------------------
// Periodic samples never queue behind themselves: a tick that finds one still
// outstanding is counted and reported on the next sample. Not thread-safe; the
// adapter serializes it under its own mutex.
class ViewCadence {
	bool pending_{};
	std::uint32_t skipped_{};
public:
	bool tick(std::uint32_t& coalesced) noexcept
	{
		if (pending_) { ++skipped_; return false; }
		pending_ = true;
		coalesced = skipped_;
		skipped_ = 0;
		return true;
	}
	void done() noexcept { pending_ = false; }
	bool pending() const noexcept { return pending_; }
};

// ---- JSON ----------------------------------------------------------------
inline std::string VecJson(Vec3 v)
{
	return Object{}.Numeric("x", v.x).Numeric("y", v.y).Numeric("z", v.z).Json();
}
inline std::string HitJson(const HitRecord& hit)
{
	Object o;
	if (hit.ref) o.String("ref", FormID(*hit.ref)); else o.Null("ref");
	if (hit.base) o.String("base", FormID(*hit.base)); else o.Null("base");
	o.Numeric("layer", hit.layer).Numeric("distance", hit.distance).String("treatment", TreatmentName(hit.treatment));
	if (hit.treatment == Treatment::faded && hit.alpha) o.Numeric("alpha", *hit.alpha); else o.Null("alpha");
	if (hit.reason.empty()) o.Null("reason"); else o.String("reason", hit.reason);
	return o.Json();
}
inline std::string RayJson(const RayRecord& ray)
{
	const auto fold = FoldRay(ray);
	Object o;
	o.String("status", StatusName(fold.status));
	if (fold.transmittance) o.Numeric("transmittance", *fold.transmittance); else o.Null("transmittance");
	std::string hits = "[";
	for (const auto& hit : ray.hits) {
		if (hits.size() > 1) hits += ',';
		hits += HitJson(hit);
	}
	o.Raw("hits", hits + ']').Boolean("truncated", ray.truncated);
	const auto reason = fold.status == RayStatus::indeterminate ? RayReason(ray) : std::string_view{};
	if (reason.empty()) o.Null("reason"); else o.String("reason", reason);
	o.Numeric("skipped_player", ray.skippedPlayer).Numeric("casts", ray.casts);
	return o.Json();
}
struct PointRecord {
	std::string name;
	std::optional<Vec3> world;
	std::optional<Projection> projection;  // nullopt: projection unavailable
	RayRecord ray;
};
inline std::string PointJson(const PointRecord& point)
{
	Object o;
	o.String("name", point.name);
	if (point.world) o.Raw("world", VecJson(*point.world)); else o.Null("world");
	if (point.world && point.projection && point.projection->valid && point.projection->inFront)
		o.Raw("screen", Object{}.Numeric("x", point.projection->x).Numeric("y", point.projection->y).Json());
	else o.Null("screen");
	if (point.world && point.projection && point.projection->valid) o.Boolean("on_screen", point.projection->onScreen);
	else o.Null("on_screen");
	o.Raw("ray", RayJson(point.ray));
	return o.Json();
}
inline std::string PointsJson(const std::vector<PointRecord>& points)
{
	std::string result = "[";
	for (const auto& point : points) {
		if (result.size() > 1) result += ',';
		result += PointJson(point);
	}
	return result + ']';
}
inline std::string CountsJson(const std::vector<PointRecord>& points)
{
	std::uint32_t counts[4]{};
	for (const auto& point : points) ++counts[static_cast<unsigned>(FoldRay(point.ray).status)];
	return Object{}.Numeric("clear", counts[0]).Numeric("faded_only", counts[1])
		.Numeric("blocked", counts[2]).Numeric("indeterminate", counts[3]).Json();
}
// Screen-space box of the in-front player points, normalized like `screen`.
inline std::string BoxJson(const std::vector<PointRecord>& points)
{
	bool any = false;
	double x0{}, x1{}, y0{}, y1{};
	for (const auto& point : points) {
		if (!point.world || !point.projection || !point.projection->valid || !point.projection->inFront) continue;
		const auto& p = *point.projection;
		if (!any) { x0 = x1 = p.x; y0 = y1 = p.y; any = true; continue; }
		x0 = (std::min)(x0, p.x); x1 = (std::max)(x1, p.x);
		y0 = (std::min)(y0, p.y); y1 = (std::max)(y1, p.y);
	}
	if (!any) return "null";
	return Object{}.Numeric("x_min", x0).Numeric("x_max", x1).Numeric("y_min", y0).Numeric("y_max", y1).Json();
}
}  // namespace skycrpg::testprobe
