// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Portable policy for the player-inventory 3D item preview placement (DYL-168).
// No engine types: the glue in item_preview_placement.cpp reads and writes the
// in-memory `fInventory3DItemPos*:Interface` settings; nothing is persisted.

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cwchar>
#include <optional>
#include <string>
#include <string_view>

namespace skycrpg::ui::inventory::preview {

enum Slot : std::size_t { kPosX, kPosXWide, kPosZ, kPosZWide, kScale, kScaleWide, kSlotCount };

// Only the inventory values. fMagic3DItemPos* is never touched.
inline constexpr std::array<const char*, kSlotCount> kSettingNames{
	"fInventory3DItemPosX:Interface", "fInventory3DItemPosXWide:Interface",
	"fInventory3DItemPosZ:Interface", "fInventory3DItemPosZWide:Interface",
	"fInventory3DItemPosScale:Interface", "fInventory3DItemPosScaleWide:Interface"
};

using Values = std::array<float, kSlotCount>;
using Writes = std::array<std::optional<float>, kSlotCount>;

// Geometry shared with inventory-presentation.as.inc (`skycrpgModelColumn`).
// SkyUI SKI_ConfigMenu.ApplySettings: stage 1280x720, centre x 640, 1 position
// unit = 8 stage units, negative X moves the model right; its 16:9 / 21:9 / 32:9
// targets are 931.225 / 858 / 785.5. The owner's 1913x1080 captures put the
// model centre at x ~1395 px = stage ~933, i.e. SkyUI's placement was active.
inline constexpr double kStageWidth = 1280.0;
inline constexpr double kStageHeight = 720.0;
inline constexpr double kStageCenterX = 640.0;
inline constexpr double kStageUnitsPerPosition = 8.0;
inline constexpr double kModelColumnFraction = 0.66;
inline constexpr double kEdgeMargin = 16.0;
// Worst observed model: the Elder Scroll, 760 px wide at 1.4945 px/unit and
// SkyUI's default scale 1.5 = 508.5 stage units, i.e. 339 units per scale unit.
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

inline double SkyUiTargetX(double aspect)
{
	if (aspect > 3.0) {
		return 785.5;
	}
	if (aspect > 2.0) {
		return 858.0;
	}
	return 931.225;
}

struct Column
{
	double left{};
	double right{};
};

// Stage-space model column for a display aspect. Narrower than 16:9 keeps the
// 1280-wide stage; wider displays reveal extra stage width around x 640.
inline Column ModelColumn(double aspect)
{
	if (!std::isfinite(aspect) || aspect <= 0.0) {
		aspect = kStageWidth / kStageHeight;
	}
	const double width = std::max(kStageWidth, kStageHeight * aspect);
	const double x = kStageCenterX - width / 2.0;
	return { x + kModelColumnFraction * width, x + width - kEdgeMargin };
}

inline float ComputeXShift(double aspect, const Config& config)
{
	if (config.xShift) {
		return *config.xShift;
	}
	const Column column = ModelColumn(aspect);
	const double centre = (column.left + column.right) / 2.0;
	const double shift = -(centre - SkyUiTargetX(aspect)) / kStageUnitsPerPosition;
	return std::clamp(static_cast<float>(shift), kMinXShift, kMaxXShift);
}

inline float ComputeScaleFactor(double aspect, float baseScale, const Config& config)
{
	if (config.scaleFactor) {
		return *config.scaleFactor;
	}
	if (!std::isfinite(baseScale) || baseScale <= 0.0f) {
		return kMaxScaleFactor;
	}
	const Column column = ModelColumn(aspect);
	const double fit = kFitFraction * (column.right - column.left) / (kWorstWidthPerScale * static_cast<double>(baseScale));
	return std::clamp(static_cast<float>(fit), kMinScaleFactor, kMaxScaleFactor);
}

// Shift right and scale down from the base values; the vertical position is kept.
inline Values Placed(const Values& base, double aspect, const Config& config)
{
	Values result = base;
	const float shift = ComputeXShift(aspect, config);
	result[kPosX] = base[kPosX] + shift;
	result[kPosXWide] = base[kPosXWide] + shift;
	result[kScale] = base[kScale] * ComputeScaleFactor(aspect, base[kScale], config);
	result[kScaleWide] = base[kScaleWide] * ComputeScaleFactor(aspect, base[kScaleWide], config);
	return result;
}

// Restore state machine. The base is captured on the first open and never
// re-captured while applied, so our own values are never mistaken for a base.
class PlacementState
{
public:
	// `current` is empty when any setting is missing or not a float.
	Writes Open(const std::optional<Values>& current, double aspect, const Config& config)
	{
		Writes writes{};
		if (!config.manage) {
			return Restore(current);
		}
		if (!current) {
			_missing = true;
			return writes;
		}
		if (!_applied) {
			_captured = *current;
			_written = Placed(_captured, aspect, config);
			_applied = true;
		}
		for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
			if ((*current)[slot] != _written[slot]) {
				writes[slot] = _written[slot];
			}
		}
		return writes;
	}

	// Close, load, new game, or another item menu. A value changed by someone
	// else since we wrote it (e.g. an MCM) is left alone.
	Writes Restore(const std::optional<Values>& current)
	{
		Writes writes{};
		if (!_applied) {
			return writes;
		}
		_applied = false;
		if (!current) {
			return writes;
		}
		for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
			if ((*current)[slot] == _written[slot] && _captured[slot] != _written[slot]) {
				writes[slot] = _captured[slot];
			}
		}
		return writes;
	}

	[[nodiscard]] bool Applied() const { return _applied; }

	// True exactly once after a missing/wrong-typed setting was seen.
	bool TakeMissingLog()
	{
		if (!_missing || _missingLogged) {
			return false;
		}
		_missingLogged = true;
		return true;
	}

private:
	Values _captured{};
	Values _written{};
	bool _applied{ false };
	bool _missing{ false };
	bool _missingLogged{ false };
};

}  // namespace skycrpg::ui::inventory::preview
