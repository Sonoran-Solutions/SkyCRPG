#pragma once

#include <algorithm>
#include <cerrno>
#include <cwchar>
#include <string>
#include <string_view>

namespace skycrpg::ui {

struct HudSettings
{
	bool enabled{ true };
	bool replaceCompass{ false };
	int scalePercent{ 100 };
	int offsetX{ 0 };
	int offsetY{ 0 };
};

struct ParseResult
{
	HudSettings settings{};
	bool supportedVersion{ true };
	bool enabledWasValid{ true };
	bool replaceCompassWasValid{ true };
	bool scaleWasValid{ true };
	bool offsetXWasValid{ true };
	bool offsetYWasValid{ true };
	bool scaleWasClamped{ false };
	bool offsetXWasClamped{ false };
	bool offsetYWasClamped{ false };
	bool resetLayoutWasValid{ true };
	bool resetLayoutApplied{ false };
};

inline bool ParseInteger(std::wstring_view text, int& value)
{
	if (text.empty() || text.size() > 16) {
		return false;
	}
	std::wstring terminated(text);
	wchar_t* end = nullptr;
	errno = 0;
	const auto parsed = std::wcstol(terminated.c_str(), &end, 10);
	if (errno == ERANGE || end == terminated.c_str() || *end != L'\0' || parsed < -1000000 || parsed > 1000000) {
		return false;
	}
	value = static_cast<int>(parsed);
	return true;
}

inline ParseResult ParseHudSettings(std::wstring_view version, std::wstring_view enabled,
	std::wstring_view scale, std::wstring_view offsetX, std::wstring_view offsetY,
	std::wstring_view replaceCompass = L"", std::wstring_view resetLayout = L"0")
{
	ParseResult result{};
	int parsedVersion = 0;
	result.supportedVersion = ParseInteger(version, parsedVersion) && parsedVersion == 1;
	if (!result.supportedVersion) {
		return result;  // Version mismatch safely resets the whole layout to defaults.
	}

	int parsed = 0;
	result.enabledWasValid = ParseInteger(enabled, parsed) && (parsed == 0 || parsed == 1);
	if (result.enabledWasValid) {
		result.settings.enabled = parsed == 1;
	}

	result.replaceCompassWasValid = ParseInteger(replaceCompass, parsed) && (parsed == 0 || parsed == 1);
	if (result.replaceCompassWasValid) {
		result.settings.replaceCompass = parsed == 1;
	}

	result.scaleWasValid = ParseInteger(scale, parsed);
	if (result.scaleWasValid) {
		const int bounded = std::clamp(parsed, 85, 115);
		result.scaleWasClamped = bounded != parsed;
		result.settings.scalePercent = bounded;
	}

	result.offsetXWasValid = ParseInteger(offsetX, parsed);
	if (result.offsetXWasValid) {
		const int bounded = std::clamp(parsed, -120, 120);
		result.offsetXWasClamped = bounded != parsed;
		result.settings.offsetX = bounded;
	}

	result.offsetYWasValid = ParseInteger(offsetY, parsed);
	if (result.offsetYWasValid) {
		const int bounded = std::clamp(parsed, -60, 60);
		result.offsetYWasClamped = bounded != parsed;
		result.settings.offsetY = bounded;
	}
	result.resetLayoutWasValid = ParseInteger(resetLayout, parsed) && (parsed == 0 || parsed == 1);
	result.resetLayoutApplied = result.resetLayoutWasValid && parsed == 1;
	if (result.resetLayoutApplied) {
		// Startup override only: preserve enable/provider policy and the INI.
		result.settings.scalePercent = 100;
		result.settings.offsetX = 0;
		result.settings.offsetY = 0;
	}
	return result;
}

}  // namespace skycrpg::ui
