// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// Test-only observer. No hooks, input emission, serialization or game-state mutations.
#include <SKSE/SKSE.h>
#include <RE/A/ActorValueOwner.h>
#include <RE/A/AIProcess.h>
#include <RE/B/BSInputDeviceManager.h>
#include <RE/B/ButtonEvent.h>
#include <RE/B/BSTimer.h>
#include <RE/C/ControlMap.h>
#include <RE/I/InputEvent.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#include <RE/M/Main.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/T/TESAmmo.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESCameraState.h>
#include <RE/T/TESContainerChangedEvent.h>
#include <RE/T/TESEquipEvent.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/B/BGSBipedObjectForm.h>
#include <RE/U/UI.h>

// CommonLib/REX headers must precede the Windows headers in the skeleton.
#include <skycrpg/skeleton.h>
#include "json_line.h"
#include "probe_config.h"
#include "snapshot_policy.h"
#include "observation_policy.h"
#include "perf_policy.h"
#include "cursor_policy.h"

#include <bcrypt.h>
#include <psapi.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <utility>

namespace {
namespace json = skycrpg::testprobe;
constexpr std::uint32_t kPlayerFormID = 0x00000014;
using Clock = std::chrono::steady_clock;

// SKSE plugins are process-lifetime modules (no hot unloading). Keep the state
// and sinks alive until process termination, including timer/task callbacks.
// No static destructor waits on a worker under the Windows loader lock.
struct Probe {
	std::mutex outputMutex;
	std::mutex logMutex;
	std::mutex lifecycleMutex;
	std::mutex perfMutex;
	LARGE_INTEGER qpcFrequency{};
	json::FrameRing<16384> frames, engineFrames;
	json::FrameChain frameChain;
	json::InventoryWindow inventory;
	std::string captureLabel;
	bool hasInventoryKey{}, inventoryMenuOpen{}, inventoryEverOpened{};
	double previousFrameMs{}, captureStartMs{}, inventoryKeyMs{}, preOpenMedian{};
	std::wstring directory;
	std::string saveName;
	bool hasSaveName{ false };
	Clock::time_point origin{ Clock::now() };
	std::atomic_bool loaded{ false };
	std::atomic_uint64_t generation{ 0 };
	std::atomic_uint32_t mainThreadID{ 0 };
	std::atomic_bool timerBusy{ false };
	std::atomic_bool timerPending{ false };
	std::atomic_bool memoryFailureLogged{ false };
	std::uint32_t interval{ 1000 };
	HANDLE timer{};
};
Probe* g_probe = nullptr;

void Log(const char* message) noexcept
{
	OutputDebugStringA(message);
	try {
		if (!g_probe || g_probe->directory.empty()) return;
		std::scoped_lock lock(g_probe->logMutex);
		const auto path = g_probe->directory + L"SkyCRPG_TestProbe.log";
		FILE* file = nullptr;
		if (_wfopen_s(&file, path.c_str(), L"ab") == 0 && file) {
			std::fputs(message, file);
			std::fclose(file);
		}
	} catch (...) {
		OutputDebugStringA("SkyCRPG_TestProbe: diagnostic write failed\n");
	}
}

void Emit(std::string_view event, const json::Object& fields = {}) noexcept
{
	try {
		if (!g_probe || g_probe->directory.empty()) return;
		std::scoped_lock lock(g_probe->outputMutex);
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_probe->origin).count();
		std::array<char, 32> stamp{};
		const auto now = std::time(nullptr);
		std::tm utc{};
		if (gmtime_s(&utc, &now) != 0 || std::strftime(stamp.data(), stamp.size(), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
			Log("SkyCRPG_TestProbe: wall clock unavailable; event skipped\n");
			return;
		}
		const auto line = json::EventLine(static_cast<std::uint64_t>(elapsed), stamp.data(), event, fields);
		const auto path = g_probe->directory + L"SkyCRPG_TestProbe.jsonl";
		FILE* file = nullptr;
		if (_wfopen_s(&file, path.c_str(), L"ab") != 0 || !file) {
			Log("SkyCRPG_TestProbe: JSONL append open failed\n");
			return;
		}
		const bool written = std::fwrite(line.data(), 1, line.size(), file) == line.size();
		const bool flushed = std::fflush(file) == 0;
		const bool closed = std::fclose(file) == 0;
		if (!written || !flushed || !closed) Log("SkyCRPG_TestProbe: JSONL write/flush failed\n");
	} catch (...) {
		Log("SkyCRPG_TestProbe: event formatting failed; event skipped\n");
	}
}

// Set only while a probe task delegate runs inside SKSE's UI task drain.
thread_local bool t_inTaskDrain = false;

bool InTaskDrain() noexcept { return t_inTaskDrain; }

template <class F>
bool QueueProbeTask(F&& task) noexcept
{
	try {
		const auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) return false;
		tasks->AddUITask([task = std::forward<F>(task)]() mutable {
			t_inTaskDrain = true;
			try { task(); } catch (...) { Log("SkyCRPG_TestProbe: queued task failed; skipped\n"); }
			t_inTaskDrain = false;
		});
		return true;
	} catch (...) { return false; }
}

void ThreadFields(json::Object& fields)
{
	fields.Numeric("current_thread_id", GetCurrentThreadId())
		.Numeric("captured_main_thread_id", g_probe->mainThreadID.load());
	const auto* main = RE::Main::GetSingleton();
	if (main) fields.Numeric("main_thread_id", main->threadID);
	else fields.Null("main_thread_id");
}

void ThreadContext(std::string_view source) noexcept
{
	try {
		json::Object fields;
		fields.String("source", source);
		ThreadFields(fields);
		Emit("thread_context", fields);
	} catch (...) { Log("SkyCRPG_TestProbe: thread diagnostic failed\n"); }
}

void CaptureMainThread()
{
	// Diagnostic only: engine reads are gated on InTaskDrain(), not this id.
	// Captured once at kDataLoaded and never rebound, so JSONL thread fields
	// stay comparable across a run.
	std::uint32_t unset = 0;
	g_probe->mainThreadID.compare_exchange_strong(unset, GetCurrentThreadId());
}

void FormField(json::Object& fields, std::string_view key, const RE::TESForm* form)
{
	if (form) fields.String(key, json::FormID(form->GetFormID()));
	else fields.Null(key);
}

void NameField(json::Object& fields, RE::TESForm* form)
{
	const auto* name = form ? form->GetName() : nullptr;
	if (name) fields.String("name", name);
	else {
		fields.Null("name");
		Log("SkyCRPG_TestProbe: form/name unavailable\n");
	}
}

json::Object Resource(RE::PlayerCharacter* player, RE::ActorValue value)
{
	json::Object result;
	const auto* owner = player->AsActorValueOwner();
	if (owner) result.Numeric("current", owner->GetActorValue(value));
	else {
		result.Null("current").Null("max");
		Log("SkyCRPG_TestProbe: actor value owner unavailable\n");
		return result;
	}
	result.Numeric("max", player->GetActorValueMax(value));
	return result;
}

// SkyUI's inventory quantity slider is an ItemCard submenu, not a menu, so no
// MenuOpenCloseEvent marks it. ItemCard sets InputHandler while any submenu
// (quantity/list/message/editName) is up and clears it on hide. Read-only GFx
// access, only from the UI task drain; null when the card cannot be resolved.
void ItemCardField(json::Object& fields, RE::UI* ui)
{
	constexpr const char* card = "_root.Menu_mc.itemCardFadeHolder.ItemCard_mc";
	if (!ui || !ui->IsMenuOpen("InventoryMenu")) { fields.Null("item_card_submenu").Null("item_card_quantity"); return; }
	const auto movie = ui->GetMovieView("InventoryMenu");
	RE::GFxValue value;
	if (!movie || !movie->GetVariable(&value, card) || value.IsUndefined()) {
		Log("SkyCRPG_TestProbe: inventory item card unavailable\n");
		fields.Null("item_card_submenu").Null("item_card_quantity");
		return;
	}
	RE::GFxValue handler;
	const bool open = movie->GetVariable(&handler, (std::string(card) + ".InputHandler").c_str()) &&
		!handler.IsUndefined() && !handler.IsNull();
	fields.Boolean("item_card_submenu", open);
	RE::GFxValue quantity;
	if (open && movie->GetVariable(&quantity, (std::string(card) + ".QuantitySlider_mc.value").c_str()) && quantity.IsNumber())
		fields.Numeric("item_card_quantity", quantity.GetNumber());
	else fields.Null("item_card_quantity");
}

// All GFx values are task-local copies; observation calls do not mutate movies.
bool Variable(RE::GFxMovieView* movie, const char* path, RE::GFxValue& value)
{
	return movie && movie->GetVariable(&value, path) && !value.IsUndefined() && !value.IsNull();
}
void BooleanVariable(json::Object& output, std::string_view name, RE::GFxMovieView* movie, const char* path)
{
	RE::GFxValue value;
	if (Variable(movie, path, value) && value.IsBool()) output.Boolean(name, value.GetBool());
	else output.Null(name);
}
void Clip(json::Object& output, std::string_view name, RE::GFxMovieView* movie, const char* path)
{
	json::Object clip;
	RE::GFxValue value;
	if (!movie) { output.Null(name); return; }
	const bool present = Variable(movie, path, value) && value.IsDisplayObject();
	clip.Boolean("present", present);
	if (!present) { clip.Boolean("visible", false); output.Raw(name, clip.Json()); return; }
	// _visible is local in AS2. Include every ancestor's visibility and alpha,
	// with a hard bound and unknown if a chain/property cannot be resolved.
	bool visible = true, complete = false;
	for (unsigned depth = 0; depth < 16; ++depth) {
		RE::GFxValue shown, alpha, parent;
		if (!value.GetMember("_visible", &shown) || !shown.IsBool() ||
			!value.GetMember("_alpha", &alpha) || !alpha.IsNumber()) break;
		visible = visible && shown.GetBool() && alpha.GetNumber() > 0;
		if (!visible) { complete = true; break; }
		if (!value.GetMember("_parent", &parent) || parent.IsUndefined() || parent.IsNull()) { complete = true; break; }
		if (!parent.IsDisplayObject()) break;
		value = parent;
	}
	if (complete) clip.Boolean("visible", visible);
	else clip.Null("visible");
	output.Raw(name, clip.Json());
}
void CursorFields(json::Object& inventory, RE::GFxMovieView* movie, RE::GFxValue& itemList, RE::GFxValue& selected)
{
	json::Object cursor;
	RE::GFxValue root, x, y, index, clip, bounds, visible, result;
	const bool hasRoot = Variable(movie, "_root", root) && root.IsDisplayObject();
	const bool hasX = hasRoot && root.GetMember("_xmouse", &x) && x.IsNumber() && json::CursorNumber(true, x.GetNumber());
	const bool hasY = hasRoot && root.GetMember("_ymouse", &y) && y.IsNumber() && json::CursorNumber(true, y.GetNumber());
	if (hasX) cursor.Numeric("x", x.GetNumber()); else cursor.Null("x");
	if (hasY) cursor.Numeric("y", y.GetNumber()); else cursor.Null("y");
	std::array<RE::GFxValue, 3> hitArgs;
	if (hasX && hasY) { hitArgs[0].SetNumber(x.GetNumber()); hitArgs[1].SetNumber(y.GetNumber()); hitArgs[2].SetBoolean(true); }
	if (hasX && hasY && itemList.IsDisplayObject() && itemList.Invoke("hitTest", &result, hitArgs) && result.IsBool()) cursor.Boolean("over_list", result.GetBool());
	else cursor.Null("over_list");
	const bool hasIndex = selected.IsObject() && selected.GetMember("clipIndex", &index) && index.IsNumber() && json::CursorNumber(true, index.GetNumber());
	std::array<RE::GFxValue, 1> args;
	if (hasIndex) args[0] = index;
	const bool hasClip = hasIndex && itemList.IsDisplayObject() && itemList.Invoke("getClipByIndex", &clip, args) && clip.IsDisplayObject();
	bool completeBounds = false, hasVisible = false;
	if (hasClip) {
		json::Object box;
		args[0] = root;
		const bool hasBounds = hasRoot && clip.Invoke("getBounds", &bounds, args) && bounds.IsObject();
		completeBounds = hasBounds;
		for (const auto& pair : { std::pair{ "x_min", "xMin" }, std::pair{ "x_max", "xMax" }, std::pair{ "y_min", "yMin" }, std::pair{ "y_max", "yMax" } }) {
			RE::GFxValue value;
			if (hasBounds && bounds.GetMember(pair.second, &value) && value.IsNumber() && json::CursorNumber(true, value.GetNumber())) box.Numeric(pair.first, value.GetNumber());
			else { box.Null(pair.first); completeBounds = false; }
		}
		hasVisible = clip.GetMember("_visible", &visible) && visible.IsBool();
		if (hasVisible) box.Boolean("visible", visible.GetBool()); else box.Null("visible");
		cursor.Raw("selected_clip", box.Json());
	} else cursor.Null("selected_clip");
	const bool hitResult = hasClip && completeBounds && hasVisible && hasX && hasY && clip.Invoke("hitTest", &result, hitArgs) && result.IsBool();
	if (json::CursorHitKnown(hasX && hasY, hasClip, hitResult)) cursor.Boolean("hit", result.GetBool()); else cursor.Null("hit");
	inventory.Raw("cursor", cursor.Json());
}
void UIFields(json::Object& fields, RE::UI* ui)
{
	const auto hud = ui ? ui->GetMovieView("HUD Menu") : nullptr;
	json::Object state;
	Clip(state, "compass", hud.get(), "_root.HUDMovieBaseInstance.CompassShoutMeterHolder.Compass");
	Clip(state, "minimap", hud.get(), "_root.HUDMovieBaseInstance.Minimap");
	Clip(state, "root", hud.get(), "_root.HUDMovieBaseInstance.SkyCRPGStatus");
	// A present root can be hidden while data is stale. Expose readiness separately.
	BooleanVariable(state, "data_ready", hud.get(), "_root.HUDMovieBaseInstance.SkyCRPGStatus._skycrpgDataReady");
	std::string slots = "[";
	for (unsigned i = 0; i < 8; ++i) {
		if (i != 0) slots += ',';
		json::Object slot;
		const auto path = "_root.HUDMovieBaseInstance.SkyCRPGHotbar.slot" + std::to_string(i);
		for (const auto* member : { "key", "kind", "name", "count" }) {
			RE::GFxValue text;
			if (Variable(hud.get(), (path + "." + member + ".text").c_str(), text) && text.IsString())
				slot.String(member, text.GetString());
			else slot.Null(member);
		}
		slots += slot.Json();
	}
	state.Raw("hotbar", slots + ']');
	fields.Raw("hud", state.Json());
	if (!ui || !ui->IsMenuOpen("InventoryMenu")) { fields.Null("inventory"); return; }
	const auto movie = ui->GetMovieView("InventoryMenu");
	constexpr const char* list = "_root.Menu_mc.inventoryLists.itemList";
	json::Object inventory;
	RE::GFxValue itemList, grid, selected, name, count, favorite;
	if (!Variable(movie.get(), list, itemList)) { fields.Null("inventory"); return; }
	// skycrpgGrid is absent from a stock movie and explicitly false on a generated fallback.
	const bool hasGrid = itemList.GetMember("skycrpgGrid", &grid) && !grid.IsUndefined();
	if (!hasGrid || grid.IsBool()) inventory.Boolean("grid_active", hasGrid && grid.GetBool());
	else inventory.Null("grid_active");
	if (itemList.GetMember("selectedEntry", &selected) && selected.IsObject()) {
		if (selected.GetMember("text", &name) && name.IsString()) inventory.String("selected_name", name.GetString());
		else inventory.Null("selected_name");
		if (selected.GetMember("count", &count) && count.IsNumber()) inventory.Numeric("selected_count", count.GetNumber());
		else inventory.Null("selected_count");
		// SkyUI 6.11 InventoryListEntry.setEntry reads entry.favorite == true.
		// Missing/invalid data stays unknown; it must never authorize a toggle.
		if (selected.GetMember("favorite", &favorite) && favorite.IsBool()) inventory.Boolean("selected_favorite", favorite.GetBool());
		else inventory.Null("selected_favorite");
	} else inventory.Null("selected_name").Null("selected_count").Null("selected_favorite");
	CursorFields(inventory, movie.get(), itemList, selected);
	fields.Raw("inventory", inventory.Json());
}

void Snapshot(bool requested, const std::string& tag, std::uint64_t generation, const json::StateRequest& request)
{
	json::Object fields;
	fields.Numeric("observation_protocol", json::kObservationProtocol).Boolean("requested", requested);
	if (requested) fields.String("tag", tag);
	ThreadFields(fields);
	if (!request.valid) {
		fields.Boolean("available", false).String("reason", "invalid_state_request");
		Emit("player_state", fields); return;
	}
	const auto currentGeneration = g_probe->generation.load();
	const bool loaded = g_probe->loaded.load();
	fields.Numeric("queued_generation", generation).Numeric("current_generation", currentGeneration)
		.Boolean("loaded", loaded);
	// A request always gets a response, even if there is no usable world yet.
	const auto reason = json::SnapshotReason(InTaskDrain(), generation, currentGeneration, loaded);
	if (!reason.empty()) {
		fields.Boolean("available", false).String("reason", reason);
		Emit("player_state", fields);
		return;
	}
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* ui = RE::UI::GetSingleton();
	if (!player || !player->Is3DLoaded() || (ui && (ui->IsMenuOpen("Main Menu") || ui->IsMenuOpen("Loading Menu")))) {
		Log("SkyCRPG_TestProbe: player/3D unavailable; snapshot skipped\n");
		fields.Boolean("available", false).String("reason", "player_unavailable");
		Emit("player_state", fields);
		return;
	}
	fields.Boolean("available", true);
	const auto position = player->GetPosition();
	fields.Raw("position", json::Object{}.Numeric("x", position.x).Numeric("y", position.y).Numeric("z", position.z).Json());
	auto* cell = player->GetParentCell();
	if (cell) {
		json::Object location;
		FormField(location, "form_id", cell);
		NameField(location, cell);
		const char* editorID = cell->GetFormEditorID();
		if (editorID) location.String("editor_id", editorID);
		else location.Null("editor_id");
		fields.Raw("cell", location.Json());
	} else {
		Log("SkyCRPG_TestProbe: parent cell unavailable\n");
		fields.Null("cell");
	}
	fields.Raw("health", Resource(player, RE::ActorValue::kHealth).Json());
	fields.Raw("magicka", Resource(player, RE::ActorValue::kMagicka).Json());
	fields.Raw("stamina", Resource(player, RE::ActorValue::kStamina).Json());
	json::Object equipped;
	FormField(equipped, "right", player->GetEquippedObject(false));
	FormField(equipped, "left", player->GetEquippedObject(true));
	FormField(equipped, "ammo", player->GetCurrentAmmo());
	const auto* right = player->GetEquippedObject(false);
	// Form-type check plus static_cast: the static CommonLib build does not
	// instantiate TESForm::As<T> (same link failure fixed in ba10da6).
	const auto* weapon = right && right->GetFormType() == RE::FormType::Weapon ? static_cast<const RE::TESObjectWEAP*>(right) : nullptr;
	equipped.Boolean("two_handed", weapon && json::TwoHandedWeapon(static_cast<unsigned>(weapon->GetWeaponType())));
	using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
	if (requested) {
		// Vanilla helmets often use Hair/Circlet, not Head (Iron Helmet BOD2 = slots 31 and 42).
		auto* head = player->GetWornArmor(Slot::kHead, true);
		if (!head) head = player->GetWornArmor(Slot::kHair, true);
		if (!head) head = player->GetWornArmor(Slot::kCirclet, true);
		FormField(equipped, "head", head);
		FormField(equipped, "body", player->GetWornArmor(Slot::kBody, true));
		FormField(equipped, "ring", player->GetWornArmor(Slot::kRing, true));
		FormField(equipped, "amulet", player->GetWornArmor(Slot::kAmulet, true));
	} else equipped.Null("head").Null("body").Null("ring").Null("amulet");
	fields.Raw("equipped", equipped.Json());
	FormField(fields, "selected_power", player->GetActorRuntimeData().selectedPower);
	const auto* process = player->GetActorRuntimeData().currentProcess;
	if (process && process->high) fields.Numeric("voice_recovery", player->GetVoiceRecoveryTime());
	else fields.Null("voice_recovery");
	fields.Boolean("weapon_drawn", player->IsWeaponDrawn());
	fields.Numeric("carry_weight", player->GetActorValue(RE::ActorValue::kInventoryWeight));
	json::Object counts, weights;
	if (request.count > 0) {
		// One filtered no-init scan per explicit request, never on the periodic path.
		const bool inventoryAvailable = player->GetInventoryChanges(true) != nullptr;
		const auto inventory = inventoryAvailable ? player->GetInventoryCounts([&request](RE::TESBoundObject& object) {
			for (std::size_t i = 0; i < request.count; ++i) if (object.GetFormID() == request.forms[i]) return true;
			return false;
		}, true) : RE::TESObjectREFR::InventoryCountMap{};
		for (std::size_t i = 0; i < request.count; ++i) {
			const auto formID = request.forms[i];
			const auto* raw = RE::TESForm::LookupByID(formID);
			const auto* form = raw && raw->IsBoundObject() ? static_cast<const RE::TESBoundObject*>(raw) : nullptr;
			if (!form) { counts.Null(json::FormID(formID)); weights.Null(json::FormID(formID)); continue; }
			weights.Numeric(json::FormID(formID), form->GetWeight());
			if (!inventoryAvailable) { counts.Null(json::FormID(formID)); continue; }
			std::int32_t count = 0;
			for (const auto& [object, quantity] : inventory) if (object && object->GetFormID() == formID) count = quantity;
			counts.Numeric(json::FormID(formID), count);
		}
	}
	fields.Raw("item_counts", counts.Json()).Raw("item_weights", weights.Json());
	UIFields(fields, ui);
	const auto* camera = RE::PlayerCamera::GetSingleton();
	if (camera && camera->currentState) fields.Numeric("camera_state", static_cast<std::uint32_t>(camera->currentState->id));
	else {
		Log("SkyCRPG_TestProbe: camera/state unavailable\n");
		fields.Null("camera_state");
	}
	if (ui) {
		fields.Boolean("paused", ui->GameIsPaused());
		std::string menus = "[";
		// Bounded by the registered menu map; no movie access or retained menus.
		for (const auto& [name, entry] : ui->menuMap) {
			if (!entry.menu || !entry.menu->OnStack()) continue;
			if (menus.size() > 1) menus += ',';
			menus += json::Quote(name.c_str());
		}
		fields.Raw("open_menus", menus + ']');
		ItemCardField(fields, ui);
	} else {
		Log("SkyCRPG_TestProbe: UI unavailable\n");
		fields.Null("paused").Null("open_menus").Null("item_card_submenu").Null("item_card_quantity");
	}
	Emit("player_state", fields);
}

// Only the timer coalesces outstanding samples, so a stalled main thread cannot
// grow an unbounded timer queue. Menu-close/load snapshots are separate tasks.
// SKSE's ordinary task-pool queue has multiple drain sites. Use the UI event
// queue (Hooks_UI.cpp:46-86); reads are gated on running inside that drain.
bool QueueSnapshot(bool requested = false, std::string tag = {}, bool timer = false) noexcept
{
	try {
		if (timer && g_probe->timerPending.exchange(true)) return false;
		const auto generation = g_probe->generation.load();
		const auto request = json::ParseStateRequest(tag);
		const bool queued = QueueProbeTask([requested, request, generation, timer] {
			try { Snapshot(requested, request.tag, generation, request); }
			catch (...) { Log("SkyCRPG_TestProbe: snapshot failed; skipped\n"); }
			if (timer) g_probe->timerPending.store(false);
		});
		if (!queued) {
			if (timer) g_probe->timerPending.store(false);
			Log("SkyCRPG_TestProbe: task interface unavailable\n");
		}
		return queued;
	} catch (...) {
		if (timer) g_probe->timerPending.store(false);
		Log("SkyCRPG_TestProbe: snapshot enqueue failed\n");
		return false;
	}
}

// QPC samples are anchored on the UI drain, with a main-queue helper between
// samples. Neither delegate requeues into its own currently draining queue.
// Persistent Run/Dispose delegates use CommonLib's passthrough pointer overloads.
double PerfNow() noexcept
{
	LARGE_INTEGER now{};
	if (!QueryPerformanceCounter(&now)) return 0;
	return static_cast<double>(now.QuadPart) * 1000.0 / static_cast<double>(g_probe->qpcFrequency.QuadPart);
}

void EmitInventory()
{
	auto& window = g_probe->inventory;
	if (!window.active) return;
	window.active = false;
	json::Object fields;
	fields.String("label", g_probe->captureLabel).Boolean("cold", window.cold)
		.Boolean("correlated", window.correlated).Numeric("pre_open_median_ms", window.preMedian)
		.Numeric("open_qpc_ms", window.openMs);
	if (window.correlated) fields.Numeric("key_qpc_ms", window.keyMs);
	else fields.Null("key_qpc_ms");
	if (window.correlated) fields.Numeric("t_event_ms", window.openMs - window.keyMs);
	else fields.Null("t_event_ms");
	if (window.settled) fields.Numeric("t_settled_ms", window.settledMs);
	else fields.Null("t_settled_ms");
	std::string periods = "[";
	for (std::size_t i = 0; i < window.count; ++i) {
		if (i) periods += ',';
		periods += json::Number(window.periods[i]);
	}
	fields.Raw("periods_ms", periods + ']');
	Emit("inventory_open", fields);
}

void QueueFrameHop(json::FrameChain::Hop hop) noexcept;

void AbortFrameCapture(const char* reason)
{
	g_probe->frameChain.stop();
	g_probe->inventory = {};
	g_probe->hasInventoryKey = false;
	Emit("perf_capture", json::Object{}.String("label", g_probe->captureLabel)
		.String("phase", "abort").Boolean("available", false).String("reason", reason));
}

class FrameUITask final : public SKSE::detail::UIDelegate_v1 {
public:
	void Run() noexcept override
	{
		try {
			{
				std::scoped_lock lock(g_probe->perfMutex);
				const double now = PerfNow();
				const auto tick = g_probe->frameChain.uiRun(now);
				if (tick == json::FrameChain::Tick::idle) return;
				if (tick == json::FrameChain::Tick::runaway) {
					Log("SkyCRPG_TestProbe: frame capture stopped: runaway\n");
					AbortFrameCapture("runaway");
					return;
				}
				const auto* timer = RE::BSTimer::GetSingleton();
				const double engineMs = timer ? static_cast<double>(timer->realTimeDelta) * 1000.0 : 0;
				if (g_probe->previousFrameMs > 0 && now > g_probe->previousFrameMs) {
					const double period = now - g_probe->previousFrameMs;
					g_probe->frames.push(period);
					if (engineMs > 0) g_probe->engineFrames.push(engineMs);
					if (g_probe->inventory.observe(period, now)) EmitInventory();
				}
				g_probe->previousFrameMs = now;
			}
			QueueFrameHop(json::FrameChain::Hop::main);
		} catch (...) {
			std::scoped_lock lock(g_probe->perfMutex);
			g_probe->frameChain.enqueueFailed();
			AbortFrameCapture("ui_task_failed");
			Log("SkyCRPG_TestProbe: frame UI task failed\n");
		}
	}
	void Dispose() noexcept override {}  // process-lifetime, never self-requeue
};

class FrameMainTask final : public SKSE::detail::TaskDelegate {
public:
	void Run() noexcept override
	{
		try {
			bool queueUI = false;
			{
				std::scoped_lock lock(g_probe->perfMutex);
				queueUI = g_probe->frameChain.mainRun();
			}
			if (queueUI) QueueFrameHop(json::FrameChain::Hop::ui);
		} catch (...) {
			std::scoped_lock lock(g_probe->perfMutex);
			g_probe->frameChain.enqueueFailed();
			AbortFrameCapture("main_task_failed");
			Log("SkyCRPG_TestProbe: frame main helper failed\n");
		}
	}
	void Dispose() noexcept override {}
};
FrameUITask g_frameUITask;
FrameMainTask g_frameMainTask;

void QueueFrameHop(json::FrameChain::Hop hop) noexcept
{
	// Never hold perfMutex while acquiring SKSE's UI queue lock: the UI drain
	// holds that lock while calling Run, which itself acquires perfMutex.
	try {
		if (const auto* tasks = SKSE::GetTaskInterface()) {
			if (hop == json::FrameChain::Hop::ui)
				tasks->AddUITask(reinterpret_cast<::UIDelegate_v1*>(&g_frameUITask));
			else if (hop == json::FrameChain::Hop::main)
				tasks->AddTask(reinterpret_cast<::TaskDelegate*>(&g_frameMainTask));
			return;
		}
	} catch (...) { /* emit a single unavailable result below */ }
	std::scoped_lock lock(g_probe->perfMutex);
	g_probe->frameChain.enqueueFailed();
	AbortFrameCapture("enqueue_failed");
	Log("SkyCRPG_TestProbe: frame enqueue failed\n");
}

void PerfRequest(const std::string& tag)
{
	const auto request = json::ParsePerfRequest(tag);
	std::unique_lock lock(g_probe->perfMutex);
	bool queueUI = false;
	bool available = false;
	if (request.command == json::PerfCommand::start && !g_probe->frameChain.active() && g_probe->loaded.load()) {
		g_probe->captureLabel = request.label;
		g_probe->frames.clear();
		g_probe->engineFrames.clear();
		g_probe->inventory = {};
		g_probe->hasInventoryKey = false;
		g_probe->previousFrameMs = 0;
		g_probe->captureStartMs = PerfNow();
		queueUI = g_probe->frameChain.start();
		available = g_probe->frameChain.queued();
		if (!available) g_probe->frameChain.stop();
		if (available && request.label == "resource") Emit("resource_check", json::Object{}.String("phase", "start"));
	} else if (request.command == json::PerfCommand::stop && g_probe->frameChain.active() && request.label == g_probe->captureLabel) {
		g_probe->frameChain.stop();
		EmitInventory();
		const auto summary = g_probe->frames.summary();
		const auto engine = g_probe->engineFrames.summary();
		json::Object fields;
		fields.String("label", g_probe->captureLabel)
			.Numeric("count", summary.count).Numeric("total_count", g_probe->frames.total())
			.Numeric("median", summary.median).Numeric("p95", summary.p95).Numeric("p99", summary.p99)
			.Numeric("max", summary.maximum).Numeric("over33", summary.over33).Numeric("over50", summary.over50)
			.Numeric("engine_count", engine.count).Numeric("engine_total_count", g_probe->engineFrames.total())
			.Numeric("engine_median", engine.median).Numeric("engine_p95", engine.p95)
			.Numeric("duration_ms", PerfNow() - g_probe->captureStartMs);
		if (engine.median > 0 && summary.count > 0) fields.Numeric("anchor_ratio", summary.median / engine.median);
		else fields.Null("anchor_ratio");
		Emit("frame_summary", fields);
		if (request.label == "resource") Emit("resource_check", json::Object{}.String("phase", "stop"));
		available = true;
	}
	Emit("perf_capture", json::Object{}.String("tag", tag).String("label", request.label)
		.String("phase", request.command == json::PerfCommand::start ? "start" : "stop").Boolean("available", available));
	lock.unlock();
	if (queueUI) QueueFrameHop(json::FrameChain::Hop::ui);
}

void InventoryMenuEvent(bool opening, double now)
{
	std::scoped_lock lock(g_probe->perfMutex);
	if (opening) {
		const bool cold = !g_probe->inventoryEverOpened;
		g_probe->inventoryEverOpened = true;
		g_probe->inventoryMenuOpen = true;
		if (g_probe->frameChain.active()) {
			EmitInventory();
			const bool correlated = g_probe->hasInventoryKey && now >= g_probe->inventoryKeyMs && now - g_probe->inventoryKeyMs <= 10000;
			g_probe->inventory.begin(g_probe->inventoryKeyMs, now, g_probe->preOpenMedian, correlated, cold);
		}
		g_probe->hasInventoryKey = false;
	} else {
		g_probe->inventoryMenuOpen = false;
		EmitInventory();
	}
}

class InputEvents final : public RE::BSTEventSink<RE::InputEvent*> {
public:
	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,
		RE::BSTEventSource<RE::InputEvent*>*) noexcept override
	{
		try {
			if (!events) return RE::BSEventNotifyControl::kContinue;
			{
				std::scoped_lock lock(g_probe->perfMutex);
				if (!g_probe->frameChain.active()) return RE::BSEventNotifyControl::kContinue;
			}
			for (auto* event = *events; event; event = event->next) {
				const auto* button = event->AsButtonEvent();
				if (!button || !button->IsDown()) continue;
				const double now = PerfNow();
				const auto* controls = RE::ControlMap::GetSingleton();
				if (!controls) continue;
				const auto device = event->GetDevice();
				const auto key = button->GetIDCode();
				const auto inventoryKey = controls->GetMappedKey("Inventory", device);
				const auto tweenKey = controls->GetMappedKey("Tween Menu", device);
				if ((inventoryKey == RE::ControlMap::kInvalid || key != inventoryKey) &&
					(tweenKey == RE::ControlMap::kInvalid || key != tweenKey)) continue;
				std::scoped_lock lock(g_probe->perfMutex);
				if (!g_probe->frameChain.active() || g_probe->inventoryMenuOpen) continue;
				g_probe->inventoryKeyMs = now;
				g_probe->preOpenMedian = g_probe->frames.summary().median;
				g_probe->hasInventoryKey = true;
			}
		} catch (...) { Log("SkyCRPG_TestProbe: input timestamp failed\n"); }
		return RE::BSEventNotifyControl::kContinue;
	}
};

struct Handle {
	HANDLE value{ INVALID_HANDLE_VALUE };
	~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

bool PinnedAddressLibrary()
{
	// Same Address Library 13 pin as the native UI/Camera. CommonLib Init is
	// noexcept and can terminate on a missing/bad ID database: guard it first.
	const auto path = skycrpg::ThisModuleDirectory() + L"versionlib-1-7-104-0.bin";
	Handle file{ CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr) };
	if (file.value == INVALID_HANDLE_VALUE) return false;
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	bool valid = false;
	if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
		BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
		std::array<UCHAR, 65536> buffer{};
		bool readOk = true;
		for (;;) {
			DWORD bytesRead{};
			if (!ReadFile(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr)) {
				readOk = false;
				break;
			}
			if (bytesRead == 0) break;
			if (BCryptHashData(hash, buffer.data(), bytesRead, 0) < 0) { readOk = false; break; }
		}
		std::array<UCHAR, 32> digest{};
		if (readOk && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0) {
			constexpr char hex[] = "0123456789abcdef";
			std::array<char, 64> actual{};
			for (std::size_t index = 0; index < digest.size(); ++index) {
				actual[index * 2] = hex[digest[index] >> 4];
				actual[index * 2 + 1] = hex[digest[index] & 0x0F];
			}
			valid = std::string_view(actual.data(), actual.size()) ==
				"8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842";
		}
	}
	if (hash) BCryptDestroyHash(hash);
	if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
	return valid;
}

bool PollRequest()
{
	const auto path = g_probe->directory + L"SkyCRPG_TestProbe.request";
	// Deny concurrent writes/renames while reading. Delete the opened file by
	// handle, so a later runner checkpoint cannot be deleted by a path race.
	Handle file{ CreateFileW(path.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr) };
	if (file.value == INVALID_HANDLE_VALUE) {
		const auto error = GetLastError();
		if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND && error != ERROR_SHARING_VIOLATION)
			Log("SkyCRPG_TestProbe: request open failed; left for retry\n");
		return false;
	}
	LARGE_INTEGER length{};
	if (!GetFileSizeEx(file.value, &length) || length.QuadPart < 0 ||
		length.QuadPart > static_cast<LONGLONG>(json::kMaxRequestBytes)) {
		Log("SkyCRPG_TestProbe: request size invalid or exceeds 65536 bytes; left on disk\n");
		return false;
	}
	std::array<char, json::kMaxRequestBytes> buffer{};
	DWORD bytesRead{};
	const auto size = static_cast<DWORD>(length.QuadPart);
	if (!ReadFile(file.value, buffer.data(), size, &bytesRead, nullptr) || bytesRead != size) {
		Log("SkyCRPG_TestProbe: request read failed; left for retry\n");
		return false;
	}
	const auto tag = json::ParseRequest(std::string_view(buffer.data(), bytesRead));
	if (!tag) return false;
	const auto perf = json::ParsePerfRequest(*tag);
	if (perf.command == json::PerfCommand::checkpoint) {
		if (!QueueSnapshot(true, *tag, true)) return false;
	} else if (!QueueProbeTask([requestTag = *tag] { PerfRequest(requestTag); })) return false;
	FILE_DISPOSITION_INFO disposition{};
	disposition.DeleteFile = TRUE;
	if (!SetFileInformationByHandle(file.value, FileDispositionInfo, &disposition, static_cast<DWORD>(sizeof(disposition))))
		Log("SkyCRPG_TestProbe: request delete failed; response queued but request may repeat\n");
	return true;
}

void SampleMemory()
{
	PROCESS_MEMORY_COUNTERS_EX counters{};
	counters.cb = static_cast<DWORD>(sizeof(counters));
	if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), counters.cb)) {
		Emit("process_memory", json::Object{}.Numeric("private_bytes", static_cast<std::uint64_t>(counters.PrivateUsage))
			.Numeric("working_set_bytes", static_cast<std::uint64_t>(counters.WorkingSetSize)));
	} else if (!g_probe->memoryFailureLogged.exchange(true)) {
		Log("SkyCRPG_TestProbe: process memory sample unavailable\n");
	}
}

void CALLBACK TimerTick(void*, BOOLEAN) noexcept
{
	if (g_probe->timerBusy.exchange(true)) return;
	try {
		if (g_probe->loaded.load()) SampleMemory();
		if (!g_probe->timerPending.load() && !PollRequest() && g_probe->loaded.load())
			QueueSnapshot(false, {}, true);
	} catch (...) {
		Log("SkyCRPG_TestProbe: timer poll failed; skipped\n");
	}
	g_probe->timerBusy.store(false);
}

void Equip(std::uint32_t formID, bool equipped, std::uint64_t generation)
{
	if (!InTaskDrain() || generation != g_probe->generation.load()) {
		Log("SkyCRPG_TestProbe: stale/out-of-drain equip observation skipped\n");
		return;
	}
	json::Object fields;
	fields.String("form_id", json::FormID(formID)).Boolean("equipped", equipped);
	NameField(fields, RE::TESForm::LookupByID(formID));
	Emit("equip", fields);
}

void Container(std::uint32_t oldID, std::uint32_t newID, std::uint32_t formID,
	std::int32_t count, bool onEventThread, std::uint64_t generation)
{
	if (!InTaskDrain() || generation != g_probe->generation.load()) {
		Log("SkyCRPG_TestProbe: stale/out-of-drain container observation skipped\n");
		return;
	}
	const bool incoming = newID == kPlayerFormID;
	const bool outgoing = oldID == kPlayerFormID;
	const auto delta = (incoming ? static_cast<std::int64_t>(count) : 0) - (outgoing ? static_cast<std::int64_t>(count) : 0);
	json::Object fields;
	fields.String("form_id", json::FormID(formID)).Numeric("count_delta", delta)
		.Boolean("player_in", incoming).Boolean("player_out", outgoing)
		.String("old_container", json::FormID(oldID)).String("new_container", json::FormID(newID));
	auto* form = RE::TESForm::LookupByID(formID);
	NameField(fields, form);
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (onEventThread && g_probe->loaded.load() && form && player && player->GetInventoryChanges(true)) {
		// a_noInit=true is essential: default inventory reads can initialize game
		// inventory. Filter to this base object; never GetDisplayName/extra data.
		const auto counts = player->GetInventoryCounts([formID](RE::TESBoundObject& object) {
			return object.GetFormID() == formID;
		}, true);
		std::int32_t current = 0;
		for (const auto& [object, itemCount] : counts) {
			if (object && object->GetFormID() == formID) current = itemCount;
		}
		fields.Numeric("new_count", current).String("count_status", "event_thread");
	} else {
		fields.Null("new_count").String("count_status", onEventThread ? "inventory_unavailable" : "off_thread");
	}
	Emit("container", fields);
}

class Events final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
	public RE::BSTEventSink<RE::TESEquipEvent>, public RE::BSTEventSink<RE::TESContainerChangedEvent> {
public:
	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*) noexcept override
	{
		try {
			if (!event) { Log("SkyCRPG_TestProbe: null menu event\n"); return RE::BSEventNotifyControl::kContinue; }
			// Capture the open endpoint before JSON/log I/O on this callback.
			if (event->menuName == "InventoryMenu") InventoryMenuEvent(event->opening, PerfNow());
			ThreadContext("menu");
			Emit("menu", json::Object{}.String("name", event->menuName.c_str()).Boolean("opening", event->opening));
			if (!event->opening) QueueSnapshot();
		} catch (...) { Log("SkyCRPG_TestProbe: menu event failed; skipped\n"); }
		return RE::BSEventNotifyControl::kContinue;
	}
	RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event,
		RE::BSTEventSource<RE::TESEquipEvent>*) noexcept override
	{
		try {
			if (!event || !event->actor) { Log("SkyCRPG_TestProbe: null equip event/actor\n"); return RE::BSEventNotifyControl::kContinue; }
			if (!event->actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
			const auto generation = g_probe->generation.load();
			const auto formID = event->baseObject;
			const bool equipped = event->equipped;
			if (!QueueProbeTask([formID, equipped, generation] { Equip(formID, equipped, generation); }))
				Log("SkyCRPG_TestProbe: no task interface for equip event\n");
		} catch (...) { Log("SkyCRPG_TestProbe: equip event failed; skipped\n"); }
		return RE::BSEventNotifyControl::kContinue;
	}
	RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,
		RE::BSTEventSource<RE::TESContainerChangedEvent>*) noexcept override
	{
		try {
			if (!event) { Log("SkyCRPG_TestProbe: null container event\n"); return RE::BSEventNotifyControl::kContinue; }
			if (event->oldContainer != kPlayerFormID && event->newContainer != kPlayerFormID) return RE::BSEventNotifyControl::kContinue;
			const auto oldID = event->oldContainer;
			const auto newID = event->newContainer;
			const auto formID = event->baseObj;
			const auto count = event->itemCount;
			const auto generation = g_probe->generation.load();
			// Inventory counts stay off: the drained task runs after the event, so
			// its count would not be the immediate post-event value.
			if (!QueueProbeTask([oldID, newID, formID, count, generation] { Container(oldID, newID, formID, count, false, generation); }))
				Log("SkyCRPG_TestProbe: no task interface for container event\n");
		} catch (...) { Log("SkyCRPG_TestProbe: container event failed; skipped\n"); }
		return RE::BSEventNotifyControl::kContinue;
	}
};

void ResetPerformance()
{
	std::scoped_lock lock(g_probe->perfMutex);
	g_probe->frameChain.stop();
	g_probe->inventory = {};
	g_probe->hasInventoryKey = false;
	g_probe->inventoryMenuOpen = false;
}

void Messages(SKSE::MessagingInterface::Message* message) noexcept
{
	try {
		if (!message) { Log("SkyCRPG_TestProbe: null SKSE message\n"); return; }
		switch (message->type) {
		case SKSE::MessagingInterface::kDataLoaded: {
			CaptureMainThread();
			ThreadContext("data_loaded");
			Emit("data_loaded");
			static auto* events = new Events();
			static auto* inputs = new InputEvents();
			static bool registered = false;
			if (registered) return;
			registered = true;
			if (auto* input = RE::BSInputDeviceManager::GetSingleton()) input->AddEventSink(inputs);
			else Log("SkyCRPG_TestProbe: input event source unavailable\n");
			if (auto* ui = RE::UI::GetSingleton()) ui->AddEventSink<RE::MenuOpenCloseEvent>(events);
			else Log("SkyCRPG_TestProbe: menu event source unavailable\n");
			if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) {
				source->AddEventSink<RE::TESEquipEvent>(events);
				source->AddEventSink<RE::TESContainerChangedEvent>(events);
			} else Log("SkyCRPG_TestProbe: script event source unavailable\n");
			if (g_probe->interval != 0 && !CreateTimerQueueTimer(&g_probe->timer, nullptr, TimerTick, nullptr,
				g_probe->interval, g_probe->interval, WT_EXECUTEDEFAULT))
				Log("SkyCRPG_TestProbe: timer creation failed; event snapshots remain available\n");
			break;
		}
		case SKSE::MessagingInterface::kPreLoadGame: {
			g_probe->loaded.store(false);
			g_probe->generation.fetch_add(1);
			ResetPerformance();
			ThreadContext("pre_load_game");
			std::scoped_lock lock(g_probe->lifecycleMutex);
			g_probe->hasSaveName = message->data && message->dataLen <= json::kMaxRequestBytes;
			g_probe->saveName.clear();
			// SKSE supplies a filename and its byte length (not including NUL).
			if (g_probe->hasSaveName) g_probe->saveName.assign(static_cast<const char*>(message->data), message->dataLen);
			break;
		}
		case SKSE::MessagingInterface::kPostLoadGame: {
			ThreadContext("post_load_game");
			// SKSE encodes success in the pointer value (0/1), not a bool*.
			const bool success = message->data != nullptr;
			g_probe->loaded.store(success);
			json::Object fields;
			fields.Boolean("success", success);
			{
				std::scoped_lock lock(g_probe->lifecycleMutex);
				if (g_probe->hasSaveName) fields.String("save_name", g_probe->saveName);
				else fields.Null("save_name");
			}
			Emit("game_loaded", fields);
			QueueSnapshot();
			// Diagnostic only: identify the old task-pool executor in the next
			// JSONL without reading player/UI/camera on that thread. Enqueue it
			// after lifecycle work, so a diagnostic allocation cannot skip load.
			if (const auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] {
				ThreadContext("skse_task_post_load");
			});
			break;
		}
		case SKSE::MessagingInterface::kNewGame: {
			ResetPerformance();
			ThreadContext("new_game");
			g_probe->generation.fetch_add(1);
			g_probe->loaded.store(true);
			{
				std::scoped_lock lock(g_probe->lifecycleMutex);
				g_probe->hasSaveName = false;
				g_probe->saveName.clear();
			}
			Emit("new_game");
			QueueSnapshot();
			break;
		}
		default: break;
		}
	} catch (...) { Log("SkyCRPG_TestProbe: SKSE message failed; skipped\n"); }
}

// INI value narrowed to ASCII digits; any other character becomes '?' so the
// strict parsers reject it instead of reading a partial number.
std::array<char, 32> ReadDigits(const std::wstring& ini, const wchar_t* key, const wchar_t* fallback)
{
	std::array<wchar_t, 32> wide{};
	std::array<char, 32> narrow{};
	GetPrivateProfileStringW(L"Probe", key, fallback, wide.data(), static_cast<DWORD>(wide.size()), ini.c_str());
	for (std::size_t index = 0; index + 1 < narrow.size(); ++index) {
		const auto character = wide[index];
		if (character == 0) break;
		narrow[index] = character >= L'0' && character <= L'9' ? static_cast<char>(character) : '?';
	}
	return narrow;
}

std::string Version(std::uint32_t version)
{
	return json::Number(version >> 24) + '.' + json::Number((version >> 16) & 0xFF) + '.' +
		json::Number((version >> 4) & 0xFFF) + '.' + json::Number(version & 0xF);
}
}  // namespace

extern "C" __declspec(dllexport) constinit skse::PluginVersionData SKSEPlugin_Version{
	.dataVersion = skse::PluginVersionData::kVersion,
	.pluginVersion = (SKYCRPG_VERSION_MAJOR << 24) | (SKYCRPG_VERSION_MINOR << 16) | (SKYCRPG_VERSION_PATCH << 4),
	.name = "SkyCRPG TestProbe",
	.author = "SkyCRPG",
	.supportEmail = "",
	.versionIndependenceEx = 0,
	.versionIndependence = 0,
	.compatibleVersions = { skse::kRuntime_1_7_104, 0 },
	.seVersionRequired = skse::kSKSE_2_3_1,
};

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const skse::Interface* skseInterface) noexcept
{
	try {
		if (!skseInterface || skseInterface->isEditor || skseInterface->runtimeVersion != skse::kRuntime_1_7_104 ||
			skseInterface->skseVersion != skse::kSKSE_2_3_1) return false;
		const auto ini = skycrpg::ThisModuleDirectory() + L"SkyCRPG_TestProbe.ini";
		// Opt-in only: without an explicit bEnabled=1 the probe registers nothing,
		// starts no timer and writes no JSONL or log, even if installed by mistake.
		if (!json::ProbeEnabled(ReadDigits(ini, L"bEnabled", L"").data())) return true;
		g_probe = new Probe();
		if (!QueryPerformanceFrequency(&g_probe->qpcFrequency) || g_probe->qpcFrequency.QuadPart <= 0) return false;
		g_probe->directory = skycrpg::LogDirectory();
		if (g_probe->directory.empty()) { Log("SkyCRPG_TestProbe: SKSE directory unavailable\n"); return false; }
		g_probe->interval = json::ParseInterval(ReadDigits(ini, L"iStateIntervalMs", L"1000").data());
		const auto fields = json::Object{}.String("runtime", Version(skseInterface->runtimeVersion))
			.String("skse", Version(skseInterface->skseVersion)).String("probe_version", SKYCRPG_VERSION)
			.String("snapshot_dispatch", "ui_task_drain_v3").Numeric("perf_protocol", 1).Numeric("observation_protocol", json::kObservationProtocol)
			.String("build_commit", SKYCRPG_SOURCE_COMMIT).Boolean("enabled", true).Numeric("state_interval_ms", g_probe->interval);
		if (!PinnedAddressLibrary()) {
			Log("SkyCRPG_TestProbe: pinned Address Library 13 missing/unreadable/mismatched; CommonLib Init skipped\n");
			return false;
		}
		SKSE::Init(reinterpret_cast<const SKSE::LoadInterface*>(skseInterface), false);
		const auto* tasks = SKSE::GetTaskInterface();
		const auto* messaging = SKSE::GetMessagingInterface();
		if (!tasks || !messaging || !messaging->RegisterListener("SKSE", Messages)) {
			Log("SkyCRPG_TestProbe: task/messaging registration failed\n");
			return false;
		}
		Emit("probe_loaded", fields);
		Log("SkyCRPG_TestProbe: enabled; read-only observer, never shipped\n");
		return true;
	} catch (...) {
		Log("SkyCRPG_TestProbe: initialization failed\n");
		return false;
	}
}
