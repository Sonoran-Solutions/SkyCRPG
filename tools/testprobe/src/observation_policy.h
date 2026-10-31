// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace skycrpg::testprobe {
inline constexpr unsigned kObservationProtocol = 2;
struct StateRequest {
	bool valid{ true };
	std::string tag;
	std::array<std::uint32_t, 16> forms{};
	std::size_t count{};
};
inline StateRequest ParseStateRequest(std::string_view text)
{
	StateRequest result;
	if (!text.starts_with("@state ")) { result.tag = text; return result; }
	text.remove_prefix(7);
	const auto space = text.find(' ');
	result.tag = text.substr(0, space);
	if (space == std::string_view::npos || result.tag.empty() || result.tag.size() > 40) {
		result.valid = false; return result;
	}
	for (const char c : result.tag) {
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
			c == '_' || c == '.' || c == '-')) { result.valid = false; return result; }
	}
	text.remove_prefix(space + 1);
	while (!text.empty()) {
		const auto comma = text.find(',');
		const auto token = text.substr(0, comma);
		std::uint32_t form{};
		if (result.count == result.forms.size() || token.size() != 10 || !token.starts_with("0x")) {
			result.valid = false; return result;
		}
		const auto parsed = std::from_chars(token.data() + 2, token.data() + token.size(), form, 16);
		if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() || form == 0) {
			result.valid = false; return result;
		}
		for (std::size_t i = 0; i < result.count; ++i) {
			if (result.forms[i] == form) { result.valid = false; return result; }
		}
		result.forms[result.count++] = form;
		if (comma == std::string_view::npos) break;
		text.remove_prefix(comma + 1);
		if (text.empty()) { result.valid = false; return result; }
	}
	result.valid = result.count > 0;
	return result;
}
// Numeric values verified in CommonLib 7.2.0 TESObjectWEAP::WEAPON_TYPE.
inline constexpr bool TwoHandedWeapon(unsigned type) noexcept
{
	return type == 5 || type == 6 || type == 7 || type == 9;
}
}  // namespace skycrpg::testprobe
