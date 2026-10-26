// SkyCRPG UI — validated, read-only HUD settings bridge.
#include <skycrpg/skeleton.h>
#include "hud_settings.h"

#include <SKSE/API.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#include "address_library_pin.h"

#include <array>
#include <string>
#include <vector>

#include <bcrypt.h>

namespace {
constexpr skycrpg::ModuleInfo kModule{ "SkyCRPG UI", L"SkyCRPG_UI", SKYCRPG_VERSION, SKYCRPG_SOURCE_COMMIT };
skycrpg::ui::HudSettings g_hudSettings{};

enum class AddressLibraryStatus
{
	Valid,
	Missing,
	FileError,
	CryptoError,
	HashMismatch
};

struct AddressLibraryResult
{
	AddressLibraryStatus status{ AddressLibraryStatus::FileError };
	DWORD systemError{};
	std::string sha256;
};

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

AddressLibraryResult VerifyPinnedAddressLibrary()
{
	const auto path = skycrpg::ThisModuleDirectory() + skycrpg::ui::kPinnedAddressLibraryFile;
	const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		const DWORD error = GetLastError();
		return { error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? AddressLibraryStatus::Missing : AddressLibraryStatus::FileError,
			error, {} };
	}

	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	DWORD objectBytes = 0;
	DWORD resultBytes = 0;
	std::vector<UCHAR> hashObject;
	std::array<UCHAR, 32> digest{};
	std::array<UCHAR, 65536> buffer{};
	AddressLibraryStatus status = AddressLibraryStatus::CryptoError;
	DWORD systemError = ERROR_SUCCESS;
	bool readFailed = false;
	bool hashFailed = false;
	std::string actual;

	if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
		BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &resultBytes, 0) >= 0 &&
		objectBytes > 0) {
		hashObject.resize(objectBytes);
		if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectBytes, nullptr, 0, 0) >= 0) {
			for (;;) {
				DWORD bytesRead = 0;
				if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr)) {
					readFailed = true;
					systemError = GetLastError();
					break;
				}
				if (bytesRead == 0) {
					break;
				}
				if (BCryptHashData(hash, buffer.data(), bytesRead, 0) < 0) {
					hashFailed = true;
					break;
				}
			}
			if (readFailed) {
				status = AddressLibraryStatus::FileError;
			} else if (!hashFailed && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0) {
				constexpr char hex[] = "0123456789abcdef";
				actual.reserve(digest.size() * 2);
				for (const UCHAR byte : digest) {
					actual.push_back(hex[byte >> 4]);
					actual.push_back(hex[byte & 0x0F]);
				}
				status = skycrpg::ui::IsPinnedAddressLibrarySha256(actual) ? AddressLibraryStatus::Valid : AddressLibraryStatus::HashMismatch;
			}
		}
	}
	if (hash) BCryptDestroyHash(hash);
	if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
	CloseHandle(file);
	return { status, systemError, actual };
}

void AppendModuleLog(const char* message)
{
	const auto logPath = skycrpg::LogDirectory() + kModule.fileStem + L".log";
	FILE* log = nullptr;
	if (_wfopen_s(&log, logPath.c_str(), L"a") == 0 && log) {
		std::fputs(message, log);
		std::fclose(log);
	}
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
	const auto addressLibrary = VerifyPinnedAddressLibrary();
	if (addressLibrary.status != AddressLibraryStatus::Valid) {
		std::array<char, 256> message{};
		switch (addressLibrary.status) {
		case AddressLibraryStatus::Missing:
			std::snprintf(message.data(), message.size(), "Address Library guard: FAIL missing %ls; expected v13 / 1.7.104 exact SHA-256 %.*s; CommonLib initialization skipped\n",
				skycrpg::ui::kPinnedAddressLibraryFile, static_cast<int>(skycrpg::ui::kPinnedAddressLibrarySha256.size()), skycrpg::ui::kPinnedAddressLibrarySha256.data());
			break;
		case AddressLibraryStatus::FileError:
			std::snprintf(message.data(), message.size(), "Address Library guard: FAIL file I/O for %ls (Win32 error %lu); CommonLib initialization skipped\n",
				skycrpg::ui::kPinnedAddressLibraryFile, addressLibrary.systemError);
			break;
		case AddressLibraryStatus::CryptoError:
			std::snprintf(message.data(), message.size(), "Address Library guard: FAIL SHA-256 verification unavailable for %ls; CommonLib initialization skipped\n",
				skycrpg::ui::kPinnedAddressLibraryFile);
			break;
		case AddressLibraryStatus::HashMismatch:
			std::snprintf(message.data(), message.size(), "Address Library guard: FAIL %ls has SHA-256 %s; expected v13 / 1.7.104 %.*s; CommonLib initialization skipped\n",
				skycrpg::ui::kPinnedAddressLibraryFile, addressLibrary.sha256.c_str(),
				static_cast<int>(skycrpg::ui::kPinnedAddressLibrarySha256.size()), skycrpg::ui::kPinnedAddressLibrarySha256.data());
			break;
		case AddressLibraryStatus::Valid:
			break;
		}
		AppendModuleLog(message.data());
		return false;
	}
	std::array<char, 256> addressLog{};
	std::snprintf(addressLog.data(), addressLog.size(), "Address Library guard: PASS %ls (v13 / 1.7.104, SHA-256 %s)\n",
		skycrpg::ui::kPinnedAddressLibraryFile, addressLibrary.sha256.c_str());
	AppendModuleLog(addressLog.data());
	const auto iniPath = skycrpg::ThisModuleDirectory() + kModule.fileStem + L".ini";
	const auto parsed = ReadHudSettings(iniPath);
	g_hudSettings = parsed.settings;
	LogHudSettings(iniPath, parsed);

	// CommonLib is used only for the pinned SKSE Scaleform API/GFx wrappers.
	// The shared load ABI stays untouched; no game memory or camera state is read.
	SKSE::Init(reinterpret_cast<const SKSE::LoadInterface*>(skseInterface), false);
	const auto* scaleform = SKSE::GetScaleformInterface();
	if (!scaleform || !scaleform->Register(&RegisterSkyCRPGUI, "SkyCRPGUI")) {
		AppendModuleLog("Scaleform settings registration: FAIL; native HUD fallback retained\n");
		return true;
	}
	AppendModuleLog("scope extension: read-only validated HUD settings exposed to Scaleform; no gameplay state reads or writes\n");
	AppendModuleLog("Scaleform settings registration: PASS read-only\n");
	return true;
}

}
