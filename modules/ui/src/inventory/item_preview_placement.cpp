// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// Runtime-only placement of the player-inventory 3D item preview (DYL-168).
// While the player InventoryMenu is open, the in-memory fInventory3DItemPos*
// settings are shifted right and scaled down, the same settings SkyUI's MCM sets
// with Utility.SetINIFloat. The captured values are restored on close, load,
// new game and when another item menu opens. No file is ever written.
#include "item_preview_placement.h"
#include <RE/G/GFxMovieView.h>
#include <RE/I/InventoryMenu.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/M/Misc.h>
#include <RE/S/Setting.h>
#include <RE/U/UI.h>
#include <skycrpg/skeleton.h>
#include "item_preview_policy.h"
#include <array>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string_view>

namespace skycrpg::ui::inventory::preview {
namespace {
std::mutex g_mutex;
PlacementState g_state;
Config g_config{};
int g_applyLogs = 0;

void Log(const char* message)
{
	const auto path = skycrpg::LogDirectory() + L"SkyCRPG_UI.log";
	FILE* log = nullptr;
	if (_wfopen_s(&log, path.c_str(), L"a") == 0 && log) {
		std::fputs(message, log);
		std::fclose(log);
	}
}

RE::Setting* FloatSetting(const char* name)
{
	auto* setting = RE::GetINISetting(name);
	return setting && setting->GetType() == RE::Setting::Type::kFloat ? setting : nullptr;
}

std::optional<Values> ReadValues()
{
	Values values{};
	for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
		const auto* setting = FloatSetting(kSettingNames[slot]);
		if (!setting) {
			return std::nullopt;
		}
		values[slot] = setting->GetFloat();
	}
	return values;
}

bool WriteValues(const Writes& writes)
{
	bool wrote = false;
	for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
		if (!writes[slot]) {
			continue;
		}
		if (auto* setting = FloatSetting(kSettingNames[slot])) {
			setting->SetFloat(*writes[slot]);
			wrote = true;
		}
	}
	return wrote;
}

double DisplayAspect()
{
	const auto* width = RE::GetINISetting("iSize W:Display");
	const auto* height = RE::GetINISetting("iSize H:Display");
	if (width && height && width->GetType() == RE::Setting::Type::kInteger &&
		height->GetType() == RE::Setting::Type::kInteger && width->GetInteger() > 0 && height->GetInteger() > 0) {
		return static_cast<double>(width->GetInteger()) / static_cast<double>(height->GetInteger());
	}
	return kStageWidth / kStageHeight;
}

void LogValues(const char* label, double aspect)
{
	if (g_applyLogs >= 16) {
		return;
	}
	++g_applyLogs;
	const auto values = ReadValues();
	if (!values) {
		return;
	}
	std::array<char, 256> line{};
	std::snprintf(line.data(), line.size(),
		"Item preview %s: aspect=%.3f X=%.3f XWide=%.3f Z=%.3f ZWide=%.3f Scale=%.3f ScaleWide=%.3f (runtime only)\n",
		label, aspect, static_cast<double>((*values)[kPosX]), static_cast<double>((*values)[kPosXWide]),
		static_cast<double>((*values)[kPosZ]), static_cast<double>((*values)[kPosZWide]),
		static_cast<double>((*values)[kScale]), static_cast<double>((*values)[kScaleWide]));
	Log(line.data());
}

// Re-request the current item's model through SkyUI's own UpdateItem3D path.
void RequestRefresh()
{
	const auto* tasks = SKSE::GetTaskInterface();
	if (!tasks) {
		return;
	}
	tasks->AddUITask([] {
		auto* ui = RE::UI::GetSingleton();
		if (!ui || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
			return;
		}
		const auto menu = ui->GetMenu<RE::InventoryMenu>();
		if (menu && menu->uiMovie) {
			menu->uiMovie->Invoke("_root.Menu_mc.skycrpgRefreshItemPreview", nullptr, nullptr, 0);
		}
	});
}

void Apply()
{
	std::scoped_lock lock(g_mutex);
	const double aspect = DisplayAspect();
	const auto writes = g_state.Open(ReadValues(), aspect, g_config);
	if (g_state.TakeMissingLog()) {
		Log("Item preview placement: an fInventory3DItemPos* setting is missing or not a float; preview left unchanged\n");
	}
	if (WriteValues(writes)) {
		LogValues("applied", aspect);
		RequestRefresh();
	}
}

void Restore(const char* reason)
{
	std::scoped_lock lock(g_mutex);
	if (!g_state.Applied()) {
		return;
	}
	if (WriteValues(g_state.Restore(ReadValues()))) {
		LogValues(reason, DisplayAspect());
	}
}

class Events final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
	{
		if (!event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const std::string_view name = event->menuName.c_str();
		if (name == RE::InventoryMenu::MENU_NAME) {
			if (event->opening) {
				Apply();
			} else {
				Restore("restored on close");
			}
		} else if (event->opening && (name == "ContainerMenu" || name == "BarterMenu" || name == "GiftMenu" || name == "MagicMenu")) {
			// Defensive: other item menus always see the captured base values.
			Restore("restored for another item menu");
		}
		return RE::BSEventNotifyControl::kContinue;
	}
};
Events g_events;
}  // namespace

void Configure(const std::wstring& iniPath)
{
	std::array<wchar_t, 40> manage{};
	std::array<wchar_t, 40> scale{};
	std::array<wchar_t, 40> shift{};
	GetPrivateProfileStringW(L"Inventory", L"bManageItemPreview", L"", manage.data(), static_cast<DWORD>(manage.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"Inventory", L"fItemPreviewScale", L"", scale.data(), static_cast<DWORD>(scale.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"Inventory", L"fItemPreviewXShift", L"", shift.data(), static_cast<DWORD>(shift.size()), iniPath.c_str());
	const auto parsed = ParseConfig(manage.data(), scale.data(), shift.data());
	{
		std::scoped_lock lock(g_mutex);
		g_config = parsed.config;
	}
	std::array<char, 256> line{};
	std::snprintf(line.data(), line.size(),
		"Item preview settings: bManageItemPreview=%d%s fItemPreviewScale=%s%s fItemPreviewXShift=%s%s\n",
		parsed.config.manage ? 1 : 0, parsed.manageValid ? "" : " (invalid, default)",
		parsed.config.scaleFactor ? "set" : "auto", parsed.scaleValid ? (parsed.scaleClamped ? " (clamped)" : "") : " (invalid, auto)",
		parsed.config.xShift ? "set" : "auto", parsed.shiftValid ? (parsed.shiftClamped ? " (clamped)" : "") : " (invalid, auto)");
	Log(line.data());
}

void OnSkseMessage(SKSE::MessagingInterface::Message* message)
{
	if (!message) {
		return;
	}
	if (message->type == SKSE::MessagingInterface::kDataLoaded) {
		static bool registered = false;
		if (registered) {
			return;
		}
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_events);
			registered = true;
		} else {
			Log("Item preview placement: menu event source unavailable; preview left unchanged\n");
		}
	} else if (message->type == SKSE::MessagingInterface::kPreLoadGame ||
		message->type == SKSE::MessagingInterface::kPostLoadGame || message->type == SKSE::MessagingInterface::kNewGame) {
		Restore("restored on load");
	}
}
}  // namespace skycrpg::ui::inventory::preview
