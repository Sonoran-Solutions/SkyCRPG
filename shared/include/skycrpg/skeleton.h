// Build-time helper for the P1-01 load-only skeletons.
//
// Each module DLL compiles its own copy of this header. It is not a runtime
// service: neither DLL exports or imports anything from the other.
//
// The SKSE declarations are the subset of skse64/PluginAPI.h and
// skse64_common/skse_version.h (SKSE64 2.3.1) that a plugin needs to be
// listed and called. The plugin uses no game addresses or game structures,
// so neither CommonLib nor Address Library is required.
#pragma once

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

namespace skse {

constexpr std::uint32_t MakeVersion(std::uint32_t major, std::uint32_t minor, std::uint32_t build, std::uint32_t sub = 0)
{
	return ((major & 0xFF) << 24) | ((minor & 0xFF) << 16) | ((build & 0xFFF) << 4) | (sub & 0xF);
}

constexpr std::uint32_t kRuntime_1_7_104 = MakeVersion(1, 7, 104);  // 0x01070680
constexpr std::uint32_t kSKSE_2_3_1 = MakeVersion(2, 3, 1);         // 0x02030010

struct Interface
{
	std::uint32_t skseVersion;
	std::uint32_t runtimeVersion;
	std::uint32_t editorVersion;
	std::uint32_t isEditor;
	// Remaining members (QueryInterface, GetPluginHandle, ...) are not used.
};

struct PluginVersionData
{
	enum
	{
		kVersion = 1,
		kVersionIndependentEx_NoStructUse = 1 << 0,
	};

	std::uint32_t dataVersion;
	std::uint32_t pluginVersion;
	char name[256];
	char author[256];
	char supportEmail[252];
	std::uint32_t versionIndependenceEx;
	std::uint32_t versionIndependence;
	std::uint32_t compatibleVersions[16];
	std::uint32_t seVersionRequired;
};
static_assert(sizeof(PluginVersionData) == 0x350, "SKSEPluginVersionData layout changed");

}  // namespace skse

namespace skycrpg {

struct ModuleInfo
{
	const char* displayName;  // e.g. "SkyCRPG Camera"
	const wchar_t* fileStem;  // e.g. L"SkyCRPG_Camera"; names the .log and .ini
	const char* version;
	const char* sourceCommit;
};

inline std::wstring ThisModuleDirectory()
{
	HMODULE self = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&ThisModuleDirectory), &self);
	wchar_t path[MAX_PATH]{};
	const DWORD len = GetModuleFileNameW(self, path, MAX_PATH);
	std::wstring dir(path, len);
	return dir.substr(0, dir.find_last_of(L"\\/") + 1);
}

// <Documents>\My Games\Skyrim Special Edition\SKSE\, the directory SKSE uses for its own log.
inline std::wstring LogDirectory()
{
	PWSTR documents = nullptr;
	std::wstring dir;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents))) {
		dir = std::wstring(documents) + L"\\My Games\\Skyrim Special Edition\\SKSE\\";
		CreateDirectoryW((std::wstring(documents) + L"\\My Games").c_str(), nullptr);
		CreateDirectoryW((std::wstring(documents) + L"\\My Games\\Skyrim Special Edition").c_str(), nullptr);
		CreateDirectoryW(dir.c_str(), nullptr);
	}
	CoTaskMemFree(documents);
	return dir;
}

inline void PrintVersion(FILE* f, const char* label, std::uint32_t v)
{
	std::fprintf(f, "%s: %u.%u.%u.%u (0x%08X)\n", label, v >> 24, (v >> 16) & 0xFF, (v >> 4) & 0xFFF, v & 0xF, v);
}

// Writes the module's load log and reports whether the runtime is the pinned one.
// Does nothing else: no hooks, no game memory access, no messaging with other plugins.
inline bool LoadSkeleton(const ModuleInfo& info, const skse::Interface* skse)
{
	const std::wstring logPath = LogDirectory() + info.fileStem + L".log";
	const std::wstring iniPath = ThisModuleDirectory() + info.fileStem + L".ini";

	const bool iniFound = GetFileAttributesW(iniPath.c_str()) != INVALID_FILE_ATTRIBUTES;
	const int logLevel = static_cast<int>(GetPrivateProfileIntW(L"Debug", L"iLogLevel", -1, iniPath.c_str()));
	const bool runtimeOk = skse && !skse->isEditor && skse->runtimeVersion == skse::kRuntime_1_7_104;

	FILE* f = nullptr;
	if (_wfopen_s(&f, logPath.c_str(), L"w") == 0 && f) {
		char stamp[32]{};
		const std::time_t now = std::time(nullptr);
		std::tm utc{};
		gmtime_s(&utc, &now);
		std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &utc);

		std::fprintf(f, "%s %s loaded at %s\n", info.displayName, info.version, stamp);
		std::fprintf(f, "source commit: %s\n", info.sourceCommit);
		if (skse) {
			PrintVersion(f, "skse", skse->skseVersion);
			PrintVersion(f, "runtime", skse->runtimeVersion);
		}
		std::fprintf(f, "settings: %ls (%s, iLogLevel=%d)\n", iniPath.c_str(), iniFound ? "found" : "missing, using defaults", logLevel);
		std::fprintf(f, "runtime check: %s\n", runtimeOk ? "PASS pinned 1.7.104" : "REJECTED unsupported runtime or editor");
		std::fprintf(f, "scope: load-only skeleton; no hooks, game memory access or inter-module calls\n");
		std::fclose(f);
	}
	return runtimeOk;
}

}  // namespace skycrpg
