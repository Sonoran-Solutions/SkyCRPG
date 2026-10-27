#include <SKSE/SKSE.h>
#include <RE/G/GFxFunctionHandler.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESObjectCELL.h>
#include <REX/W32/KERNEL32.h>
#include <skycrpg/skeleton.h>
#include "adapter.h"
#include "pins.h"
#include <bcrypt.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

namespace skycrpg::ui::minimap
{
namespace {
using Clock = std::chrono::steady_clock;
struct Cache {
    std::mutex mutex;
    bool enabled{}, exact{}, loaded{}, available{}, pending{}, gameplay{}, dead{};
    std::uint64_t generation{};
    std::uint32_t cell{}, token{};
    Clock::time_point sampled{}, requested{};
    bool (*menuBlocked)(){};
} cache;

bool HashMatches(const std::wstring& path, std::string_view expected)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectBytes = 0, resultBytes = 0;
    std::vector<UCHAR> object;
    std::array<UCHAR, 32> digest{};
    std::array<UCHAR, 65536> buffer{};
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &resultBytes, 0) >= 0 && objectBytes > 0) {
        object.resize(objectBytes);
        if (BCryptCreateHash(algorithm, &hash, object.data(), objectBytes, nullptr, 0, 0) >= 0) {
            bool failed = false;
            for (;;) {
                DWORD read = 0;
                if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) { failed = true; break; }
                if (!read) break;
                if (BCryptHashData(hash, buffer.data(), read, 0) < 0) { failed = true; break; }
            }
            if (!failed && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0) {
                constexpr char hex[] = "0123456789abcdef";
                std::string actual;
                for (const UCHAR byte : digest) { actual.push_back(hex[byte >> 4]); actual.push_back(hex[byte & 15]); }
                ok = actual == expected;
            }
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return ok;
}

void CheckInstall()
{
    constexpr std::array names{ L"DragonsEyeMinimap.dll", L"InfinityUI.dll", L"LocalMapUpgrade.dll" };
    bool exact = true;
    const auto logPath = skycrpg::LogDirectory() + L"SkyCRPG_UI.log";
    FILE* log = nullptr;
    _wfopen_s(&log, logPath.c_str(), L"a");
    for (std::size_t i = 0; i < kPins.size(); ++i) {
        std::wstring path;
        bool loaded = true;
        if (i < names.size()) {
            const auto module = REX::W32::GetModuleHandleW(names[i]);
            std::array<wchar_t, 32768> buffer{};
            const auto length = module ? REX::W32::GetModuleFileNameW(module, buffer.data(), static_cast<std::uint32_t>(buffer.size())) : 0;
            loaded = length > 0 && length < static_cast<std::uint32_t>(buffer.size());
            if (loaded) path.assign(buffer.data(), length);
        } else {
            path = L"Data/";
            path.append(kPins[i].path.begin(), kPins[i].path.end());
        }
        const bool match = loaded && HashMatches(path, kPins[i].sha256);
        if (log) std::fwprintf(log, L"Minimap preflight %ls: %ls\n", path.empty() ? names[i] : path.c_str(), match ? L"exact" : L"unavailable or mismatch");
        exact = exact && match;
    }
    if (log) { std::fwprintf(log, L"Minimap preflight exact=%d\n", exact); std::fclose(log); }
    std::scoped_lock lock(cache.mutex);
    cache.exact = exact;
}

void Sample(std::uint64_t generation)
{
    std::scoped_lock lock(cache.mutex);
    cache.pending = false;
    if (!cache.loaded || cache.generation != generation) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    const bool dead = player && player->IsDead();
    auto* cell = player ? player->GetParentCell() : nullptr;
    const auto id = cell ? cell->GetFormID() : 0;
    if (id != cache.cell || (dead && !cache.dead)) { ++cache.token; cache.cell = id; }
    cache.dead = dead;
    cache.gameplay = player && !dead && player->Is3DLoaded() && cell && cache.menuBlocked && !cache.menuBlocked();
    cache.sampled = Clock::now();
    cache.available = true;
}

class Handler final : public RE::GFxFunctionHandler {
public:
    void Call(Params& params) override {
        if (!params.retVal) return;
        params.retVal->SetUndefined();
        if (!params.movie) return;
        bool available, enabled, exact, gameplay, request = false;
        std::uint32_t token;
        std::uint64_t generation = 0;
        {
            std::scoped_lock lock(cache.mutex);
            const auto now = Clock::now();
            available = cache.loaded && cache.available && now - cache.sampled <= std::chrono::milliseconds(500);
            enabled = cache.enabled; exact = cache.exact;
            gameplay = cache.gameplay && cache.menuBlocked && !cache.menuBlocked();
            token = cache.token;
            if (cache.loaded && !cache.pending && now - cache.requested >= std::chrono::milliseconds(100) && SKSE::GetTaskInterface()) {
                cache.pending = true; cache.requested = now; generation = cache.generation; request = true;
            }
        }
        if (request) SKSE::GetTaskInterface()->AddTask([generation] { Sample(generation); });
        if (!available) return;
        RE::GFxValue object, value;
        params.movie->CreateObject(&object);
        value.SetBoolean(enabled); if (!object.SetMember("enabled", value)) return;
        value.SetBoolean(exact); if (!object.SetMember("exact", value)) return;
        value.SetBoolean(gameplay); if (!object.SetMember("gameplay", value)) return;
        value.SetNumber(token); if (!object.SetMember("cellToken", value)) return;
        *params.retVal = object;
    }
};
}
void Initialize(bool enabled, bool (*menuBlocked)()) { std::scoped_lock lock(cache.mutex); cache.enabled = enabled; cache.menuBlocked = menuBlocked; }
void OnMessage(SKSE::MessagingInterface::Message* message)
{
    if (!message) return;
    if (message->type == SKSE::MessagingInterface::kDataLoaded) { CheckInstall(); return; }
    if (message->type != SKSE::MessagingInterface::kPreLoadGame && message->type != SKSE::MessagingInterface::kPostLoadGame && message->type != SKSE::MessagingInterface::kNewGame) return;
    std::scoped_lock lock(cache.mutex);
    ++cache.generation; ++cache.token; cache.cell = 0; cache.dead = false; cache.available = false;
    cache.loaded = message->type == SKSE::MessagingInterface::kNewGame || (message->type == SKSE::MessagingInterface::kPostLoadGame && message->data != nullptr);
}
bool Register(RE::GFxMovieView* movie, RE::GFxValue* root)
{
    static auto* handler = new Handler();
    RE::GFxValue function;
    movie->CreateFunction(&function, handler);
    return root->SetMember("getMinimapCapability", function);
}
}
