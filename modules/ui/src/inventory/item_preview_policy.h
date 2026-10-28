// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Portable policy for the player-inventory 3D item preview placement (DYL-168).
// No engine types: item_preview_placement.cpp copies RE::Inventory3DManager's
// cached itemPos/itemScale (and their copies) in and out of these structs.
// No INI setting is read for placement or written at all.

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <optional>
#include <string>
#include <string_view>

namespace skycrpg::ui::inventory::preview {

// Stage geometry. SkyUI SKI_ConfigMenu.ApplySettings: stage 1280x720, centre
// x 640, 1 position unit = 8 stage units, negative X moves the model right.
// Round-4 evidence: fInventory3DItemPosX -36.403 put the model centre at stage
// ~934 (640 + 8 * 36.403 = 931.2). Assumption: Inventory3DManager::itemPos.x
// holds that same X (copied from the INI at Begin3D), itemPos.z the INI Z and
// itemScale the INI scale; the owner run verifies it from the logged values.
inline constexpr double kStageWidth = 1280.0;
inline constexpr double kStageHeight = 720.0;
inline constexpr double kStageCenterX = 640.0;
inline constexpr double kStageUnitsPerPosition = 8.0;
inline constexpr double kEdgeMargin = 16.0;
// Worst observed model: the Elder Scroll, 760 px at 1.4945 px/unit and scale
// 1.5 = 508.5 stage units, i.e. 339 units per scale unit.
inline constexpr double kWorstWidthPerScale = 339.0;
inline constexpr double kFitFraction = 0.9;
inline constexpr float kMinScaleFactor = 0.4f;
inline constexpr float kMaxScaleFactor = 1.0f;
inline constexpr float kMinXShift = -100.0f;
inline constexpr float kMaxXShift = 20.0f;

struct Config
{
	bool manage{ true };
	std::optional<float> scaleFactor;  // unset: computed from the column
	std::optional<float> xShift;       // unset: computed from the column
};

struct ConfigParse
{
	Config config{};
	bool manageValid{ true };
	bool scaleValid{ true };
	bool shiftValid{ true };
	bool scaleClamped{ false };
	bool shiftClamped{ false };
};

// Empty text means "not set" and is valid. Rejects junk, NaN and infinities.
inline bool ParseFloat(std::wstring_view text, float& value)
{
	if (text.empty() || text.size() > 32) {
		return false;
	}
	std::wstring terminated(text);
	wchar_t* end = nullptr;
	errno = 0;
	const double parsed = std::wcstod(terminated.c_str(), &end);
	if (errno == ERANGE || end == terminated.c_str() || *end != L'\0' || !std::isfinite(parsed) ||
		parsed < -1000000.0 || parsed > 1000000.0) {
		return false;
	}
	value = static_cast<float>(parsed);
	return true;
}

inline ConfigParse ParseConfig(std::wstring_view manage, std::wstring_view scale, std::wstring_view shift)
{
	ConfigParse result{};
	if (!manage.empty()) {
		result.manageValid = manage == L"0" || manage == L"1";
		if (result.manageValid) {
			result.config.manage = manage == L"1";
		}
	}
	float parsed = 0.0f;
	if (!scale.empty()) {
		result.scaleValid = ParseFloat(scale, parsed);
		if (result.scaleValid) {
			const float bounded = std::clamp(parsed, kMinScaleFactor, kMaxScaleFactor);
			result.scaleClamped = bounded != parsed;
			result.config.scaleFactor = bounded;
		}
	}
	if (!shift.empty()) {
		result.shiftValid = ParseFloat(shift, parsed);
		if (result.shiftValid) {
			const float bounded = std::clamp(parsed, kMinXShift, kMaxXShift);
			result.shiftClamped = bounded != parsed;
			result.config.xShift = bounded;
		}
	}
	return result;
}

// Where the model goes, in stage units: the card's centre and the half-width
// of its column (centre to the screen edge less 16).
struct Target
{
	double centre{};
	double half{};
};

// Fallback when the menu has not reported its card column: the centre of the
// right third (5/6 of the visible width), which matches the measured card.
inline Target DefaultTarget(double aspect)
{
	if (!std::isfinite(aspect) || aspect <= 0.0) {
		aspect = kStageWidth / kStageHeight;
	}
	const double width = std::max(kStageWidth, kStageHeight * aspect);
	const double x = kStageCenterX - width / 2.0;
	const double centre = x + width * 5.0 / 6.0;
	return { centre, x + width - kEdgeMargin - centre };
}

// A reported target must lie right of the stage centre and fit a sane column.
inline bool ValidTarget(const Target& target)
{
	return std::isfinite(target.centre) && std::isfinite(target.half) && target.centre > kStageCenterX &&
	       target.centre < 4000.0 && target.half >= 40.0 && target.half <= 1000.0;
}

struct Placement
{
	float x{};
	float y{};
	float z{};
	float scale{};

	bool operator==(const Placement&) const = default;
};

inline bool Finite(const Placement& p)
{
	return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && std::isfinite(p.scale);
}

// Centre over the card and shrink to fit; y/z (depth and vertical) are kept.
inline Placement Placed(const Placement& base, const Target& target, const Config& config)
{
	Placement result = base;
	if (config.xShift) {
		result.x = base.x + *config.xShift;
	} else {
		result.x = static_cast<float>(-(target.centre - kStageCenterX) / kStageUnitsPerPosition);
	}
	float factor = kMaxScaleFactor;
	if (config.scaleFactor) {
		factor = *config.scaleFactor;
	} else if (std::isfinite(base.scale) && base.scale > 0.0f) {
		const double fit = kFitFraction * 2.0 * target.half / (kWorstWidthPerScale * static_cast<double>(base.scale));
		factor = std::clamp(static_cast<float>(fit), kMinScaleFactor, kMaxScaleFactor);
	}
	result.scale = base.scale * factor;
	return result;
}

struct EngineState
{
	Placement live{};  // itemPos / itemScale
	Placement copy{};  // itemPosCopy / itemScaleCopy (un-zoomed placement)
	float zoomProgress{};
	bool startedZoom{};
};

enum class Outcome
{
	Written,
	Unchanged,
	ZoomActive,
	Disabled,
	Invalid
};

struct Result
{
	Outcome outcome{ Outcome::Unchanged };
	Placement base{};
	Placement written{};
	bool newBase{ false };
};

// One player-inventory open. Idempotent: values equal to what we last wrote are
// ours; any other live value is a new engine base (Begin3D or a reset), so our
// own output is never shifted twice. Never writes while a zoom is in progress.
class Session
{
public:
	void Begin()
	{
		_hasWritten = false;
		_open = true;
	}
	void End()
	{
		_hasWritten = false;
		_open = false;
	}
	[[nodiscard]] bool Open() const { return _open; }

	Result Apply(const EngineState& state, const Target& target, const Config& config)
	{
		Result result{};
		if (!config.manage) {
			result.outcome = Outcome::Disabled;
			return result;
		}
		if (!_open || !Finite(state.live) || !Finite(state.copy) || !ValidTarget(target)) {
			result.outcome = Outcome::Invalid;
			return result;
		}
		if (state.zoomProgress != 0.0f || state.startedZoom) {
			result.outcome = Outcome::ZoomActive;
			return result;
		}
		if (!_hasWritten || !(state.live == _written)) {
			_base = state.live;
			result.newBase = true;
		}
		const Placement target_placement = Placed(_base, target, config);
		result.base = _base;
		result.written = target_placement;
		if (_hasWritten && state.live == target_placement && state.copy == target_placement) {
			result.outcome = Outcome::Unchanged;
			return result;
		}
		_written = target_placement;
		_hasWritten = true;
		result.outcome = Outcome::Written;
		return result;
	}

private:
	Placement _base{};
	Placement _written{};
	bool _hasWritten{ false };
	bool _open{ false };
};

}  // namespace skycrpg::ui::inventory::preview
