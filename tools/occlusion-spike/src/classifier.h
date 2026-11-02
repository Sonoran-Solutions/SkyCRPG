// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <cmath>
#include <cstdint>

namespace skycrpg::occlusion {
// A contact has no thickness or room-membership field. This is a hypothesis
// label for manual false-positive counts, never permission to discard a hit.
struct Candidate {
	float normalZ{};
	std::uint32_t layer{};
	bool resolvedStatic{};
};
inline bool OverheadCandidate(const Candidate& c)
{
	return c.resolvedStatic && c.layer == 1 && std::isfinite(c.normalZ) && c.normalZ < -0.5f;
}
inline bool Allowlisted(std::uint32_t ref, std::uint32_t base,
	const auto& refs, const auto& bases)
{
	for (auto id : refs) if (id && id == ref) return true;
	for (auto id : bases) if (id && id == base) return true;
	return false;
}
} // namespace skycrpg::occlusion
