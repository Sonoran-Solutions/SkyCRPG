// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
namespace skycrpg::occlusion {
// An unexpected value belongs to the engine/another writer. Preserve it and
// report a conflict instead of claiming successful restoration.
template<class T>
bool RestoreValue(T& current, const T& original, const T& applied)
{
	if (current == applied) current = original;
	return current == original;
}
} // namespace skycrpg::occlusion
