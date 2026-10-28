// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string_view>

namespace skycrpg::testprobe {
// SKSE drains UI tasks on rotating engine worker threads (observed ids 424-444
// on 1.7.104 under Proton), never on Main::threadID or the kDataLoaded thread.
// The drain itself is the synchronisation point, so engine reads are allowed
// only from a probe task while it runs inside that drain. A stale request must
// never read the replacement world, even after that world finishes loading.
inline constexpr std::string_view SnapshotReason(bool inTaskDrain,
	std::uint64_t queuedGeneration, std::uint64_t currentGeneration, bool loaded) noexcept
{
	if (!inTaskDrain) return "outside_task_drain";
	if (queuedGeneration != currentGeneration) return "generation_changed";
	if (!loaded) return "not_loaded";
	return {};
}
}  // namespace skycrpg::testprobe
