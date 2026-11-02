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
public:
	void clear() { next_ = count_ = total_ = 0; }
	void push(double ms) {
		if (!std::isfinite(ms) || ms < 0) return;
		values_[next_] = ms; next_ = (next_ + 1) % Capacity;
		count_ = std::min(count_ + 1, Capacity); ++total_;
	}
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
// Advance only one representable step beyond the contact projected on the ray.
// Never jump a hull diameter (which could skip enclosure). Overlap/repeated hits
// are kept at the depth bound; this can refuse a beam, never turn the bound into miss.
inline bool ContinueRay(Point start, Point goal, Point contact, Point& next)
{
	if (!Finite(start) || !Finite(goal) || !Finite(contact)) return false;
	const Point d{ goal.x-start.x, goal.y-start.y, goal.z-start.z };
	const double length2 = double(d.x)*d.x + double(d.y)*d.y + double(d.z)*d.z;
	if (!std::isfinite(length2) || length2 <= 0) return false;
	const double t = ((double(contact.x)-start.x)*d.x + (double(contact.y)-start.y)*d.y + (double(contact.z)-start.z)*d.z)/length2;
	if (!std::isfinite(t) || t < 0 || t >= 1) return false;
	next = Mix(start, goal, static_cast<float>(t));
	next.x = std::nextafter(next.x, goal.x); next.y = std::nextafter(next.y, goal.y); next.z = std::nextafter(next.z, goal.z);
	return Finite(next) && !(next.x == goal.x && next.y == goal.y && next.z == goal.z) &&
		!(next.x == start.x && next.y == start.y && next.z == start.z);
}
class Continuation {
	unsigned count_{};
public:
	enum class Step { cast, depthLimit, noProgress };
	unsigned count() const { return count_; }
	Step advance(Point start, Point goal, Point contact, Point& next) {
		if (count_ == maxRecasts) return Step::depthLimit;
		if (!ContinueRay(start, goal, contact, next)) return Step::noProgress;
		++count_; return Step::cast;
	}
};

} // namespace skycrpg::occlusion
