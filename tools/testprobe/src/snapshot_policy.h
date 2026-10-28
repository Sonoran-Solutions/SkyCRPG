// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string_view>

namespace skycrpg::testprobe {
inline constexpr bool IsMainThread(std::uint32_t current, std::uint32_t captured) noexcept
{
	return captured != 0 && current == captured;
}

// Check thread affinity before touching engine objects. A stale request must
// never read the replacement world, even after that world finishes loading.
inline constexpr std::string_view SnapshotReason(bool onMainThread,
	std::uint64_t queuedGeneration, std::uint64_t currentGeneration, bool loaded) noexcept
{
	if (!onMainThread) return "off_main_thread";
	if (queuedGeneration != currentGeneration) return "generation_changed";
	if (!loaded) return "not_loaded";
	return {};
}
}  // namespace skycrpg::testprobe
