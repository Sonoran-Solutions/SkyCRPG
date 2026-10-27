// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "equipment_bridge.h"
#include "equipment_policy.h"
#include <RE/G/GFxFunctionHandler.h>
#include <RE/G/GFxMovieView.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/M/Main.h>
#include <RE/L/LoadingMenu.h>
#include <RE/U/UI.h>
#include <RE/I/InventoryMenu.h>
#include <RE/I/InventoryEntryData.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/E/ExtraTextDisplayData.h>
#include <RE/E/ExtraWorn.h>
#include <RE/E/ExtraWornLeft.h>
#include <REX/W32/KERNEL32.h>
#include <array>
#include <string>

namespace skycrpg::ui::inventory::equipment {
namespace {
using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;
static_assert(static_cast<std::uint32_t>(BipedSlot::kHead) == (1u << 0));
static_assert(static_cast<std::uint32_t>(BipedSlot::kHair) == (1u << 1));
static_assert(static_cast<std::uint32_t>(BipedSlot::kBody) == (1u << 2));
static_assert(static_cast<std::uint32_t>(BipedSlot::kHands) == (1u << 3));
static_assert(static_cast<std::uint32_t>(BipedSlot::kFeet) == (1u << 7));
static_assert(static_cast<std::uint32_t>(BipedSlot::kShield) == (1u << 9));
static_assert(static_cast<std::uint32_t>(BipedSlot::kCirclet) == (1u << 12));
static_assert(static_cast<std::uint32_t>(BipedSlot::kAmulet) == (1u << 5));
static_assert(static_cast<std::uint32_t>(BipedSlot::kRing) == (1u << 6));
struct Copy {
    std::uint32_t id{}, type{};
    std::string name;
    bool known{true};
};
Copy Read(RE::TESForm* form, RE::InventoryEntryData* entry, bool left)
{
    if (!form) return {};
    Copy result{form->GetFormID(), static_cast<std::uint32_t>(form->GetFormType()), {}, true};
    const char* name = form->GetName();
    if (name) result.name = name;
    // Copy an existing worn display name only; GetDisplayName may mutate extras.
    if (entry && entry->object == form && entry->extraLists) {
        for (auto* extra : *entry->extraLists) {
            if (!extra || (left ? !extra->HasType<RE::ExtraWornLeft>() : !extra->HasType<RE::ExtraWorn>())) continue;
            const auto* display = extra->GetByType<RE::ExtraTextDisplayData>();
            if (display && !display->displayName.empty()) result.name = display->displayName.c_str();
            break;
        }
    }
    return result;
}
class Handler final : public RE::GFxFunctionHandler {
public:
    void Call(Params& params) override
    {
        if (!params.retVal || !params.movie) return;
        params.retVal->SetUndefined();
        const auto* main = RE::Main::GetSingleton();
        if (!main || main->threadID != REX::W32::GetCurrentThreadId()) return;
        auto* ui = RE::UI::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!ui || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) ||
            ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || !player ||
            !player->Is3DLoaded() || player->IsDead() || !player->GetActorRuntimeData().currentProcess) return;
        std::array<Copy, SlotCount> slots;
        auto inventory = player->GetInventory([](RE::TESBoundObject& object) { return object.IsArmor(); }, true);
        for (auto& [form, value] : inventory) {
            if (!form || value.first <= 0 || !value.second || !value.second->IsWorn()) continue;
            const auto* armor = form->As<RE::TESObjectARMO>();
            if (!armor) continue;
            const auto covered = ArmorSlots(armor->GetSlotMask().underlying());
            const auto copy = Read(form, value.second.get(), covered[static_cast<std::size_t>(Slot::Left)]);
            for (std::size_t index = 0; index < SlotCount; ++index) {
                if (!covered[index]) continue;
                if (Conflicting(slots[index].id, copy.id)) slots[index].known = false;
                else if (slots[index].known) slots[index] = copy;
            }
        }
        // Actual hands include spells/torches, not just inventory weapons.
        for (const bool left : {true, false}) {
            const auto index = static_cast<std::size_t>(left ? Slot::Left : Slot::Right);
            auto* form = player->GetEquippedObject(left);
            if (!form) continue; // A worn shield remains visible when native hand is empty.
            if (Conflicting(slots[index].id, form->GetFormID())) slots[index].known = false;
            else if (slots[index].known) slots[index] = Read(form, player->GetEquippedEntryData(left), left);
        }
        params.movie->CreateArray(params.retVal);
        for (const auto& copy : slots) {
            RE::GFxValue object, field;
            params.movie->CreateObject(&object);
            field.SetBoolean(copy.known); object.SetMember("known", field);
            field.SetBoolean(copy.known && copy.id == 0); object.SetMember("empty", field);
            field.SetNumber(copy.type); object.SetMember("formType", field);
            field.SetString(copy.name.c_str()); object.SetMember("name", field);
            params.retVal->PushBack(object);
        }
    }
};
}
bool Register(RE::GFxMovieView* movie, RE::GFxValue* root)
{
    if (!movie || !root) return false;
    static auto* handler = new Handler();
    RE::GFxValue function;
    movie->CreateFunction(&function, handler);
    return root->SetMember("getInventoryEquipment", function);
}
}
