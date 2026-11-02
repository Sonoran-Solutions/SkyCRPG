// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/magic_policy.h"

#include <cassert>
#include <limits>

int main()
{
	using skycrpg::testprobe::MagicFavorite;
	assert(MagicFavorite(0) == false);
	assert(MagicFavorite(1) == true);
	assert(MagicFavorite(0b101) == true);
	assert(MagicFavorite(2) == false);
	assert(!MagicFavorite(std::nullopt));
	assert(!MagicFavorite(std::numeric_limits<double>::quiet_NaN()));
	assert(!MagicFavorite(std::numeric_limits<double>::infinity()));
	assert(!MagicFavorite(-1));
	assert(!MagicFavorite(1.5));
	assert(!MagicFavorite(4294967296.0));
	assert(MagicFavorite(4294967295.0) == true);
}
