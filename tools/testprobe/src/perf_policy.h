// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace skycrpg::testprobe {
struct FrameSummary {
	std::size_t count{}, over33{}, over50{};
	double median{}, p95{}, p99{}, maximum{};
};

template <std::size_t Capacity>
class FrameRing {
	static_assert(Capacity > 0);
	std::array<double, Capacity> values_{};
	std::size_t next_{}, count_{}, total_{};
public:
	void clear() { next_ = count_ = total_ = 0; }
	void push(double ms) {
		if (!std::isfinite(ms) || ms < 0) return;
		values_[next_] = ms;
		next_ = (next_ + 1) % Capacity;
		count_ = std::min(count_ + 1, Capacity);
		++total_;
	}
	std::size_t total() const { return total_; }
	FrameSummary summary() const {
		FrameSummary result{};
		result.count = count_;
		if (!count_) return result;
		auto sorted = values_;
		std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(count_));
		auto percentile = [&](double p) {
			return sorted[static_cast<std::size_t>(std::ceil(p * static_cast<double>(count_))) - 1];
		};
		result.median = count_ % 2 ? sorted[count_ / 2] : (sorted[count_ / 2 - 1] + sorted[count_ / 2]) / 2;
		result.p95 = percentile(.95);
		result.p99 = percentile(.99);
		result.maximum = sorted[count_ - 1];
		for (std::size_t i = 0; i < count_; ++i) {
			result.over33 += sorted[i] > 33.3;
			result.over50 += sorted[i] > 50;
		}
		return result;
	}
};

// One reservation spans both queues, including the gap before actual enqueue.
// stop() never releases it: a pending hop drains or is reused by a restart.
// The adapter serializes these transitions, then enqueues outside its mutex.
class FrameChain {
public:
	enum class Hop { none, ui, main };
	enum class Tick { idle, sample, runaway };
private:
	bool active_{};
	Hop queued_{ Hop::none };
	std::array<double, 8> recent_{};
	std::size_t next_{}, count_{};
public:
	bool active() const { return active_; }
	bool queued() const { return queued_ != Hop::none; }
	Hop hop() const { return queued_; }
	// Returns true only when a new UI hop must be enqueued.
	bool start() {
		if (active_) return false;
		active_ = true;
		next_ = count_ = 0;
		if (queued()) return false;
		queued_ = Hop::ui;
		return true;
	}
	void stop() { active_ = false; }
	void enqueueFailed() { active_ = false; queued_ = Hop::none; }
	Tick uiRun(double now) {
		if (queued_ != Hop::ui) return Tick::idle;
		queued_ = Hop::none;
		if (!active_) return Tick::idle;
		// The ninth call in any trailing 1 ms window aborts the capture.
		if (!std::isfinite(now) || (count_ == recent_.size() && now - recent_[next_] <= 1.0)) {
			active_ = false;
			return Tick::runaway;
		}
		recent_[next_] = now;
		next_ = (next_ + 1) % recent_.size();
		count_ = std::min(count_ + 1, recent_.size());
		queued_ = Hop::main;
		return Tick::sample;
	}
	bool mainRun() {
		if (queued_ != Hop::main) return false;
		queued_ = active_ ? Hop::ui : Hop::none;
		return active_;
	}
};

// The first qualifying frame and five consecutive successors establish settling.
class SettleDetector {
	double threshold_{}, firstEnd_{};
	unsigned consecutive_{};
	bool found_{};
public:
	explicit SettleDetector(double median) : threshold_(2 * median) {}
	bool observe(double period, double end, double& settledEnd) {
		if (found_) return false;
		if (!std::isfinite(threshold_) || threshold_ <= 0 || !std::isfinite(period) ||
			!std::isfinite(end) || period < 0 || period > threshold_) {
			consecutive_ = 0;
			return false;
		}
		if (!consecutive_) firstEnd_ = end;
		if (++consecutive_ == 6) { settledEnd = firstEnd_; found_ = true; return true; }
		return false;
	}
};

enum class PerfCommand { checkpoint, start, stop, invalid };
struct PerfRequest { PerfCommand command; std::string_view label; };
inline PerfRequest ParsePerfRequest(std::string_view text) {
	if (!text.starts_with("@perf ")) return { PerfCommand::checkpoint, {} };
	text.remove_prefix(6);
	const auto space = text.find(' ');
	if (space == std::string_view::npos) return { PerfCommand::invalid, {} };
	const auto verb = text.substr(0, space);
	const auto label = text.substr(space + 1);
	if (label.empty() || label.size() > 40) return { PerfCommand::invalid, {} };
	for (const char c : label) {
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) return { PerfCommand::invalid, {} };
	}
	return { verb == "start" ? PerfCommand::start : verb == "stop" ? PerfCommand::stop : PerfCommand::invalid, label };
}

// All times are QPC milliseconds on one clock; no engine objects or heap storage.
struct InventoryWindow {
	bool active{}, correlated{}, cold{}, settled{};
	double keyMs{}, openMs{}, settledMs{}, preMedian{};
	std::array<double, 30> periods{};
	std::size_t count{};
	SettleDetector detector{ 0 };
	void begin(double key, double open, double median, bool hasKey, bool first) {
		*this = InventoryWindow{};
		active = true; correlated = hasKey; cold = first;
		keyMs = key; openMs = open; preMedian = median;
		detector = SettleDetector(median);
	}
	bool observe(double period, double end) {
		if (!active || end <= openMs) return false;
		if (count < periods.size()) periods[count++] = period;
		double firstEnd{};
		if (detector.observe(period, end, firstEnd) && correlated) {
			settled = true; settledMs = firstEnd - keyMs;
		}
		return end - openMs >= 3000;
	}
};
}  // namespace skycrpg::testprobe
