// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace skycrpg::testprobe {
// SkyUI 6.11 FILTERFLAG_MAGIC_FAVORITES (Inventory.as:49); MagicMenu.as updateBottomBar.
inline constexpr std::uint32_t kMagicFavoritesFlag = 1U;

inline std::optional<bool> MagicFavorite(std::optional<double> filterFlag)
{
	if (!filterFlag || !std::isfinite(*filterFlag) || *filterFlag < 0 ||
		*filterFlag > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
		std::trunc(*filterFlag) != *filterFlag) return std::nullopt;
	return (static_cast<std::uint32_t>(*filterFlag) & kMagicFavoritesFlag) != 0;
}
}  // namespace skycrpg::testprobe
