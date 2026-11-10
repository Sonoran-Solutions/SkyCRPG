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
#include <RE/M/MenuControls.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/PlayerControls.h>
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
// P5.5-01 view_sample: camera, Havok ray and scene-graph reads (README "View samples").
#include <RE/B/BSAtomic.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSShaderProperty.h>
#include <RE/B/bhkWorld.h>
#include <RE/H/hkpCollidable.h>
#include <RE/H/hkpRigidBody.h>  // hkpWorld.h names hkpRigidBody without declaring it
#include <RE/H/hkpWorld.h>
#include <RE/H/hkpWorldRayCastInput.h>
#include <RE/H/hkpWorldRayCastOutput.h>
#include <RE/N/NiAlphaProperty.h>
#include <RE/N/NiCamera.h>
#include <RE/N/NiNode.h>
#include <RE/T/TESHavokUtilities.h>

// CommonLib/REX headers must precede the Windows headers in the skeleton.
#include <skycrpg/skeleton.h>
#include "json_line.h"
#include "probe_config.h"
#include "snapshot_policy.h"
#include "observation_policy.h"
#include "perf_policy.h"
#include "proton_policy.h"
#include "cursor_policy.h"
#include "magic_policy.h"
#include "view_policy.h"

#include <bcrypt.h>
#include <psapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace json = skycrpg::testprobe;
constexpr std::uint32_t kPlayerFormID = 0x00000014;
using Clock = std::chrono::steady_clock;

// view_sample bone cache (PlayerBones); lives in the never-destroyed Probe.
struct ViewBoneCache {
	RE::NiPointer<RE::NiAVObject> root;
	std::array<RE::NiPointer<RE::NiAVObject>, 4> nodes;
	std::uint64_t generation{};
	bool capped{}, valid{};
};

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
	json::InputDiagnostics inputDiagnostics;
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
	std::atomic_bool commitWalkFailureLogged{ false };
	std::uint32_t interval{ 1000 };
	HANDLE timer{};
	// view_sample state. Settings are read once before any sink or timer.
	json::ViewSettings view;
	std::mutex viewMutex;                  // cadence, seq, since-last and emission order
	json::ViewCadence viewCadence;
	std::uint64_t viewSeq{};
	double viewLastMs{ -1 };
	std::uint32_t viewLastFrame{};
	bool viewHasFrame{};
	std::atomic_uint32_t viewInFlight{ 0 };
	std::atomic_int viewBinaryGuard{ 0 };  // 0 unchecked, 1 pass, 2 fail
	std::atomic_uint32_t viewReasonsLogged{ 0 };
	std::atomic<std::uintptr_t> frameCounter{ 0 };  // ID 411489, set by a passing binary guard
	std::mutex viewBonesMutex;                       // viewBones and viewRetired
	ViewBoneCache viewBones;
	std::vector<RE::NiPointer<RE::NiAVObject>> viewRetired;
	HANDLE viewTimer{};
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
void CursorFields(json::Object& inventory, RE::GFxMovieView* movie, RE::GFxValue& itemList, RE::GFxValue& selected, bool gridActive)
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
	// Local painted rectangle, from skycrpgPaintCell; skycrpgGridBounds explains inflated native children.
	constexpr double cellWidth = 165.0, cellHeight = 68.0;
	double coords[4]{};
	bool mapped = hasRoot && hasClip && gridActive;
	for (int i = 0; i < 2 && mapped; ++i) {
		RE::GFxValue point, px, py;
		movie->CreateObject(&point);
		px.SetNumber(i == 0 ? 0.0 : cellWidth);
		py.SetNumber(i == 0 ? 0.0 : cellHeight);
		std::array<RE::GFxValue, 1> pointArg;
		pointArg[0] = point;
		mapped = point.IsObject() && point.SetMember("x", px) && point.SetMember("y", py) &&
			clip.Invoke("localToGlobal", nullptr, pointArg) && root.Invoke("globalToLocal", nullptr, pointArg) &&
			point.GetMember("x", &px) && px.IsNumber() && point.GetMember("y", &py) && py.IsNumber();
		if (mapped) { coords[i * 2] = px.GetNumber(); coords[i * 2 + 1] = py.GetNumber(); }
	}
	const auto cell = json::SelectedCell(gridActive, hasClip, mapped, coords[0], coords[1], coords[2], coords[3]);
	if (cell) {
		json::Object box;
		box.Numeric("x_min", cell->x_min).Numeric("x_max", cell->x_max).Numeric("y_min", cell->y_min).Numeric("y_max", cell->y_max);
		cursor.Raw("selected_cell", box.Json());
	} else cursor.Null("selected_cell");
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
	if (!ui || !ui->IsMenuOpen("MagicMenu")) fields.Null("magic");
	else {
		const auto magicMovie = ui->GetMovieView("MagicMenu");
		RE::GFxValue magicList, magicSelected, magicName, magicFlag;
		json::Object magic;
		if (Variable(magicMovie.get(), "_root.Menu_mc.inventoryLists.itemList", magicList) &&
			magicList.GetMember("selectedEntry", &magicSelected) && magicSelected.IsObject()) {
			if (magicSelected.GetMember("text", &magicName) && magicName.IsString()) magic.String("selected_name", magicName.GetString());
			else magic.Null("selected_name");
			std::optional<double> filterFlag;
			if (magicSelected.GetMember("filterFlag", &magicFlag) && magicFlag.IsNumber()) {
				filterFlag = magicFlag.GetNumber();
				magic.Numeric("selected_filter_flag", *filterFlag);
			} else magic.Null("selected_filter_flag");
			const auto favorite = json::MagicFavorite(filterFlag);
			if (favorite) magic.Boolean("selected_favorite", *favorite);
			else magic.Null("selected_favorite");
		} else magic.Null("selected_name").Null("selected_filter_flag").Null("selected_favorite");
		fields.Raw("magic", magic.Json());
	}
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
	CursorFields(inventory, movie.get(), itemList, selected, hasGrid && grid.IsBool() && grid.GetBool());
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
		.String("phase", "abort").Boolean("available", false).String("reason", reason)
		.Numeric("input_buttons_seen", g_probe->inputDiagnostics.buttons)
		.Numeric("input_downs_seen", g_probe->inputDiagnostics.downs)
		.Numeric("input_accepted", g_probe->inputDiagnostics.accepted)
		.Numeric("input_diag_count", g_probe->inputDiagnostics.count));
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

void EmitInputDiagnostics(const json::InputDiagnostics& diagnostics, std::string_view label)
{
	for (std::size_t i = diagnostics.emitted; i < diagnostics.count; ++i) {
		const auto& d = diagnostics.records[i];
		Emit("input_diag", json::Object{}.String("label", label)
			.Numeric("device", d.device).Numeric("idCode", d.idCode)
			.Numeric("value", d.value).Numeric("heldDownSecs", d.heldDownSecs)
			.String("userEvent", d.userEvent.data()).Boolean("userEvent_truncated", d.userEventTruncated)
			.Numeric("inventory_key", d.inventoryKey).Numeric("quick_inventory_key", d.quickInventoryKey).Numeric("tween_key", d.tweenKey)
			.Numeric("qpc_ms", d.qpcMs).Boolean("down", d.down).Boolean("mapped", d.mapped)
			.Boolean("inventory_menu_open", d.menuOpen).Boolean("capture_active", true).Boolean("accepted", d.accepted));
	}
}

void FlushInputDiagnostics()
{
	json::InputDiagnostics diagnostics;
	std::string label;
	{
		std::scoped_lock lock(g_probe->perfMutex);
		if (g_probe->inputDiagnostics.emitted == g_probe->inputDiagnostics.count) return;
		diagnostics = g_probe->inputDiagnostics;
		label = g_probe->captureLabel;
		g_probe->inputDiagnostics.emitted = diagnostics.count;
	}
	EmitInputDiagnostics(diagnostics, label);
}

void PerfRequest(const std::string& tag)
{
	const auto request = json::ParsePerfRequest(tag);
	std::unique_lock lock(g_probe->perfMutex);
	bool queueUI = false;
	bool available = false;
	json::InputDiagnostics diagnostics;
	std::string diagnosticLabel;
	if (request.command == json::PerfCommand::start && !g_probe->frameChain.active() && g_probe->loaded.load()) {
		g_probe->captureLabel = request.label;
		g_probe->inputDiagnostics = {};
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
		diagnostics = g_probe->inputDiagnostics;
		g_probe->inputDiagnostics.emitted = diagnostics.count;
		diagnosticLabel = g_probe->captureLabel;
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
	lock.unlock();
	// The timer drains during capture; stop writes any remaining records.
	// No input callback formats JSON or writes output, or holds an engine pointer.
	EmitInputDiagnostics(diagnostics, diagnosticLabel);
	json::Object acknowledgement;
	acknowledgement.String("tag", tag).String("label", request.label)
		.String("phase", request.command == json::PerfCommand::start ? "start" : "stop").Boolean("available", available);
	if (available && request.command == json::PerfCommand::stop)
		acknowledgement.Numeric("input_buttons_seen", diagnostics.buttons).Numeric("input_downs_seen", diagnostics.downs)
			.Numeric("input_accepted", diagnostics.accepted).Numeric("input_diag_count", diagnostics.count);
	Emit("perf_capture", acknowledgement);
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

std::array<std::uint32_t, 3> InventoryMappings(const RE::ControlMap* controls, RE::INPUT_DEVICE device) noexcept
{
	std::array<std::uint32_t, 3> keys{ RE::ControlMap::kInvalid, RE::ControlMap::kInvalid, RE::ControlMap::kInvalid };
	if (!controls || static_cast<std::int32_t>(device) < 0 || device >= RE::INPUT_DEVICE::kTotal) return keys;
	const auto* context = controls->controlMap[RE::ControlMap::InputContextID::kGameplay];
	if (!context) return keys;
	std::array<bool, 3> found{};
	constexpr std::array<std::string_view, 3> names{ "Inventory", "Quick Inventory", "Tween Menu" };
	// Same first-match/context/device lookup as GetMappedKey, without its
	// temporary BSFixedString intern operation on the input hot path.
	for (const auto& mapping : context->deviceMappings[device]) {
		const std::string_view name{ mapping.eventID.c_str(), mapping.eventID.size() };
		for (std::size_t i = 0; i < names.size(); ++i)
			if (!found[i] && name == names[i]) { keys[i] = mapping.inputKey; found[i] = true; }
		if (found[0] && found[1] && found[2]) break;
	}
	return keys;
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
				if (!button) continue;
				json::InputDiagnostic d;
				d.qpcMs = PerfNow();  // precedes mapping, sorting and all output
				const auto device = event->GetDevice();
				d.device = static_cast<std::int32_t>(device);
				d.idCode = button->GetIDCode();
				d.value = button->Value();
				d.heldDownSecs = button->HeldDuration();
				d.down = button->IsDown();
				const auto keys = InventoryMappings(RE::ControlMap::GetSingleton(), device);
				d.inventoryKey = keys[0];
				d.quickInventoryKey = keys[1];
				d.tweenKey = keys[2];
				d.mapped = json::AcceptInventoryKey(true, false, true, d.idCode, keys, RE::ControlMap::kInvalid);
				std::scoped_lock lock(g_probe->perfMutex);
				if (!g_probe->frameChain.active()) continue;
				d.menuOpen = g_probe->inventoryMenuOpen;
				d.accepted = json::AcceptInventoryKey(true, d.menuOpen, d.down, d.idCode, keys, RE::ControlMap::kInvalid);
				if (d.accepted) {
					g_probe->inventoryKeyMs = d.qpcMs;
					g_probe->preOpenMedian = g_probe->frames.summary().median;
					g_probe->hasInventoryKey = true;
				}
				g_probe->inputDiagnostics.observe(d, button->GetUserEvent().c_str());
			}
		} catch (...) { Log("SkyCRPG_TestProbe: input timestamp failed\n"); }
		return RE::BSEventNotifyControl::kContinue;
	}
};

void RegisterInputObserver(InputEvents* observer)
{
	auto* input = RE::BSInputDeviceManager::GetSingleton();
	if (!input) { Log("SkyCRPG_TestProbe: input event source unavailable\n"); return; }
	// Verified CommonLib 7.2.0 API, not an assumed input-initialization order.
	input->PrependEventSink(observer);
	auto* menu = static_cast<RE::BSTEventSink<RE::InputEvent*>*>(RE::MenuControls::GetSingleton());
	auto* player = static_cast<RE::BSTEventSink<RE::InputEvent*>*>(RE::PlayerControls::GetSingleton());
	std::int64_t ownIndex = -1, menuIndex = -1, playerIndex = -1, count = 0;
	bool notifying = false;
	{
		RE::BSSpinLockGuard lock(input->lock);
		notifying = input->notifying;
		for (const auto* sink : input->sinks) {
			if (sink == observer) ownIndex = count;
			if (sink == menu) menuIndex = count;
			if (sink == player) playerIndex = count;
			++count;
		}
	}
	// Once per process; scalar indices only, no retained engine pointers.
	Emit("input_registration", json::Object{}.String("method", "PrependEventSink")
		.Numeric("sink_count", count).Numeric("probe_index", ownIndex)
		.Numeric("menu_controls_index", menuIndex).Numeric("player_controls_index", playerIndex)
		.Boolean("notifying", notifying));
}

// ---- P5.5-01 view_sample (view_policy.h; README "View samples") -----------
// Dispatch is the player_state UI task drain. During unpaused play SKSE's
// drain (hook at 82082+0xAB5) runs inside the main job list's "UI" job
// (ID 39042, job table .data 0x20B4EF0) on a worker thread; around menu
// transitions and while paused Main::Update (ID 36564) runs it on
// Main::threadID. Havok reads follow bhkWorld::PickObject (ID 77860): the
// world read lock (bhkWorld+0xC598) around hkpWorld::castRay (ID 61399), with
// each hit collidable resolved before the lock is released. LockForWrite
// (68234) only CASes the count from 0 and sets no pending bit, so readers can
// delay writers but a nested read cannot deadlock; the probe still never holds
// the lock across an engine call that takes it again. A sample runs while
// SKSE holds its UI queue lock and the main thread waits for the UI job, so
// its cost (sample_ms) adds to that frame. With bViewMainThreadOnly=1 a drain
// on another thread is retried through the main task queue (whose helper only
// re-queues) and otherwise refused.
struct ViewJob {
	bool requested{}, valid{ true }, timer{};
	std::string tag;
	std::uint64_t generation{};
	double queuedMs{}, startedMs{};
	std::uint32_t hops{}, coalesced{};
};

// Entry bytes and RVAs read from the pinned SkyrimSE.exe 1.7.104 (SHA-256
// 846efccf...) through Address Library 13. Any other live entry (a different
// binary or another mod's detour) makes every view sample guard_binary.
struct ViewGuardBytes {
	std::uint64_t id;
	std::uintptr_t rva;
	const char* bytes;
	std::size_t size;
};
constexpr std::array<ViewGuardBytes, 7> kViewGuards{ {
	{ 61399, 0xB598C0, "\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x57\x48\x81\xec\x80\x00\x00\x00", 18 },  // hkpWorld::castRay
	{ 26003, 0x404740, "\x40\x57\x48\x83\xec\x30\x48\xc7\x44\x24\x20\xfe\xff\xff\xff", 15 },  // FindCollidableRef
	{ 15870, 0x2236D0, "\x48\x83\xec\x38\x48\x85\xc9\x74\x79\x8b\x41\x2c", 12 },  // FindCollidableObject
	{ 36609, 0x65C480, "\x48\x8b\x05\xb9\x42\xbd\x02\x48\x8b\x80\x28\x01\x00\x00\xc3", 15 },  // Main::WorldRootCamera
	{ 18995, 0x2BE9B0, "\xf6\x41\x40\x01\x74\x09\x48\x83\xc1\x48", 10 },  // TESObjectCELL::GetbhkWorld
	{ 68233, 0xCE3250, "\x40\x57\x48\x83\xec\x20\x48\x8b\xf9", 9 },  // BSReadWriteLock::LockForRead
	{ 68239, 0xCE3510, "\xf0\xff\x49\x04\xc3", 5 },  // BSReadWriteLock::UnlockForRead
} };
constexpr std::uint64_t kFrameCounterID = 411489;     // label only; sole writer inc at RVA 100A02B
constexpr std::uintptr_t kFrameCounterRVA = 0x33363F4;
constexpr std::uint32_t kThirdPersonState = 9;        // PlayerCamera state id of normal play
// Plain members read by the view sample, each checked in the binary (evidence doc).
static_assert(offsetof(RE::NiObjectNET, name) == 0x10);
static_assert(offsetof(RE::NiAVObject, parent) == 0x30);
static_assert(offsetof(RE::NiAVObject, world) == 0x7C);
static_assert(offsetof(RE::NiAlphaProperty, alphaFlags) == 0x30);
static_assert(offsetof(RE::BSShaderProperty, material) == 0x78);
static_assert(offsetof(RE::BSLightingShaderMaterialBase, materialAlpha) == 0x80);
static_assert(offsetof(RE::BSGeometry::GEOMETRY_RUNTIME_DATA, alphaProperty) == 0);
static_assert(offsetof(RE::BSGeometry::GEOMETRY_RUNTIME_DATA, shaderProperty) == 8);
static_assert(offsetof(RE::NiCamera::RUNTIME_DATA2, viewFrustum) == 0);
static_assert(offsetof(RE::NiCamera::RUNTIME_DATA2, port) == 0x24);
static_assert(offsetof(RE::bhkWorld, worldLock) == 0xC598);
static_assert(offsetof(RE::hkpWorldRayCastInput, filterInfo) == 0x24);
static_assert(offsetof(RE::hkpShapeRayCastCollectorOutput, hitFraction) == 0x10);
static_assert(offsetof(RE::hkpWorldRayCastOutput, rootCollidable) == 0x50);
static_assert(sizeof(RE::hkpWorldRayCastInput) == 0x30 && sizeof(RE::hkpWorldRayCastOutput) == 0x60);
static_assert(offsetof(RE::hkpCollidable, broadPhaseHandle) == 0x24);
static_assert(offsetof(RE::hkpTypedBroadPhaseHandle, collisionFilterInfo) == 0x8);  // filter info at +0x2C (5577D3)
static_assert(offsetof(RE::TESCamera, cameraRoot) == 0x20);
// The NiCamera and PlayerCamera accessors are this + constant by construction
// (CommonLib NiCamera.h 0x110/0x150, PlayerCamera.h 0x13C); those constants
// match the binary audit. In a flat build the members exist directly and are
// checked here as well.
#if defined(EXCLUSIVE_SKYRIM_FLAT)
static_assert(offsetof(RE::NiCamera, worldToCam) == 0x110);
static_assert(offsetof(RE::NiCamera, viewFrustum) == 0x150);
static_assert(offsetof(RE::NiCamera, port) == 0x174);
static_assert(offsetof(RE::PlayerCamera, worldFOV) == 0x13C);
#endif

// One log line per distinct refusal code per process.
void LogViewReason(std::string_view reason) noexcept
{
	static constexpr std::array<std::string_view, 17> kReasons{ "invalid_view_request", "view_busy", "enqueue_failed",
		"outside_task_drain", "generation_changed", "not_loaded", "off_main_thread", "player_unavailable", "game_paused",
		"camera_unavailable", "camera_state", "guard_binary", "world_camera_unavailable", "cell_unavailable",
		"world_changed", "sample_failed", "periodic_opt_in_missing" };
	std::uint32_t bit = 31;
	for (std::uint32_t index = 0; index < kReasons.size(); ++index)
		if (kReasons[index] == reason) bit = index;
	const auto mask = 1u << bit;
	if (g_probe->viewReasonsLogged.fetch_or(mask) & mask) return;
	try {
		const auto line = "SkyCRPG_TestProbe: view_sample unavailable, reason " + std::string(reason) + " (logged once)\n";
		Log(line.c_str());
	} catch (...) { Log("SkyCRPG_TestProbe: view_sample unavailable (reason not formatted)\n"); }
}

void OptionalNumber(json::Object& fields, std::string_view key, std::optional<double> value)
{
	if (value) fields.Numeric(key, *value);
	else fields.Null(key);
}

std::optional<std::uint32_t> ReadFrame() noexcept
{
	const auto address = g_probe->frameCounter.load();
	if (!address) return std::nullopt;
	return *reinterpret_cast<const volatile std::uint32_t*>(address);
}

// Every started job reaches exactly one FinishView, which releases its
// in-flight reservation and, for a periodic job, the cadence slot.
void FinishView(const ViewJob& job, std::string_view reason, const json::Object* body = nullptr,
	std::optional<std::uint32_t> frame = std::nullopt) noexcept
{
	const bool available = reason.empty();
	if (!available) LogViewReason(reason);
	try {
		json::Object fields;
		std::scoped_lock lock(g_probe->viewMutex);
		const double now = PerfNow();
		fields.Numeric("view_protocol", json::kViewProtocol).Numeric("seq", ++g_probe->viewSeq)
			.Boolean("requested", job.requested);
		if (job.requested) fields.String("tag", job.tag);
		fields.Boolean("available", available);
		if (available) fields.Null("reason");
		else fields.String("reason", reason);
		if (frame) fields.Numeric("frame", *frame);
		else fields.Null("frame");
		if (g_probe->viewLastMs >= 0) fields.Numeric("since_last_ms", now - g_probe->viewLastMs);
		else fields.Null("since_last_ms");
		if (frame && g_probe->viewHasFrame) fields.Numeric("since_last_frames", *frame - g_probe->viewLastFrame);
		else fields.Null("since_last_frames");
		fields.Numeric("coalesced_ticks", job.coalesced).Numeric("hops", job.hops).Numeric("queued_ms", now - job.queuedMs);
		if (job.startedMs > 0) fields.Numeric("sample_ms", now - job.startedMs);
		else fields.Null("sample_ms");
		ThreadFields(fields);
		if (body) fields.Append(*body);
		g_probe->viewLastMs = now;
		if (frame) {
			g_probe->viewLastFrame = *frame;
			g_probe->viewHasFrame = true;
		}
		Emit("view_sample", fields);
	} catch (...) { Log("SkyCRPG_TestProbe: view_sample formatting failed\n"); }
	if (job.timer) {
		try {
			std::scoped_lock lock(g_probe->viewMutex);
			g_probe->viewCadence.done();
		} catch (...) { Log("SkyCRPG_TestProbe: view cadence release failed\n"); }
	}
	g_probe->viewInFlight.fetch_sub(1);
}

bool ViewBinaryGuard() noexcept
{
	const int state = g_probe->viewBinaryGuard.load();
	if (state != 0) return state == 1;
	bool pass = true;
	try {
		const auto base = REL::Module::get().base();
		for (const auto& guard : kViewGuards) {
			const auto address = REL::ID(guard.id).address();
			if (address == base + guard.rva &&
				std::memcmp(reinterpret_cast<const void*>(address), guard.bytes, guard.size) == 0) continue;
			pass = false;
			std::array<char, 128> line{};
			std::snprintf(line.data(), line.size(), "SkyCRPG_TestProbe: view guard failed for ID %llu\n",
				static_cast<unsigned long long>(guard.id));
			Log(line.data());
		}
		const auto counter = REL::ID(kFrameCounterID).address();
		if (counter != base + kFrameCounterRVA) {
			pass = false;
			Log("SkyCRPG_TestProbe: view guard failed for frame counter ID 411489\n");
		}
		if (pass) g_probe->frameCounter.store(counter);
	} catch (...) { pass = false; }
	g_probe->viewBinaryGuard.store(pass ? 1 : 2);
	return pass;
}

bool SameNodeName(const char* name, std::string_view expected) noexcept
{
	if (!name) return false;
	std::size_t index = 0;
	for (; name[index] != 0 && index < expected.size(); ++index) {
		auto a = name[index], b = expected[index];
		if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
		if (a != b) return false;
	}
	return index == expected.size() && name[index] == 0;
}

// Head, chest and shoulder bones of the third-person skeleton, found by node
// name (no BSFixedString construction, so no string-pool lock).
constexpr std::array<std::string_view, 4> kBoneNames{ "NPC Head [Head]", "NPC Spine2 [Spn2]",
	"NPC L UpperArm [LUar]", "NPC R UpperArm [RUar]" };
struct BoneSearch {
	std::array<RE::NiPointer<RE::NiAVObject>, 4> nodes;
	bool capped{};
};
BoneSearch FindBones(RE::NiAVObject* root)
{
	BoneSearch result;
	if (!root) return result;
	std::vector<RE::NiPointer<RE::NiAVObject>> stack;
	stack.emplace_back(root);
	std::uint32_t visited = 0, found = 0;
	while (!stack.empty() && found < kBoneNames.size()) {
		if (++visited > json::kViewSkeletonNodes) { result.capped = true; break; }
		RE::NiPointer<RE::NiAVObject> current = std::move(stack.back());
		stack.pop_back();
		const char* name = current->name.c_str();
		for (std::size_t index = 0; index < kBoneNames.size(); ++index) {
			if (result.nodes[index] || !SameNodeName(name, kBoneNames[index])) continue;
			result.nodes[index] = current;
			++found;
		}
		if (auto* node = current->AsNode()) {
			auto& children = node->GetChildren();
			const auto count = (std::min)(children.free_idx(), children.capacity());
			for (std::uint16_t index = 0; index < count; ++index)
				if (auto child = children[index]; child) stack.push_back(std::move(child));
		}
	}
	return result;
}

// The skeleton is searched once per player 3D root and load generation; later
// samples read only the held bones' world.translate, after checking that each
// held bone still hangs under the held root (else the search runs again).
// Holding the root means a replacement root (3D reload, race menu, beast form)
// has a different address. The cache and the retired holds are guarded by
// Probe::viewBonesMutex. kPreLoadGame retires the cache. Superseded holds are
// released on Main::threadID (load messages, or a menu event delivered there)
// because the probe's reference may be the last one; beyond kRetiredCap they
// are released where they are, on the drain thread.
constexpr std::size_t kRetiredCap = 10;

// Caller holds viewBonesMutex. Overflow is returned for release outside the lock.
void RetireBonesLocked(std::vector<RE::NiPointer<RE::NiAVObject>>& overflow)
{
	auto& cache = g_probe->viewBones;
	auto& retired = g_probe->viewRetired;
	if (cache.root) retired.push_back(std::move(cache.root));
	for (auto& node : cache.nodes)
		if (node) retired.push_back(std::move(node));
	cache = {};
	if (retired.size() > kRetiredCap) {
		for (auto& node : retired) overflow.push_back(std::move(node));
		retired.clear();
	}
}

// retireCache: kPreLoadGame drops the previous session's player root and bones
// on any thread; the holds themselves are released only on Main::threadID.
void ReleaseRetiredViewNodes(bool retireCache) noexcept
{
	if (!g_probe->view.enabled) return;  // base behaviour without bViewSamples
	const auto* main = RE::Main::GetSingleton();
	const bool onMain = main && main->threadID == GetCurrentThreadId();
	if (!retireCache && !onMain) return;
	std::vector<RE::NiPointer<RE::NiAVObject>> released;
	try {
		std::scoped_lock lock(g_probe->viewBonesMutex);
		if (retireCache && g_probe->viewBones.valid) RetireBonesLocked(released);
		if (onMain) {
			for (auto& node : g_probe->viewRetired) released.push_back(std::move(node));
			g_probe->viewRetired.clear();
		}
	} catch (...) { Log("SkyCRPG_TestProbe: view bone release failed\n"); }
	if (!onMain && !released.empty()) Log("SkyCRPG_TestProbe: view bone holds released off the main thread (cap reached)\n");
}

// Each held bone must still reach the held root through its parents.
bool BonesAttached(const ViewBoneCache& cache)
{
	for (const auto& bone : cache.nodes) {
		if (!bone) continue;
		const RE::NiAVObject* node = bone.get();
		std::uint32_t links = 0;
		while (node && node != cache.root.get() && links < json::kViewParentLinks) {
			node = node->parent;
			++links;
		}
		if (node != cache.root.get()) return false;
	}
	return true;
}

std::array<std::optional<json::Vec3>, 4> PlayerBones(RE::PlayerCharacter* player, std::uint64_t generation, bool& capped)
{
	std::array<std::optional<json::Vec3>, 4> result;
	std::vector<RE::NiPointer<RE::NiAVObject>> overflow;
	{
		std::scoped_lock lock(g_probe->viewBonesMutex);
		auto& cache = g_probe->viewBones;
		auto* root = player->Get3D(false);
		if (!root) {
			if (cache.valid) RetireBonesLocked(overflow);
		} else {
			const bool fresh = cache.valid && cache.root.get() == root && cache.generation == generation && BonesAttached(cache);
			if (!fresh) {
				if (cache.valid) RetireBonesLocked(overflow);
				auto search = FindBones(root);
				cache.root.reset(root);
				cache.nodes = std::move(search.nodes);
				cache.capped = search.capped;
				cache.generation = generation;
				cache.valid = true;
			}
			capped = cache.capped;
			for (std::size_t index = 0; index < cache.nodes.size(); ++index) {
				if (!cache.nodes[index]) continue;
				const auto& translate = cache.nodes[index]->world.translate;
				result[index] = json::Vec3{ translate.x, translate.y, translate.z };
			}
		}
	}
	if (!overflow.empty()) Log("SkyCRPG_TestProbe: view bone holds released on the drain thread (cap reached)\n");
	return result;
}

// Bounded Havok rays plus per-reference treatment for one sample.
class ViewCaster {
public:
	ViewCaster(RE::TESObjectCELL* cell, RE::TESObjectREFR* player, std::uint32_t maxHits) :
		player_(player), maxHits_(maxHits)
	{
		world_ = cell ? cell->GetbhkWorld() : nullptr;
		havok_ = world_ ? world_->GetWorld1() : nullptr;
		scale_ = RE::bhkWorld::GetWorldScale();
	}
	std::uint32_t casts() const noexcept { return casts_; }

	json::RayRecord Cast(json::Vec3 eye, json::Vec3 target)
	{
		json::RayRecord ray;
		const auto plan = json::PlanRay(eye, target);
		if (!plan) { ray.reason = "invalid_geometry"; return ray; }
		if (!world_ || !havok_ || !std::isfinite(scale_) || scale_ <= 0) { ray.reason = "no_havok_world"; return ray; }
		const json::Vec3 direction = (plan->to - plan->from) * (1.0 / plan->length);
		json::Vec3 from = plan->from;
		for (;;) {
			if (casts_ >= json::kViewCastBudget) { ray.reason = "cast_budget"; return ray; }
			if (ray.casts >= json::kViewRayCasts) { ray.truncated = true; ray.ran = true; return ray; }
			++casts_;
			++ray.casts;
			RE::hkpWorldRayCastInput input;
			input.from = RE::hkVector4(static_cast<float>(from.x * scale_), static_cast<float>(from.y * scale_),
				static_cast<float>(from.z * scale_), 0.0f);
			input.to = RE::hkVector4(static_cast<float>(plan->to.x * scale_), static_cast<float>(plan->to.y * scale_),
				static_cast<float>(plan->to.z * scale_), 0.0f);
			input.enableShapeCollectionFilter = false;
			input.filterInfo.filter = json::kViewRayLayer;  // group 0, layer L_LOS
			RE::hkpWorldRayCastOutput output;
			bool hit = false, isPlayer = false, isActor = false, hasReference = false;
			json::HitClass hitClass = json::HitClass::ordinary;
			double fraction = 1;
			std::uint32_t layer = 0;
			std::optional<std::uint32_t> refID, baseID;
			RE::NiPointer<RE::NiAVObject> root;
			{
				RE::BSReadLockGuard lock(world_->worldLock);
				havok_->CastRay(input, output);
				const auto* collidable = output.rootCollidable;
				if (collidable && output.hitFraction >= 0.0f && output.hitFraction <= 1.0f) {
					hit = true;
					fraction = output.hitFraction;
					const std::uint32_t filterInfo = collidable->broadPhaseHandle.collisionFilterInfo.filter;
					layer = filterInfo & 0x7F;
					auto* ref = RE::TESHavokUtilities::FindCollidableRef(*collidable);
					isPlayer = ref && ref == player_;
					if (!isPlayer) {
						hasReference = ref != nullptr;
						if (ref) {
							// Form type 3Eh, the Character test the native caster makes at 5578C7.
							isActor = ref->GetFormType() == RE::FormType::ActorCharacter;
							refID = ref->GetFormID();
							if (const auto* base = ref->GetBaseObject()) baseID = base->GetFormID();
						}
						hitClass = json::ClassifyHit(layer, filterInfo);
						// Landscape skips FindCollidableObject's grid lookup; volume and
						// detection hits have fixed treatments and need no 3D.
						if (hitClass == json::HitClass::ordinary) {
							if (ref) root.reset(ref->Get3D());
							else root.reset(RE::TESHavokUtilities::FindCollidableObject(*collidable));
						}
					}
				}
			}
			if (!hit) { ray.ran = true; return ray; }
			const json::Vec3 point = from + (plan->to - from) * fraction;
			const json::Vec3 next = json::Advance(from, plan->to, fraction);
			if (isPlayer) {
				if (++ray.skippedPlayer > json::kViewPlayerSkips) { ray.truncated = true; ray.ran = true; return ray; }
			} else {
				// A reference-less object is held for the rest of the sample, so its
				// address can serve as identity; anything else unidentified never merges.
				std::uintptr_t heldObject = 0;
				if (!hasReference && root) {
					holds_.push_back(root);
					heldObject = reinterpret_cast<std::uintptr_t>(root.get());
				}
				const auto identity = json::MakeIdentity(refID, heldObject);
				json::HitRecord record;
				record.hitClass = hitClass;
				record.key = identity.key;
				record.mergeable = identity.mergeable;
				record.distance = json::Length(point - eye);
				if (!json::KnownHit(ray, hitClass, identity)) {
					// A new reference of this class is treated once; another surface of the
					// same reference and class is merged by AddHit.
					record.ref = refID;
					record.base = baseID;
					record.layer = layer;
					json::TreatmentResult treated;
					if (const auto fixed = json::FixedTreatment(hitClass, hasReference, root && root->GetAppCulled())) treated = *fixed;
					else if (isActor) treated = ActorTreatment(root.get());
					else treated = Treat(root.get());
					record.treatment = treated.treatment;
					record.alpha = treated.alpha;
					record.reason = treated.reason;
				}
				if (json::AddHit(ray, record, maxHits_) == json::AddResult::full) { ray.truncated = true; ray.ran = true; return ray; }
			}
			if (!(json::Dot(plan->to - next, direction) > 0)) { ray.ran = true; return ray; }
			from = next;
		}
	}

private:
	struct Cached {
		RE::NiAVObject* root;
		json::TreatmentResult result;
	};

	// Actor 3D may be restructured by jobs that run beside the UI job (Process
	// lists), so only the root's app-culled flag is read; the rest is unknown.
	static json::TreatmentResult ActorTreatment(RE::NiAVObject* root)
	{
		if (!root) return { json::Treatment::unknown, std::nullopt, "unresolved" };
		if (root->GetAppCulled()) return { json::Treatment::hidden, std::nullopt, {} };
		return { json::Treatment::unknown, std::nullopt, "actor" };
	}

	// hidden only when the reference root is app-culled; otherwise every drawn
	// geometry part decides, with culled nodes hiding only their own subtree
	// (view_policy.h TreatmentAccumulator).
	json::TreatmentResult Treat(RE::NiAVObject* root)
	{
		if (!root) return { json::Treatment::unknown, std::nullopt, "unresolved" };
		for (const auto& cached : cache_)
			if (cached.root == root) return cached.result;
		json::TreatmentAccumulator accumulator;
		if (root->GetAppCulled()) accumulator.rootCulled();
		struct Entry {
			RE::NiPointer<RE::NiAVObject> node;
			bool culled;
		};
		std::vector<Entry> stack;
		stack.push_back({ RE::NiPointer<RE::NiAVObject>(root), false });
		std::uint32_t visited = 0;
		while (!stack.empty()) {
			if (++visited > json::kViewTreatmentNodes) { accumulator.nodeCap(); break; }
			const Entry entry = std::move(stack.back());
			stack.pop_back();
			const bool culled = entry.culled || entry.node->GetAppCulled();
			if (auto* geometry = entry.node->AsGeometry()) {
				json::PartSample part;
				part.culled = culled;
				const auto& data = geometry->GetGeometryRuntimeData();
				const RE::NiPointer<RE::NiAlphaProperty> alpha = data.alphaProperty;
				part.blending = alpha && (alpha->alphaFlags & 1) != 0;
				const RE::NiPointer<RE::BSShaderProperty> shader = data.shaderProperty;
				const RE::BSShaderMaterial* material = shader ? shader->material : nullptr;
				part.lighting = material && material->GetType() == RE::BSShaderMaterial::Type::kLighting;
				if (part.lighting) part.alpha = static_cast<const RE::BSLightingShaderMaterialBase*>(material)->materialAlpha;
				accumulator.part(part);
				continue;
			}
			if (auto* node = entry.node->AsNode()) {
				auto& children = node->GetChildren();
				const auto count = (std::min)(children.free_idx(), children.capacity());
				for (std::uint16_t index = 0; index < count; ++index)
					if (auto child = children[index]; child) stack.push_back({ std::move(child), culled });
			}
		}
		const auto result = accumulator.result();
		cache_.push_back({ root, result });
		holds_.emplace_back(root);  // cache and merge keys must not be reused by a new object within the sample
		return result;
	}

	RE::bhkWorld* world_{};
	RE::hkpWorld* havok_{};
	RE::TESObjectREFR* player_{};
	float scale_{};
	std::uint32_t maxHits_{};
	std::uint32_t casts_{};
	std::vector<Cached> cache_;
	std::vector<RE::NiPointer<RE::NiAVObject>> holds_;
};

void SampleView(ViewJob& job, bool onMain)
{
	job.startedMs = PerfNow();
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* ui = RE::UI::GetSingleton();
	if (!player || !player->Is3DLoaded() || (ui && (ui->IsMenuOpen("Main Menu") || ui->IsMenuOpen("Loading Menu"))))
		return FinishView(job, "player_unavailable");
	if (ui && ui->GameIsPaused()) return FinishView(job, "game_paused");
	auto* playerCamera = RE::PlayerCamera::GetSingleton();
	if (!playerCamera || !playerCamera->currentState || !playerCamera->cameraRoot) return FinishView(job, "camera_unavailable");
	const auto state = static_cast<std::uint32_t>(playerCamera->currentState->id);
	if (state != kThirdPersonState) {
		json::Object extra;
		extra.Numeric("camera_state", state);
		return FinishView(job, "camera_state", &extra);
	}
	if (!ViewBinaryGuard()) return FinishView(job, "guard_binary");
	const auto frame = ReadFrame();
	auto* camera = RE::Main::WorldRootCamera();
	if (!camera) return FinishView(job, "world_camera_unavailable", nullptr, frame);
	auto* cell = player->GetParentCell();
	if (!cell) return FinishView(job, "cell_unavailable", nullptr, frame);

	// Rendered camera: the world root NiCamera (36609 -> [global 0x3230740]+0x128),
	// whose worldToCam (+0x110) and port (+0x174) the engine projects with.
	float matrix[4][4]{};
	std::memcpy(matrix, camera->GetRuntimeData().worldToCam, sizeof(matrix));
	const auto& lens = camera->GetRuntimeData2();
	// NiRect keeps left, right, top, bottom (+0x174..+0x180) protected.
	float portEdges[4]{};
	static_assert(sizeof(lens.port) == sizeof(portEdges));
	std::memcpy(portEdges, &lens.port, sizeof(portEdges));
	const json::Port port{ portEdges[0], portEdges[1], portEdges[2], portEdges[3] };
	const auto& eyePoint = camera->world.translate;
	const json::Vec3 eye{ eyePoint.x, eyePoint.y, eyePoint.z };
	const auto& rotation = camera->world.rotate.entry;
	const json::Vec3 matrixForward{ matrix[3][0], matrix[3][1], matrix[3][2] };
	const json::Vec3 rotationForward{ rotation[0][0], rotation[1][0], rotation[2][0] };
	const auto angles = json::ForwardAngles(matrixForward);
	const bool ortho = lens.viewFrustum.bOrtho;
	auto* cameraRoot = playerCamera->cameraRoot.get();
	bool underRoot = false;
	{
		auto& children = cameraRoot->GetChildren();
		const auto count = (std::min)(children.free_idx(), children.capacity());
		for (std::uint16_t index = 0; index < count && !underRoot; ++index) underRoot = children[index].get() == camera;
	}
	const auto& rootPoint = cameraRoot->world.translate;
	const json::Vec3 rootPosition{ rootPoint.x, rootPoint.y, rootPoint.z };
	json::Object cameraFields;
	cameraFields.Numeric("state", state).Raw("pos", json::VecJson(eye));
	OptionalNumber(cameraFields, "yaw_deg", angles ? std::optional<double>(angles->yaw) : std::nullopt);
	OptionalNumber(cameraFields, "pitch_deg", angles ? std::optional<double>(angles->pitch) : std::nullopt);
	OptionalNumber(cameraFields, "fov_deg", ortho ? std::nullopt : json::FrustumDegrees(lens.viewFrustum.fLeft, lens.viewFrustum.fRight));
	OptionalNumber(cameraFields, "vfov_deg", ortho ? std::nullopt : json::FrustumDegrees(lens.viewFrustum.fBottom, lens.viewFrustum.fTop));
	cameraFields.Numeric("world_fov_deg", playerCamera->GetRuntimeData2().worldFOV).Boolean("ortho", ortho)
		.String("node", "world_root_camera").Boolean("under_camera_root", underRoot)
		.Raw("camera_root_pos", json::VecJson(rootPosition)).Numeric("camera_root_delta", json::Length(rootPosition - eye));
	OptionalNumber(cameraFields, "rotation_forward_delta_deg", json::AngleBetweenDegrees(matrixForward, rotationForward));

	const auto position = player->GetPosition();
	const json::Vec3 feet{ position.x, position.y, position.z };
	json::Object cellFields;
	FormField(cellFields, "form_id", cell);
	const char* editorID = cell->GetFormEditorID();
	if (editorID) cellFields.String("editor_id", editorID);
	else cellFields.Null("editor_id");
	json::Object playerFields;
	playerFields.Raw("pos", json::VecJson(feet)).Numeric("heading_deg", json::NormalizeDegrees(json::Degrees(player->GetAngleZ())))
		.Raw("cell", cellFields.Json()).Boolean("interior", cell->IsInteriorCell());
	const json::Vec3 offset = eye - feet;
	const auto offsetJson = json::Object{}.Numeric("x", offset.x).Numeric("y", offset.y).Numeric("z", offset.z)
		.Numeric("distance", json::Length(offset)).Json();

	bool bonesCapped = false;
	const auto bones = PlayerBones(player, job.generation, bonesCapped);
	const json::Vec3 lift{ 0, 0, json::kViewPointLift };
	std::vector<json::PointRecord> points(5);
	points[0].name = "head";
	points[0].world = bones[0];
	points[1].name = "chest";
	points[1].world = bones[1];
	points[2].name = "feet";
	points[2].world = feet + lift;
	points[3].name = "shoulder_l";
	points[3].world = bones[2];
	points[4].name = "shoulder_r";
	points[4].world = bones[3];
	const auto& settings = g_probe->view;
	std::vector<json::PointRecord> ring(settings.ringPoints);
	for (std::uint32_t index = 0; index < settings.ringPoints; ++index) {
		ring[index].name = "ring_" + std::to_string(index);
		ring[index].world = json::RingPoint(feet, settings.ringRadius, settings.ringPoints, index, angles ? angles->yaw : 0.0,
			json::kViewPointLift);
	}
	ViewCaster caster(cell, player, settings.maxHits);
	for (auto* list : { &points, &ring }) {
		for (auto& point : *list) {
			if (!point.world) {
				point.ray.reason = bonesCapped ? "skeleton_cap" : "point_unavailable";
				continue;
			}
			point.projection = json::Project(matrix, port, *point.world);
			point.ray = caster.Cast(eye, *point.world);
		}
	}
	// Backstop: a world swap during the sample discards it.
	if (job.generation != g_probe->generation.load() || !g_probe->loaded.load() || player->GetParentCell() != cell)
		return FinishView(job, "world_changed", nullptr, frame);
	json::Object body;
	body.Raw("camera", cameraFields.Json()).Raw("player", playerFields.Json()).Raw("offset", offsetJson)
		.Raw("points", json::PointsJson(points)).Raw("ring", json::PointsJson(ring))
		.Raw("player_box", json::BoxJson(points))
		.Raw("counts", json::Object{}.Raw("points", json::CountsJson(points)).Raw("ring", json::CountsJson(ring)).Json())
		.Numeric("casts", caster.casts()).Boolean("on_main_thread", onMain)
		.Raw("config", json::Object{}.Numeric("ring_points", settings.ringPoints).Numeric("ring_radius", settings.ringRadius)
			.Numeric("max_hits", settings.maxHits).Numeric("layer", json::kViewRayLayer)
			.Numeric("end_clearance", json::kViewEndClearance).Numeric("point_lift", json::kViewPointLift)
			.Boolean("main_thread_only", settings.mainThreadOnly).Boolean("periodic_off_main", settings.periodicOffMain).Json());
	FinishView(job, {}, &body, frame);
}

void RunView(ViewJob job) noexcept;

bool QueueView(const ViewJob& job) noexcept
{
	try {
		return QueueProbeTask([job]() mutable { RunView(std::move(job)); });
	} catch (...) { return false; }
}

// Main task queue helper for bViewMainThreadOnly: no engine reads; it only
// re-queues the UI task, so neither queue requeues into its own drain.
bool QueueViewHop(const ViewJob& job) noexcept
{
	try {
		const auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) return false;
		tasks->AddTask([job]() {
			if (!QueueView(job)) FinishView(job, "enqueue_failed");
		});
		return true;
	} catch (...) { return false; }
}

void RunView(ViewJob job) noexcept
{
	try {
		const auto reason = json::SnapshotReason(InTaskDrain(), job.generation, g_probe->generation.load(), g_probe->loaded.load());
		if (!reason.empty()) return FinishView(job, reason);
		const auto* main = RE::Main::GetSingleton();
		const bool onMain = main && main->threadID == GetCurrentThreadId();
		if (!onMain && g_probe->view.mainThreadOnly) {
			if (job.hops < json::kViewMaxHops && PerfNow() - job.queuedMs < json::kViewMaxWaitMs) {
				++job.hops;
				if (!QueueViewHop(job)) FinishView(job, "enqueue_failed");
				return;
			}
			return FinishView(job, "off_main_thread");
		}
		SampleView(job, onMain);
	} catch (...) {
		Log("SkyCRPG_TestProbe: view sample failed; reported unavailable\n");
		FinishView(job, "sample_failed");
	}
}

// Requests and periodic ticks both end here; every call emits exactly one line.
void StartView(ViewJob job) noexcept
{
	job.generation = g_probe->generation.load();
	job.queuedMs = PerfNow();
	if (g_probe->viewInFlight.fetch_add(1) >= json::kViewMaxInFlight) return FinishView(job, "view_busy");
	if (!job.valid) return FinishView(job, "invalid_view_request");
	if (!QueueView(job)) FinishView(job, "enqueue_failed");
}

void CALLBACK ViewTimerTick(void*, BOOLEAN) noexcept
{
	try {
		if (!g_probe->loaded.load()) return;
		ViewJob job;
		job.timer = true;
		{
			std::scoped_lock lock(g_probe->viewMutex);
			if (!g_probe->viewCadence.tick(job.coalesced)) return;
		}
		StartView(std::move(job));
	} catch (...) { Log("SkyCRPG_TestProbe: view timer failed; skipped\n"); }
}

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
	// "@view " is reserved only while bViewSamples=1; otherwise it stays a raw tag.
	const auto view = g_probe->view.enabled ? json::ParseViewRequest(*tag) : json::ViewRequest{};
	const auto perf = json::ParsePerfRequest(*tag);
	if (view.command != json::ViewCommand::none) {
		// Always answered: queued, or emitted at once as view_busy/invalid/enqueue_failed.
		ViewJob job;
		job.requested = true;
		job.tag = view.label;
		job.valid = view.command == json::ViewCommand::sample;
		StartView(std::move(job));
	} else if (perf.command == json::PerfCommand::checkpoint) {
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
	// Timer thread only. No engine/UI access and no perfMutex.
	const auto started = Clock::now();
	SYSTEM_INFO system{};
	GetSystemInfo(&system);
	const auto maximum = reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
	const auto walk = json::WalkPrivateCommit(static_cast<std::uint64_t>(maximum),
		[](std::uint64_t address, json::MemoryRegion& region) {
			MEMORY_BASIC_INFORMATION info{};
			if (VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), &info, sizeof(info)) != sizeof(info)) return false;
			region = { static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(info.BaseAddress)),
				static_cast<std::uint64_t>(info.RegionSize), info.State == MEM_COMMIT && info.Type == MEM_PRIVATE };
			return true;
		}, [&started] { return Clock::now() - started >= std::chrono::milliseconds(100); });
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - started).count();
	json::Object fields;
	fields.Numeric("private_commit_regions", walk.regions).Numeric("private_commit_matching_regions", walk.privateRegions)
		.Numeric("private_commit_walk_us", elapsed).Boolean("private_commit_complete", walk.complete);
	if (walk.complete) fields.Numeric("private_commit_bytes", walk.bytes);
	else {
		fields.Null("private_commit_bytes");
		if (!g_probe->commitWalkFailureLogged.exchange(true))
			Log("SkyCRPG_TestProbe: private commit walk incomplete (query, range or 100ms/region limit); bytes unavailable\n");
	}
	PROCESS_MEMORY_COUNTERS_EX counters{};
	counters.cb = static_cast<DWORD>(sizeof(counters));
	if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), counters.cb)) {
		fields.Numeric("private_bytes", static_cast<std::uint64_t>(counters.PrivateUsage))
			.Numeric("working_set_bytes", static_cast<std::uint64_t>(counters.WorkingSetSize));
	} else {
		fields.Null("private_bytes").Null("working_set_bytes");
		if (!g_probe->memoryFailureLogged.exchange(true)) Log("SkyCRPG_TestProbe: process memory sample unavailable\n");
	}
	Emit("process_memory", fields);
}

void CALLBACK TimerTick(void*, BOOLEAN) noexcept
{
	if (g_probe->timerBusy.exchange(true)) return;
	try {
		if (g_probe->loaded.load()) SampleMemory();
		FlushInputDiagnostics();
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
			ReleaseRetiredViewNodes(false);  // no-op unless on Main::threadID
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
			RegisterInputObserver(inputs);
			if (auto* ui = RE::UI::GetSingleton()) ui->AddEventSink<RE::MenuOpenCloseEvent>(events);
			else Log("SkyCRPG_TestProbe: menu event source unavailable\n");
			if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) {
				source->AddEventSink<RE::TESEquipEvent>(events);
				source->AddEventSink<RE::TESContainerChangedEvent>(events);
			} else Log("SkyCRPG_TestProbe: script event source unavailable\n");
			if (g_probe->interval != 0 && !CreateTimerQueueTimer(&g_probe->timer, nullptr, TimerTick, nullptr,
				g_probe->interval, g_probe->interval, WT_EXECUTEDEFAULT))
				Log("SkyCRPG_TestProbe: timer creation failed; event snapshots remain available\n");
			// A separate timer, only with bViewSamples=1, iViewIntervalMs>0 and either
			// bViewMainThreadOnly=1 or bViewPeriodicOffMain=1. It only queues; engine
			// reads happen in the task drain. Without the opt-in the timer is refused
			// and one view_sample says so.
			switch (json::Periodic(g_probe->view)) {
			case json::PeriodicMode::on:
				if (!CreateTimerQueueTimer(&g_probe->viewTimer, nullptr, ViewTimerTick, nullptr,
					g_probe->view.intervalMs, g_probe->view.intervalMs, WT_EXECUTEDEFAULT))
					Log("SkyCRPG_TestProbe: view timer creation failed; @view requests remain available\n");
				break;
			case json::PeriodicMode::refused: {
				ViewJob job;
				job.queuedMs = PerfNow();
				g_probe->viewInFlight.fetch_add(1);
				FinishView(job, "periodic_opt_in_missing");
				break;
			}
			default: break;
			}
			break;
		}
		case SKSE::MessagingInterface::kPreLoadGame: {
			g_probe->loaded.store(false);
			ReleaseRetiredViewNodes(true);  // drop the previous session's player 3D
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
			ReleaseRetiredViewNodes(false);
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
		g_probe->view = json::ParseViewSettings(ReadDigits(ini, L"bViewSamples", L"").data(),
			ReadDigits(ini, L"iViewIntervalMs", L"").data(), ReadDigits(ini, L"iViewRingPoints", L"").data(),
			ReadDigits(ini, L"iViewRingRadius", L"").data(), ReadDigits(ini, L"iViewMaxHits", L"").data(),
			ReadDigits(ini, L"bViewMainThreadOnly", L"").data(), ReadDigits(ini, L"bViewPeriodicOffMain", L"").data());
		auto fields = json::Object{}.String("runtime", Version(skseInterface->runtimeVersion))
			.String("skse", Version(skseInterface->skseVersion)).String("probe_version", SKYCRPG_VERSION)
			.String("snapshot_dispatch", "ui_task_drain_v3").Numeric("perf_protocol", 1).Numeric("observation_protocol", json::kObservationProtocol)
			.String("build_commit", SKYCRPG_SOURCE_COMMIT).Boolean("enabled", true).Numeric("state_interval_ms", g_probe->interval);
		// Additive and only with bViewSamples=1, so a probe without the key logs the base line.
		if (g_probe->view.enabled)
			fields.Numeric("view_protocol", json::kViewProtocol).Numeric("view_interval_ms", g_probe->view.intervalMs)
				.Numeric("view_ring_points", g_probe->view.ringPoints).Numeric("view_ring_radius", g_probe->view.ringRadius)
				.Numeric("view_max_hits", g_probe->view.maxHits).Boolean("view_main_thread_only", g_probe->view.mainThreadOnly)
				.Boolean("view_periodic_off_main", g_probe->view.periodicOffMain)
				.String("view_periodic", json::Periodic(g_probe->view) == json::PeriodicMode::on ? "on" :
					json::Periodic(g_probe->view) == json::PeriodicMode::refused ? "refused" : "off");
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
