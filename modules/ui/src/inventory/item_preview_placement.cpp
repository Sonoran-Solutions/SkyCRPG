// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// Placement of the player-inventory 3D item preview (DYL-168 round 5).
// The engine copies fInventory3DItemPos* into RE::Inventory3DManager when the
// menu begins its 3D view, before any menu-open event, and does not re-read them
// for later items (owner run 303bb937: values written at open had no effect).
// So, while the player InventoryMenu is open, this adjusts only the manager's
// cached itemPos/itemScale and their copies. No INI setting is read for the
// placement or written. Nothing is restored: the next Begin3D of any item menu
// copies its own INI values again, and writing on close could clobber a menu
// (e.g. Magic) whose 3D view has already begun.
#include "item_preview_placement.h"
#include <RE/G/GFxFunctionHandler.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#include <RE/I/Inventory3DManager.h>
#include <RE/I/InventoryMenu.h>
#include <RE/M/Main.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/M/Misc.h>
#include <RE/S/Setting.h>
#include <RE/U/UI.h>
#include <REX/W32/KERNEL32.h>
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
Session g_session;
Config g_config{};
int g_openLogs = 0;
bool g_loggedThisOpen = false;
bool g_loggedZoom = false;

void Log(const char* message)
{
	const auto path = skycrpg::LogDirectory() + L"SkyCRPG_UI.log";
	FILE* log = nullptr;
	if (_wfopen_s(&log, path.c_str(), L"a") == 0 && log) {
		std::fputs(message, log);
		std::fclose(log);
	}
}

bool MainThread()
{
	const auto* main = RE::Main::GetSingleton();
	return main && main->threadID == REX::W32::GetCurrentThreadId();
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

// Only while the player inventory is the item menu with a 3D view.
bool InventoryOnly(RE::UI* ui)
{
	if (!ui || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
		return false;
	}
	for (const std::string_view other : { std::string_view("ContainerMenu"), std::string_view("BarterMenu"),
			 std::string_view("GiftMenu"), std::string_view("MagicMenu") }) {
		if (ui->IsMenuOpen(other)) {
			return false;
		}
	}
	return true;
}

void LogPlacement(const char* label, const Placement& p)
{
	std::array<char, 200> line{};
	std::snprintf(line.data(), line.size(), "Item preview %s: itemPos=(%.3f, %.3f, %.3f) itemScale=%.3f\n", label,
		static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(p.z), static_cast<double>(p.scale));
	Log(line.data());
}

// Runs on the main thread only.
void Apply(std::optional<Target> reported)
{
	std::scoped_lock lock(g_mutex);
	if (!g_session.Open() || !InventoryOnly(RE::UI::GetSingleton())) {
		return;
	}
	auto* manager = RE::Inventory3DManager::GetSingleton();
	if (!manager || manager->currentLightScheme != RE::INTERFACE_LIGHT_SCHEME::kInventory) {
		return;
	}
	auto& runtime = manager->GetRuntimeData();
	EngineState state{};
	state.live = { manager->itemPos.x, manager->itemPos.y, manager->itemPos.z, manager->itemScale };
	state.copy = { manager->itemPosCopy.x, manager->itemPosCopy.y, manager->itemPosCopy.z, manager->itemScaleCopy };
	state.zoomProgress = runtime.zoomProgress;
	state.startedZoom = runtime.startedZoom;
	const Target target = reported && ValidTarget(*reported) ? *reported : DefaultTarget(DisplayAspect());
	const Result result = g_session.Apply(state, target, g_config);
	if (result.outcome == Outcome::ZoomActive && !g_loggedZoom) {
		g_loggedZoom = true;
		Log("Item preview: zoom in progress; engine placement left alone\n");
	}
	if (result.outcome != Outcome::Written) {
		return;
	}
	manager->itemPos.x = result.written.x;
	manager->itemPos.y = result.written.y;
	manager->itemPos.z = result.written.z;
	manager->itemScale = result.written.scale;
	manager->itemPosCopy.x = result.written.x;
	manager->itemPosCopy.y = result.written.y;
	manager->itemPosCopy.z = result.written.z;
	manager->itemScaleCopy = result.written.scale;
	if (!g_loggedThisOpen && g_openLogs < 32) {
		g_loggedThisOpen = true;
		++g_openLogs;
		std::array<char, 160> line{};
		std::snprintf(line.data(), line.size(), "Item preview target: centre=%.1f half=%.1f (%s)\n", target.centre,
			target.half, reported && ValidTarget(*reported) ? "card column" : "5/6 W fallback");
		Log(line.data());
		LogPlacement("before", result.base);
		LogPlacement("after", result.written);
	}
}

void ApplyOnMainThread(std::optional<Target> reported)
{
	if (MainThread()) {
		Apply(reported);
	} else if (const auto* tasks = SKSE::GetTaskInterface()) {
		tasks->AddTask([reported] { Apply(reported); });
	}
}

// placeItemPreview(centre, half): the menu reports its card column in stage units.
class PlaceHandler final : public RE::GFxFunctionHandler
{
public:
	void Call(Params& params) override
	{
		std::optional<Target> reported;
		if (params.argCount == 2 && params.args[0].IsNumber() && params.args[1].IsNumber()) {
			reported = Target{ params.args[0].GetNumber(), params.args[1].GetNumber() };
		}
		ApplyOnMainThread(reported);
		if (params.retVal) {
			params.retVal->SetBoolean(true);
		}
	}
};

// Ask the menu (UI task) to report its card column; that call applies.
void RequestPlacement()
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

class Events final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
	{
		if (!event || std::string_view(event->menuName.c_str()) != RE::InventoryMenu::MENU_NAME) {
			return RE::BSEventNotifyControl::kContinue;
		}
		bool request = false;
		{
			std::scoped_lock lock(g_mutex);
			if (event->opening) {
				g_session.Begin();
				g_loggedThisOpen = false;
				request = g_config.manage;
			} else {
				g_session.End();
			}
		}
		if (request) {
			RequestPlacement();
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
	std::array<char, 300> line{};
	std::snprintf(line.data(), line.size(),
		"Item preview settings: bManageItemPreview=%d%s fItemPreviewScale=%s%s fItemPreviewXShift=%s%s; "
		"adjusts Inventory3DManager placement only, no INI writes, no restore needed (Begin3D re-reads per open)\n",
		parsed.config.manage ? 1 : 0, parsed.manageValid ? "" : " (invalid, default)",
		parsed.config.scaleFactor ? "set" : "auto", parsed.scaleValid ? (parsed.scaleClamped ? " (clamped)" : "") : " (invalid, auto)",
		parsed.config.xShift ? "set" : "auto", parsed.shiftValid ? (parsed.shiftClamped ? " (clamped)" : "") : " (invalid, auto)");
	Log(line.data());
}

bool Register(RE::GFxMovieView* movie, RE::GFxValue* root)
{
	if (!movie || !root) {
		return false;
	}
	static PlaceHandler* handler = new PlaceHandler();
	RE::GFxValue function;
	movie->CreateFunction(&function, handler);
	return root->SetMember("placeItemPreview", function);
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
		std::scoped_lock lock(g_mutex);
		g_session.End();
	}
}
}  // namespace skycrpg::ui::inventory::preview
