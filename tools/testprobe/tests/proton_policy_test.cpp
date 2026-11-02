// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/proton_policy.h"
#include <cassert>
#include <string>
using namespace skycrpg::testprobe;
int main()
{
	constexpr std::uint32_t invalid = 0xFFFFFFFF;
	using Keys = std::array<std::uint32_t, 3>;
	assert(AcceptInventoryKey(true, false, true, 23, Keys{ 23, invalid, invalid }, invalid));
	// Live 2026-11-02 keyboard map: Inventory unbound, I = Quick Inventory, TAB = Tween Menu.
	assert(AcceptInventoryKey(true, false, true, 23, Keys{ invalid, 23, 15 }, invalid));
	assert(AcceptInventoryKey(true, false, true, 15, Keys{ invalid, 23, 15 }, invalid));
	assert(!AcceptInventoryKey(true, false, true, 24, Keys{ invalid, 23, 15 }, invalid));
	assert(!AcceptInventoryKey(false, false, true, 23, Keys{ 23, invalid, invalid }, invalid));
	assert(!AcceptInventoryKey(true, true, true, 23, Keys{ 23, invalid, invalid }, invalid));
	assert(!AcceptInventoryKey(true, false, false, 23, Keys{ 23, invalid, invalid }, invalid));
	assert(!AcceptInventoryKey(true, false, true, invalid, Keys{ invalid, invalid, invalid }, invalid));
	InputDiagnostics diagnostics;
	InputDiagnostic d;
	d.down = d.accepted = true;
	for (int i = 0; i < 100; ++i) diagnostics.observe(d, std::string(80, 'x'));
	assert(diagnostics.buttons == 100 && diagnostics.downs == 100 && diagnostics.accepted == 100);
	assert(diagnostics.count == 32 && diagnostics.records[0].userEventTruncated);
	assert(diagnostics.records[0].userEvent.back() == '\0');
	diagnostics = {};
	assert(!diagnostics.count && !diagnostics.buttons);
	auto noDeadline = [] { return false; };
	auto query = [](std::uint64_t address, MemoryRegion& region) {
		region = {address, 4096, address == 4096};
		return true;
	};
	const auto walk = WalkPrivateCommit(12287, query, noDeadline);
	assert(walk.complete && walk.bytes == 4096 && walk.regions == 3 && walk.privateRegions == 1);
	assert(!WalkPrivateCommit(12287, query, [] { return true; }).complete);
	assert(!WalkPrivateCommit(12287, [](std::uint64_t, MemoryRegion&) { return false; }, noDeadline).complete);
	assert(!WalkPrivateCommit(12287, [](std::uint64_t, MemoryRegion& r) { r = {}; return true; }, noDeadline).complete);
	// Nonprogress, overflow, and absurd address-space fragmentation fail closed.
	assert(!WalkPrivateCommit(12287, [](std::uint64_t, MemoryRegion& r) { r = {0, 1, false}; return true; }, noDeadline).complete);
	assert(!WalkPrivateCommit(12287, [](std::uint64_t, MemoryRegion& r) { r = {1, UINT64_MAX, true}; return true; }, noDeadline).complete);
	const auto limited = WalkPrivateCommit(UINT64_MAX, [](std::uint64_t a, MemoryRegion& r) { r = {a, 1, false}; return true; }, noDeadline);
	assert(!limited.complete && limited.regions == 1000000);
	const auto end = WalkPrivateCommit(UINT64_MAX, [](std::uint64_t a, MemoryRegion& r) { r = {a, a ? 1 : UINT64_MAX, false}; return true; }, noDeadline);
	assert(end.complete && end.regions == 2);
}
