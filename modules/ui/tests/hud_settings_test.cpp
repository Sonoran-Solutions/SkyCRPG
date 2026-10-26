#include "../src/hud_settings.h"
#include "../src/address_library_pin.h"
#include "../src/skse_version_pin.h"

#include <cassert>

int main()
{
	using skycrpg::ui::IsPinnedAddressLibrarySha256;
	assert(std::wstring_view(skycrpg::ui::kPinnedAddressLibraryFile) == L"versionlib-1-7-104-0.bin");
	assert(IsPinnedAddressLibrarySha256("8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842"));
	assert(!IsPinnedAddressLibrarySha256("be0c7c07fb63fd1c1403690d4797c7a8fdf8799118acceb605f285d7b0ee055f"));
	assert(!IsPinnedAddressLibrarySha256(""));
	assert(skycrpg::ui::IsPinnedSkseVersion(0x02030010U));
	assert(!skycrpg::ui::IsPinnedSkseVersion(0x02030000U));
	assert(!skycrpg::ui::IsPinnedSkseVersion(0));

	using skycrpg::ui::ParseHudSettings;
	auto defaults = ParseHudSettings(L"1", L"", L"", L"", L"");
	assert(defaults.settings.enabled);
	assert(defaults.settings.scalePercent == 100);
	assert(defaults.settings.offsetX == 0 && defaults.settings.offsetY == 0);
	assert(!defaults.enabledWasValid && !defaults.scaleWasValid);

	auto bounded = ParseHudSettings(L"1", L"0", L"200", L"-400", L"80");
	assert(!bounded.settings.enabled);
	assert(bounded.settings.scalePercent == 115 && bounded.scaleWasClamped);
	assert(bounded.settings.offsetX == -120 && bounded.offsetXWasClamped);
	assert(bounded.settings.offsetY == 60 && bounded.offsetYWasClamped);

	auto malformed = ParseHudSettings(L"1", L"2", L"1.15", L"west", L"9999999999999999999");
	assert(malformed.settings.enabled && !malformed.enabledWasValid);
	assert(malformed.settings.scalePercent == 100 && !malformed.scaleWasValid);
	assert(malformed.settings.offsetX == 0 && !malformed.offsetXWasValid);
	assert(malformed.settings.offsetY == 0 && !malformed.offsetYWasValid);

	auto wrongVersion = ParseHudSettings(L"2", L"0", L"85", L"120", L"60");
	assert(!wrongVersion.supportedVersion);
	assert(wrongVersion.settings.enabled && wrongVersion.settings.scalePercent == 100);
	assert(wrongVersion.settings.offsetX == 0 && wrongVersion.settings.offsetY == 0);

	auto disableHud = ParseHudSettings(L"1", L"0", L"85", L"120", L"-60");
	assert(disableHud.settings.enabled == false);
	assert(disableHud.settings.scalePercent == 85);
	assert(disableHud.settings.offsetX == 120 && disableHud.settings.offsetY == -60);
}
