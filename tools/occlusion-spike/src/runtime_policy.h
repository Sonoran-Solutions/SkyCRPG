// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "config.h"
#include <atomic>
#include <algorithm>
#include <vector>
#include "../../testprobe/src/perf_policy.h"

namespace skycrpg::occlusion {
inline bool MainThread(std::uint32_t current, std::uint32_t main) { return main && current == main; }
inline constexpr unsigned armRequest = 1, stopRequest = 2, menuRequest = 4, detachRequest = 8,
	loadRequest = 16, povRequest = 32;
inline bool MayArm(unsigned requests) { return requests == armRequest; }
// Off-thread producers retain only flags, never scene pointers. Stops dominate
// an arm request arriving in the same drain, including an earlier queued arm.
class Requests {
	std::atomic_uint pending_{};
public:
	void post(unsigned flags) { pending_.fetch_or(flags); }
	unsigned take() { return pending_.exchange(0); }
	bool pending() const { return pending_.load() != 0; }
};
// Caller storage and sorting scratch both live on the heap. No 512 KB stack copy.
template<std::size_t Capacity>
class FrameRing {
	std::vector<double> values_ = std::vector<double>(Capacity);
	std::size_t next_{}, count_{}, total_{};
	double sum_{};
public:
	void clear() { next_ = count_ = total_ = 0; sum_ = 0; }
	void push(double ms) {
		if (!std::isfinite(ms) || ms < 0) return;
		sum_ += ms;
		values_[next_] = ms; next_ = (next_ + 1) % Capacity;
		count_ = std::min(count_ + 1, Capacity); ++total_;
	}
	double sum() const { return sum_; }
	std::size_t total() const { return total_; }
	skycrpg::testprobe::FrameSummary summary() const {
		skycrpg::testprobe::FrameSummary result{};
		result.count = count_;
		if (!count_) return result;
		std::vector<double> sorted(values_.begin(), values_.begin() + static_cast<std::ptrdiff_t>(count_));
		std::sort(sorted.begin(), sorted.end());
		auto percentile = [&](double p) { return sorted[static_cast<std::size_t>(std::ceil(p * static_cast<double>(count_))) - 1]; };
		result.median = count_ % 2 ? sorted[count_/2] : (sorted[count_/2-1] + sorted[count_/2])/2;
		result.p95 = percentile(.95); result.p99 = percentile(.99); result.maximum = sorted.back();
		return result;
	}
};
inline constexpr unsigned maxRecasts = 8;
inline bool ZeroLengthProbe(const std::array<float, 4>& start, const std::array<float, 4>& goal)
{
	return Finite({start[0], start[1], start[2]}) && Finite({goal[0], goal[1], goal[2]}) &&
		start[0] == goal[0] && start[1] == goal[1] && start[2] == goal[2];
}
// A jump is authorized only after an all-contact hull sweep covers the skipped
// prefix, including start overlaps. A closest-hit ray cannot certify that gap.
inline bool ContinueRay(Point start, Point goal, Point contact, float hull, Point& next)
{
	if (!Finite(start) || !Finite(goal) || !Finite(contact) || !std::isfinite(hull) || hull <= 0) return false;
	const Point d{ goal.x-start.x, goal.y-start.y, goal.z-start.z };
	const double length2 = double(d.x)*d.x + double(d.y)*d.y + double(d.z)*d.z;
	if (!std::isfinite(length2) || length2 <= 0) return false;
	const double length = std::sqrt(length2);
	const double along = ((double(contact.x)-start.x)*d.x + (double(contact.y)-start.y)*d.y + (double(contact.z)-start.z)*d.z)/length;
	if (!std::isfinite(along) || along >= length) return false;
	// Native penetration contacts can project just behind the start. Advancing
	// from max(start, contact) prevents repeated overlap from reversing the ray.
	const double distance = std::max(0.0, along) + std::max(30.0, 2.0*double(hull));
	if (distance >= length) return false; // never turn reaching the goal into a miss
	next = Mix(start, goal, static_cast<float>(distance/length));
	return Finite(next) && !(next.x == goal.x && next.y == goal.y && next.z == goal.z) &&
		!(next.x == start.x && next.y == start.y && next.z == start.z);
}
// Exact 1.7.104 caster exclusions: app-culled objects except layer 17, and
// the player on the actor-processing branches of its layer switch.
inline bool NativeIgnoredContact(unsigned layer, bool culled, bool player)
{
	if (culled && layer != 17) return true;
	return player && (layer == 5 || layer == 8 || layer == 19 || layer == 20 ||
		layer == 25 || layer == 30 || layer == 32 || layer == 33 || layer == 35);
}
inline constexpr unsigned maxGuardContacts = 256;
class GapGuard {
	unsigned count_{};
	bool safe_{ true };
public:
	void contact(bool resolvedStatic, bool explicitMatch) {
		if (count_ < maxGuardContacts + 1) ++count_;
		safe_ = safe_ && count_ <= maxGuardContacts && resolvedStatic && explicitMatch;
	}
	void invalid() { safe_ = false; }
	unsigned count() const { return count_; }
	bool safe() const { return safe_; }
};
class Continuation {
	unsigned count_{};
public:
	enum class Step { cast, depthLimit, noProgress };
	unsigned count() const { return count_; }
	Step advance(Point start, Point goal, Point contact, float hull, Point& next) {
		if (count_ == maxRecasts) return Step::depthLimit;
		if (!ContinueRay(start, goal, contact, hull, next)) return Step::noProgress;
		++count_; return Step::cast;
	}
};

} // namespace skycrpg::occlusion
