// SkyCRPG Camera — P1-01 load-only skeleton. No gameplay behavior.
#include <skycrpg/skeleton.h>

namespace {
constexpr skycrpg::ModuleInfo kModule{ "SkyCRPG Camera", L"SkyCRPG_Camera", SKYCRPG_VERSION, SKYCRPG_SOURCE_COMMIT };
}

extern "C" {

__declspec(dllexport) constinit skse::PluginVersionData SKSEPlugin_Version{
	.dataVersion = skse::PluginVersionData::kVersion,
	.pluginVersion = (SKYCRPG_VERSION_MAJOR << 24) | (SKYCRPG_VERSION_MINOR << 16) | (SKYCRPG_VERSION_PATCH << 4),
	.name = "SkyCRPG Camera",
	.author = "SkyCRPG",
	.supportEmail = "",
	.versionIndependenceEx = skse::PluginVersionData::kVersionIndependentEx_NoStructUse,
	.versionIndependence = 0,
	.compatibleVersions = { skse::kRuntime_1_7_104, 0 },
	.seVersionRequired = skse::kSKSE_2_3_1,
};

__declspec(dllexport) bool SKSEPlugin_Load(const skse::Interface* skse)
{
	return skycrpg::LoadSkeleton(kModule, skse);
}

}
