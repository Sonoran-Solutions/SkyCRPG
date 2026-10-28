#include "../src/hud_settings.h"

#include <cassert>

int main()
{
	using skycrpg::ui::ParseHudSettings;
	for (const auto* scale : { L"85", L"115", L"malformed" }) {
		const auto reset = ParseHudSettings(L"1", L"0", scale, L"120", L"-60", L"1", L"1");
		assert(reset.resetLayoutWasValid && reset.resetLayoutApplied);
		assert(reset.settings.scalePercent == 100);
		assert(reset.settings.offsetX == 0 && reset.settings.offsetY == 0);
		assert(!reset.settings.enabled && reset.settings.replaceCompass);
	}
	for (const auto* flag : { L"0", L"", L"2", L"-1", L"yes", L"99999999999999999999" }) {
		const auto result = ParseHudSettings(L"1", L"1", L"115", L"120", L"-60", L"0", flag);
		assert(!result.resetLayoutApplied);
		assert(result.settings.scalePercent == 115 && result.settings.offsetX == 120 && result.settings.offsetY == -60);
		assert(result.resetLayoutWasValid == (std::wstring_view(flag) == L"0"));
	}
	const auto future = ParseHudSettings(L"2", L"0", L"115", L"120", L"-60", L"1", L"1");
	assert(!future.supportedVersion && !future.resetLayoutApplied);
	assert(future.settings.scalePercent == 100 && !future.settings.replaceCompass);
	const auto defaults = ParseHudSettings(L"1", L"1", L"85", L"-120", L"60");
	assert(defaults.resetLayoutWasValid && !defaults.resetLayoutApplied);
	assert(defaults.settings.scalePercent == 85 && defaults.settings.offsetX == -120);
}
