// SkyCRPG UI — validated, read-only HUD settings bridge.
#include <skycrpg/skeleton.h>
#include "hud_settings.h"

#include <SKSE/API.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>

#include <array>

namespace {
constexpr skycrpg::ModuleInfo kModule{ "SkyCRPG UI", L"SkyCRPG_UI", SKYCRPG_VERSION, SKYCRPG_SOURCE_COMMIT };
skycrpg::ui::HudSettings g_hudSettings{};

skycrpg::ui::ParseResult ReadHudSettings(const std::wstring& iniPath)
{
	std::array<wchar_t, 32> version{};
	std::array<wchar_t, 32> enabled{};
	std::array<wchar_t, 32> scale{};
	std::array<wchar_t, 32> offsetX{};
	std::array<wchar_t, 32> offsetY{};
	GetPrivateProfileStringW(L"Settings", L"iVersion", L"", version.data(), static_cast<DWORD>(version.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"HUD", L"bEnabled", L"", enabled.data(), static_cast<DWORD>(enabled.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"HUD", L"iScalePercent", L"", scale.data(), static_cast<DWORD>(scale.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"HUD", L"iOffsetX", L"", offsetX.data(), static_cast<DWORD>(offsetX.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"HUD", L"iOffsetY", L"", offsetY.data(), static_cast<DWORD>(offsetY.size()), iniPath.c_str());
	return skycrpg::ui::ParseHudSettings(version.data(), enabled.data(), scale.data(), offsetX.data(), offsetY.data());
}

void LogHudSettings(const std::wstring& iniPath, const skycrpg::ui::ParseResult& parsed)
{
	const auto logPath = skycrpg::LogDirectory() + kModule.fileStem + L".log";
	FILE* log = nullptr;
	if (_wfopen_s(&log, logPath.c_str(), L"a") != 0 || !log) {
		return;
	}
	const auto& settings = parsed.settings;
	std::fwprintf(log, L"HUD settings: %ls; version=%ls; enabled=%d (valid=%d); scale=%d%% (valid=%d clamped=%d); offset=(%d,%d) (valid=%d/%d clamped=%d/%d)\n",
		iniPath.c_str(), parsed.supportedVersion ? L"1" : L"unsupported/defaults", settings.enabled,
		parsed.enabledWasValid, settings.scalePercent, parsed.scaleWasValid, parsed.scaleWasClamped,
		settings.offsetX, settings.offsetY, parsed.offsetXWasValid, parsed.offsetYWasValid,
		parsed.offsetXWasClamped, parsed.offsetYWasClamped);
	std::fclose(log);
}

bool RegisterSkyCRPGUI(RE::GFxMovieView*, RE::GFxValue* root)
{
	if (!root) {
		return false;
	}
	RE::GFxValue value;
	value.SetBoolean(g_hudSettings.enabled);
	if (!root->SetMember("hudEnabled", value)) return false;
	value.SetNumber(g_hudSettings.scalePercent);
	if (!root->SetMember("hudScale", value)) return false;
	value.SetNumber(g_hudSettings.offsetX);
	if (!root->SetMember("hudOffsetX", value)) return false;
	value.SetNumber(g_hudSettings.offsetY);
	return root->SetMember("hudOffsetY", value);
}
}

extern "C" {

__declspec(dllexport) constinit skse::PluginVersionData SKSEPlugin_Version{
	.dataVersion = skse::PluginVersionData::kVersion,
	.pluginVersion = (SKYCRPG_VERSION_MAJOR << 24) | (SKYCRPG_VERSION_MINOR << 16) | (SKYCRPG_VERSION_PATCH << 4),
	.name = "SkyCRPG UI",
	.author = "SkyCRPG",
	.supportEmail = "",
	// This DLL uses SKSE Scaleform/GFx structures; it is runtime-pinned.
	.versionIndependenceEx = 0,
	.versionIndependence = 0,
	.compatibleVersions = { skse::kRuntime_1_7_104, 0 },
	.seVersionRequired = skse::kSKSE_2_3_1,
};

__declspec(dllexport) bool SKSEPlugin_Load(const skse::Interface* skseInterface)
{
	if (!skycrpg::LoadSkeleton(kModule, skseInterface)) {
		return false;
	}
	const auto iniPath = skycrpg::ThisModuleDirectory() + kModule.fileStem + L".ini";
	const auto parsed = ReadHudSettings(iniPath);
	g_hudSettings = parsed.settings;
	LogHudSettings(iniPath, parsed);

	// CommonLib is used only for the pinned SKSE Scaleform API/GFx wrappers.
	// The shared load ABI stays untouched; no game memory or camera state is read.
	SKSE::Init(reinterpret_cast<const SKSE::LoadInterface*>(skseInterface), false);
	const auto* scaleform = SKSE::GetScaleformInterface();
	if (!scaleform || !scaleform->Register(&RegisterSkyCRPGUI, "SkyCRPGUI")) {
		const auto logPath = skycrpg::LogDirectory() + kModule.fileStem + L".log";
		FILE* log = nullptr;
		if (_wfopen_s(&log, logPath.c_str(), L"a") == 0 && log) {
			std::fputs("Scaleform settings registration: FAIL; native HUD fallback retained\n", log);
			std::fclose(log);
		}
		return true;
	}
	const auto logPath = skycrpg::LogDirectory() + kModule.fileStem + L".log";
	FILE* log = nullptr;
	if (_wfopen_s(&log, logPath.c_str(), L"a") == 0 && log) {
		std::fputs("scope extension: read-only validated HUD settings exposed to Scaleform; no gameplay state reads or writes\n", log);
		std::fputs("Scaleform settings registration: PASS read-only\n", log);
		std::fclose(log);
	}
	return true;
}

}
