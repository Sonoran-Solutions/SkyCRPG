// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

namespace skycrpg::testprobe {
// The probe is a never-shipped test component: a missing INI, a missing key or
// any value other than exactly "1" leaves it fully inert. The runner must opt in.
inline constexpr bool ProbeEnabled(std::string_view value) noexcept
{
	return value == "1";
}
}  // namespace skycrpg::testprobe
