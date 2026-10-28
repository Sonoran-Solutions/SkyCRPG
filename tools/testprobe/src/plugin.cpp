// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// Test-only observer. No hooks, input, serialization or game-state mutations.
#include <SKSE/SKSE.h>
#include <RE/A/ActorValueOwner.h>
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
#include <RE/U/UI.h>

// CommonLib/REX headers must precede the Windows headers in the skeleton.
#include <skycrpg/skeleton.h>
#include "json_line.h"
#include "snapshot_policy.h"

#include <bcrypt.h>

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
	std::wstring directory;
	std::string saveName;
	bool hasSaveName{ false };
	Clock::time_point origin{ Clock::now() };
	std::atomic_bool loaded{ false };
	std::atomic_uint64_t generation{ 0 };
	std::atomic_uint32_t mainThreadID{ 0 };
	std::atomic_bool timerBusy{ false };
	std::atomic_bool timerPending{ false };
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
	// kDataLoaded is a known main-thread callback. Never learn affinity from
	// a task delegate, and never silently rebind it to a conflicting callback.
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
	if (!ui || !ui->IsMenuOpen("InventoryMenu")) { fields.Null("item_card_submenu"); return; }
	const auto movie = ui->GetMovieView("InventoryMenu");
	RE::GFxValue value;
	if (!movie || !movie->GetVariable(&value, card) || value.IsUndefined()) {
		Log("SkyCRPG_TestProbe: inventory item card unavailable\n");
		fields.Null("item_card_submenu");
		return;
	}
	RE::GFxValue handler;
	const bool open = movie->GetVariable(&handler, (std::string(card) + ".InputHandler").c_str()) &&
		!handler.IsUndefined() && !handler.IsNull();
	fields.Boolean("item_card_submenu", open);
}

void Snapshot(bool requested, const std::string& tag, std::uint64_t generation)
{
	json::Object fields;
	fields.Boolean("requested", requested);
	if (requested) fields.String("tag", tag);
	ThreadFields(fields);
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
	fields.Raw("equipped", equipped.Json());
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
		fields.Null("paused").Null("open_menus").Null("item_card_submenu");
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
		const bool queued = QueueProbeTask([requested, tag = std::move(tag), generation, timer] {
			try { Snapshot(requested, tag, generation); }
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
	if (!tag || !QueueSnapshot(true, *tag, true)) return false;
	FILE_DISPOSITION_INFO disposition{};
	disposition.DeleteFile = TRUE;
	if (!SetFileInformationByHandle(file.value, FileDispositionInfo, &disposition, static_cast<DWORD>(sizeof(disposition))))
		Log("SkyCRPG_TestProbe: request delete failed; response queued but request may repeat\n");
	return true;
}

void CALLBACK TimerTick(void*, BOOLEAN) noexcept
{
	if (g_probe->timerBusy.exchange(true)) return;
	try {
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
			static bool registered = false;
			if (registered) return;
			registered = true;
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
		g_probe = new Probe();
		g_probe->directory = skycrpg::LogDirectory();
		if (g_probe->directory.empty()) { Log("SkyCRPG_TestProbe: SKSE directory unavailable\n"); return false; }
		const auto ini = skycrpg::ThisModuleDirectory() + L"SkyCRPG_TestProbe.ini";
		const bool enabled = GetPrivateProfileIntW(L"Probe", L"bEnabled", 1, ini.c_str()) != 0;
		std::array<char, 32> interval{};
		std::array<wchar_t, 32> wideInterval{};
		GetPrivateProfileStringW(L"Probe", L"iStateIntervalMs", L"1000", wideInterval.data(), static_cast<DWORD>(wideInterval.size()), ini.c_str());
		for (std::size_t index = 0; index < interval.size(); ++index) {
			const auto character = wideInterval[index];
			interval[index] = character >= L'0' && character <= L'9' ? static_cast<char>(character) : (character == 0 ? '\0' : '?');
		}
		g_probe->interval = json::ParseInterval(interval.data());
		const auto fields = json::Object{}.String("runtime", Version(skseInterface->runtimeVersion))
			.String("skse", Version(skseInterface->skseVersion)).String("probe_version", SKYCRPG_VERSION)
			.String("snapshot_dispatch", "ui_task_drain_v3")
			.String("build_commit", SKYCRPG_SOURCE_COMMIT).Boolean("enabled", enabled).Numeric("state_interval_ms", g_probe->interval);
		if (!enabled) { Emit("probe_loaded", fields); return true; }
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
