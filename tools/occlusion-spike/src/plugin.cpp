// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// Disposable instrumentation. SmoothCam owns all normal camera transforms.
#define NOMINMAX // before CommonLib's Windows headers; matches skycrpg/skeleton.h
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <skycrpg/skeleton.h>
#include "classifier.h"
#include "config.h"
#include "restore.h"
#include "runtime_policy.h"
#include "../../testprobe/src/json_line.h"
#include "../../testprobe/src/perf_policy.h"
#pragma warning(push)
#pragma warning(disable: 4191)
#define SMOOTHCAM_API_COMMONLIB
#include "../../../modules/camera/include/third_party/SmoothCamAPI.h"
#pragma warning(pop)
#include <bcrypt.h>
#include <intrin.h>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <memory>
#include <vector>

namespace {
namespace policy = skycrpg::occlusion;
namespace json = skycrpg::testprobe;
using policy::Mode;
constexpr REL::Version runtime{ 1, 7, 104, 0 };
constexpr std::uint32_t armKey = 0x47, stopKey = 0x49; // KP7 / KP9
constexpr char exeHash[] = "846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f";
constexpr char libraryHash[] = "8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842";
constexpr char smoothHash[] = "b20bf8a276bf76fbe35277dd9b25c64c57d17bfdcc20daa74a5f941c90f563da";
constexpr std::array<std::uint8_t, 14> casterPrologue{ 0x48,0x8b,0xc4,0x4c,0x89,0x48,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55 };
constexpr std::array<std::uint8_t, 16> roomPrologue{ 0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0xfa };
static_assert(sizeof(RE::hkpRootCdPoint) == 64);
static_assert(offsetof(RE::hkpRootCdPoint, rootCollidableB) == 0x30);
static_assert(offsetof(RE::NiAlphaProperty, alphaFlags) == 0x30);

struct Config {
	Mode mode{ Mode::observe };
	std::string name{ "observe" };
	policy::IDs refs{}, bases{};
	std::uint32_t selected{}, maxRecords{ 100000 };
	float alpha{ .35f }, seedDistance{ 1500 };
};
struct Hit {
	std::uint64_t sequence{}, frame{}, caller{}, object{}, collision{}, capture{};
	std::uint32_t callingThread{}, mainThread{}, depth{};
	std::uint32_t ref{}, base{}, cell{}, formType{}, layer{};
	bool hit{}, smooth{}, matched{}, candidate{}, metadata{};
	float hull{}, normal[4]{};
	std::array<float, 4> start{}, goal{}, end{};
	char model[260]{}, node[128]{};
};
struct FadePart {
	RE::NiPointer<RE::BSGeometry> geometry;
	RE::NiPointer<RE::BSShaderProperty> shader;
	RE::NiPointer<RE::NiAlphaProperty> alphaProperty, installedAlpha;
	float materialAlpha{};
	RE::BSTSmartPointer<RE::BSLightingShaderMaterialBase> material;
	std::uint32_t sharedGeometries{};
	std::uint16_t flags{}, appliedFlags{};
	std::uint8_t threshold{};
};
struct Fade {
	RE::NiPointer<RE::NiAVObject> root;
	RE::ObjectRefHandle handle;
	std::vector<FadePart> parts;
	bool culled{}, applied{};
};
struct RoomSnapshot { policy::Point eye{}; };
struct State {
	Config config;
	std::wstring ini;
	FILE* output{};
	LARGE_INTEGER frequency{};
	std::atomic_bool enabled{}, armed{}, eligible{}, capturing{};
	std::atomic_bool loading{ true }, killed{}, roomActive{}, inconclusive{};
	bool initialized{};
	policy::Requests requests;
	std::atomic_uint64_t capture{}, matched{}, discarded{}, keptBehind{}, sampledOut{};
	std::atomic_uint64_t frame{}, hookCalls{}, missed{}, seedCalls{}, seedClaims{};
	std::atomic_uint64_t nextSequence{}, captureMissed{};
	std::uint64_t records{};
	std::uint32_t cell{};
	std::string captureLabel{ "unlabeled" };
	double lastPoll{}, lastReport{}, origin{}, lastFrame{};
	std::mutex queueMutex, perfMutex, logMutex;
	std::array<Hit, 256> queue{};
	std::size_t pending{};
	policy::FrameRing<65536> frames, intervals, hooks, originals, adapters;
	json::FrameChain chain;
	std::atomic<std::shared_ptr<const RoomSnapshot>> room;
	Fade fade;
	SmoothCamAPI::IVSmoothCam3* provider{};
	HMODULE smooth{};
};
// Process-lifetime storage: do not run scene destruction under the loader lock.
State* g{};
using Caster = bool(*)(void*, RE::bhkWorld*, float*, float*, RE::hkpRootCdPoint*, RE::TESObjectREFR**, float);
Caster originalCaster{};
using RoomTest = bool(*)(RE::BSMultiBoundRoom*, const RE::NiPoint3&);
RoomTest originalRoom{};

double Now()
{
	LARGE_INTEGER value{};
	QueryPerformanceCounter(&value);
	return static_cast<double>(value.QuadPart) * 1000 / static_cast<double>(g->frequency.QuadPart);
}
bool OnMain()
{
	const auto main = RE::Main::GetSingleton();
	return policy::MainThread(GetCurrentThreadId(), main ? main->threadID : 0);
}
void Request(unsigned flags)
{
	if (flags != policy::armRequest) { g->armed = false; g->roomActive = false; g->eligible = false; }
	g->requests.post(flags);
}
void Emit(std::string_view event, json::Object fields = {})
{
	if (!g || !g->output) return;
	std::scoped_lock lock(g->logMutex);
	fields.String("event", event).String("mode", g->config.name).Numeric("frame", g->frame.load())
		.Numeric("t_ms", Now() - g->origin).Numeric("capture_id", g->capture.load()).Numeric("thread_id", GetCurrentThreadId());
	const auto line = fields.Json() + "\n";
	if (std::fwrite(line.data(), 1, line.size(), g->output) != line.size()) {
		g->enabled = false; g->armed = false; g->inconclusive = true;
		OutputDebugStringA("SkyCRPG_OcclusionSpike: log failure; mutations disabled\n");
	}
}
bool Flush()
{
	std::scoped_lock lock(g->logMutex);
	if (std::fflush(g->output) == 0 && g->enabled) return true;
	g->enabled = false; g->armed = false; g->inconclusive = true;
	return false;
}
void Inconclusive(std::string_view reason)
{
	if (!g->inconclusive.exchange(true)) {
		Emit("inconclusive", json::Object{}.String("reason", reason)); Flush();
	}
	Request(policy::stopRequest);
}
bool HashFile(const std::filesystem::path& path, std::string_view expected)
{
	std::ifstream file(path, std::ios::binary);
	BCRYPT_ALG_HANDLE algorithm{};
	BCRYPT_HASH_HANDLE hash{};
	if (!file || BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
	bool ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
	std::array<unsigned char, 65536> buffer{};
	while (ok && file) {
		file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
		if (file.gcount() > 0) ok = BCryptHashData(hash, buffer.data(), static_cast<ULONG>(file.gcount()), 0) >= 0;
	}
	std::array<unsigned char, 32> digest{};
	ok = ok && file.eof() && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
	if (hash) BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(algorithm, 0);
	std::string result;
	constexpr char digits[] = "0123456789abcdef";
	for (auto byte : digest) { result += digits[byte >> 4]; result += digits[byte & 15]; }
	return ok && result == expected;
}
bool HashModule(HMODULE module, std::string_view expected)
{
	std::array<wchar_t, 32768> path{};
	return module && GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size())) && HashFile(path.data(), expected);
}
std::string Setting(const char* key, const char* fallback)
{
	std::array<char, 2048> text{};
	const auto path = std::filesystem::path(g->ini).string();
	const auto size = GetPrivateProfileStringA("Spike", key, fallback, text.data(), static_cast<DWORD>(text.size()), path.c_str());
	if (size >= text.size() - 1) return "invalid_truncated_setting";
	return text.data();
}
bool LoadConfig()
{
	auto& c = g->config;
	c.name = Setting("sMode", "observe"); c.mode = policy::ParseMode(c.name);
	if (c.mode == Mode::invalid || !policy::ParseIDs(Setting("sReferenceIDs", ""), c.refs) ||
		!policy::ParseIDs(Setting("sBaseFormIDs", ""), c.bases)) return false;
	policy::IDs selected{};
	if (!policy::ParseIDs(Setting("sSelectedReference", ""), selected) || selected[1]) return false;
	c.selected = selected[0];
	auto number = [](const std::string& text, float& value) {
		const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
		return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::isfinite(value);
	};
	if (!number(Setting("fAlpha", "0.35"), c.alpha) || !policy::ValidAlpha(c.alpha) ||
		!number(Setting("fSeedDistance", "1500"), c.seedDistance) || c.seedDistance < 0 || c.seedDistance > 5000) return false;
	const auto count = Setting("iMaxHitRecords", "100000");
	const auto parsed = std::from_chars(count.data(), count.data() + count.size(), c.maxRecords);
	return parsed.ec == std::errc{} && parsed.ptr == count.data() + count.size() && c.maxRecords > 0 && c.maxRecords <= 100000;
}
bool StaticReference(RE::TESObjectREFR* ref)
{
	const auto base = ref ? ref->GetBaseObject() : nullptr;
	return base && (base->Is(RE::FormType::Static) || base->Is(RE::FormType::MovableStatic));
}
policy::Point Point(const RE::NiPoint3& p) { return { p.x, p.y, p.z }; }
RE::NiPoint3 Native(policy::Point p) { return { p.x, p.y, p.z }; }
bool Eligible()
{
	if (!OnMain() || !g->enabled || g->loading || !g->provider || !g->provider->IsCameraEnabled() ||
		g->provider->GetCameraOwner() != SKSE::kInvalidPluginHandle) return false;
	const auto player = RE::PlayerCharacter::GetSingleton();
	const auto camera = RE::PlayerCamera::GetSingleton();
	const auto ui = RE::UI::GetSingleton();
	const auto controls = RE::PlayerControls::GetSingleton();
	const auto map = RE::ControlMap::GetSingleton();
	if (!player || !player->GetParentCell() || !player->Is3DLoaded() || player->IsDead() || !camera ||
		!camera->currentState || camera->currentState->id != RE::CameraState::kThirdPerson || !ui ||
		ui->GameIsPaused() || ui->IsApplicationMenuOpen() || ui->IsMenuOpen("Loading Menu") || ui->IsMenuOpen("Dialogue Menu") ||
		!controls || !map || !map->IsLookingControlsEnabled() || !map->IsPOVSwitchControlsEnabled()) return false;
	const auto actor = player->AsActorState();
	return !player->IsOnMount() && !player->IsInKillMove() && !actor->IsSwimming() && !actor->IsBleedingOut() &&
		actor->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && !controls->data.povScriptMode &&
		!controls->data.povBeastMode && !controls->data.fovSlideMode &&
		!static_cast<RE::ThirdPersonState*>(camera->currentState.get())->stateNotActive;
}
void Drain();
void Summary(std::string_view reason);
void Restore(std::string_view reason)
{
	if (!OnMain()) { Request(policy::stopRequest); return; }
	if (g->capturing.exchange(false)) { Drain(); Summary(reason); }
	g->armed = false;
	g->roomActive = false;
	auto& f = g->fade;
	if (!f.applied) return;
	bool ok = true;
	if (g->config.mode == Mode::appCulled) {
		const bool current = f.root->GetAppCulled();
		// On conflict, do not overwrite a different writer's state.
		bool restored = current;
		ok = policy::RestoreValue(restored, f.culled, true);
		f.root->SetAppCulled(restored);
		Emit("restore_part", json::Object{}.Boolean("original_culled", f.culled).Boolean("current_culled", current)
			.Boolean("restored_culled", f.root->GetAppCulled()).String("result", ok ? "PASS" : "FAIL"));
	} else for (auto& part : f.parts) {
		auto& data = part.geometry->GetGeometryRuntimeData();
		bool partOK = data.shaderProperty.get() == part.shader.get() && part.shader->material == part.material.get();
		// The retained material remains alive even if the shader swaps it. Restore
		// only our alpha value on that original allocation; never write the replacement.
		const bool alphaOK = policy::RestoreValue(part.material->materialAlpha, part.materialAlpha, g->config.alpha);
		partOK = partOK && alphaOK;
		if (g->config.mode == Mode::alphaProperty) {
			const bool restored = policy::RestoreValue(data.alphaProperty, part.alphaProperty, part.installedAlpha);
			partOK = partOK && restored;
		}
		ok = ok && partOK;
		Emit("restore_part", json::Object{}.Numeric("geometry", reinterpret_cast<std::uintptr_t>(part.geometry.get()))
			.Numeric("original_alpha", part.materialAlpha).Raw("current_alpha", data.shaderProperty.get() == part.shader.get() && part.shader->material == part.material.get() ? json::Number(part.material->materialAlpha) : "null")
			.Numeric("current_alpha_property", reinterpret_cast<std::uintptr_t>(data.alphaProperty.get())).Boolean("identity_match", data.shaderProperty.get() == part.shader.get())
			.String("result", partOK ? "PASS" : "FAIL"));
	}
	Emit("restore", json::Object{}.String("reason", reason).String("result", ok ? "PASS" : "FAIL"));
	if (!ok) { g->enabled = false; g->killed = true; }
	f = {};
	std::fflush(g->output);
}
void ApplyFade()
{
	if (!Eligible() || !g->config.selected) { Emit("refused", json::Object{}.String("reason", "unsafe_state_or_no_selected_reference")); return; }
	auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(g->config.selected);
	const auto player = RE::PlayerCharacter::GetSingleton();
	if (!StaticReference(ref) || ref->GetParentCell() != player->GetParentCell() || !ref->Get3D()) {
		Emit("refused", json::Object{}.String("reason", "selected_reference_not_loaded_static_in_player_cell")); return;
	}
	auto& f = g->fade;
	f.root.reset(ref->Get3D()); f.handle = ref->GetHandle(); f.culled = f.root->GetAppCulled();
	bool valid = true;
	if (g->config.mode != Mode::appCulled) {
		RE::BSVisit::TraverseScenegraphGeometries(f.root.get(), [&](RE::BSGeometry* geometry) {
			if (f.parts.size() == 128) { Inconclusive("fade_geometry_cap"); valid = false; return RE::BSVisit::BSVisitControl::kStop; }
			auto& data = geometry->GetGeometryRuntimeData();
			auto shader = geometry->lightingShaderProp_cast();
			if (!shader || !shader->material || shader->material->GetType() != RE::BSShaderMaterial::Type::kLighting) {
				valid = false; return RE::BSVisit::BSVisitControl::kStop;
			}
			FadePart part;
			part.geometry.reset(geometry); part.shader.reset(shader); part.material.reset(static_cast<RE::BSLightingShaderMaterialBase*>(shader->material));
			part.materialAlpha = part.material->materialAlpha; part.alphaProperty = data.alphaProperty;
			if (!std::isfinite(part.materialAlpha)) valid = false;
			if (part.alphaProperty) {
				part.flags = part.alphaProperty->alphaFlags; part.threshold = part.alphaProperty->alphaThreshold;
			}
			if (g->config.mode == Mode::alphaProperty) {
				// Audited native no-argument factory, ID 70687. A private property
				// also supports opaque meshes whose original property is null.
				using Factory = RE::NiAlphaProperty*(*)();
				const REL::Relocation<Factory> factory{ REL::ID(70687) };
				part.installedAlpha.reset(factory());
				if (!part.installedAlpha) { valid = false; return RE::BSVisit::BSVisitControl::kStop; }
				part.appliedFlags = static_cast<std::uint16_t>((part.flags & ~0x03ffu) | 1u | (6u << 1) | (7u << 5));
				part.installedAlpha->alphaFlags = part.appliedFlags;
				part.installedAlpha->alphaThreshold = part.threshold;
			}
			f.parts.push_back(std::move(part));
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		valid = valid && !f.parts.empty();
	}
	if (!valid) {
		Emit("refused", json::Object{}.String("reason", "unsupported_material_factory_failure_or_geometry_cap")); f = {}; return;
	}
	if (g->config.mode != Mode::appCulled) {
		std::uint32_t scanned{}, visited{};
		bool capped = false;
		const auto root = RE::Main::WorldRootNode();
		if (!root) { Emit("refused", json::Object{}.String("reason", "sharing_scene_unavailable")); f = {}; return; }
		RE::BSVisit::TraverseScenegraphObjects(root, [&](RE::NiAVObject* object) {
			if (++visited > 32768) { capped = true; return RE::BSVisit::BSVisitControl::kStop; }
			const auto geometry = object->AsGeometry();
			if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
			if (scanned == 16384) { capped = true; return RE::BSVisit::BSVisitControl::kStop; }
			++scanned;
			const auto shader = geometry->lightingShaderProp_cast();
			for (auto& part : f.parts)
				if (geometry != part.geometry.get() && shader && shader->material == part.material.get()) ++part.sharedGeometries;
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		for (const auto& part : f.parts) Emit("material_sharing", json::Object{}
			.Numeric("material", reinterpret_cast<std::uintptr_t>(part.material.get())).Numeric("geometry", reinterpret_cast<std::uintptr_t>(part.geometry.get()))
			.Numeric("other_live_geometries", part.sharedGeometries).Numeric("scanned_geometries", scanned).Numeric("visited_objects", visited).Boolean("scan_capped", capped)
			.String("scope", "Main_WorldRootNode_attached_geometry_only"));
		if (capped) { Inconclusive("material_sharing_scan_cap"); f = {}; return; }
	}
	// Capture and persist every original before the first write, including shared pointers.
	Emit("original", json::Object{}.String("reference", json::FormID(g->config.selected)).Boolean("culled", f.culled).Numeric("parts", f.parts.size()));
	for (const auto& part : f.parts) Emit("original_part", json::Object{}.Numeric("geometry", reinterpret_cast<std::uintptr_t>(part.geometry.get()))
		.Numeric("material", reinterpret_cast<std::uintptr_t>(part.material.get())).Numeric("alpha_property", reinterpret_cast<std::uintptr_t>(part.alphaProperty.get()))
		.Numeric("material_alpha", part.materialAlpha).Numeric("alpha_flags", part.flags).Numeric("alpha_threshold", part.threshold));
	if (!Flush()) { f = {}; return; }
	f.applied = true;
	if (g->config.mode == Mode::appCulled) f.root->SetAppCulled(true);
	else {
		f.root->UpdateMaterialAlpha(g->config.alpha, false);
		if (g->config.mode == Mode::alphaProperty)
			for (auto& part : f.parts) part.geometry->GetGeometryRuntimeData().alphaProperty = part.installedAlpha;
	}
	for (const auto& part : f.parts) Emit("apply_part", json::Object{}.Numeric("geometry", reinterpret_cast<std::uintptr_t>(part.geometry.get()))
		.Numeric("material_alpha", part.material->materialAlpha).Numeric("alpha_property", reinterpret_cast<std::uintptr_t>(part.geometry->GetGeometryRuntimeData().alphaProperty.get()))
		.Numeric("alpha_flags", part.installedAlpha ? part.installedAlpha->alphaFlags : part.flags));
	Emit("apply", json::Object{}.String("reference", json::FormID(g->config.selected)).Numeric("alpha", g->config.alpha)
		.String("state", "captured_before_write").Boolean("sharing_scan_bounded", true));
	std::fflush(g->output);
}
void CheckFade()
{
	if (!OnMain()) { Request(policy::stopRequest); return; }
	auto& f = g->fade;
	if (!f.applied) return;
	auto ref = f.handle.get();
	if (!ref || ref->Get3D() != f.root.get()) { Restore("reference_or_3d_changed"); return; }
	bool reset = g->config.mode == Mode::appCulled && !f.root->GetAppCulled();
	for (const auto& part : f.parts) {
		const auto& data = part.geometry->GetGeometryRuntimeData();
		reset = reset || data.shaderProperty.get() != part.shader.get() || part.shader->material != part.material.get();
		if (data.shaderProperty.get() == part.shader.get() && part.shader->material == part.material.get())
			reset = reset || part.material->materialAlpha != g->config.alpha;
		if (g->config.mode == Mode::alphaProperty) reset = reset || data.alphaProperty != part.installedAlpha ||
			part.installedAlpha->alphaFlags != part.appliedFlags || part.installedAlpha->alphaThreshold != part.threshold;
	}
	if (reset) { Emit("engine_reset", json::Object{}.String("attribution", "engine_or_other_writer")); Restore("engine_reset"); }
}
void Miss()
{
	++g->missed;
	if (g->capturing) ++g->captureMissed;
}
void RecordTime(double start, double beforeOriginal, double originalEnd, double end)
{
	if (!g->capturing) return;
	std::unique_lock lock(g->perfMutex, std::try_to_lock);
	if (!lock.owns_lock()) { Miss(); return; }
	g->hooks.push(end - start); g->originals.push(originalEnd - beforeOriginal); g->adapters.push((end - start) - (originalEnd - beforeOriginal));
}
json::Object Vector(const std::array<float, 4>& value);
void LogHit(const Hit& hit, std::string_view event)
{
	Emit(event, json::Object{}.Numeric("sequence", hit.sequence).Numeric("contact_frame", hit.frame)
		.Numeric("contact_capture_id", hit.capture).Numeric("calling_thread_id", hit.callingThread).Numeric("main_thread_id", hit.mainThread)
		.Numeric("depth", hit.depth).Numeric("caller", hit.caller).Boolean("smoothcam_caller", hit.smooth).Boolean("hit", hit.hit).Numeric("collision", hit.collision)
		.Boolean("contact_metadata_available", hit.metadata).Numeric("object", hit.object).String("reference", json::FormID(hit.ref)).String("base_form", json::FormID(hit.base))
		.String("cell", json::FormID(hit.cell)).Raw("base_form_type", hit.base ? json::Number(hit.formType) : "null").Raw("layer", hit.metadata ? json::Number(hit.layer) : "null").Numeric("hull", hit.hull)
		.Raw("normal_x", hit.metadata ? json::Number(hit.normal[0]) : "null").Raw("normal_y", hit.metadata ? json::Number(hit.normal[1]) : "null")
		.Raw("normal_z", hit.metadata ? json::Number(hit.normal[2]) : "null").Raw("separation", hit.metadata ? json::Number(hit.normal[3]) : "null")
		.String("model", hit.model).String("node", hit.node).Boolean("allowlist_match", hit.matched).Boolean("overhead_candidate", hit.candidate)
		.Raw("start", Vector(hit.start).Json()).Raw("goal", Vector(hit.goal).Json()).Raw("native_end", Vector(hit.end).Json())
		.Null("thin").Null("goal_open").String("classifier_status", "Hypothesis_only_never_filters"));
}
bool Metadata(Hit& record, const RE::hkpRootCdPoint* result)
{
	if (!record.hit || !result || !result->rootCollidableB || !OnMain()) return false;
	record.metadata = true;
	const auto collision = result->rootCollidableB;
	record.collision = reinterpret_cast<std::uintptr_t>(collision);
	record.layer = static_cast<std::uint32_t>(collision->GetCollisionLayer());
	std::memcpy(record.normal, &result->contact.separatingNormal, 16);
	const auto object = RE::TESHavokUtilities::FindCollidableObject(*collision);
	const auto ref = RE::TESHavokUtilities::FindCollidableRef(*collision);
	record.object = reinterpret_cast<std::uintptr_t>(object);
	if (object && object->name.c_str()) strncpy_s(record.node, object->name.c_str(), _TRUNCATE);
	if (ref) {
		record.ref = ref->GetFormID();
		if (const auto cell = ref->GetParentCell()) record.cell = cell->GetFormID();
		if (const auto base = ref->GetBaseObject()) {
			record.base = base->GetFormID(); record.formType = static_cast<std::uint32_t>(base->GetFormType());
			if (const auto model = base->As<RE::TESModel>(); model && model->GetModel()) strncpy_s(record.model, model->GetModel(), _TRUNCATE);
		}
	}
	record.matched = policy::Allowlisted(record.ref, record.base, g->config.refs, g->config.bases);
	const bool isStatic = StaticReference(ref);
	record.candidate = policy::OverheadCandidate({ record.normal[2], record.layer, isStatic });
	return isStatic;
}
void QueueHit(const Hit& record)
{
	std::unique_lock lock(g->queueMutex, std::try_to_lock);
	if (lock.owns_lock() && g->pending < g->queue.size() && g->records < g->config.maxRecords) {
		g->queue[g->pending++] = record; ++g->records;
	} else { Miss(); Inconclusive("hit_queue_or_observe_record_cap"); }
}
bool HookCaster(void* physics, RE::bhkWorld* world, float* start, float* end,
	RE::hkpRootCdPoint* result, RE::TESObjectREFR** character, float hull)
{
	const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
	if (!g->enabled || !policy::CasterMode(g->config.mode)) return originalCaster(physics, world, start, end, result, character, hull);
	static thread_local bool inCaster{};
	if (inCaster) return originalCaster(physics, world, start, end, result, character, hull);
	struct Guard { bool& active; ~Guard() { active = false; } } guard{ inCaster };
	inCaster = true;
	const double begin = Now();
	Hit record;
	record.caller = caller; record.frame = g->frame.load(); record.hull = hull; record.capture = g->capture;
	record.callingThread = GetCurrentThreadId();
	const auto main = RE::Main::GetSingleton(); record.mainThread = main ? main->threadID : 0;
	std::memcpy(record.start.data(), start, 16); std::memcpy(record.goal.data(), end, 16);
	const double beforeOriginal = Now();
	const bool firstHit = originalCaster(physics, world, start, end, result, character, hull);
	double originalTime = Now() - beforeOriginal;
	const auto calls = ++g->hookCalls;
	record.hit = firstHit; record.sequence = ++g->nextSequence;
	std::memcpy(record.end.data(), end, 16);
	MEMORY_BASIC_INFORMATION memory{};
	record.smooth = VirtualQuery(reinterpret_cast<void*>(caller), &memory, sizeof(memory)) && memory.AllocationBase == g->smooth;
	// Observe sampling happens before resolving scene names/models; no Eligible()
	// or BSFixedString menu work on the observe hot path.
	const bool filtering = g->config.mode == Mode::filter && g->armed && g->eligible && OnMain() && record.smooth && Eligible();
	if (!filtering && (calls & 15) != 0) {
		++g->sampledOut; RecordTime(begin, beforeOriginal, beforeOriginal + originalTime, Now()); return firstHit;
	}
	bool isStatic = Metadata(record, result);
	if (filtering && record.matched) ++g->matched;
	if (filtering && (calls & 15) == 0) QueueHit(record);
	bool hit = firstHit;
	if (filtering && firstHit && result && character) {
		const auto firstEnd = record.end;
		const auto firstContact = *result;
		const auto firstCharacter = *character;
		std::array<Hit, policy::maxRecasts> removed{};
		unsigned count{};
		policy::Continuation continuation;
		bool valid = true;
		while (hit && policy::CanDiscard(g->config.mode, g->armed, g->eligible && !g->requests.pending(),
			record.smooth, isStatic, record.matched, !g->inconclusive)) {
			policy::Point next;
			const auto step = continuation.advance(
				{record.start[0],record.start[1],record.start[2]}, {record.goal[0],record.goal[1],record.goal[2]},
				{record.end[0],record.end[1],record.end[2]}, next);
			if (step != policy::Continuation::Step::cast) {
				Inconclusive(step == policy::Continuation::Step::depthLimit ? "recast_depth_cap" : "recast_no_forward_progress"); valid = false; break;
			}
			LogHit(record, "recast_step");
			if (!Flush()) { valid = false; break; }
			removed[count++] = record;
			alignas(16) std::array<float, 4> from{ next.x, next.y, next.z, record.start[3] };
			alignas(16) auto to = record.goal;
			RE::hkpRootCdPoint contact{}; RE::TESObjectREFR* actor{};
			const double recastBegin = Now();
			// Gateway bypasses our entry detour. Same context/world/hull: the native
			// hull write is idempotent; native phantom positioning/locking still run.
			hit = originalCaster(physics, world, from.data(), to.data(), &contact, &actor, hull);
			originalTime += Now() - recastBegin;
			Hit nextRecord;
			nextRecord.sequence = record.sequence; nextRecord.frame = record.frame; nextRecord.capture = record.capture;
			nextRecord.caller = caller; nextRecord.smooth = record.smooth; nextRecord.hull = hull;
			nextRecord.callingThread = record.callingThread; nextRecord.mainThread = record.mainThread; nextRecord.depth = count;
			nextRecord.start = from; nextRecord.goal = record.goal; nextRecord.end = to; nextRecord.hit = hit;
			record = nextRecord; isStatic = Metadata(record, &contact);
			if (record.matched) ++g->matched;
			LogHit(record, "recast_result");
			if (!Flush()) { valid = false; break; }
			std::memcpy(end, to.data(), 16);
			if (hit) *result = contact;
			else *result = RE::hkpRootCdPoint{};
			*character = actor;
		}
		if (count) {
			alignas(16) auto resetStart = removed[0].start;
			alignas(16) auto resetEnd = removed[0].goal;
			RE::hkpRootCdPoint resetContact{}; RE::TESObjectREFR* resetCharacter{};
			const double resetBegin = Now();
			const bool resetHit = originalCaster(physics, world, resetStart.data(), resetEnd.data(), &resetContact, &resetCharacter, hull);
			originalTime += Now() - resetBegin;
			Emit("recast_context_reset", json::Object{}.Numeric("sequence", record.sequence).Boolean("hit", resetHit)
				.Numeric("hull", hull).Raw("start", Vector(resetStart).Json()).String("method", "gateway_original_inputs_outputs_ignored"));
			valid = Flush() && valid;
			// A lifecycle stop or incomplete journal rolls back the whole chain.
			valid = valid && g->enabled && g->armed && !g->requests.pending() && !g->inconclusive;
			if (valid) {
				for (unsigned i = 0; i < count; ++i) { LogHit(removed[i], "discard"); if (!Flush()) { valid = false; break; } }
				if (valid) Emit("filter_complete", json::Object{}.Numeric("sequence", record.sequence).Numeric("steps", count)
					.Boolean("returned_hit", hit).Boolean("kept_behind", hit && record.metadata && record.ref && !record.matched)
					.Boolean("returned_allowlist_match", record.matched).String("returned_reference", json::FormID(record.ref)));
				valid = Flush() && valid;
			}
			if (!valid) {
				std::memcpy(end, firstEnd.data(), 16); *result = firstContact; *character = firstCharacter; hit = firstHit;
				Emit("filter_rollback", json::Object{}.Numeric("sequence", record.sequence)); Flush();
			} else { g->discarded += count; if (hit && record.metadata && record.ref && !record.matched) ++g->keptBehind; }
		} else LogHit(record, "caster");
	} else QueueHit(record);
	RecordTime(begin, beforeOriginal, beforeOriginal + originalTime, Now());
	return hit;
}
bool HookRoom(RE::BSMultiBoundRoom* room, const RE::NiPoint3& point)
{
	if (!g->enabled) return originalRoom(room, point);
	const auto calls = ++g->seedCalls;
	const bool sample = (calls & 63) == 0;
	const double begin = sample ? Now() : 0;
	const bool vanilla = originalRoom(room, point);
	const double originalEnd = sample ? Now() : 0;
	bool claim = false;
	if (!vanilla && g->armed && g->eligible) {
		const auto snapshot = g->room.load();
		if (g->roomActive && snapshot) {
			const unsigned mode = static_cast<unsigned>(g->config.mode) - static_cast<unsigned>(Mode::roomSeed1) + 1;
			claim = policy::Seed(mode, Point(point), snapshot->eye, Point(room->worldBound.center), room->worldBound.radius,
				g->config.seedDistance, [&](policy::Point p) { return originalRoom(room, Native(p)); });
		}
	}
	if (claim) ++g->seedClaims;
	if (sample) RecordTime(begin, begin, originalEnd, Now());
	return vanilla || claim;
}
std::array<std::uint8_t, 14> AbsoluteJump(std::uintptr_t destination)
{
	std::array<std::uint8_t, 14> bytes{ 0xff, 0x25, 0, 0, 0, 0 };
	std::memcpy(bytes.data() + 6, &destination, sizeof(destination)); return bytes;
}
bool InstallMechanism()
{
	const auto base = REL::Module::get().base();
	if (policy::CasterMode(g->config.mode)) {
		const auto target = REL::ID(33007).address();
		if (target != base + 0x5574f0 || std::memcmp(reinterpret_cast<const void*>(target), casterPrologue.data(), casterPrologue.size()) != 0) {
			Emit("refused", json::Object{}.String("reason", "caster_address_or_prologue_mismatch")); return false;
		}
		// Exactly 14 bytes of complete instructions, no RIP-relative operand. Gateway
		// preserves them verbatim, including the original RSP used by the prologue.
		SKSE::AllocTrampoline(64);
		auto gateway = static_cast<std::uint8_t*>(SKSE::GetTrampoline().allocate(32));
		std::memcpy(gateway, casterPrologue.data(), casterPrologue.size());
		const auto back = AbsoluteJump(target + casterPrologue.size());
		std::memcpy(gateway + casterPrologue.size(), back.data(), back.size());
		FlushInstructionCache(GetCurrentProcess(), gateway, 28);
		originalCaster = reinterpret_cast<Caster>(gateway);
		const auto jump = AbsoluteJump(reinterpret_cast<std::uintptr_t>(&HookCaster));
		REL::safe_write(target, jump.data(), jump.size());
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target), jump.size());
		Emit("installed", json::Object{}.Numeric("id", 33007).Numeric("rva", 0x5574f0));
	} else if (policy::SeedMode(g->config.mode)) {
		REL::Relocation<std::uintptr_t> table{ RE::VTABLE_BSMultiBoundRoom[0] };
		const auto target = reinterpret_cast<const std::uintptr_t*>(table.address())[0x3f];
		if (table.address() != base + 0x1a69ac8 || target != base + 0xfce0c0 ||
			std::memcmp(reinterpret_cast<const void*>(target), roomPrologue.data(), roomPrologue.size()) != 0) {
			Emit("refused", json::Object{}.String("reason", "qpointwithin_address_slot_or_prologue_mismatch")); return false;
		}
		originalRoom = reinterpret_cast<RoomTest>(table.write_vfunc(0x3f, HookRoom));
		Emit("installed", json::Object{}.Numeric("vtable_rva", 0x1a69ac8).Numeric("slot", 0x3f).Numeric("target_rva", 0xfce0c0));
	} else if (g->config.mode == Mode::alphaProperty) {
		const auto factory = REL::ID(70687).address();
		constexpr std::array<std::uint8_t, 15> expected{ 0x40,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff };
		if (factory != base + 0xef2be0 || std::memcmp(reinterpret_cast<const void*>(factory), expected.data(), expected.size()) != 0) {
			Emit("refused", json::Object{}.String("reason", "alpha_factory_address_or_prologue_mismatch")); return false;
		}
	} else if (g->config.mode == Mode::nearClip) {
		// The render call is confirmed. The source's void UpdateCameraData ABI is
		// contradicted by 1.7.104's live EDX input, so no projection patch is safe.
		const auto site = REL::ID(107642).address() + 0x28f;
		constexpr std::array<std::uint8_t, 5> expected{ 0xe8,0x6c,0x01,0x00,0x00 };
		Emit("refused", json::Object{}.String("reason", "projection_update_77258_has_unresolved_edx_input")
			.Boolean("render_call_verified", site == base + 0x15601cf && std::memcmp(reinterpret_cast<const void*>(site), expected.data(), expected.size()) == 0));
		return false;
	}
	return true;
}
json::Object Vector(const std::array<float, 4>& value)
{
	return json::Object{}.Numeric("x", value[0]).Numeric("y", value[1]).Numeric("z", value[2]).Numeric("w", value[3]);
}
void GoalContext(const Hit& hit)
{
	if (!hit.smooth || !hit.hit) return;
	const auto player = RE::PlayerCharacter::GetSingleton();
	const auto cell = player ? player->GetParentCell() : nullptr;
	if (!cell || !hit.cell || cell->GetFormID() != hit.cell || !Eligible()) return;
	const auto goal = RE::NiPoint3{ hit.goal[0], hit.goal[1], hit.goal[2] };
	if (!policy::Finite(Point(goal))) return;
	const auto world = cell->GetbhkWorld();
	if (!world) return;
	unsigned blocked{};
	const float scale = RE::bhkWorld::GetWorldScale();
	RE::CFilter filter{};
	player->GetCollisionFilterInfo(filter);
	// Six finite rays cannot establish whether the point is embedded in a solid.
	const std::array<RE::NiPoint3, 6> axes{ RE::NiPoint3{16,0,0}, {-16,0,0}, {0,16,0}, {0,-16,0}, {0,0,16}, {0,0,-16} };
	{ RE::BSReadLockGuard lock(world->worldLock);
		for (const auto& axis : axes) {
			RE::bhkPickData pick{};
			pick.rayInput.from = { goal.x*scale, goal.y*scale, goal.z*scale, 0 };
			const auto to = goal + axis;
			pick.rayInput.to = { to.x*scale, to.y*scale, to.z*scale, 0 };
			pick.rayInput.filterInfo = filter;
			world->PickObject(pick);
			blocked += pick.rayOutput.HasHit();
		}
	}
	std::uintptr_t playerRoom{}, goalRoom{};
	const auto loaded = cell->GetRuntimeData().loadedData;
	const auto graph = loaded ? loaded->portalGraph.get() : nullptr;
	if (graph) for (const auto& room : graph->rooms) if (room) {
		// Observer uses native membership, never the experimental seeding answer.
		const auto contains = [&](const RE::NiPoint3& p) { return originalRoom ? originalRoom(room.get(), p) : room->QPointWithin(p); };
		if (!playerRoom && contains(player->GetPosition())) playerRoom = reinterpret_cast<std::uintptr_t>(room.get());
		if (!goalRoom && contains(goal)) goalRoom = reinterpret_cast<std::uintptr_t>(room.get());
	}
	Emit("goal_context", json::Object{}.Numeric("sequence", hit.sequence).Numeric("player_room", playerRoom).Numeric("goal_room", goalRoom)
		.Boolean("room_graph_available", graph != nullptr).Numeric("clearance_rays", 6).Numeric("blocked_rays", blocked)
		.Null("goal_open").Null("thickness").String("sample", "deferred_same_cell_not_atomic_with_contact"));
}
void Drain()
{
	std::vector<Hit> records;
	{ std::scoped_lock lock(g->queueMutex); records.assign(g->queue.begin(), g->queue.begin() + static_cast<std::ptrdiff_t>(g->pending)); g->pending = 0; }
	bool testedGoal = false;
	for (const auto& hit : records) {
		LogHit(hit, "caster");
		if (!testedGoal && hit.hit && hit.smooth) { GoalContext(hit); testedGoal = true; }
	}
}
void Summary(std::string_view reason)
{
	if (!OnMain()) { Request(policy::stopRequest); return; }
	std::scoped_lock lock(g->perfMutex);
	auto emit = [&](const char* label, const auto& ring) {
		const auto s = ring.summary();
		Emit("timing", json::Object{}.String("reason", reason).String("capture_label", g->captureLabel).String("metric", label).Numeric("count", s.count).Numeric("total", ring.total())
			.Numeric("overwritten", ring.total() - s.count).Numeric("median_ms", s.median).Numeric("p95_ms", s.p95)
			.Numeric("p99_ms", s.p99).Numeric("maximum_ms", s.maximum).Numeric("missed", g->captureMissed.load()).Numeric("session_missed", g->missed.load())
			.Numeric("matched_hits", g->matched.load()).Numeric("discards", g->discarded.load()).Numeric("kept_behind", g->keptBehind.load())
			.Boolean("inconclusive", g->inconclusive).Numeric("observe_sampled_out", g->sampledOut.load()).Numeric("caster_calls", g->hookCalls.load()).Numeric("room_calls", g->seedCalls.load()).Numeric("room_claims", g->seedClaims.load())
			.Boolean("capture_active", g->capturing).Numeric("hook_sample_stride", policy::SeedMode(g->config.mode) ? 64 : 1));
	};
	emit("engine_frame", g->frames); emit("ui_interval", g->intervals); emit("hook_inclusive", g->hooks);
	emit("original_inclusive", g->originals); emit("adapter_exclusive", g->adapters);
	std::fflush(g->output);
}
void ConsumeRequests()
{
	const auto flags = g->requests.take();
	if (!flags) return;
	Emit("request_consumed", json::Object{}.Numeric("flags", flags));
	Restore("main_thread_request");
	if (!policy::MayArm(flags)) return;
	const auto map = RE::ControlMap::GetSingleton();
	if (!map || !map->GetUserEventName(armKey, RE::INPUT_DEVICE::kKeyboard).empty() ||
		!map->GetUserEventName(stopKey, RE::INPUT_DEVICE::kKeyboard).empty()) {
		Emit("refused", json::Object{}.String("reason", "diagnostic_key_has_native_mapping")); return;
	}
	if (g->killed || g->inconclusive || !Eligible()) { Emit("refused", json::Object{}.String("reason", "KP7_ineligible_or_killed")); return; }
	Drain();
	++g->capture;
	g->captureLabel = Setting("sCaptureLabel", "unlabeled");
	{
		std::scoped_lock lock(g->perfMutex);
		g->frames.clear(); g->intervals.clear(); g->hooks.clear(); g->originals.clear(); g->adapters.clear(); g->lastFrame = 0; g->captureMissed = 0;
		g->matched = 0; g->discarded = 0; g->keptBehind = 0;
	}
	if (policy::FadeMode(g->config.mode)) ApplyFade();
	else { g->armed = true; Emit("armed", json::Object{}.String("reason", "fresh_KP7")); }
	g->capturing = g->fade.applied || g->armed;
	Emit("capture", json::Object{}.Boolean("active", g->capturing).String("label", g->captureLabel));
	if (g->requests.pending()) Restore("request_during_arm");
}
void Tick()
{
	if (!OnMain()) { Inconclusive("main_task_wrong_thread"); g->enabled = false; g->killed = true; return; }
	const double now = Now();
	if (!g->enabled) { Restore("disabled_or_log_failure"); g->killed = true; }
	const auto player = RE::PlayerCharacter::GetSingleton();
	const auto cell = player ? player->GetParentCell() : nullptr;
	const auto id = cell ? cell->GetFormID() : 0;
	if (id != g->cell) { Request(policy::detachRequest); Restore("cell_change"); g->cell = id; }
	g->eligible = Eligible();
	if (!g->eligible) Restore("ineligible_state");
	CheckFade();
	ConsumeRequests();
	if (policy::SeedMode(g->config.mode) && g->armed && g->eligible && player) {
		auto snapshot = std::make_shared<RoomSnapshot>();
		snapshot->eye = Point(player->GetPosition()); snapshot->eye.z += 64;
		g->room.store(snapshot); g->roomActive = !g->requests.pending();
	} else g->roomActive = false;
	if (now - g->lastPoll >= 500) {
		g->lastPoll = now;
		// Kill is latched until process restart; changing mode also requires restart.
		if (Setting("bEnabled", "0") != "1" || Setting("sMode", "observe") != g->config.name) {
			Restore("ini_kill_or_mode_change"); g->enabled = false; g->killed = true;
		}
		Drain();
	}
	const auto main = RE::Main::GetSingleton();
	if ((main && main->GetRuntimeData().quitGame) || now - g->origin >= 1200000) {
		Restore("exit_or_20_minute_deadline"); g->enabled = false; g->killed = true;
	}
	if (g->killed) Drain();
	if (now - g->lastReport >= 5000 || g->killed) {
		g->lastReport = now; Summary(g->killed ? "stop" : "periodic");
	}
}
void QueueUI();
void QueueMain();
class UITask final : public SKSE::detail::UIDelegate_v1 {
public:
	void Run() noexcept override
	{
		try {
			const double now = Now();
			{ std::scoped_lock lock(g->perfMutex);
				const auto tick = g->chain.uiRun(now);
				if (tick == json::FrameChain::Tick::idle) return;
				if (tick == json::FrameChain::Tick::runaway) { g->enabled = false; g->killed = true; }
				else {
					++g->frame;
					if (g->capturing) {
						if (g->lastFrame > 0) {
							g->intervals.push(now - g->lastFrame);
							const auto timer = RE::BSTimer::GetSingleton();
							if (timer && timer->realTimeDelta > 0) g->frames.push(static_cast<double>(timer->realTimeDelta) * 1000);
						}
						g->lastFrame = now;
					}
				}
			}
			QueueMain();
		} catch (...) { g->enabled = false; g->killed = true; QueueMain(); }
	}
	void Dispose() noexcept override {}
};
class MainTask final : public SKSE::detail::TaskDelegate {
public:
	void Run() noexcept override
	{
		try {
			Tick();
			bool next{};
			{ std::scoped_lock lock(g->perfMutex);
				if (g->killed) g->chain.stop();
				next = g->chain.mainRun();
			}
			if (next) QueueUI();
		} catch (...) {
			g->enabled = false; g->killed = true; Restore("main_task_exception");
			std::scoped_lock lock(g->perfMutex); g->chain.enqueueFailed();
		}
	}
	void Dispose() noexcept override {}
};
UITask uiTask;
MainTask mainTask;
void QueueUI() { SKSE::GetTaskInterface()->AddUITask(reinterpret_cast<::UIDelegate_v1*>(&uiTask)); }
void QueueMain() { SKSE::GetTaskInterface()->AddTask(reinterpret_cast<::TaskDelegate*>(&mainTask)); }
class Events final : public RE::BSTEventSink<RE::InputEvent*>, public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
	public RE::BSTEventSink<RE::TESCellAttachDetachEvent> {
public:
	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override
	{
		if (!events) return RE::BSEventNotifyControl::kContinue;
		for (auto event = *events; event; event = event->next) {
			const auto button = event->AsButtonEvent();
			if (!button || !button->IsDown()) continue;
			const bool keyboard = event->GetDevice() == RE::INPUT_DEVICE::kKeyboard;
			const auto id = button->GetIDCode();
			if ((keyboard && (id == 66 || id == 64)) || button->QUserEvent() == "Toggle POV") { Request(policy::povRequest); continue; }
			if (!keyboard || (id != armKey && id != stopKey)) continue;
			if (!button->QUserEvent().empty()) {
				Emit("refused", json::Object{}.String("reason", "diagnostic_key_has_native_mapping").Numeric("key", id)); continue;
			}
			Request(id == armKey ? policy::armRequest : policy::stopRequest);
		}
		// Passive observer: never remove/consume native or SmoothCam input.
		return RE::BSEventNotifyControl::kContinue;
	}
	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
	{
		if (event && event->opening) Request(policy::menuRequest);
		return RE::BSEventNotifyControl::kContinue;
	}
	RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent* event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override
	{
		// Deliberately conservative: do not dereference off-thread event references.
		if (event && !event->attached) Request(policy::detachRequest);
		return RE::BSEventNotifyControl::kContinue;
	}
};
Events events;
void Message(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kPostLoad:
		// Missing registration/dispatch is refused by the provider check at data load.
		static_cast<void>(SmoothCamAPI::RegisterInterfaceLoaderCallback(SKSE::GetMessagingInterface(), [](void* api, SmoothCamAPI::InterfaceVersion version) {
			if (version == SmoothCamAPI::InterfaceVersion::V3) g->provider = static_cast<SmoothCamAPI::IVSmoothCam3*>(api);
		})); break;
	case SKSE::MessagingInterface::kPostPostLoad:
		static_cast<void>(SmoothCamAPI::RequestInterface(SKSE::GetMessagingInterface())); break;
	case SKSE::MessagingInterface::kDataLoaded: {
		if (g->initialized) break;
		g->initialized = true;
		Emit("thread_context", json::Object{}.String("source", "data_loaded").Numeric("main_thread_id", RE::Main::GetSingleton() ? RE::Main::GetSingleton()->threadID : 0));
		g->smooth = GetModuleHandleW(L"SmoothCam.dll");
		if (!HashModule(g->smooth, smoothHash) || !g->provider || !SKSE::GetTaskInterface() ||
			!RE::BSInputDeviceManager::GetSingleton() || !RE::UI::GetSingleton() || !RE::ScriptEventSourceHolder::GetSingleton()) {
			Emit("refused", json::Object{}.String("reason", "pinned_smoothcam_api_or_lifecycle_sources_unavailable")); std::fflush(g->output); break;
		}
		if (!InstallMechanism()) { std::fflush(g->output); break; }
		RE::BSInputDeviceManager::GetSingleton()->AddEventSink(static_cast<RE::BSTEventSink<RE::InputEvent*>*>(&events));
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESCellAttachDetachEvent>(&events);
		g->enabled = true;
		Emit("ready", json::Object{}.String("mutation", "requires_fresh_KP7").String("restore", "KP9_F8_F6_POV_menu_load_cell_exit_kill")
			.String("room_seed_3_policy", "standalone_armed_no_reference_clip_gate"));
		{ std::scoped_lock lock(g->perfMutex); g->chain.start(); }
		QueueUI(); break;
	}
	case SKSE::MessagingInterface::kPreLoadGame: g->loading = true; Request(policy::loadRequest); break;
	case SKSE::MessagingInterface::kNewGame:
	case SKSE::MessagingInterface::kPostLoadGame: g->loading = false; Request(policy::loadRequest); break;
	default: break;
	}
}
} // namespace
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
	SKSE::PluginVersionData data;
	data.PluginVersion({ SKYCRPG_VERSION_MAJOR, SKYCRPG_VERSION_MINOR, SKYCRPG_VERSION_PATCH, 0 });
	data.PluginName("SkyCRPG Occlusion Spike"); data.AuthorName("devils_17");
	data.UsesAddressLibrary(); data.UsesUpdatedStructs();
	data.versionIndependenceEx |= SKSE::PluginVersionData::kVersionIndependentEx_AddressLibraryV5;
	data.CompatibleVersions({ runtime }); data.xseMinimum = REL::Version{ 2, 3, 1, 0 }.pack(); return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
	if (!skse || skse->IsEditor() || skse->RuntimeVersion() != runtime || skse->SKSEVersion() != REL::Version{ 2, 3, 1, 0 }.pack()) return false;
	const auto ini = skycrpg::ThisModuleDirectory() + L"SkyCRPG_OcclusionSpike.ini";
	if (GetPrivateProfileIntW(L"Spike", L"bEnabled", 0, ini.c_str()) != 1) return true;
	g = new State; g->ini = ini;
	if (!QueryPerformanceFrequency(&g->frequency) || g->frequency.QuadPart <= 0) return false;
	g->origin = Now();
	const auto log = skycrpg::LogDirectory() + L"SkyCRPG_OcclusionSpike.jsonl";
	if (_wfopen_s(&g->output, log.c_str(), L"ab") != 0 || !g->output) return false;
	std::setvbuf(g->output, nullptr, _IOFBF, 65536);

	if (!LoadConfig()) { Emit("refused", json::Object{}.String("reason", "invalid_ini")); std::fflush(g->output); return true; }
	Emit("session", json::Object{}.String("source_commit", SKYCRPG_SOURCE_COMMIT).String("runtime", "1.7.104.0")
		.String("expected_exe_sha256", exeHash).String("expected_library_sha256", libraryHash).String("expected_smoothcam_sha256", smoothHash).String("status", "NOT_RUN_until_game_evidence_review"));
	if (!HashModule(GetModuleHandleW(nullptr), exeHash) || !HashFile(std::filesystem::path(skycrpg::ThisModuleDirectory()) / L"versionlib-1-7-104-0.bin", libraryHash)) {
		Emit("refused", json::Object{}.String("reason", "executable_or_address_library_hash_mismatch")); std::fflush(g->output); return true;
	}
	Emit("identity", json::Object{}.String("executable_and_library", "PASS"));
	SKSE::Init(skse, false);
	const auto messaging = SKSE::GetMessagingInterface();
	return messaging && messaging->RegisterListener(Message);
}
