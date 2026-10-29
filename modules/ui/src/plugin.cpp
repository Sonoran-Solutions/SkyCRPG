// SkyCRPG UI — HUD bridge and native favorite hotbar dispatch.
#include <SKSE/SKSE.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#include <RE/G/GFxFunctionHandler.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/E/ExtraHotkey.h>
#include <RE/I/InventoryEntryData.h>
#include <RE/M/MagicFavorites.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/U/UI.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESShout.h>
#include <RE/A/ActorEquipManager.h>
#include <RE/B/BGSEquipSlot.h>
#include <RE/B/BSInputDeviceManager.h>
#include <RE/B/ButtonEvent.h>
#include <RE/I/InputEvent.h>
#include <RE/S/SpellItem.h>

// CommonLib's REX/W32 headers must precede the Windows API headers pulled by
// the shared skeleton. Keep the skeleton below every CommonLib include.
#include <skycrpg/skeleton.h>
#include "hud_settings.h"
#include "address_library_pin.h"
#include "skse_version_pin.h"
#include "resource_snapshot.h"
#include "menu_focus.h"
#include "lifecycle/dispatch_epoch.h"
#include "favorite_slots.h"
#include "minimap/adapter.h"
#include "follower/adapter.h"
#include "inventory/inventory_bridge.h"
#include "inventory/equipment_bridge.h"
#include "inventory/item_preview_placement.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <bcrypt.h>

namespace {
constexpr skycrpg::ModuleInfo kModule{ "SkyCRPG UI", L"SkyCRPG_UI", SKYCRPG_VERSION, SKYCRPG_SOURCE_COMMIT };
static_assert(skycrpg::ui::kPinnedSkseVersion == skse::kSKSE_2_3_1);
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
	std::array<wchar_t, 32> replaceCompass{};
	std::array<wchar_t, 32> resetLayout{};
	GetPrivateProfileStringW(L"Settings", L"iVersion", L"", version.data(), static_cast<DWORD>(version.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"HUD", L"bEnabled", L"", enabled.data(), static_cast<DWORD>(enabled.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"HUD", L"iScalePercent", L"", scale.data(), static_cast<DWORD>(scale.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"HUD", L"iOffsetX", L"", offsetX.data(), static_cast<DWORD>(offsetX.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"HUD", L"iOffsetY", L"", offsetY.data(), static_cast<DWORD>(offsetY.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"Minimap", L"bReplaceCompass", L"", replaceCompass.data(), static_cast<DWORD>(replaceCompass.size()), iniPath.c_str());
	GetPrivateProfileStringW(L"HUD", L"bResetLayout", L"0", resetLayout.data(), static_cast<DWORD>(resetLayout.size()), iniPath.c_str());
	return skycrpg::ui::ParseHudSettings(version.data(), enabled.data(), scale.data(), offsetX.data(), offsetY.data(), replaceCompass.data(), resetLayout.data());
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

// Menu events gate visibility immediately. The GFx thread reads only the atomic;
// the game task queue resamples the actual UI after the transition completes.
std::atomic_bool g_menuBlocked{ true };
skycrpg::ui::lifecycle::DispatchEpoch g_focusEpoch;
void RefreshMenuFocus()
{
	auto* ui = RE::UI::GetSingleton();
	bool blocked = true;
	if (ui) {
		const bool uiFocus = ui->GameIsPaused() || ui->IsApplicationMenuOpen() || ui->IsModalMenuOpen();
		const bool namedFocus = ui->IsMenuOpen("FavoritesMenu") || ui->IsMenuOpen("Dialogue Menu") ||
			ui->IsMenuOpen("Console") || ui->IsMenuOpen("Loading Menu") || ui->IsMenuOpen("Main Menu");
		bool stackFocus = false;
		for (const auto& menu : ui->menuStack) {
			if (!menu || menu->menuFlags.any(RE::UI_MENU_FLAGS::kPausesGame, RE::UI_MENU_FLAGS::kApplicationMenu,
				RE::UI_MENU_FLAGS::kUsesCursor, RE::UI_MENU_FLAGS::kUsesMenuContext, RE::UI_MENU_FLAGS::kModal)) {
				stackFocus = true; break;
			}
		}
		blocked = skycrpg::ui::MenuFocusBlocked(true, uiFocus, namedFocus, stackFocus);
	}
	g_menuBlocked.store(blocked, std::memory_order_release);
}
class MenuFocusEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
		if (!event) { g_focusEpoch.Invalidate(); g_menuBlocked.store(true, std::memory_order_release); return RE::BSEventNotifyControl::kContinue; }
		auto* ui = RE::UI::GetSingleton();
		if (!ui) { g_focusEpoch.Invalidate(); g_menuBlocked.store(true, std::memory_order_release); return RE::BSEventNotifyControl::kContinue; }
		const std::string_view name = event->menuName.c_str();
		const auto menu = ui->GetMenu(name);
		const bool focusFlags = menu && menu->menuFlags.any(RE::UI_MENU_FLAGS::kPausesGame, RE::UI_MENU_FLAGS::kApplicationMenu,
			RE::UI_MENU_FLAGS::kUsesCursor, RE::UI_MENU_FLAGS::kUsesMenuContext, RE::UI_MENU_FLAGS::kModal);
		if (skycrpg::ui::MenuBlocks(name, static_cast<bool>(menu), focusFlags)) {
			g_focusEpoch.Invalidate();
			g_menuBlocked.store(true, std::memory_order_release);
			// No menu or actor pointer survives the callback. Re-read the current
			// stack after the event rather than trusting pre-transition flags.
			if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { RefreshMenuFocus(); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}
} g_menuFocusEvents;

class MenuFocusBlocked final : public RE::GFxFunctionHandler {
public:
	void Call(Params& params) override {
		if (params.retVal) params.retVal->SetBoolean(g_menuBlocked.load(std::memory_order_acquire));
	}
};

using ResourceClock = std::chrono::steady_clock;
struct HotbarSlot {
	std::string name;
	std::string kind;
	int count{};
	bool equipped{};
	float voiceRecovery{-1.0f}; // unavailable, never inferred from a missing process
	std::uint32_t formID{}; // temporary snapshot identity; never exposed to Scaleform
	bool bound{};
};
struct HotbarCache {
	std::mutex mutex;
	std::array<HotbarSlot, 8> slots{};
	std::string voiceName;
	float voiceRecovery{-1.0f};
	ResourceClock::time_point sampled{};
	std::uint64_t generation{};
	bool available{}, pending{}, loaded{};
} g_hotbar;

const char* HotbarKind(RE::TESForm* form)
{
	switch (form->GetFormType()) {
	case RE::FormType::Weapon: return "WEAPON";
	case RE::FormType::Armor: return "ARMOR";
	case RE::FormType::AlchemyItem: case RE::FormType::Ingredient: return "ALCHEMY";
	case RE::FormType::Spell: return "SPELL";
	case RE::FormType::Shout: return "SHOUT";
	case RE::FormType::Scroll: return "SCROLL";
	default: return "ITEM";
	}
}

// Mirrors SKSE GetHotkeyBoundObject: magic slots take precedence over item
// ExtraHotkey. Only copied values leave the game task; no form is retained.
void SampleHotbar(std::uint64_t generation)
{
	{
		std::scoped_lock lock(g_hotbar.mutex);
		if (!g_hotbar.loaded || generation != g_hotbar.generation) { g_hotbar.pending = false; return; }
	}
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* magic = RE::MagicFavorites::GetSingleton();
	if (!player || !player->Is3DLoaded() || !magic || !player->GetInventoryChanges(true)) {
		std::scoped_lock lock(g_hotbar.mutex);
		g_hotbar.pending = false;
		g_hotbar.available = false;
		return;
	}
	std::array<HotbarSlot, 8> slots{};
	std::vector<HotbarSlot> favorites;
	auto* selectedVoice = player->GetActorRuntimeData().selectedPower;
	const char* voiceName = selectedVoice ? selectedVoice->GetName() : nullptr;
	const auto equippedLeft = player->GetEquippedObject(true);
	const auto equippedRight = player->GetEquippedObject(false);
	float voiceRecovery = -1.0f;
	auto* process = player->GetActorRuntimeData().currentProcess;
	if (process && process->high) {
		const float seconds = player->GetVoiceRecoveryTime();
		if (std::isfinite(seconds) && seconds >= 0.0f) voiceRecovery = seconds;
	}
	auto inventory = player->GetInventory([](RE::TESBoundObject&) { return true; }, true);
	for (auto& [form, item] : inventory) {
		if (!form || !skycrpg::ui::HasInventoryCount(item.first) || !item.second || !item.second->extraLists) continue;
		bool favorited = false;
		bool equipped = false;
		for (auto* extra : *item.second->extraLists) {
			if (!extra) continue;
			const bool worn = extra->HasType<RE::ExtraWorn>() || extra->HasType<RE::ExtraWornLeft>();
			equipped = equipped || worn;
			auto* binding = extra->GetByType<RE::ExtraHotkey>();
			if (!binding) continue;
			favorited = true;
			const auto index = static_cast<unsigned>(binding->hotkey.underlying());
			if (index >= slots.size()) continue;
			// Do not call InventoryEntryData::GetDisplayName: it may add extra data.
			const auto* name = form->GetName();
			skycrpg::ui::SetNativeBinding(slots, index, HotbarSlot{ name ? name : "", HotbarKind(form), item.first,
				worn, -1.0f, form->GetFormID(), true });
		}
		if (favorited) {
			const auto* name = form->GetName();
			favorites.push_back({ name ? name : "", HotbarKind(form), item.first,
				equipped, -1.0f, form->GetFormID(), false });
		}
	}
	// BSTArray::size_type is uint32_t; the fixed eight-slot bound avoids narrowing.
	for (RE::BSTArray<RE::TESForm*>::size_type i = 0; i < 8 && i < magic->hotkeys.size(); ++i) {
		auto* form = magic->hotkeys[i];
		if (!form) continue;
		const auto* name = form->GetName();
		skycrpg::ui::SetNativeBinding(slots, i, HotbarSlot{ name ? name : "", HotbarKind(form), 0,
			form == equippedLeft || form == equippedRight || form == selectedVoice,
			-1.0f, form->GetFormID(), true });
		if (form->GetFormType() == RE::FormType::Shout) slots[i].voiceRecovery = voiceRecovery;
	}
	// SKSE IsObjectFavorited uses MagicFavorites::spells for magic forms and
	// ExtraHotkey (including kUnbound) for inventory instances. The order in
	// the Favorites menu is not assumed; users select these through that menu.
	for (auto* form : magic->spells) {
		if (!form) continue;
		const auto* name = form->GetName();
		HotbarSlot entry{ name ? name : "", HotbarKind(form), 0,
			form == equippedLeft || form == equippedRight || form == selectedVoice,
			-1.0f, form->GetFormID(), false };
		if (form->GetFormType() == RE::FormType::Shout) entry.voiceRecovery = voiceRecovery;
		favorites.push_back(std::move(entry));
	}
	skycrpg::ui::FillFavoriteSlots(slots, std::move(favorites));
	std::scoped_lock lock(g_hotbar.mutex);
	g_hotbar.pending = false;
	if (!g_hotbar.loaded || generation != g_hotbar.generation) return;
	g_hotbar.slots = std::move(slots);
	g_hotbar.voiceName = voiceName ? voiceName : "";
	g_hotbar.voiceRecovery = voiceRecovery;
	g_hotbar.sampled = ResourceClock::now();
	g_hotbar.available = true;
}

// The input callback retains only copied snapshot data. Engine objects are
// resolved and validated again on the game task queue.
using skycrpg::ui::ActionKind;
using skycrpg::ui::HotbarAction;
ActionKind DispatchKind(RE::TESForm* form)
{
    if (!form) return ActionKind::None;
    switch (form->GetFormType()) {
    case RE::FormType::Weapon: return ActionKind::Weapon;
    case RE::FormType::Armor: return ActionKind::Armor;
    case RE::FormType::AlchemyItem: case RE::FormType::Ingredient: return ActionKind::Consumable;
    case RE::FormType::Scroll: return ActionKind::Scroll;
    case RE::FormType::Shout: return ActionKind::Shout;
    case RE::FormType::Spell: {
        auto* spell = static_cast<RE::SpellItem*>(form);
        const auto type = spell->GetSpellType();
        return type == RE::MagicSystem::SpellType::kPower || type == RE::MagicSystem::SpellType::kLesserPower ? ActionKind::Power : ActionKind::Spell;
    }
    default: return ActionKind::None;
    }
}
const char* ActionName(HotbarAction action)
{
    switch (action) {
    case HotbarAction::Equip: return "equip";
    case HotbarAction::Unequip: return "unequip";
    case HotbarAction::Use: return "use";
    case HotbarAction::EquipSpell: return "spell";
    case HotbarAction::EquipShout: return "shout";
    case HotbarAction::EquipPower: return "power";
    default: return "none";
    }
}
std::atomic_uint g_hotkeyLogCount{};
void DispatchHotbar(unsigned index, HotbarSlot pressed, std::uint64_t generation, bool available, bool blockedAtPress, std::uint64_t focusEpoch)
{
    RefreshMenuFocus();
    bool blocked = blockedAtPress || !g_focusEpoch.Current(focusEpoch) || g_menuBlocked.load(std::memory_order_acquire);
    // Refresh bindings before dispatch so a newly assigned native key cannot
    // race the snapshot and cause a second action.
    SampleHotbar(generation);
    {
        std::scoped_lock lock(g_hotbar.mutex);
        if (!g_hotbar.loaded || !g_hotbar.available || generation != g_hotbar.generation ||
            g_hotbar.slots[index].bound || g_hotbar.slots[index].formID != pressed.formID) blocked = true;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* manager = RE::ActorEquipManager::GetSingleton();
    auto* form = pressed.formID ? RE::TESForm::LookupByID(pressed.formID) : nullptr;
    auto kind = DispatchKind(form);
    int count = 0;
    bool equipped = false;
    RE::ExtraDataList* extra = nullptr;
    RE::TESBoundObject* boundObject = nullptr; // From GetInventory; avoids RTTI casts.
    if (!available || !player || player->IsDead() || !player->Is3DLoaded() || !manager) blocked = true;
    if (!blocked && form && (kind == ActionKind::Spell || kind == ActionKind::Power)) {
        count = player->HasSpell(static_cast<RE::SpellItem*>(form)) ? 1 : 0;
    } else if (!blocked && form && kind == ActionKind::Shout) {
        count = player->HasShout(static_cast<RE::TESShout*>(form)) ? 1 : 0;
    } else if (!blocked && form) {
        auto inventory = player->GetInventory([](RE::TESBoundObject&) { return true; }, true);
        for (auto& [object, item] : inventory) {
            if (!object || object->GetFormID() != pressed.formID || !item.second) continue;
            count = item.first;
            boundObject = object;
            if (item.second->extraLists) for (auto* candidate : *item.second->extraLists) {
                if (!candidate) continue;
                const bool worn = candidate->HasType<RE::ExtraWorn>() || candidate->HasType<RE::ExtraWornLeft>();
                if (worn) { equipped = true; extra = candidate; break; }
                if (!extra && candidate->GetByType<RE::ExtraHotkey>()) extra = candidate;
            }
            break;
        }
        // The inventory owns these instance pointers until the native call.
        auto action = skycrpg::ui::DecideHotbarAction(index, pressed.bound, kind, equipped, count, blocked);
        if (auto* object = boundObject) {
            if (action == HotbarAction::Unequip) manager->UnequipObject(player, object, extra);
            else if (action == HotbarAction::Equip || action == HotbarAction::Use) manager->EquipObject(player, object, extra);
        } else action = HotbarAction::None;
        if (g_hotkeyLogCount.fetch_add(1) < 64) {
            std::array<char, 192> line{};
            std::snprintf(line.data(), line.size(), "Hotkey slot=%u bound=%d kind=%s formID=%08X decision=%s\n", index + 1, pressed.bound, pressed.kind.c_str(), pressed.formID, ActionName(action));
            AppendModuleLog(line.data());
        }
        return;
    }
    auto action = skycrpg::ui::DecideHotbarAction(index, pressed.bound, kind, equipped, count, blocked);
    if (action == HotbarAction::EquipSpell || action == HotbarAction::EquipPower) {
        // Skyrim.esm equip slots: right hand 0x13F42, voice 0x25BEE.
        auto* slotForm = RE::TESForm::LookupByID(action == HotbarAction::EquipPower ? 0x25BEE : 0x13F42);
        auto* slot = slotForm && slotForm->GetFormType() == RE::FormType::EquipSlot ? static_cast<RE::BGSEquipSlot*>(slotForm) : nullptr;
        if (slot) manager->EquipSpell(player, static_cast<RE::SpellItem*>(form), slot);
        else action = HotbarAction::None;
    } else if (action == HotbarAction::EquipShout) manager->EquipShout(player, static_cast<RE::TESShout*>(form));
    if (g_hotkeyLogCount.fetch_add(1) < 64) {
        std::array<char, 192> line{};
        std::snprintf(line.data(), line.size(), "Hotkey slot=%u bound=%d kind=%s formID=%08X decision=%s\n", index + 1, pressed.bound, pressed.kind.c_str(), pressed.formID, ActionName(action));
        AppendModuleLog(line.data());
    }
}
class HotbarInput final : public RE::BSTEventSink<RE::InputEvent*> {
public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!events) return RE::BSEventNotifyControl::kContinue;
        for (auto* event = *events; event; event = event->next) {
            auto* button = event->AsButtonEvent();
            if (!button || event->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown()) continue;
            const std::string_view name = button->QUserEvent().c_str();
            if (name.size() != 7 || name.substr(0, 6) != "Hotkey" || name[6] < '1' || name[6] > '8') continue;
            const unsigned index = static_cast<unsigned>(name[6] - '1');
            HotbarSlot slot;
            std::uint64_t generation;
            bool available;
            {
                std::scoped_lock lock(g_hotbar.mutex);
                available = g_hotbar.loaded && g_hotbar.available && ResourceClock::now() - g_hotbar.sampled <= std::chrono::seconds(2);
                slot = g_hotbar.slots[index];
                generation = g_hotbar.generation;
            }
            const auto focusEpoch = g_focusEpoch.Capture();
            const bool blockedAtPress = g_menuBlocked.load(std::memory_order_acquire);
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([index, slot, generation, available, blockedAtPress, focusEpoch] { DispatchHotbar(index, slot, generation, available, blockedAtPress, focusEpoch); });
        }
        return RE::BSEventNotifyControl::kContinue;
    }
} g_hotbarInput;

class HotbarSnapshot final : public RE::GFxFunctionHandler {
public:
	void Call(Params& params) override {
		if (!params.retVal) return;
		params.retVal->SetUndefined();
		if (!params.movie) return;
		std::array<HotbarSlot, 8> slots;
		std::string voiceName;
		float voiceRecovery = -1.0f;
		bool available = false, request = false;
		std::uint64_t generation = 0;
		{
			std::scoped_lock lock(g_hotbar.mutex);
			available = g_hotbar.loaded && g_hotbar.available && ResourceClock::now() - g_hotbar.sampled <= std::chrono::seconds(2);
			if (available) { slots = g_hotbar.slots; voiceName = g_hotbar.voiceName; voiceRecovery = g_hotbar.voiceRecovery; }
			if (g_hotbar.loaded && !g_hotbar.pending) {
				g_hotbar.pending = true; generation = g_hotbar.generation; request = true;
			}
		}
		if (request) SKSE::GetTaskInterface()->AddTask([generation]() { SampleHotbar(generation); });
		if (!available) return;
		RE::GFxValue array;
		params.movie->CreateArray(&array);
		RE::GFxValue voiceValue;
		params.movie->CreateString(&voiceValue, voiceName.c_str());
		if (!array.SetMember("voiceName", voiceValue)) return;
		voiceValue.SetNumber(voiceRecovery);
		if (!array.SetMember("voiceRecovery", voiceValue)) return;
		for (const auto& slot : slots) {
			RE::GFxValue object, value;
			params.movie->CreateObject(&object);
			params.movie->CreateString(&value, slot.name.c_str());
			if (!object.SetMember("name", value)) return;
			params.movie->CreateString(&value, slot.kind.c_str());
			if (!object.SetMember("kind", value)) return;
			value.SetNumber(slot.count);
			if (!object.SetMember("count", value)) return;
			value.SetBoolean(slot.equipped);
			if (!object.SetMember("equipped", value)) return;
			value.SetNumber(slot.voiceRecovery);
			if (!object.SetMember("voiceRecovery", value)) return;
			value.SetBoolean(slot.bound);
			if (!object.SetMember("bound", value) || !array.PushBack(object)) return;
		}
		*params.retVal = array;
	}
};
struct ResourceCache {
	std::mutex mutex;
	std::array<skycrpg::ui::ResourceValue, 3> values{};
	ResourceClock::time_point sampled{};
	std::uint64_t generation{};
	bool available{};
	bool pending{};
	bool loaded{};
} g_resources;

void ResourceLifecycle(SKSE::MessagingInterface::Message* message)
{
	skycrpg::ui::inventory::OnSkseMessage(message);
	skycrpg::ui::inventory::preview::OnSkseMessage(message);
	if (!message) return;
	skycrpg::ui::minimap::OnMessage(message);
	skycrpg::ui::follower::OnMessage(message);
	if (message->type == SKSE::MessagingInterface::kDataLoaded) {
		static bool registered = false;
        static bool inputRegistered = false;
        if (auto* input = RE::BSInputDeviceManager::GetSingleton(); g_hudSettings.enabled && input && !inputRegistered) {
            input->AddEventSink(&g_hotbarInput);
            inputRegistered = true;
        } else if (g_hudSettings.enabled && !inputRegistered) {
            AppendModuleLog("Hotbar input event source unavailable; dispatch disabled\n");
        }
		if (auto* ui = RE::UI::GetSingleton()) {
			if (!registered) { ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuFocusEvents); registered = true; }
			RefreshMenuFocus();
		} else {
			AppendModuleLog("Menu visibility event source unavailable; hotbar hidden\n");
		}
		return;
	}
	if (message->type != SKSE::MessagingInterface::kPreLoadGame &&
		message->type != SKSE::MessagingInterface::kPostLoadGame &&
		message->type != SKSE::MessagingInterface::kNewGame) return;
	g_focusEpoch.Invalidate();
	{
		std::scoped_lock hotbarLock(g_hotbar.mutex);
		++g_hotbar.generation;
		g_hotbar.available = false;
		g_hotbar.loaded = message->type == SKSE::MessagingInterface::kNewGame ||
			(message->type == SKSE::MessagingInterface::kPostLoadGame && message->data != nullptr);
	}
	std::scoped_lock lock(g_resources.mutex);
	++g_resources.generation;
	g_resources.available = false;
	g_resources.loaded = message->type == SKSE::MessagingInterface::kNewGame ||
		(message->type == SKSE::MessagingInterface::kPostLoadGame && message->data != nullptr);
}

// Runs on the SKSE game task queue. No actor or GFx pointers cross threads.
void SamplePlayerResources(std::uint64_t generation)
{
	std::scoped_lock lock(g_resources.mutex);
	g_resources.pending = false;
	if (!g_resources.loaded || generation != g_resources.generation) return;
	g_resources.available = false;
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player || !player->Is3DLoaded()) return;
	constexpr std::array values{ RE::ActorValue::kHealth, RE::ActorValue::kMagicka, RE::ActorValue::kStamina };
	std::array<skycrpg::ui::ResourceValue, 3> resources{};
	for (std::size_t i = 0; i < values.size(); ++i) {
		resources[i] = { player->AsActorValueOwner()->GetActorValue(values[i]), player->GetActorValueMax(values[i]) };
		if (!skycrpg::ui::IsValidResource(resources[i])) return;
	}
	g_resources.values = resources;
	g_resources.sampled = ResourceClock::now();
	g_resources.available = true;
}

// One process-owned handler, matching pinned SKSE ScaleformCallbacks.h caching.
// It stores no movie/actor pointers; only copied numbers reach Scaleform.
class PlayerResources final : public RE::GFxFunctionHandler
{
public:
	void Call(Params& params) override
	{
		if (!params.retVal) return;
		params.retVal->SetUndefined();
		if (!params.movie) return;
		std::array<skycrpg::ui::ResourceValue, 3> resources{};
		bool available = false;
		bool request = false;
		std::uint64_t generation = 0;
		{
			std::scoped_lock lock(g_resources.mutex);
			available = g_resources.loaded && g_resources.available &&
				ResourceClock::now() - g_resources.sampled <= std::chrono::milliseconds(500);
			if (available) resources = g_resources.values;
			if (g_resources.loaded && !g_resources.pending) {
				g_resources.pending = true;
				generation = g_resources.generation;
				request = true;
			}
		}
		if (request) SKSE::GetTaskInterface()->AddTask([generation]() { SamplePlayerResources(generation); });
		if (!available) {
			LogUnavailable();
			return;
		}
		RE::GFxValue snapshot;
		params.movie->CreateObject(&snapshot);
		constexpr std::array currentNames{ "health", "magicka", "stamina" };
		constexpr std::array maxNames{ "healthMax", "magickaMax", "staminaMax" };
		for (std::size_t i = 0; i < resources.size(); ++i) {
			RE::GFxValue value;
			value.SetNumber(resources[i].current);
			if (!snapshot.SetMember(currentNames[i], value)) return;
			value.SetNumber(resources[i].maximum);
			if (!snapshot.SetMember(maxNames[i], value)) return;
		}
		*params.retVal = snapshot;
		if (!_loggedValid.exchange(true)) {
			std::array<char, 256> message{};
			std::snprintf(message.data(), message.size(), "HUD resource snapshot: first valid current/max H=%.3f/%.3f M=%.3f/%.3f S=%.3f/%.3f\n",
				resources[0].current, resources[0].maximum, resources[1].current, resources[1].maximum, resources[2].current, resources[2].maximum);
			AppendModuleLog(message.data());
		}
	}
private:
	void LogUnavailable()
	{
		if (!_loggedUnavailable.exchange(true)) {
			AppendModuleLog("HUD resource snapshot: first unavailable/invalid sample; native fallback retained\n");
		}
	}
	std::atomic_bool _loggedValid{};
	std::atomic_bool _loggedUnavailable{};
};

bool RegisterSkyCRPGUI(RE::GFxMovieView* movie, RE::GFxValue* root)
{
	if (!movie || !root) {
		return false;
	}
	RE::GFxValue menuFunction;
	static auto* menuHandler = new MenuFocusBlocked();
	movie->CreateFunction(&menuFunction, menuHandler);
	if (!root->SetMember("isMenuFocusBlocked", menuFunction)) return false;
	RE::GFxValue hotbarFunction;
	static auto* hotbarHandler = new HotbarSnapshot();
	movie->CreateFunction(&hotbarFunction, hotbarHandler);
	if (!root->SetMember("getHotbar", hotbarFunction)) return false;
	RE::GFxValue function;
	static auto* handler = new PlayerResources();
	movie->CreateFunction(&function, handler);
	if (!root->SetMember("getPlayerResources", function)) return false;
	if (!skycrpg::ui::minimap::Register(movie, root)) return false;
	if (!skycrpg::ui::follower::Register(movie, root)) return false;
	if (!skycrpg::ui::inventory::Register(movie, root)) return false;
	if (!skycrpg::ui::inventory::equipment::Register(movie, root)) return false;
	if (!skycrpg::ui::inventory::preview::Register(movie, root)) return false;
	RE::GFxValue value;
	value.SetNumber(g_hudSettings.scalePercent);
	if (!root->SetMember("hudScale", value)) return false;
	value.SetNumber(g_hudSettings.offsetX);
	if (!root->SetMember("hudOffsetX", value)) return false;
	value.SetNumber(g_hudSettings.offsetY);
	if (!root->SetMember("hudOffsetY", value)) return false;
	// This is the readiness/enable gate read by HUDMenu. Publish it only once
	// every numeric setting is present so a partial GFx object stays inert.
	value.SetBoolean(g_hudSettings.enabled);
	return root->SetMember("hudEnabled", value);
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
	if (!skseInterface || !skycrpg::ui::IsPinnedSkseVersion(skseInterface->skseVersion)) {
		return false;
	}
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
	skycrpg::ui::minimap::Initialize(g_hudSettings.enabled && g_hudSettings.replaceCompass, [] { return g_menuBlocked.load(std::memory_order_acquire); });
	LogHudSettings(iniPath, parsed);
	if (!g_hudSettings.enabled) AppendModuleLog("UI disabled by configuration; native UI and compass retained\n");
	skycrpg::ui::inventory::preview::Configure(iniPath);
	if (parsed.resetLayoutApplied) {
		AppendModuleLog("HUD reset-layout: startup defaults applied (100%, 0, 0); INI unchanged\n");
	}

	// The UI bridge and hotbar input use the pinned CommonLib runtime.
	SKSE::Init(reinterpret_cast<const SKSE::LoadInterface*>(skseInterface), false);
	const auto* tasks = SKSE::GetTaskInterface();
	const auto* messaging = SKSE::GetMessagingInterface();
	if (!tasks || !messaging || !messaging->RegisterListener("SKSE", ResourceLifecycle)) {
		AppendModuleLog("HUD resource task/lifecycle registration: FAIL; native HUD fallback retained\n");
		return true;
	}
	const auto* scaleform = SKSE::GetScaleformInterface();
	if (!scaleform || !scaleform->Register(&RegisterSkyCRPGUI, "SkyCRPGUI")) {
		AppendModuleLog("Scaleform settings registration: FAIL; native HUD fallback retained\n");
		return true;
	}
	AppendModuleLog("scope extension: HUD snapshots and native favorite hotbar dispatch\n");
	AppendModuleLog("Scaleform settings registration: PASS read-only\n");
	return true;
}

}
