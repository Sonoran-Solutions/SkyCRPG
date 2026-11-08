// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// Test-only SKSE host for upstream vinput (see tools/vinput/README.md). Never shipped.
//
// The plugin has no CommonLib or Address Library dependency and installs no hooks of
// its own. When the INI beside it opts in and the runtime is the pinned 1.7.104.0, it
// calls vinput's VInput_Init(), which starts vinput's worker thread inside the game
// process (vinput then installs its own hooks and serves \\.\pipe\vinput_<pid>). The
// plugin records the Windows PID so the Linux wrapper can find that pipe under Wine.
#include <skycrpg/skeleton.h>
#include <vinput.h>

#include <cstdio>
#include <ctime>
#include <string>

namespace {

constexpr const char* kDisplayName = "SkyCRPG VInput (test only)";
constexpr const wchar_t* kStem = L"SkyCRPG_VInput";

bool OptedIn(const std::wstring& ini)
{
	return GetPrivateProfileIntW(L"VInput", L"bEnabled", 0, ini.c_str()) == 1u;
}

void WritePidFile(unsigned long pid)
{
	const std::wstring path = skycrpg::LogDirectory() + kStem + L".pid";
	FILE* f = nullptr;
	if (_wfopen_s(&f, path.c_str(), L"w") == 0 && f) {
		std::fprintf(f, "%lu\n", pid);
		std::fclose(f);
	}
}

}  // namespace

extern "C" __declspec(dllexport) constinit skse::PluginVersionData SKSEPlugin_Version = [] {
	skse::PluginVersionData data{};
	data.dataVersion = skse::PluginVersionData::kVersion;
	data.pluginVersion = 1;
	constexpr char name[] = "SkyCRPG VInput (test only)";
	constexpr char author[] = "devils_17";
	for (std::size_t i = 0; i < sizeof(name); ++i) data.name[i] = name[i];
	for (std::size_t i = 0; i < sizeof(author); ++i) data.author[i] = author[i];
	data.versionIndependenceEx = skse::PluginVersionData::kVersionIndependentEx_NoStructUse;
	data.compatibleVersions[0] = skse::kRuntime_1_7_104;
	return data;
}();

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const skse::Interface* skse)
{
	const std::wstring iniPath = skycrpg::ThisModuleDirectory() + kStem + L".ini";
	const std::wstring logPath = skycrpg::LogDirectory() + kStem + L".log";
	const unsigned long pid = static_cast<unsigned long>(GetCurrentProcessId());
	const bool runtimeOk = skse && !skse->isEditor && skse->runtimeVersion == skse::kRuntime_1_7_104;
	const bool enabled = OptedIn(iniPath);

	FILE* f = nullptr;
	if (_wfopen_s(&f, logPath.c_str(), L"w") != 0 || !f) {
		OutputDebugStringA("SkyCRPG_VInput: load log unavailable; staying inert\n");
		return true;
	}
	char stamp[32]{};
	const std::time_t now = std::time(nullptr);
	std::tm utc{};
	if (gmtime_s(&utc, &now) == 0) std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
	std::fprintf(f, "%s loaded at %s\n", kDisplayName, stamp);
	std::fprintf(f, "source commit: %s\n", SKYCRPG_SOURCE_COMMIT);
	std::fprintf(f, "upstream vinput: %s at %s\n", VInput_Version(), SKYCRPG_VINPUT_UPSTREAM);
	std::fprintf(f, "pid=%lu\n", pid);
	if (skse) {
		skycrpg::PrintVersion(f, "skse", skse->skseVersion);
		skycrpg::PrintVersion(f, "runtime", skse->runtimeVersion);
	}
	std::fprintf(f, "settings: %ls ([VInput] bEnabled=%d)\n", iniPath.c_str(), enabled ? 1 : 0);
	std::fprintf(f, "runtime check: %s\n", runtimeOk ? "PASS pinned 1.7.104" : "REJECTED unsupported runtime or editor");
	std::fprintf(f, "scope: test-only vinput host; no SkyCRPG hooks, no game memory access, no inter-module calls\n");
	if (!runtimeOk || !enabled) {
		std::fprintf(f, "vinput: not started (%s)\n", runtimeOk ? "opt-in missing" : "runtime rejected");
		std::fclose(f);
		return true;
	}
	const int started = VINPUT_INIT();
	std::fprintf(f, "vinput: VInput_Init returned %d (%s)\n", started,
		started == 1 ? "started by this plugin" : "already running in this process");
	std::fprintf(f, "vinput: pipe \\\\.\\pipe\\vinput_%lu; vinput log <plugin dir>\\logs\\vinput_%lu.log\n", pid, pid);
	std::fclose(f);
	WritePidFile(pid);
	return true;
}
