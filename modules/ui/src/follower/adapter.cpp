#include "adapter.h"
#include "policy.h"
#include <RE/G/GFxFunctionHandler.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/T/TESObjectCELL.h>
#include <chrono>
#include <mutex>
#include <string>
#include <cstdint>

namespace skycrpg::ui::follower {
namespace {
using Clock = std::chrono::steady_clock;
struct Cache {
    std::mutex mutex;
    std::uint64_t generation{};
    bool loaded{};
    bool pending{};
    bool available{};
    RE::ActorHandle actor{};
    RE::ActorHandle deadActor{};
    RE::FormID cell{};
    State state{State::unknown};
    std::string name;
    float health{};
    Clock::time_point sampled{};
} cache;

void Sample(std::uint64_t generation)
{
    std::scoped_lock lock(cache.mutex);
    cache.pending = false;
    if (!cache.loaded || generation != cache.generation) return;
    cache.available = false;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || player->IsDead() || !player->Is3DLoaded() || !player->GetParentCell()) {
        cache.actor.reset(); cache.deadActor.reset(); cache.cell = 0; cache.name.clear(); return;
    }
    const auto cell = player->GetParentCell()->GetFormID();
    if (cell != cache.cell) { cache.actor.reset(); cache.deadActor.reset(); cache.name.clear(); cache.cell = cell; }
    auto* processes = RE::ProcessLists::GetSingleton();
    if (!processes) { cache.actor.reset(); cache.name.clear(); return; }
    Observation observation{true};
    RE::ActorHandle selected;
    for (const auto& handle : processes->highActorHandles) {
        if (cache.deadActor && handle == cache.deadActor) continue;
        auto actor = handle.get();
        if (!actor || !actor->IsPlayerTeammate()) continue;
        ++observation.count;
        if (observation.count == 1) selected = handle;
    }
    if (observation.count == 0 && cache.actor) {
        auto actor = cache.actor.get();
        if (!actor) {
            observation.available = false;
        } else if (actor->IsDead()) {
            observation.count = 1; observation.dead = true; cache.deadActor = cache.actor;
        } else if (actor->IsPlayerTeammate()) {
            observation.count = 1; selected = cache.actor;
        }
    }
    std::string name;
    if (observation.count == 1 && !observation.dead) {
        auto actor = selected.get();
        if (!actor) observation.available = false;
        else if (actor->IsDead()) { observation.dead = true; cache.deadActor = selected; }
        else {
            observation.humanoid = actor->IsHumanoid();
            observation.loaded = actor->Is3DLoaded() && actor->GetParentCell() != nullptr;
            if (observation.loaded && observation.humanoid) {
                const char* displayName = actor->GetDisplayFullName();
                if (displayName) name = displayName;
                observation.named = !name.empty();
                observation.current = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
                observation.maximum = actor->GetActorValueMax(RE::ActorValue::kHealth);
            }
        }
    }
    cache.state = Classify(observation);
    if (cache.state == State::live) {
        cache.actor = selected; cache.name = name;
        cache.health = observation.current / observation.maximum;
    } else if (cache.state == State::unknown && observation.count == 1) {
        cache.actor = selected;
    } else {
        cache.actor.reset(); cache.name.clear();
    }
    cache.sampled = Clock::now(); cache.available = true;
}

class Snapshot final : public RE::GFxFunctionHandler {
public:
    void Call(Params& params) override
    {
        if (!params.retVal) return;
        params.retVal->SetUndefined();
        if (!params.movie) return;
        State state = State::unknown;
        std::string name;
        float health = 0.0F;
        bool request = false;
        std::uint64_t generation = 0;
        {
            std::scoped_lock lock(cache.mutex);
            if (cache.loaded && cache.available && Clock::now() - cache.sampled <= std::chrono::milliseconds(500)) {
                state = cache.state; name = cache.name; health = cache.health;
            }
            if (cache.loaded && !cache.pending) {
                cache.pending = true; generation = cache.generation; request = true;
            }
        }
        if (request) SKSE::GetTaskInterface()->AddTask([generation] { Sample(generation); });
        RE::GFxValue result, value;
        params.movie->CreateObject(&result);
        const char* label = "unknown";
        switch (state) {
        case State::none: label = "none"; break;
        case State::live: label = "live"; break;
        case State::dead: label = "dead"; break;
        case State::unsupported: label = "unsupported"; break;
        case State::unknown: break;
        }
        value.SetString(label); if (!result.SetMember("state", value)) return;
        value.SetString(name.c_str()); if (!result.SetMember("name", value)) return;
        if (state == State::live) {
            value.SetNumber(health); if (!result.SetMember("health", value)) return;
        }
        *params.retVal = result;
    }
};
}
bool Register(RE::GFxMovieView* movie, RE::GFxValue* root)
{
    if (!movie || !root) return false;
    RE::GFxValue function;
    static auto* handler = new Snapshot();
    movie->CreateFunction(&function, handler);
    return root->SetMember("getFollowerStatus", function);
}
void OnMessage(SKSE::MessagingInterface::Message* message)
{
    if (!message || (message->type != SKSE::MessagingInterface::kPreLoadGame &&
        message->type != SKSE::MessagingInterface::kPostLoadGame &&
        message->type != SKSE::MessagingInterface::kNewGame)) return;
    std::scoped_lock lock(cache.mutex);
    ++cache.generation; cache.available = false; cache.actor.reset(); cache.deadActor.reset(); cache.name.clear(); cache.cell = 0;
    cache.loaded = message->type == SKSE::MessagingInterface::kNewGame ||
        (message->type == SKSE::MessagingInterface::kPostLoadGame && message->data != nullptr);
}
}
