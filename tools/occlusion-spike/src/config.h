// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace skycrpg::occlusion {
enum class Mode { baseline, observe, filter, materialAlpha, alphaProperty, appCulled, roomSeed1, roomSeed2, roomSeed3, nearClip, filterMaterialAlpha, filterAlphaProperty, invalid };
inline Mode ParseMode(std::string_view s)
{
	constexpr std::array names{ "baseline", "observe", "filter", "material_alpha", "alpha_property", "app_culled", "room_seed_1", "room_seed_2", "room_seed_3", "near_clip", "filter_material_alpha", "filter_alpha_property" };
	for (std::size_t i = 0; i < names.size(); ++i) if (s == names[i]) return static_cast<Mode>(i);
	return Mode::invalid;
}
using IDs = std::array<std::uint32_t, 64>;
inline bool ParseIDs(std::string_view s, IDs& out)
{
	out = {};
	if (s == "none") return true;
	std::size_t count{};
	while (!s.empty()) {
		const auto end = s.find(',');
		auto part = s.substr(0, end);
		while (!part.empty() && part.front() == ' ') part.remove_prefix(1);
		while (!part.empty() && part.back() == ' ') part.remove_suffix(1);
		if (part.starts_with("0x") || part.starts_with("0X")) part.remove_prefix(2);
		if (part.empty() || part.size() > 8 || count == out.size()) return false;
		std::uint32_t id{};
		const auto parsed = std::from_chars(part.data(), part.data() + part.size(), id, 16);
		if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size() || !id) return false;
		out[count++] = id;
		if (end == std::string_view::npos) break;
		s.remove_prefix(end + 1);
		if (s.empty()) return false;
	}
	return true;
}
inline bool FilterMode(Mode m) { return m == Mode::filter || m == Mode::filterMaterialAlpha || m == Mode::filterAlphaProperty; }
inline bool AlphaPropertyMode(Mode m) { return m == Mode::alphaProperty || m == Mode::filterAlphaProperty; }
inline bool CombinedMode(Mode m) { return m == Mode::filterMaterialAlpha || m == Mode::filterAlphaProperty; }
inline bool CombinedTarget(Mode m, std::uint32_t selected, const IDs& refs, const IDs& bases)
{
	if (!CombinedMode(m)) return true;
	if (!selected || refs[0] != selected) return false;
	for (std::size_t i = 1; i < refs.size(); ++i) if (refs[i]) return false;
	for (auto id : bases) if (id) return false;
	return true;
}
inline bool FadeMode(Mode m) { return m == Mode::materialAlpha || m == Mode::alphaProperty || m == Mode::appCulled || CombinedMode(m); }
inline bool SeedMode(Mode m) { return m >= Mode::roomSeed1 && m <= Mode::roomSeed3; }
inline bool CasterMode(Mode m) { return m == Mode::observe || FilterMode(m) || FadeMode(m); }
// Six short endpoint rays measure clearance, not solid occupancy or thickness.
inline bool ValidAlpha(float a) { return std::isfinite(a) && a >= .1f && a < 1; }
inline bool CanDiscard(Mode m, bool armed, bool eligible, bool smoothCamCaller,
	bool resolvedStatic, bool explicitMatch, bool journalAvailable)
{
	return FilterMode(m) && armed && eligible && smoothCamCaller && resolvedStatic && explicitMatch && journalAvailable;
}
struct Point { float x{}, y{}, z{}; };
inline bool Finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline Point Mix(Point a, Point b, float t) { return { a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, a.z + (b.z-a.z)*t }; }
// Independent bounded implementation of the three documented seeding policies.
template<class Contains>
bool Seed(unsigned mode, Point viewer, Point eye, Point center, float radius, float distance, Contains contains)
{
	if (!Finite(viewer) || !Finite(eye)) return false;
	if (mode == 1) return contains(eye);
	if (mode == 2) {
		for (unsigned step = 4; step > 0; --step) if (contains(Mix(viewer, eye, static_cast<float>(step) / 4))) return true;
		return false;
	}
	if (mode != 3 || !Finite(center) || !std::isfinite(radius) || radius < 0 || !std::isfinite(distance) || distance < 0) return false;
	const auto d = Point{ center.x-eye.x, center.y-eye.y, center.z-eye.z };
	const float reach = distance + radius;
	return d.x*d.x + d.y*d.y + d.z*d.z < reach*reach;
}
} // namespace skycrpg::occlusion
