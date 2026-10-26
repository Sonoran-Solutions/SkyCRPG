#pragma once

#include <cstdint>

namespace skycrpg::ui {

// SKSE 2.3.1's PluginAPI MakeVersion encoding, independently testable without
// importing Windows or CommonLib headers.
inline constexpr std::uint32_t kPinnedSkseVersion = 0x02030010U;

constexpr bool IsPinnedSkseVersion(std::uint32_t version) noexcept
{
	return version == kPinnedSkseVersion;
}

}  // namespace skycrpg::ui
