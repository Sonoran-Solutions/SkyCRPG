// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <cmath>
#include <algorithm>
#include <optional>
namespace skycrpg::testprobe {
inline bool CursorNumber(bool numeric, double value) { return numeric && std::isfinite(value); }
struct CellBox { double x_min, x_max, y_min, y_max; };
inline std::optional<CellBox> SelectedCell(bool grid, bool clip, bool valid, double x0, double y0, double x1, double y1)
{
 if (!grid || !clip || !valid || !std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) || !std::isfinite(y1)) return std::nullopt;
 return CellBox{std::min(x0,x1), std::max(x0,x1), std::min(y0,y1), std::max(y0,y1)};
}
inline bool CursorHitKnown(bool coordinates, bool clip, bool resultIsBoolean)
{
	return coordinates && clip && resultIsBoolean;
}
}
