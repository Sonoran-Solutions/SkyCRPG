// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace skycrpg::testprobe {
inline std::string Quote(std::string_view value)
{
	constexpr char hex[] = "0123456789abcdef";
	std::string result{"\""};
	for (const char character : value) {
		const auto byte = static_cast<unsigned char>(character);
		switch (byte) {
		case '"': result += "\\\""; break;
		case '\\': result += "\\\\"; break;
		case '\b': result += "\\b"; break;
		case '\f': result += "\\f"; break;
		case '\n': result += "\\n"; break;
		case '\r': result += "\\r"; break;
		case '\t': result += "\\t"; break;
		default:
			if (byte < 0x20) {
				result += "\\u00";
				result += hex[byte >> 4];
				result += hex[byte & 0x0F];
			} else {
				result += character;  // Preserve UTF-8 bytes, without locale conversion.
			}
		}
	}
	result += '"';
	return result;
}

template <std::integral T> requires (!std::same_as<T, bool>)
inline std::string Number(T value)
{
	std::array<char, 32> buffer{};
	const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
	return converted.ec == std::errc{} ? std::string(buffer.data(), converted.ptr) : "null";
}

inline std::string Number(double value)
{
	if (!std::isfinite(value)) return "null";
	std::array<char, 64> buffer{};
	const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general);
	return converted.ec == std::errc{} ? std::string(buffer.data(), converted.ptr) : "null";
}

inline std::string FormID(std::uint32_t value)
{
	constexpr char hex[] = "0123456789ABCDEF";
	std::string result{"0x00000000"};
	for (unsigned index = 0; index < 8; ++index) {
		result[9 - index] = hex[value & 0x0F];
		value >>= 4;
	}
	return result;
}

// Raw accepts only JSON assembled by this header/the adapter, never file text.
class Object {
public:
	Object& Raw(std::string_view key, std::string_view json)
	{
		if (!fields_.empty()) fields_ += ',';
		fields_ += Quote(key);
		fields_ += ':';
		fields_ += json;
		return *this;
	}
	Object& String(std::string_view key, std::string_view value) { return Raw(key, Quote(value)); }
	Object& Boolean(std::string_view key, bool value) { return Raw(key, value ? "true" : "false"); }
	template <typename T> Object& Numeric(std::string_view key, T value) { return Raw(key, Number(value)); }
	Object& Null(std::string_view key) { return Raw(key, "null"); }
	std::string Json() const { return "{" + fields_ + "}"; }
	const std::string& Fields() const { return fields_; }
private:
	std::string fields_;
};

inline std::string EventLine(std::uint64_t milliseconds, std::string_view wall,
	std::string_view event, const Object& fields = {})
{
	Object envelope;
	envelope.Numeric("t", milliseconds).String("wall", wall).String("ev", event);
	std::string line = "{" + envelope.Fields();
	if (!fields.Fields().empty()) line += ',' + fields.Fields();
	return line + "}\n";
}

inline constexpr std::size_t kMaxRequestBytes = 65536;

// The file is a raw UTF-8 tag, not JSON or an INI. A touch/empty tag is valid.
// Preserve whitespace, newlines and embedded NULs; Quote escapes them later.
// Oversized requests stay on disk and are diagnosed, never silently truncated.
inline std::optional<std::string> ParseRequest(std::string_view contents)
{
	if (contents.size() > kMaxRequestBytes) return std::nullopt;
	return std::string(contents);
}

inline std::uint32_t ParseInterval(std::string_view text) noexcept
{
	std::uint32_t value{};
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
	if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value > 0x7FFFFFFFU) return 1000;
	return value;
}
}  // namespace skycrpg::testprobe
