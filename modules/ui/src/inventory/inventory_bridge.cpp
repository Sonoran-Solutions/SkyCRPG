// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "inventory_bridge.h"
#include <RE/G/GFxFunctionHandler.h>
#include <RE/G/GFxMovieView.h>
#include <RE/A/AlchemyItem.h>
#include <RE/E/EnchantmentItem.h>
#include <RE/T/TESBoundObject.h>
#include <RE/E/ExtraCharge.h>
#include <RE/E/ExtraEnchantment.h>
#include <RE/E/ExtraHealth.h>
#include <RE/E/ExtraHotkey.h>
#include <RE/E/ExtraOwnership.h>
#include <RE/E/ExtraPoison.h>
#include <RE/E/ExtraTextDisplayData.h>
#include <RE/E/ExtraWorn.h>
#include <RE/E/ExtraWornLeft.h>
#include <RE/I/InventoryMenu.h>
#include <RE/I/InventoryEntryData.h>
#include <RE/I/ItemList.h>
#include <RE/M/Main.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/T/TESEnchantableForm.h>
#include <RE/T/TESFile.h>
#include <RE/T/TESContainerChangedEvent.h>
#include <RE/T/TESDeathEvent.h>
#include <RE/T/TESEquipEvent.h>
#include <RE/U/UI.h>
#include <REX/W32/KERNEL32.h>
#include <skycrpg/skeleton.h>
#include "action_intent.h"
#include <atomic>
#include <cstdio>
#include <string_view>
#include <limits>
#include <mutex>

namespace skycrpg::ui::inventory {
namespace {
// Native observations are copied synchronously only on the main/UI dispatch
// thread. Off-thread GFx calls refuse; no task issues or retries an action.
std::recursive_mutex g_mutex;
InventorySnapshots g_snapshots;
ActionIntents g_intents;
std::optional<Selection> g_selection;
std::uint64_t g_nextToken{};
bool g_loaded{}, g_open{}, g_refreshQueued{};
std::set<std::uint64_t> g_loggedTokens;
struct NativeForm {
    FormIdentity base;
    int count{};
    std::vector<std::pair<ItemTraits, int>> instances;
    bool operator==(const NativeForm&) const = default;
};
std::vector<NativeForm> g_native;
std::atomic_bool g_dirty{true};

bool MainThread()
{
    const auto* main = RE::Main::GetSingleton();
    return main && main->threadID == REX::W32::GetCurrentThreadId();
}
void LogRefusal(std::uint64_t token, const char* reason)
{
    if (g_loggedTokens.size() >= 256 || !g_loggedTokens.insert(token).second) return;
    const auto path = skycrpg::LogDirectory() + L"SkyCRPG_UI.log";
    FILE* log = nullptr;
    if (_wfopen_s(&log, path.c_str(), L"a") == 0 && log) {
        std::fprintf(log, "Inventory refusal token=%llu reason=%s\n", static_cast<unsigned long long>(token), reason);
        std::fclose(log);
    }
}
void RequestListUpdate()
{
    if (g_refreshQueued) return;
    const auto* tasks = SKSE::GetTaskInterface();
    if (!tasks) return;
    g_refreshQueued = true;
    const auto stamp = g_snapshots.Get().stamp;
    tasks->AddUITask([stamp] {
        std::scoped_lock lock(g_mutex);
        g_refreshQueued = false;
        if (!g_loaded || !g_open || stamp.world != g_snapshots.Get().stamp.world ||
            stamp.session != g_snapshots.Get().stamp.session) return;
        auto* ui = RE::UI::GetSingleton();
        const auto menu = ui ? ui->GetMenu<RE::InventoryMenu>() : nullptr;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (menu && player && menu->GetRuntimeData().itemList) {
            menu->GetRuntimeData().itemList->Update(player);
            g_dirty.store(false);
        }
    });
}
// An engine event may have removed entries the menu list still references.
void Refresh()
{
    g_dirty.store(true);
    RequestListUpdate();
}
FormIdentity Form(RE::TESForm* form)
{
    if (!form) return {};
    auto* file = form->GetFile(0);
    return {form->GetFormID(), form->GetLocalFormID(), file ? std::string(file->GetFilename()) : std::string{}};
}
ItemTraits Traits(RE::TESBoundObject* object, RE::ExtraDataList* extra, RE::PlayerCharacter* player)
{
    ItemTraits traits;
    traits.base = Form(object);
    const auto* baseName = object->GetName();
    traits.name = baseName ? baseName : "";
    auto* enchantable = object->As<RE::TESEnchantableForm>();
    if (enchantable) traits.enchantment = Form(enchantable->formEnchanting);
    if (!extra) return traits;
    if (auto* value = extra->GetByType<RE::ExtraEnchantment>()) traits.enchantment = Form(value->enchantment);
    if (auto* value = extra->GetByType<RE::ExtraHealth>()) traits.temper = value->health;
    if (auto* value = extra->GetByType<RE::ExtraOwnership>()) traits.owner = Form(value->owner);
    if (auto* value = extra->GetByType<RE::ExtraTextDisplayData>()) traits.name = value->displayName.c_str();
    if (auto* value = extra->GetByType<RE::ExtraCharge>()) traits.charge = value->charge;
    if (auto* value = extra->GetByType<RE::ExtraPoison>()) {
        traits.poison = Form(value->poison); traits.poisonCount = value->count;
    }
    // A transient wrapper borrows this list only inside the callback. Its
    // destructor deletes the list container, never the borrowed ExtraDataList.
    RE::InventoryEntryData instance(object, 1);
    instance.AddExtraList(extra);
    traits.stolen = !instance.IsOwnedBy(player, true);
    traits.quest = extra->HasQuestObjectAlias();
    traits.worn = extra->HasType<RE::ExtraWorn>();
    traits.wornLeft = extra->HasType<RE::ExtraWornLeft>();
    traits.favorite = extra->HasType<RE::ExtraHotkey>();
    return traits;
}
// Overlays that open alongside the inventory without taking its context.
bool Passive(std::string_view name)
{
    return name == "HUD Menu" || name == "Cursor Menu" || name == "Fader Menu" || name == "Mist Menu";
}
bool Focused(RE::UI* ui, RE::InventoryMenu* inventory)
{
    if (!ui || !inventory || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) return false;
    if (ui->IsMenuOpen("Loading Menu") || ui->IsMenuOpen("Console") || ui->IsModalMenuOpen()) return false;
    for (const auto& menu : ui->menuStack) {
        if (!menu) return false;
        if (menu.get() == inventory) continue;
        if (menu->menuFlags.any(RE::UI_MENU_FLAGS::kApplicationMenu, RE::UI_MENU_FLAGS::kUsesCursor,
            RE::UI_MENU_FLAGS::kUsesMenuContext, RE::UI_MENU_FLAGS::kModal)) return false;
    }
    return true; // Inventory's own pause/cursor flags are expected.
}
bool Sample(std::uint32_t& selectedIndex)
{
    if (!MainThread() || !g_loaded || !g_open) return false;
    auto* ui = RE::UI::GetSingleton();
    const auto menu = ui ? ui->GetMenu<RE::InventoryMenu>() : nullptr;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || player->IsDead() || !player->Is3DLoaded() || !Focused(ui, menu.get())) return false;
    auto* list = menu->GetRuntimeData().itemList;
    if (!list || g_dirty.load()) { Refresh(); return false; }
    auto* selected = list->GetSelectedItem();
    if (!selected) return false;
    auto live = player->GetInventory([](RE::TESBoundObject&) { return true; }, true);
    std::vector<NativeForm> observed;
    for (auto& [object, inventory] : live) {
        if (!object || inventory.first <= 0 || !inventory.second) continue;
        NativeForm form{Form(object), inventory.first, {}};
        if (inventory.second->extraLists) for (auto* extra : *inventory.second->extraLists) {
            if (extra) form.instances.emplace_back(Traits(object, extra, player), extra->GetCount());
        }
        observed.push_back(std::move(form));
    }
    std::sort(observed.begin(), observed.end(), [](const NativeForm& left, const NativeForm& right) {
        return left.base.runtime < right.base.runtime;
    });
    // External favorite/name/temper changes may have no event. Reconcile the
    // displayed list, but keep validating: each row below is checked against
    // the live inventory, and Publish bumps the generation when rows differ.
    if (!g_native.empty() && g_native != observed) RequestListUpdate();
    g_native = std::move(observed);
    std::vector<Entry> entries;
    bool foundSelection = false;
    for (RE::BSTArray<RE::ItemList::Item*>::size_type index = 0; index < list->items.size(); ++index) {
        auto* item = list->items[index];
        if (!item || !item->data.objDesc || !item->data.objDesc->object) continue;
        auto* desc = item->data.objDesc;
        const auto actual = live.find(desc->object);
        if (actual == live.end() || actual->second.first <= 0 || !actual->second.second) return false;
        Entry entry{{index, Traits(desc->object, nullptr, player)}, item->data.GetCount(), false, item->data.GetEnabled()};
        if (entry.count > static_cast<std::uint32_t>(actual->second.first)) return false;
        bool first = true;
        if (desc->extraLists) for (auto* extra : *desc->extraLists) {
            if (!extra) continue;
            bool exists = false;
            if (actual->second.second->extraLists) for (auto* current : *actual->second.second->extraLists) {
                if (current == extra) { exists = true; break; }
            }
            if (!exists) return false; // Do not dereference a stale menu extra list.
            const auto traits = Traits(desc->object, extra, player);
            if (first) { entry.identity.traits = traits; first = false; }
            else if (entry.identity.traits != traits) entry.ambiguous = true;
        }
        if (!ValidTraits(entry.identity.traits)) entry.ambiguous = true;
        entries.push_back(std::move(entry));
        if (item == selected) { selectedIndex = index; foundSelection = true; }
    }
    if (!foundSelection) return false;
    g_snapshots.Publish(std::move(entries));
    return g_snapshots.Get().available;
}
void Invalidate(bool world = false)
{
    if (world) { g_loaded = false; g_snapshots.World(false); g_native.clear(); }
    else g_snapshots.Invalidate();
    g_selection.reset(); g_intents.Cancel(); g_dirty.store(true);
}
class Events final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
    public RE::BSTEventSink<RE::TESContainerChangedEvent>, public RE::BSTEventSink<RE::TESEquipEvent>,
    public RE::BSTEventSink<RE::TESDeathEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        std::scoped_lock lock(g_mutex);
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (std::string_view(event->menuName.c_str()) == RE::InventoryMenu::MENU_NAME) {
            g_open = event->opening; g_snapshots.Menu(g_open); g_intents.Invalidate();
            g_selection.reset(); g_native.clear(); g_loggedTokens.clear(); g_dirty.store(true);
            if (g_open) Refresh();
        } else if (!Passive(event->menuName.c_str())) {
            Invalidate(); g_intents.Invalidate(); // native context cancels held inputs and quantities
            if (g_open) Refresh();
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
        if (event && (event->oldContainer == 0x14 || event->newContainer == 0x14)) Changed();
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) override {
        if (event && event->actor && event->actor->GetFormID() == 0x14) Changed();
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*) override {
        if (event && event->dead && event->actorDying && event->actorDying->GetFormID() == 0x14) {
            std::scoped_lock lock(g_mutex); Invalidate(true); g_intents.Invalidate();
        }
        return RE::BSEventNotifyControl::kContinue;
    }
private:
    void Changed() { std::scoped_lock lock(g_mutex); Invalidate(); if (g_open) Refresh(); }
} g_events;

// One handler with an operation discriminator keeps registration additive.
enum class Operation { Snapshot, Validate, Edge, Cancel, View };
class Handler final : public RE::GFxFunctionHandler {
public:
    explicit Handler(Operation operation) : _operation(operation) {}
    void Call(Params& params) override {
        if (!params.retVal) return;
        params.retVal->SetUndefined();
        std::scoped_lock lock(g_mutex);
        if (_operation == Operation::Cancel) { g_intents.Cancel(); return; }
        if (_operation == Operation::View) {
            g_snapshots.View(); g_selection.reset(); g_intents.Cancel(); return;
        }
        if (_operation == Operation::Edge) {
            if (params.argCount == 2 && params.args[0].IsNumber() && params.args[1].IsBool()) {
                const double key = params.args[0].GetNumber();
                if (std::isfinite(key) && key >= 0 && key <= 65535) g_intents.Edge(static_cast<unsigned>(key), params.args[1].GetBool());
            }
            return;
        }
        std::uint32_t index = 0;
        if (!MainThread()) {
            // Live reads are unsafe here. Native validation stays authoritative
            // instead of trapping every action; the log marks the gate open.
            LogRefusal(0, "unsupported-thread; native behavior retained");
            if (_operation == Operation::Validate) params.retVal->SetBoolean(true);
            return;
        }
        if (_operation == Operation::Snapshot) {
            if (!Sample(index)) { g_selection.reset(); RequestListUpdate(); return; }
            const auto& snapshot = g_snapshots.Get();
            const auto* entry = snapshot.Find(index);
            if (!entry) return;
            if (!g_intents.Pending() || !g_intents.Pending()->quantity) {
                g_intents.Observe();
                g_selection = Selection{snapshot.stamp, *entry, ++g_nextToken};
            }
            if (!g_selection) return;
            params.movie->CreateObject(params.retVal);
            Number(*params.retVal, "token", static_cast<double>(g_selection->token));
            Number(*params.retVal, "selectedIndex", g_selection->entry.identity.nativeIndex);
            Number(*params.retVal, "world", static_cast<double>(snapshot.stamp.world));
            Number(*params.retVal, "session", static_cast<double>(snapshot.stamp.session));
            Number(*params.retVal, "generation", static_cast<double>(snapshot.stamp.inventory));
            Number(*params.retVal, "view", static_cast<double>(snapshot.stamp.view));
            RE::GFxValue rows; params.movie->CreateArray(&rows);
            for (const auto& row : snapshot.entries) {
                RE::GFxValue object; params.movie->CreateObject(&object);
                Number(object, "itemIndex", row.identity.nativeIndex);
                Number(object, "count", row.count);
                Number(object, "formID", row.identity.traits.base.runtime);
                Number(object, "localID", row.identity.traits.base.local);
                String(object, "plugin", row.identity.traits.base.plugin);
                String(object, "name", row.identity.traits.name);
                Number(object, "enchantment", row.identity.traits.enchantment.runtime);
                Number(object, "temper", row.identity.traits.temper);
                Boolean(object, "stolen", row.identity.traits.stolen);
                Boolean(object, "quest", row.identity.traits.quest);
                Number(object, "owner", row.identity.traits.owner.runtime);
                Number(object, "charge", row.identity.traits.charge);
                Number(object, "poison", row.identity.traits.poison.runtime);
                Number(object, "poisonCount", row.identity.traits.poisonCount);
                Boolean(object, "worn", row.identity.traits.worn);
                Boolean(object, "wornLeft", row.identity.traits.wornLeft);
                Boolean(object, "favorite", row.identity.traits.favorite);
                Boolean(object, "available", !row.ambiguous);
                rows.PushBack(object);
            }
            params.retVal->SetMember("entries", rows);
            return;
        }
        params.retVal->SetBoolean(false);
        if (params.argCount != 4 || !params.args[0].IsNumber() || !params.args[1].IsString() ||
            !params.args[2].IsNumber() || !params.args[3].IsString()) { LogRefusal(0, "arguments"); return; }
        const double rawToken = params.args[0].GetNumber();
        const double amount = params.args[2].GetNumber();
        if (!std::isfinite(rawToken) || rawToken < 1 || rawToken > 9007199254740991.0 || std::floor(rawToken) != rawToken ||
            !std::isfinite(amount) || amount < 1 || amount > std::numeric_limits<std::uint32_t>::max() || std::floor(amount) != amount) {
            LogRefusal(0, "arguments"); return;
        }
        const auto token = static_cast<std::uint64_t>(rawToken);
        const std::string_view actionName = params.args[1].GetString();
        const std::string_view phase = params.args[3].GetString();
        Action action;
        if (actionName == "activate") action = Action::Activate;
        else if (actionName == "favorite") action = Action::Favorite;
        else if (actionName == "drop") action = Action::Drop;
        else { LogRefusal(token, "action"); return; }
        if (!g_selection || token != g_selection->token) { LogRefusal(token, "stale-token"); RequestListUpdate(); return; }
        const bool sampled = Sample(index);
        const bool focused = sampled && index == g_selection->entry.identity.nativeIndex;
        auto result = Refusal::Unavailable;
        if (sampled) {
            if (phase == "confirm" && action == Action::Drop && g_intents.Pending() &&
                g_intents.Pending()->selection.token == token) {
                result = g_intents.Confirm(g_snapshots.Get(), focused, static_cast<std::uint32_t>(amount));
            } else if (phase == "dispatch" || (phase == "quantity" && action == Action::Drop)) {
                result = g_intents.Begin(*g_selection, g_snapshots.Get(), focused, action,
                    static_cast<std::uint32_t>(amount), phase == "quantity", true, true);
            }
        }
        if (result == Refusal::None) { params.retVal->SetBoolean(true); return; }
        LogRefusal(token, Reason(result));
        g_intents.Cancel(); RequestListUpdate();
    }
private:
    static void Number(RE::GFxValue& object, const char* key, double value) { RE::GFxValue field; field.SetNumber(value); object.SetMember(key, field); }
    static void Boolean(RE::GFxValue& object, const char* key, bool value) { RE::GFxValue field; field.SetBoolean(value); object.SetMember(key, field); }
    static void String(RE::GFxValue& object, const char* key, const std::string& value) { RE::GFxValue field; field.SetString(value.c_str()); object.SetMember(key, field); }
    Operation _operation;
};
}
bool Register(RE::GFxMovieView* movie, RE::GFxValue* root)
{
    if (!movie || !root) return false;
    static Handler* handlers[] = {new Handler(Operation::Snapshot), new Handler(Operation::Validate),
        new Handler(Operation::Edge), new Handler(Operation::Cancel), new Handler(Operation::View)};
    constexpr const char* names[] = {"getInventorySnapshot", "validateItemAction", "inventoryInputEdge", "cancelInventoryAction", "inventoryViewChanged"};
    for (std::size_t index = 0; index < std::size(handlers); ++index) {
        RE::GFxValue function; movie->CreateFunction(&function, handlers[index]);
        if (!root->SetMember(names[index], function)) return false;
    }
    return true;
}
void OnSkseMessage(SKSE::MessagingInterface::Message* message)
{
    if (!message) return;
    std::scoped_lock lock(g_mutex);
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        static bool registered = false;
        if (registered) return;
        auto* ui = RE::UI::GetSingleton();
        auto* events = RE::ScriptEventSourceHolder::GetSingleton();
        if (!ui || !events) return;
        ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_events);
        events->AddEventSink<RE::TESContainerChangedEvent>(&g_events);
        events->AddEventSink<RE::TESEquipEvent>(&g_events);
        events->AddEventSink<RE::TESDeathEvent>(&g_events);
        registered = true;
    } else if (message->type == SKSE::MessagingInterface::kPreLoadGame ||
        message->type == SKSE::MessagingInterface::kPostLoadGame || message->type == SKSE::MessagingInterface::kNewGame) {
        g_loaded = message->type == SKSE::MessagingInterface::kNewGame ||
            (message->type == SKSE::MessagingInterface::kPostLoadGame && message->data != nullptr);
        g_open = false; g_snapshots.World(g_loaded); g_intents.Invalidate();
        g_selection.reset(); g_native.clear(); g_dirty.store(true);
    }
}
}
