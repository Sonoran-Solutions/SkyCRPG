// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace skycrpg::testprobe {
struct InputDiagnostic {
	std::int32_t device{};
	std::uint32_t idCode{}, inventoryKey{}, quickInventoryKey{}, tweenKey{};
	float value{}, heldDownSecs{};
	double qpcMs{};
	bool down{}, accepted{}, menuOpen{}, mapped{}, userEventTruncated{};
	std::array<char, 64> userEvent{};
};

struct InputDiagnostics {
	std::uint64_t buttons{}, downs{}, accepted{};
	std::size_t count{}, emitted{};
	std::array<InputDiagnostic, 32> records{};
	void observe(InputDiagnostic record, std::string_view userEvent) noexcept {
		++buttons;
		downs += record.down;
		accepted += record.accepted;
		if (count == records.size()) return;
		const auto length = userEvent.size() < record.userEvent.size() - 1 ? userEvent.size() : record.userEvent.size() - 1;
		for (std::size_t i = 0; i < length; ++i) record.userEvent[i] = userEvent[i];
		record.userEventTruncated = length != userEvent.size();
		records[count++] = record;
	}
};

// Keys are the gameplay mappings for Inventory, Quick Inventory and Tween Menu.
// The default keyboard map binds I to Quick Inventory and leaves Inventory unbound.
inline bool AcceptInventoryKey(bool active, bool menuOpen, bool down, std::uint32_t key,
	const std::array<std::uint32_t, 3>& keys, std::uint32_t invalid) noexcept
{
	if (!active || menuOpen || !down || key == invalid) return false;
	for (const auto mapped : keys)
		if (mapped != invalid && key == mapped) return true;
	return false;
}

struct MemoryRegion {
	std::uint64_t base{}, size{};
	bool privateCommit{};
};
struct CommitWalk {
	std::uint64_t bytes{}, regions{}, privateRegions{};
	bool complete{};
};

// Query and deadline are supplied by the timer adapter; failed/partial walks
// never become a usable byte count. Every query must advance the address.
template <class Query, class Expired>
CommitWalk WalkPrivateCommit(std::uint64_t maximum, Query query, Expired expired)
{
	CommitWalk result;
	std::uint64_t address = 0;
	while (address <= maximum) {
		if (result.regions == 1000000 || expired()) return result;
		MemoryRegion region;
		if (!query(address, region)) return result;
		++result.regions;
		if (!region.size || region.base > address || region.size - 1 > std::numeric_limits<std::uint64_t>::max() - region.base)
			return result;
		const auto last = region.base + region.size - 1;
		if (last < address) return result;
		if (region.privateCommit) {
			if (region.size > std::numeric_limits<std::uint64_t>::max() - result.bytes) return result;
			result.bytes += region.size;
			++result.privateRegions;
		}
		if (last >= maximum) { result.complete = true; return result; }
		address = last + 1;
	}
	return result;
}
}
