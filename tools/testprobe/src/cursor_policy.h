// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <cmath>
namespace skycrpg::testprobe {
inline bool CursorNumber(bool numeric, double value) { return numeric && std::isfinite(value); }
inline bool CursorHitKnown(bool coordinates, bool clip, bool resultIsBoolean)
{
	return coordinates && clip && resultIsBoolean;
}
}
