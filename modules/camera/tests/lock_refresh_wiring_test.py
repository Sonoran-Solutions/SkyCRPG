#!/usr/bin/env python3
# Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
"""Keep the portable refresh model connected to the native adapter's scheduling."""
from pathlib import Path

source = (Path(__file__).resolve().parents[1] / "src/plugin.cpp").read_text()


def body(start, end):
    return source[source.index(start):source.index(end, source.index(start))]


timer = body("void CALLBACK queueDeferredCameraWork(", "bool prepareDeferredCameraWork()")
assert timer.index("serviceLockConfigIO();") < timer.index("deferredCameraQueued.exchange(true)")
assert "deferredConfigBusy.test_and_set()" in timer
io = body("void serviceLockConfigIO()", "void CALLBACK queueDeferredCameraWork(")
assert "lockConfigRefresh.beginRead()" in io and "lockConfigRefresh.finishRead(result)" in io
assert "readLockMovementConfig()" in io and "precisionDiagnostics.drain(" in io
assert not any(gate in io for gate in ("outsideMenuLocks()", "mainThread()", "!loading", "!movementLease"))
apply = body("void applyLockConfigRefresh()", "void updateTargetLockMain(Context& c);")
assert "main->threadID != GetCurrentThreadId()" in apply
assert "lockConfigRefresh.apply(" in apply and "publishedLockMovementConfigSafe.store" in apply
assert not any(call in apply for call in ("readLockMovementConfig", "log(", "stopIndicator(", "outsideMenuLocks"))
bindings = body("bool lockBindingsSafe()", "void applyLockConfigRefresh()")
assert "mapping.eventID == *lockPOVEvent" in bindings and 'GetMappedKey(' not in bindings
service = body("void serviceFirstPersonReturn() {", "void serviceCameraCall(")
assert service.index("applyLockConfigRefresh();") < service.index("if (loading")
assert service.index("publishedReturnChainReady.store(providerChainReady())") < service.index("servicePrecisionEntry();")
assert "if (lockConfigRefresh.requested()) { precisionDiagnostics.note(PrecisionReason::refreshPending); return; }" in service
assert "precisionReturn.take(c,haveLease,locked,sessionToken)" in service
context = body("Context context(", '#include "cursor_mode.h"')
assert context.index("applyLockConfigRefresh();") < context.index("const bool baseReady")
assert "lockConfigSafe && !lockConfigRefresh.requested() && !deferredIndicatorStop.load()" in source
assert source.count("lockConfigRefresh.request(") == 3  # startup, selected menus, lock-off menus
input_sink = body("    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent*", "Events events;")
assert "lockConfigRefresh.request(" not in input_sink  # F8/B/U are not producers
assert "event->menuName == cursorMode::menuName) return" in source
assert "if (lockMenuRefresh(name,event->opening)) lockConfigRefresh.request(lockRefreshSource(name,event->opening));" in source
print("PASS native wiring: timer IO before queue gate; Main applies before loading/FP; fresh F6 chain check; barriers retained; exact producer set")

# Entry needs a per-frame Main consumer even when the ordinary task drain never
# reaches Main. Inspect both installer branches and their unconditional caller.
installer = body("bool installLockCameraService() {", "RE::NiPoint3 thinGoal()")
assert "targetLockEnabled" not in installer
assert "previousCameraCall = thin::runtime->update;" in installer
assert "thin::runtime->update = serviceCameraCall;" in installer
assert "write_call<5>(site,serviceCameraCall)" in installer
assert "REL::ID(50784).address()+0x1A6" in installer
startup = body("        lockHooksReady = targetLockEnabled;", "        if (const auto source = RE::ScriptEventSourceHolder")
assert startup.startswith("        lockHooksReady = targetLockEnabled;\n        thin::installHooks(ready);\n        if (!installLockCameraService()) {")
assert "if (!targetLockEnabled) ready = false;" in startup
callback = body("void serviceCameraCall(RE::PlayerCamera* camera) {", "bool installLockCameraService() {")
assert "if (targetLockEnabled) serviceFirstPersonReturn();\n    else serviceLockOffPrecisionEntry();" in callback
assert callback.count("previousCameraCall(camera);") == 1
assert callback.index("serviceLockOffPrecisionEntry();") < callback.index("previousCameraCall(camera);")
lock_off = body("void serviceLockOffPrecisionEntry() {", "void serviceCameraCall(")
assert lock_off.index("!cursorMode::mainThread()") < lock_off.index("applyLockConfigRefresh();")
assert lock_off.index("publishedReturnChainReady.store(providerChainReady());") < lock_off.index("servicePrecisionEntry();")
assert not any(gate in lock_off for gate in ("targetLockEnabled", "lockHooksReady", "cursorSettings", "thin::", "outsideMenuLocks"))
entry = body("void servicePrecisionEntry() {", "void serviceFirstPersonReturn() {")
assert entry.index("!cursorMode::mainThread()") < entry.index("const auto before = readEntry();")
assert entry.index("const auto before = readEntry();") < entry.index("ForceFirstPerson()")
assert "if (!before) return;" in entry
assert "PrecisionReason::serviceOther" in entry and "PrecisionReason::serviceMain" in entry
entry_input = body("                    if (request == ToggleRequest::firstPerson) {", "                    } else if (request == ToggleRequest::enableOnly)")
assert "precisionClaimTrace.publish(precisionReturn,c,returnSession," in entry_input and "ForceFirstPerson" not in entry_input
assert "{pad?2u:1u,button->IsPressed(),button->IsDown(),keys[keyIndex]}" in entry_input
assert "PrecisionReason::entryAccepted" in entry_input and "PrecisionReason::entryRejected" in entry_input
assert "if (targetLockEnabled && i == 0 && button->IsDown())\n                        precisionDiagnostics.note" not in input_sink
assert "if (targetLockEnabled && request == ToggleRequest::none)" not in input_sink
print("PASS lock-off wiring: unconditional thin/trampoline installation; Main frame refreshes chain then services entry; one predecessor; input publishes only; diagnostics ungated")

# Event 11 is emitted once, only after Main has successfully claimed the native
# transition. The reader's local locks end before enter() and tracePrecision().
assert source.count("tracePrecision(PrecisionTraceEvent::toggleClaim,") == 1
assert "toggleClaim" not in input_sink
assert "const auto claimInput = precisionClaimTrace.read(sessionToken).value_or(PrecisionClaimInput{});" in entry
assert "if (!claimInput)" not in entry  # the debug sample must never gate F6 entry
assert entry.index("precisionTraceEnabled && !cursorMode::outsideMenuLocks()") < entry.index("precisionReturn.enter(")
assert entry.index("ForceFirstPerson()") < entry.index("after = readEntry()") < entry.index("if (claimed) {")
assert "if (claimed) {\n        after.owned = true;" in entry
assert "tracePrecision(PrecisionTraceEvent::toggleClaim,*before,after,\n            claimInput.device,claimInput.pressed,claimInput.edge);" in entry
assert "return traceState(c,hard,claimKey,false,*mounted);" in entry
assert "keys[" not in entry  # use the published input sample, not mutable sink keys
reader = entry[entry.index("const auto readEntry ="):entry.index("const auto before = readEntry();")]
assert "const cursorMode::UITryLock menuLock;" in reader and "callbackMount(*player)" in reader
assert "ForceFirstPerson" not in reader and "tracePrecision(" not in reader
trace = body("void tracePrecision(PrecisionTraceEvent event, PrecisionTraceState before, PrecisionTraceState after,\n    unsigned device,", "void lifecycleLog(")
assert trace.index("if (!precisionTraceEnabled || !precisionTraceBudget.take()) return;") < trace.index("char line[")
assert trace.index("precisionTraceBudget.take()") < trace.index("formatPrecisionTrace(") < trace.index("log(line)")
print("PASS claim trace wiring: unique Main success emission after setter/read locks; stored input; inherited UI-lock guard; debug/budget before formatting and IO")

# P5.5-02 disposable spike (DYL-217). With [FixedCameraSpike] off there is no
# runtime object, so every entry point below must be unreachable or a no-op;
# with it on, the frame bracket, hold and suppression must be wired in order.
import re

spike = (Path(__file__).resolve().parents[1] / "src/fixed_spike.h").read_text()
thin = (Path(__file__).resolve().parents[1] / "src/thin_occluder.h").read_text()


def spike_body(start, end):
    return spike[spike.index(start):spike.index(end, spike.index(start))]


assert spike.count("runtime = new Runtime;") == 1 and "runtime = new Runtime;" in spike_body("inline void configure(", "} // namespace")
loader = body("void loadSettings() {", "void requestPreferences() {")
gate = loader[loader.index('if (flag("FixedCameraSpike","bEnabled",0)) {'):loader.index("if (badSettings) log(")]
assert source.count("fixed::configure(") == 1 and "fixed::configure(" in gate
assert source.count('"FixedCameraSpike"') == loader.count('"FixedCameraSpike"') == gate.count('"FixedCameraSpike"')
# Null-safe entry points used without a guard at the call site.
for helper in ("inline bool engaged() { return runtime && ",
               "inline void requestRestore(Exit cause) {\n    if (runtime) ",
               "inline void dropSnaps() {\n    if (!runtime) return;",
               "inline void lifecycle(Exit cause) {\n    if (!runtime) return;",
               "inline void install(bool prerequisites,bool service) {\n    if (!runtime) return;"):
    assert helper in spike, helper
unguarded = ("fixed::engaged()", "fixed::dropSnaps()", "fixed::lifecycle(", "fixed::install(", "fixed::Exit::", "fixed::Config f;",
             "fixed::configure(", "fixed::runtime", "fixed::shader::Config shaderCut;")
guarded_blocks = (body("    if (rig) {", "    previousLook(state,filtered);"),
                  body("        if (fixed::engaged()) {", "        // Deliberate F6 can return from its OWN first-person request"))
for line in source.splitlines():
    if "fixed::" in line and not any(token in line for token in unguarded):
        assert any(line in block for block in guarded_blocks), line
assert callback.index("if (fixed::runtime) fixed::beginFrame();") < callback.index("previousCameraCall(camera);") < \
    callback.index("if (fixed::runtime) fixed::endFrame();")
update_hook = body("void update(RE::ThirdPersonState* state,RE::BSTSmartPointer<RE::TESCameraState>& next) {", "void pollLockInput(")
assert update_hook.index("if (fixed::runtime) fixed::clearFrame();") < update_hook.index("previousUpdate(state,next);") < \
    update_hook.index("if (fixed::runtime) fixed::observeFrame(select(c),c.slide);") < \
    update_hook.index("resetObstruction(); return;") < update_hook.index("if (fixed::runtime && fixed::holdFrame()) {") < \
    update_hook.index("settings.zoom = preferredZoomRequest(")
assert update_hook.count("previousUpdate(state,next);") == 1
hold = update_hook[update_hook.index("if (fixed::runtime && fixed::holdFrame()) {"):update_hook.index("// Native controller POV-slide zoom")]
assert "settings.zoom" not in hold and "applyZoom" not in hold and "return;" in hold
look_hook = body("void look(RE::ThirdPersonState* state, const RE::NiPoint2& input) {", "void traceLockFrame(Context c);")
assert look_hook.count("previousLook(") == 5  # one native call on every path, as on the branch base
assert "const bool rig = fixed::engaged() && fixed::rigEligible(true,select(c));" in look_hook
assert "if (pauseCorrection(c) && !rig) { previousLook(state,input); return; }" in look_hook
assert look_hook.index("if (rig) {") < look_hook.index("} else if (recenter) {") < look_hook.index("previousLook(state,filtered);")
assert "freeRotation.x =" not in look_hook and "freeRotation.x =" not in spike  # input requests only, no direct write
sample = body("void sampleObstruction(RE::ThirdPersonState* state) {", "void look(")
assert sample.index("if (!fixed::obstructionSampled(fixed::engaged())) { resetObstruction(); return; }") < sample.index("auto c = context();")
startup_spike = body("        if (!installLockCameraService()) {", "        if (const auto source = RE::ScriptEventSourceHolder")
assert "fixed::install(ready && provider,previousCameraCall != nullptr);" in startup_spike
assert "fixed::lifecycle(std::strstr(event,\"load\") ? fixed::Exit::load : fixed::Exit::cell);" in body("void resetLifecycle(", "void observeLifecycle(")
snap = body("        if (fixed::engaged()) {", "        // Deliberate F6 can return from its OWN first-person request")
assert snap.index("button->IsDown()") < snap.index("fixed::rigEligible(true,select(c))") < snap.index("GetUserEventName(") < \
    snap.index("fixed::requestSnap(direction);")
assert "RE::INPUT_DEVICE::kKeyboard" in snap and "kGamepad" not in snap
print("PASS spike off wiring: no runtime without the INI flag; null-safe helpers; base look/update/zoom paths unchanged when off")

# Suppression is armed only by this frame's update hook and disarmed by the
# bracket; the detour is shared with the thin hook, never doubled.
caster = thin[thin.index("inline bool hookCaster("):thin.index("inline std::array<std::uint8_t,14> jump(")]
chain = "if (runtime->spikeCast && runtime->spikeCast(physics,world,start,end,hull)) return false;"
assert caster.count(chain) == 1 and thin.count("spikeCast") == 3
assert caster.index("runtime->smoothCastSeen = true;") < caster.index(chain) < caster.index("if (!runtime->allowed || !safeNow()) {")
assert spike.count("thin::runtime->spikeCast = suppressCast;") == 1 and "spikeCast" not in source
install_caster = spike_body("inline void installCaster(", "inline void installRoomHook(")
assert install_caster.index("if (thin::runtime && thin::runtime->installed) {") < install_caster.index("REL::ID(33007).address()")
assert "target != base+0x5574f0" in install_caster and "std::memcmp(reinterpret_cast<const void*>(target),prologue.data(),prologue.size()) != 0" in install_caster
assert "AllocTrampoline" not in spike  # a second trampoline allocation could release the camera-call relay
assert install_caster.index("r.caster = reinterpret_cast<thin::Caster>(gateway);") < install_caster.index("REL::safe_write(target,")
# The gateway is written read-write, sealed execute-read, and only then flushed and used.
assert "PAGE_EXECUTE_READWRITE" not in spike
assert install_caster.index("PAGE_READWRITE)") < install_caster.index("std::memcpy(gateway+prologue.size()") < \
    install_caster.index("VirtualProtect(gateway,64,PAGE_EXECUTE_READ,&previous)") < install_caster.index("Refusal::casterProtect") < \
    install_caster.index("FlushInstructionCache(GetCurrentProcess(),gateway,") < install_caster.index("r.caster = reinterpret_cast<thin::Caster>(gateway);")
suppress = spike_body("inline bool suppressCast(", "inline bool hookCaster(")
assert suppress.index("castDecision(r.castReady,r.frameEligible,true,true) != Cast::suppress") < suppress.index("++r.suppressed;")
assert "std::memcpy(end" not in suppress and "*end" not in suppress  # the requested end point is left as passed
own = spike_body("inline bool hookCaster(", "inline void traceRig(")
assert own.index("_ReturnAddress()") < own.index("!thin::onMain()") < own.index("memory.AllocationBase != r.smooth") < own.index("suppressCast(")
end_frame = spike_body("inline void endFrame() {", "// ---- install")
assert end_frame.index("r.frameEligible = false;") < end_frame.index("restoreAll(") < end_frame.index("publishView(")
assert end_frame.index("if (!thin::onMain())") < end_frame.index("r.now = thin::seconds();")
assert "r.frameEligible = r.frameSeen = false;" in spike_body("inline void beginFrame() {", "inline void clearFrame() {")
assert "runtime->frameEligible = false;" in spike_body("inline void clearFrame() {", "inline void observeFrame(")
assert spike.count("r.frameEligible = rigEligible(") == 1 and "r.frameEligible = true" not in spike
room = spike_body("inline void installRoomHook(", "// kDataLoaded, after the look/update hooks")
assert "table.address() != base+0x1a69ac8" in room and "slots[0x3f] != base+0xfce0c0" in room and "prologue.size()) != 0" in room
assert room.index("r.originalRoom = reinterpret_cast<RoomTest>(slots[0x3f]);") < room.index("table.write_vfunc(0x3f,hookRoom)")
# Claims are gated on the caller: the culling functions are guarded (address,
# live unwind extent, call-site bytes) and published before the slot is written.
assert room.index("cullingCallersGuard(base)") < room.index("Refusal::roomCallers") < room.index("r.callers = roomCallersAt(base,") < \
    room.index("table.write_vfunc(0x3f,hookRoom)")
guard = spike_body("inline unsigned cullingCallersGuard(", "inline void installRoomHook(")
assert "REL::ID(cullingIds[i]).address() != base+cullingRvas[i].begin" in guard and "functionExtent(base,cullingRvas[i])" in guard
assert "base+cullingReturnRvas[i]-call.size()" in guard and "0xff,0x90,0xf8,0x01,0x00,0x00" in guard
assert "RtlLookupFunctionEntry(base+rva.begin,&image,nullptr)" in spike
assert "if (r.config.roomSeedMode) installRoomHook(base);" in spike
hook_room = spike_body("inline bool hookRoom(", "inline bool within(")
assert hook_room.index("r.originalRoom(room,point)") < hook_room.index("r.roomActive.load(") < hook_room.index("viewerQuery(*view,asked)") < \
    hook_room.index("seedClaim(")
assert hook_room.index("roomCaller(runtime->callers,reinterpret_cast<std::uintptr_t>(_ReturnAddress()))") < hook_room.index("r.originalRoom(room,point)")
assert hook_room.index("if (caller != RoomCaller::culling) {") < hook_room.index("r.counters.viewerCalls") < hook_room.index("seedClaim(caller,*view,asked,")
assert hook_room.count("return true;") == 1 and "vanillaTrue" in hook_room[:hook_room.index("return true;")]  # true only repeats a vanilla true
for engine in ("RE::PlayerCharacter", "RE::PlayerCamera", "RE::TESObjectCELL", "GetAppCulled", "logger", "emit("):
    assert engine not in hook_room, engine  # culling thread: the published copy and atomics only
# Hide is interior-only in this wave: an exterior cell reaches neither the scan nor its rays.
assert end_frame.index("if (hideScanned(r.config.seeThroughMode,parent->IsInteriorCell())) { scanHide(player,parent);") < \
    end_frame.index("noteExterior(cellId);")
assert spike.count("scanHide(player,parent)") == 1 and spike.count("castRays(player,parent,") == 1
assert "castRays(player,parent," in spike_body("inline void scanHide(", "// Exterior cell with hide configured")
scan = spike_body("inline void scanHide(", "// Exterior cell with hide configured")
assert scan.count("r.config.hideRadius") == 2 and "groundReach(feet," in scan  # both classify calls are radius-bounded
assert scan.index("release(slot,Exit::underfoot)") < scan.index("nearestFirst(due,dueCount);") < scan.index("SetAppCulled(true)")
assert "if (!r.budget.take()) break;" in scan
claim = spike_body("inline Hidden* claimSlot(", "inline void noteSkip(")
assert claim.index("farthestSlot(r.hidden,") < claim.index("evicts(reach,") < claim.index("release(r.hidden[static_cast<std::size_t>(farthest)],Exit::evicted);")
assert spike.count("SetAppCulled(true)") == 1 and spike.count("SetAppCulled(false)") == 1
assert "SetAppCulled(false)" in spike_body("inline void release(", "inline void restoreAll(")
assert "slots[0x33] != REL::Module::get().base()+0x104be80" in spike
for forbidden in ("BSFixedString", "GetMenu(", "IsMenuOpen(", "RequestCameraControl", "AddTask("):
    assert forbidden not in spike, forbidden
codes = dict(re.findall(r"(\w+) = (\d+)", spike_body("enum class Refusal : unsigned {", "};")))
assert len(codes) == 19 and len(set(codes.values())) == len(codes)
for name in codes:
    assert (spike + source).count("Refusal::" + name) == 1, name  # one site per code
print("PASS spike on wiring: frame-scoped suppression; shared caster detour; sealed gateway; caller-gated room hook; hook-thread isolation; "
      "interior-only radius-bounded nearest-first hide; one site per refusal code")

# Wave 2 shader cut (DYL-217): iSeeThroughMode=3. Off, or in any other mode,
# there is no shader runtime and every entry point is a no-op; on, the plane is
# published by the main thread, a pass differs from native only between its own
# SetupGeometry and RestoreGeometry, and neither engine code nor engine state
# is ever written: only two vtable slots per class and two device bindings.
cut = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader.h").read_text()
cut_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_policy.h").read_text()
spike_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_policy.h").read_text()
ini = (Path(__file__).resolve().parents[1] / "config/SkyCRPG_Camera.ini").read_text()


def cut_body(start, end):
    return cut[cut.index(start):cut.index(end, cut.index(start))]


assert cut.count("runtime = new Runtime;") == 1 and "runtime = new Runtime;" in cut_body("inline void configure(", "} // namespace")
assert spike.count("shader::configure(") == 1 and "shader::configure(" not in source and source.count("fixed::shader::") == 1
configure_spike = spike_body("inline void configure(", "} // namespace")
assert configure_spike.index("r.config = validate(requested,base,cursorKey,&adjusted);") < \
    configure_spike.index("if (shader::shaderMode(r.config.seeThroughMode))\n        shader::configure(shaderRequested,logger,") < \
    configure_spike.index("r.traceReady = r.writer.start(tracePath,rows);")
assert "constexpr bool shaderMode(unsigned seeThroughMode) { return seeThroughMode == 3; }" in cut_policy
assert "if (c.seeThroughMode > 1 && c.seeThroughMode != 3) c.seeThroughMode = 0;" in spike_policy
assert "constexpr bool hideScanned(unsigned seeThroughMode,bool interiorCell) { return seeThroughMode == 1 && interiorCell; }" in spike_policy
for helper in ("inline void install(std::uintptr_t base) {\n    if (!runtime) return;",
               "inline void deactivate() {\n    if (!runtime) return;",
               "inline void frameStart() {\n    if (!runtime) return;",
               "inline void publish(Vec eye,Vec forward,Vec feet,bool interiorCell,unsigned long long frame,double now) {\n    if (!runtime) return;",
               "inline void trace(double now) {\n    if (!runtime) return;",
               "inline bool tracing() { return runtime && ",
               "inline const char* status() { return !runtime ? "):
    assert helper in cut, helper
for key in ("fShaderCutHeight", "fShaderCutRadius", "iShaderCutMaxPerFrame", "iShaderCutShaders", "iShaderCutTrace",
            "bShaderCutEffect", "bVoidColor", "sVoidColor", "bVoidHideSky", "bShaderCutEffectWhole"):
    assert gate.count('"FixedCameraSpike","' + key + '"') == 1 and ("\n" + key + "=") in ini, key
assert "\niSeeThroughMode=0\n" in ini and "\nfShaderCutHeight=160\n" in ini and "\nfShaderCutRadius=0\n" in ini and "\niShaderCutTrace=0\n" in ini
# The spike's bracket drives it: any thread may only switch it off.
assert "shader::deactivate(); // wave 2 shader" in spike_body("inline void lifecycle(Exit cause) {", "// Plain placed references only.")
begin_frame = spike_body("inline void beginFrame() {", "inline void clearFrame() {")
assert begin_frame.index("!thin::onMain()) return;") < begin_frame.index("shader::frameStart();")
assert end_frame.index("shader::deactivate(); shader::trace(r.now);") < end_frame.index("r.previousEligible = false;") < \
    end_frame.index("publishView(viewer,eye);") < end_frame.index("shader::publish(viewer,") < end_frame.index("shader::trace(r.now);\n}")
assert "parent->IsInteriorCell(),r.frame,r.now);" in end_frame[end_frame.index("shader::publish(viewer,"):]
assert spike.count("shader::publish(") == 1 and spike.count("shader::frameStart(") == 1 and spike.count("shader::install(") == 1
install_spike = spike_body("inline void install(bool prerequisites,bool service) {", "// loadSettings, only when")
assert install_spike.index("Refusal::service") < install_spike.index("shader::install(base);") < install_spike.index("r.active.store(true,")
# kDataLoaded only asks; every guard, the one buffer and the slot writes run in the main-thread camera frame.
assert "commit" not in cut_body("inline void install(std::uintptr_t base) {", "// Any thread.")
frame_start = cut_body("inline void frameStart() {", "// Main thread, after the camera update")
assert cut.count("commit();") == 1 and "if (r.pending.exchange(false,std::memory_order_acq_rel)) commit();" in frame_start
assert frame_start.index("commit();") < frame_start.index("if (!r.installed) return;") < frame_start.index("fault(Fault::frame)") < \
    frame_start.index("Refusal::thread") < frame_start.index("Refusal::context") < frame_start.index("Refusal::view")
commit = cut_body("inline void commit() {", "// ---- called by the spike's frame bracket")
assert commit.index("foreignRenderer()") < commit.index("addressGuard(base)") < commit.index("tableGuard(base,r.config.shaders)") < \
    commit.index("method::createBuffer") < commit.index("if (answer < 0 || !created) {") < commit.index("Refusal::device") < \
    commit.index("r.buffer = created;") < commit.index("hookClass<0>(); hookClass<1>();") < commit.index("r.installed = true;")
assert commit.count("return;") == 4 and "device && context ? invoke<std::int32_t>(device,method::createBuffer," in commit
# Second pass: Effect and the void colour only after the cut is installed and logged, each refusing on its own.
assert commit.index('"FixedCameraSpike shader cut installed shaders=') < commit.index("installEffect(base);") < \
    commit.index("installVoid(base);") < commit.index('"FixedCameraSpike shader look installed void=')
foreign = cut_body("inline const char* foreignRenderer() {", "// 0 = every ID resolves")
for needle in ('L"CommunityShaders.dll"', 'L"d3dcompiler_46e.dll"', 'GetModuleHandleW(L"d3d11.dll")', "GetSystemDirectoryW(", "_wcsnicmp(loaded,folder,prefix) != 0"):
    assert needle in foreign, needle
# No code is patched and nothing is allocated executable: two slot writes per class, originals stored first, restore first.
for forbidden in ("safe_write", "VirtualAlloc", "VirtualProtect", "AllocTrampoline", "write_call", "write_branch", "poke", "BSFixedString",
                  "GetMenu(", "AddTask(", "_ReturnAddress"):
    assert forbidden not in cut, forbidden
hook_class = cut_body("template<std::size_t Index>\nvoid hookClass() {", "// Main thread, inside the camera frame service.")
assert cut.count("write_vfunc(") == 2 and hook_class.index("r.setup[Index] = ") < hook_class.index("r.restore[Index] = ") < \
    hook_class.index("table.write_vfunc(restoreSlot,&hookRestore<Index>)") < hook_class.index("table.write_vfunc(setupSlot,&hookSetup<Index>)")
slot_guard = cut_body("inline unsigned slotGuard(", "// Lighting and Utility, as enabled")
assert "slots[setupSlot] != base+c.setupRva" in slot_guard and "slots[restoreSlot] != base+c.restoreRva" in slot_guard
assert "c.setupBytes.size()) != 0" in slot_guard and "c.restoreBytes.size()) != 0" in slot_guard
table_guard = cut_body("inline unsigned tableGuard(", "// Effect: 0, 1 its vtable ID")
assert "for (unsigned i = 0;i < mainClasses;++i) {" in table_guard and "slotGuard(base,i)" in table_guard
effect_guard = cut_body("inline unsigned effectGuard(", "template<std::size_t Index>\nvoid hookClass() {")
assert effect_guard.index("REL::ID(c.table.id).address() != base+c.table.rva") < effect_guard.index("slotGuard(base,effectClass)")
assert "REL::ID(data[i].id).address() != base+data[i].rva" in cut and "REL::ID(shaderClasses[i].table.id).address() != base+shaderClasses[i].table.rva" in cut
# Each hook calls the engine function exactly once; a pass is considered only after its own SetupGeometry.
setup_hook = cut_body("std::uintptr_t hookSetup(", "template<std::size_t Index>\nstd::uintptr_t hookRestore(")
assert setup_hook.count("r.setup[Index](self,pass,flags,extra)") == 1 and cut.count("r.setup[Index](") == 1
assert setup_hook.index("fault(Fault::setup)") < setup_hook.index("r.setup[Index](self,pass,flags,extra)") < \
    setup_hook.index("if (pass && r.active.load(std::memory_order_acquire)) {") < setup_hook.index("consider(pass,Index);")
restore_hook = cut_body("std::uintptr_t hookRestore(", "// ---- install")
assert restore_hook.count("r.restore[Index](self,pass,flags,extra)") == 1 and cut.count("r.restore[Index](") == 1
assert restore_hook.index("r.restore[Index](self,pass,flags,extra)") < restore_hook.index("r.opened.load(") < \
    restore_hook.index("if (!thin::onMain()) r.offMain[groupOf(Index)].fetch_add(") < restore_hook.index("else if (!finish()) fault(Fault::shaderChanged);")
# Selection order, cheapest first; nothing is read from engine objects off the main thread or outside the published view.
consider = cut_body("inline void consider(", "// ---- hooks")
assert consider.index("r.latched.load(") < consider.index("if (!thin::onMain()) { r.offMain[group].fetch_add(1ull,std::memory_order_relaxed); return; }") < \
    consider.index("++w.passes;") < consider.index("r.opened.load(") < consider.index("!fresh(r.view,r.clock)") < \
    consider.index("peek<Vec>(r.state,offset::posAdjust)") < consider.index("if (!sameView(r.view,eye,direction)) {") < \
    consider.index("peek<const std::uint8_t*>(pass,offset::passGeometry)") < consider.index("skip = reach(r.view,bound,") < \
    consider.index("skip = ownerSkip(geometry,owner);") < consider.index("shaderFor(group)") < consider.index("!r.budgets[group].take()") < \
    consider.index("apply(*entry,whole ? wholeConstant : wanted,shaderClass);")
assert "geometry && usable(wanted)" in consider and "offset::geometrySkin) != nullptr" in consider and cut.count("apply(*entry,") == 1
# Effect passes take exactly this path (same view test, bound, owner rules, audited bytecode); they never feed the FCS-76 view check.
assert "const std::size_t group = groupOf(shaderClass);" in consider and "if (!group) ++r.frameMatched;" in consider and \
    "++w.passes; if (!group) ++r.framePasses;" in consider
off_thread = consider[consider.index("if (!thin::onMain())"):consider.index("auto& w = r.windows[group];")]
assert off_thread.count(";") == 2  # one counter, then return: nothing else is touched off the main thread
# The owner is found as the engine finds it: a user only on a fade node, past carried ammunition; actors and excluded kinds stay.
owner = cut_body("inline RE::TESObjectREFR* ownerOf(", "// The pass is drawn with the patched shader")
assert owner.index("offset::asFadeNode") < owner.index("peek<RE::TESObjectREFR*>(fade,offset::objectUser)") < owner.index("!= ammoForm) break;") < \
    owner.index("offset::objectParent")
assert "hop < maxOwnerHops" in owner and "RE::FormType::ActorCharacter" in owner and "kindOf(ref,ref->GetBaseObject())" in owner
# Only audited bytecode is patched, and a shader is used only after the device accepted it.
prepare = cut_body("inline void prepare(", "// The vertex shader the engine bound")
assert prepare.index("size < containerHeader || size > largestBlob") < prepare.index("codeHash(code,size)") < prepare.index("if (!auditedFor(group,hash)) {") < \
    prepare.index("patchVertexShader(code,size)") < prepare.index("if (patched.result != Patch::ok) {") < prepare.index("method::createVertexShader") < \
    prepare.index("if (answer < 0 || !created) {") < prepare.index("entry.patched = created;")
assert prepare.count("return;") == 4 and cut.count("entry.patched = ") == 1 and cut.count("method::createVertexShader") == 1
# A cut pass: the two calls that can fail come first; buffer, then shader; closed by the engine's own shader and an empty slot.
apply = cut_body("inline void apply(", "// A pass of a hooked shader class")
assert apply.index("method::map") < apply.index("if (answer < 0 || !mapped.data) {") < apply.index("latch();") < apply.index("Refusal::map") < \
    apply.index("std::memcpy(mapped.data,&wanted,sizeof(wanted));") < apply.index("method::unmap") < \
    apply.index("method::vsSetConstantBuffers,cutBuffer,1u,") < apply.index("method::vsSetShader,entry.patched,") < \
    apply.index("r.opened.store(true,") < apply.index("++w.cut;")
finish = cut_body("inline bool finish() {", "inline void fault(")
assert finish.index("peek<const std::uint8_t*>(r.state,offset::currentVertexShader) == r.openObject") < \
    finish.index("if (same) invoke<void>(r.context,method::vsSetShader,peek<void*>(r.openObject,offset::shaderObject),") < \
    finish.index("void* none[1]{};") < finish.index("method::vsSetConstantBuffers,cutBuffer,1u,static_cast<void* const*>(none)") < \
    finish.index("r.opened.store(false,")
assert cut.count("method::vsSetShader") == 2 and cut.count("method::vsSetConstantBuffers") == 2 and cut.count("std::memcpy(") == 2
fault = cut_body("inline void fault(", "// The patched device shader")
assert fault.index("finish()") < fault.index("latch();") < fault.index("Refusal::bracket")
publish = cut_body("inline void publish(", "inline void trace(double now) {")
assert publish.index("if (r.published+1u != frame) r.plane.reset();") < \
    publish.index("if (publishes(r.installed,r.latched.load(std::memory_order_acquire),interiorCell,feet.z))") < \
    publish.index("makeView(r.config,eye,forward,feet,r.plane.update(feet.z,now),frame,now)") < \
    publish.index("if (!next) { r.active.store(false,std::memory_order_release); settleSky(false); return; }") < publish.index("r.active.store(true,")
assert "inline bool publishes(bool installed,bool latched,bool interiorCell,float feetZ) {\n    return installed && !latched && interiorCell && " \
    "std::isfinite(feetZ);" in cut_policy
void = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_void.h").read_text()
cut_codes = dict(re.findall(r"(\w+) = (\d+)", cut_policy[cut_policy.index("enum class Refusal : unsigned {"):cut_policy.index("constexpr unsigned firstRefusal")]))
assert len(cut_codes) == 27 and sorted(int(v) for v in cut_codes.values()) == list(range(66, 93))
sky = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_sky.h").read_text()
sky_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_sky_policy.h").read_text()
style = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_style.h").read_text()
style_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_style_policy.h").read_text()
prepare_table = cut_policy[cut_policy.index("constexpr std::array<PrepareCodes,2> prepareCodes{{"):cut_policy.index("// ---- [FixedCameraSpike] shader-cut keys")]
for name in cut_codes:
    sites = cut.count("Refusal::" + name) + void.count("Refusal::" + name) + sky.count("Refusal::" + name) + style.count("Refusal::" + name) + \
        prepare_table.count("Refusal::" + name)
    assert sites == 1, name  # one site per code
for field in ("object", "unaudited", "patch", "create"):
    assert prepare.count("refuse(codes." + field + ",text); return;") == 1, field
assert "std::uint32_t{1}" not in cut and "refusalBit(code)" in cut_body("inline void refuse(", "inline void latch()")  # own once-mask: codes above 63
for constant in ("constexpr Address stateAddress{388819,0x20cfc00};", "constexpr Address deviceAddress{411348,0x3330190};",
                 "constexpr Address contextAddress{411391,0x3331f30};", '{"lighting",{255053,0x1b47420},0x1549550,0x154a900,',
                 '{"utility",{255302,0x1b4ab58},0x15673f0,0x15689c0,', "constexpr std::size_t setupSlot = 6, restoreSlot = 7;",
                 "currentVertexShader = 0x348, posAdjust = 0x35c, viewDirection = 0x3a0;", "objectParent = 0x30, objectBound = 0xe4, objectUser = 0xf8;",
                 "asFadeNode = 0x28;", "geometrySkin = 0x130;", "shaderObject = 0x08, shaderCodeSize = 0x10, shaderCode = 0x68;",
                 "createBuffer = 0x18, createVertexShader = 0x60;", "vsSetConstantBuffers = 0x38, vsSetShader = 0x58, map = 0x70, unmap = 0x78;",
                 "constexpr std::uint32_t cutBuffer = 13, cutRegister = 16;", "constexpr std::uint32_t mapWriteDiscard = 4;",
                 "constexpr std::uint32_t ammoForm = 0x2a;", '{"effect",{255194,0x1b48ab0},0x1557090,0x1557f50,',
                 "constexpr std::size_t mainClasses = 2, effectClass = 2;", "constexpr Address mainViewAddress{36558,0x656d10};",
                 "constexpr Address clearColorAddress{77249,0x100a3f0};", "constexpr std::uintptr_t voidCallRva = 0x656daa;"):
    assert constant in cut_policy, constant
print("PASS shader cut wiring: no runtime outside mode 3; main-thread install after every guard; no code or engine-state write; "
      "engine function called once per hook; audited bytecode only; fail-before-bind; own shader and empty slot restored; one site per FCS-66..92")

# Wave 2 shader, second pass (owner decision 2026-11-10): the Effect class and
# the void colour. Both exist only inside the shader runtime (mode 3), are
# installed by its main-thread commit after every cut guard (so ENB and
# Community Shaders refuse them with the cut, FCS-67), and are native whenever
# the cut's snapshot is not published.
assert cut.endswith('#include "fixed_spike_shader_void.h" // second pass: the void colour\n'
                    '#include "fixed_spike_shader_sky.h"  // third pass: the sky dome over the void\n'
                    '#include "fixed_spike_shader_style.h" // fourth pass: Stage 1 of the seamless cutaway\n')
assert "inline void installVoid(std::uintptr_t base);" in cut and cut.count("installVoid(base);") == 1 and cut.count("installEffect(base);") == 1
assert void.count("inline void installVoid(std::uintptr_t base) {") == 1 and "runtime = " not in void
install_effect = cut_body("inline void installEffect(std::uintptr_t base) {", "// Main thread, inside the camera frame service.")
assert install_effect.index("if (!r.config.effect) { r.effectState = Feature::off; return; }") < install_effect.index("effectGuard(base)") < \
    install_effect.index("Refusal::effectTable") < install_effect.index("hookClass<effectClass>();") < install_effect.index("Feature::installed")
assert cut.count("hookClass<effectClass>();") == 1 and "if (!classEnabled(r.config,Index)) return;" in hook_class
assert "constexpr bool shaderEnabled(unsigned mask,std::size_t index) { return index < mainClasses && " in cut_policy
assert "constexpr bool classEnabled(const Config& c,std::size_t index) { return index == effectClass ? c.effect : shaderEnabled(c.shaders,index); }" in cut_policy
assert "if (!r.config.effect)" not in consider  # the hooks decide; once hooked, Effect passes follow the same gate as Lighting
# The void hook: the engine's SetClearColor exactly once on every path, with its own arguments unless the decision is `applied`.
def void_body(start, end):
    return void[void.index(start):void.index(end, void.index(start))]
hook = void_body("inline void hookClearColor(", "// Committed read-write memory")
assert hook.count("r.clearColor(") == 2 and \
    "    if (decision == Look::applied) r.clearColor(renderer,r.config.color.red,r.config.color.green,r.config.color.blue,alpha);\n" \
    "    else r.clearColor(renderer,red,green,blue,alpha);\n}" in hook
assert hook.index("const bool onMainThread = thin::onMain();") < hook.index("if (!onMainThread) r.voidOffMain.fetch_add(") < \
    hook.index("lookDecision(onMainThread,r.active.load(std::memory_order_acquire),r.latched.load(std::memory_order_acquire),") < \
    hook.index("onMainThread && r.haveView && fresh(r.view,thin::seconds())") < hook.index("if (onMainThread) {")
assert "constexpr Look lookDecision(bool onMain,bool active,bool latched,bool freshView) {\n    return !onMain ? Look::offMain : latched ? " \
    "Look::latched : !active ? Look::inactive : !freshView ? Look::stale : Look::applied;" in cut_policy
# One code write, after every check, to the call's displacement only; the relay is sealed execute-read first; nothing else is written.
install_void = void_body("inline void installVoid(std::uintptr_t base) {", "} // namespace")
assert install_void.index("if (!r.config.voidColor) { r.voidState = Feature::off; return; }") < install_void.index("voidSiteCheck(base,mainView,clearColor,") < \
    install_void.index("Refusal::voidSite") < install_void.index("relayPage(site)") < install_void.index("relay(reinterpret_cast<std::uintptr_t>(&hookClearColor))") < \
    install_void.index("VirtualProtect(page,relaySize,PAGE_EXECUTE_READ,&previous)") < install_void.index("Refusal::voidMemory") < \
    install_void.index("FlushInstructionCache(GetCurrentProcess(),page,relaySize);") < install_void.index("r.clearColor = reinterpret_cast<ClearColor>(clearColor);") < \
    install_void.index("REL::safe_write(site+1u,&value,sizeof(value));") < install_void.index("FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(site),5);") < \
    install_void.index("r.voidState = Feature::installed;")
assert void.count("safe_write(") == 1 and "const std::int32_t value = *displacement;" in install_void and install_void.count("return;") == 3
assert "const std::uintptr_t site = base+voidCallRva;" in install_void and "PAGE_EXECUTE_READWRITE" not in void
for forbidden in ("write_call", "write_branch", "AllocTrampoline", "GetTrampoline", "write_vfunc", "0x3d3e7", "0x27d8", "BSFixedString", "AddTask("):
    assert forbidden not in void, forbidden
assert "0x3d3e7" not in cut_policy and "0x27d8" not in cut_policy and "0x27d8" not in cut
page = void_body("inline std::uint8_t* relayPage(", "// Main thread, from commit.")
assert "relayReach(site,granularity)" in page and "region.State == MEM_FREE" in page and "MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE" in page and \
    "if (end <= at) return nullptr;" in page
# The thread rule and the trace: each group reports its own off-main calls; FixedShader keeps the first pass's counts.
assert frame_start.index("Refusal::thread") < frame_start.index("Refusal::effectThread") < frame_start.index("Refusal::voidThread")
trace_body = cut_body("inline void trace(double now) {", "inline bool tracing()")
assert "s.window = r.windows[0];" in trace_body and "look.effect = r.windows[1];" in trace_body and \
    trace_body.index("if (format(line,sizeof(line),s)) r.emitter(line);") < trace_body.index("if (formatLook(line,sizeof(line),look)) r.lookEmitter(line);")
assert '"relPlane=%.2f passes=%llu cut=%llu byClass=%llu,%llu peak=%u cap=%u uploads=%llu shaders=%u,%u,%u created=%llu open=%llu "' in cut_policy
assert "LineBudget shaderLines{shader::traceLines},shaderLookLines{shader::traceLines};" in spike and \
    "[](const char* text) { emit(runtime->shaderLookLines,text); },[](const char* text) { emit(runtime->shaderVoidLines,text); },\n" \
    "            [](const char* text) { emit(runtime->shaderStyleLines,text); },[](const char* text) { emit(runtime->shaderStyle2Lines,text); });" \
    in configure_spike and \
    "(shader::tracing() ? shader::traceKinds()*shader::traceLines : 0u)" in configure_spike
assert 'shaderCut.colorText = read("FixedCameraSpike","sVoidColor","0,0,0");' in gate and \
    "const bool colorChanged = parseColor(c.colorText,c.color) != ColorText::ok;" in cut_policy
assert "\nbShaderCutEffect=1\n" in ini and "\nbVoidColor=1\n" in ini and "\nsVoidColor=0,0,0\n" in ini
print("PASS shader second-pass wiring: Effect and void only inside mode 3 after every cut guard; Effect through the cut's own path with its own "
      "guard, list and counters; one engine call per void hook, engine colour unless applied; one 4-byte code write after every check; "
      "FixedShader row unchanged")

# Wave 2 shader, third pass: the sky dome over the void and Effect passes
# clipped whole. The sky root's hidden bit is the only engine write; it is made
# on the main thread only, set only on a frame that published the cut, put
# back on the first main-thread frame that did not, and only where the
# engine's own SetMode rule still says the root is shown.
assert "inline void installSky(std::uintptr_t base);" in cut and "inline void settleSky(bool want);" in cut and cut.count("installSky(base);") == 1
assert commit.index("installVoid(base);") < commit.index('"FixedCameraSpike shader look installed void=') < commit.index("installSky(base);") < \
    commit.index('"FixedCameraSpike shader void installed sky=%s effectWhole=%s"')
def sky_body(start, end):
    return sky[sky.index(start):sky.index(end, sky.index(start))]
install_sky = sky_body("inline void installSky(std::uintptr_t base) {", "// Main thread only:")
assert install_sky.index("if (!r.config.hideSky) { r.skyState = Feature::off; return; }") < install_sky.index("skySiteCheck(base,REL::ID(skyAddress.id).address(),") < \
    install_sky.index("Refusal::skySite") < install_sky.index("r.skyState = Feature::installed;")
settle = sky_body("inline void settleSky(bool want) {", "} // namespace")
assert settle.index("if (r.skyState != Feature::installed) return;") < settle.index("skyStep(want,r.skyRoot != nullptr,root != nullptr,root == r.skyRoot,")
assert settle.count("std::memcpy(root+sky::objectFlags,&value,sizeof(value));") == 2 and sky.count("std::memcpy(") == 2
assert "const std::uint32_t value = objectFlags | sky::hidden;" in settle and "const std::uint32_t value = objectFlags & ~sky::hidden;" in settle
assert settle.index("if (step == SkyStep::hide) {") < settle.index("objectFlags | sky::hidden") < settle.index("} else if (step == SkyStep::restore) {") < \
    settle.index("objectFlags & ~sky::hidden") < settle.index("} else if (step == SkyStep::leave) {") < settle.index("Refusal::skyLeft")
for forbidden in ("safe_write", "VirtualAlloc", "VirtualProtect", "write_vfunc", "write_call", "write_branch", "AllocTrampoline", "runtime = ",
                  "GetSingleton", "AddTask(", "BSFixedString"):
    assert forbidden not in sky, forbidden
# settleSky runs only on the main thread: publish (true only after the snapshot is published), the main-thread
# branch of deactivate, frameStart after a latch.
assert cut.count("settleSky(") == 5 and cut.count("settleSky(true);") == 1
assert publish.index("r.active.store(true,std::memory_order_release);") < publish.index("settleSky(true);")
deactivate = cut_body("inline void deactivate() {", "// Main thread, before the camera update.")
assert "if (thin::onMain()) settleSky(false);" in deactivate and deactivate.index("runtime->active.store(false,") < deactivate.index("settleSky(false)")
assert frame_start.index("if (!r.installed) return;") < frame_start.index("if (r.latched.load(std::memory_order_acquire)) settleSky(false);")
assert "constexpr Address skyAddress{401652,0x31e0d80};" in sky_policy and "constexpr Address skyModeAddress{26222,0x411660};" in sky_policy and \
    "constexpr std::uintptr_t skyRuleRva = 0x411794;" in sky_policy and \
    "constexpr std::size_t root = 0x08, mode = 0x1bc, flags = 0x1dc;" in sky_policy and "constexpr std::size_t objectFlags = 0xf4;" in sky_policy
assert "constexpr bool engineShowsSky(std::uint32_t mode,std::uint32_t flags) { return (mode == 2u || mode == 3u) && !(flags & sky::hideSky); }" in sky_policy
# Effect passes: whole only for the Effect group with the key on; the selection is unchanged.
assert "const bool whole = group && r.config.effectWhole && wholeEffect(r.view,bound);" in consider and \
    consider.index("bound = {peek<Vec>(geometry,offset::objectBound),") < consider.index("skip = reach(r.view,bound,") < \
    consider.index("const bool whole = group && r.config.effectWhole && wholeEffect(r.view,bound);") < \
    consider.index("apply(*entry,whole ? wholeConstant : wanted,shaderClass);")
assert "constexpr Constant wholeConstant{0.0f,-1.0f,0.0f,0.0f};" in sky_policy
assert "inline Skip ownerSkip(const std::uint8_t* geometry,std::uint32_t& owner) {" in cut and "owner = ref->GetFormID();" in cut
assert trace_body.index("if (formatLook(line,sizeof(line),look)) r.lookEmitter(line);") < trace_body.index("if (formatVoid(line,sizeof(line),voids)) r.voidEmitter(line);")
assert "r.windows = {}; r.look = {}; r.voidWindow = {}; r.haveOther = false;" in trace_body
assert "LineBudget shaderVoidLines{shader::traceLines};" in spike and "\nbVoidHideSky=1\n" in ini and "\nbShaderCutEffectWhole=1\n" in ini
print("PASS shader third-pass wiring: sky root hidden only on published main-thread frames, put back only under the engine's own rule, "
      "two one-bit writes and nothing else; Effect clipped whole only in its group with the key on; FixedShaderVoid after FixedShaderLook; "
      "one site per FCS-89..90")

# Wave 2 shader, fourth pass: Stage 1 of the seamless cutaway (iCutStyle=1).
# A per-reference choice of the constant buffer in slot 13 inside the same
# hooks; nothing new is hooked or written; iCutStyle=0 allocates nothing and
# takes the third pass's path unchanged.
def style_body(start, end):
    return style[style.index(start):style.index(end, style.index(start))]
assert "inline bool styled();" in cut and "inline void styledPass(void* pass,std::size_t shaderClass,Vec eye);" in cut and \
    "inline void shadowPass(void* pass,std::size_t shaderClass);" in cut
assert "        if (!group && styled()) shadowPass(pass,shaderClass); // fourth pass: shadows of what the main view removed whole\n        return;\n    }" in consider
assert consider.index("++w.otherView;") < consider.index("if (!group && styled()) shadowPass(pass,shaderClass);") < \
    consider.index("if (!group) ++r.frameMatched;") < consider.index("if (!group && styled()) { styledPass(pass,shaderClass,eye); return; }") < \
    consider.index("peek<const std::uint8_t*>(pass,offset::passGeometry)")
assert cut.count("styledPass(") == 2 and cut.count("shadowPass(") == 2 and cut.count("configureStyle();") == 2 and cut.count("installStyle();") == 2  # declaration and one call
styled_fn = style_body("inline bool styled() {", "inline const char* modelPath(")
assert "runtime->style->state == Feature::installed && !runtime->latched.load(" in styled_fn
configure_style = style_body("inline void configureStyle() {", "// commit, after the sky.")
assert "if (r.config.style != stageOneStyle) return;" in configure_style and style.count("new StyleRuntime") == 1 and "runtime = " not in style
assert commit.index("installSky(base);") < commit.index("installStyle();") < commit.index('"FixedCameraSpike shader style installed style=%u state=%s gallery=%u shadows=%u"')
install_style = style_body("inline void installStyle() {", "inline void traceStyle(")
assert install_style.index("if (!r.style) return;") < install_style.index("method::createBuffer") < \
    install_style.index("Refusal::styleBuffers") < install_style.index("s.state = Feature::installed;")
assert install_style.count("method::createBuffer") == 2 and "const InitialData initial{&removeConstant,0u,0u};" in install_style and \
    "static_cast<const void*>(&initial),&remove)" in install_style
# The per-pass path: the flat cut's selection, then the reference, then audited bytecode and the budget before any bind.
pass_fn = style_body("inline void styledPass(", "// A Utility pass of another view")
assert pass_fn.index("skip = reach(r.view,bound,") < pass_fn.index("ownerOf(geometry)") < pass_fn.index("ownerRule(") < pass_fn.index("entryFor(ref)") < \
    pass_fn.index("if (d.treat == Treat::keep)") < pass_fn.index("shaderFor(0)") < pass_fn.index("!r.budgets[0].take()") < \
    pass_fn.index("bindStyle(*entry,s.removeBuffer,shaderClass,true);") < pass_fn.index("apply(*entry,plane,shaderClass);") < \
    pass_fn.index("uploadStorey(") < pass_fn.index("bindStyle(*entry,s.storeyBuffer,shaderClass,true);")
assert "if (skip != Skip::none && !(below && s.cellStyle == CellStyle::kit)) { skipped(skip); return; }" in pass_fn and \
    "if (below && d.treat != Treat::remove) { skipped(Skip::below); return; }" in pass_fn
shadow = style_body("inline void shadowPass(", "// ---- frame, cell, install, trace")
assert "if (!r.config.styleShadows || shaderClass != 1 || s.cellStyle != CellStyle::kit) return;" in shadow and \
    shadow.index("wasRemoved(geometry)") < shadow.index("shaderFor(0)") < shadow.index("s.shadowBudget.take()") < \
    shadow.index("bindStyle(*entry,s.removeBuffer,shaderClass,false);")
bind = style_body("inline void bindStyle(", "// ---- the passes")
assert bind.index("method::vsSetConstantBuffers,cutBuffer,1u,") < bind.index("method::vsSetShader,entry.patched,") < bind.index("r.opened.store(true,")
# Only the storey buffer is ever Mapped; the remove buffer is filled once, at creation. The only copy goes into the mapped buffer.
assert style.count("method::map") == 1 and "method::map,s.storeyBuffer," in style and style.count("std::memcpy(") == 1 and \
    "std::memcpy(mapped.data,&wanted,sizeof(wanted));" in style
for forbidden in ("safe_write", "VirtualAlloc", "VirtualProtect", "write_vfunc", "write_call", "write_branch", "AllocTrampoline", "BSFixedString",
                  "AddTask(", "GetSingleton", "SetAppCulled", "GetMenu(", "_ReturnAddress"):
    assert forbidden not in style, forbidden
# Frame, cell and trace hooks: on the main thread, inside the spike's bracket.
assert publish.index("for (auto& budget : r.budgets) budget.frame();") < publish.index("styleFrame();") < publish.index("r.active.store(true,")
assert end_frame.index("shader::noteCell(parent);") < end_frame.index("shader::publish(viewer,") and spike.count("shader::noteCell(") == 1
assert "if (!runtime || !runtime->style || !cell || !thin::onMain()) return;" in style_body("inline void noteCell(", "// configure, after validate")
assert trace_body.index("if (formatVoid(line,sizeof(line),voids)) r.voidEmitter(line);") < trace_body.index("traceStyle(now,voids.window);")
assert "if (!r.style) return;" in style_body("inline void traceStyle(", "} // namespace")
assert "LineBudget shaderStyleLines{shader::traceLines};" in spike
assert "inline unsigned traceKinds() { return runtime && runtime->config.style == stageOneStyle ? 5u : 3u; }" in cut
assert "LineBudget shaderStyle2Lines{shader::traceLines};" in spike
# W5 fixes: FixedShaderStyle2 after FixedShaderStyle; the style row's format is unchanged; roofs never kept; kit overhead removed;
# back pieces kept only standing and within the cap, else cut at the cap and not counted as slicedFar; gallery keeps that exceed
# their box are removed, never flat-cut.
trace_style = style_body("inline void traceStyle(", "} // namespace")
assert trace_style.index("if (formatStyle(line,sizeof(line),sample)) r.styleEmitter(line);") < trace_style.index("if (formatStyle2(line,sizeof(line),second)) r.style2Emitter(line);")
assert '"FixedShaderStyle2 t=%.3f frame=%llu capZ=%.2f cappedFar=%llu roofRemoved=%llu overheadRemoved=%llu galleryRemoved=%llu sample=window",' in style_policy
rules_fn = style_policy[style_policy.index("constexpr Decision rules(const Facts& f) {"):style_policy.index("constexpr Decision decide(const Facts& f) {")]
assert rules_fn.index("if (f.roof && f.piece == Piece::kit && !f.walkable) return {Treat::remove,Bucket::removedWhole,Note::roof};") < \
    rules_fn.index("if (f.above || (!object && f.overhead)) {")
assert "f.walkable = box.valid && box.high.z <= feet.z+floorTop;" in style
assert "if ((d.bucket == Bucket::keptFar || d.bucket == Bucket::cappedFar) && geometryExceeds(e->box,bound.centre,bound.radius))" in pass_fn
assert "f.standing && !f.overCap ? Decision{Treat::keep,Bucket::keptFar} : Decision{Treat::cap,Bucket::cappedFar,Note::cap};" in rules_fn
assert "if (d.bucket != Bucket::cappedFar && d.bucket != Bucket::interactCut && (e ? e->side == Side::back : false)) ++sw.slicedFar;" in pass_fn
assert "else if (d.bucket == Bucket::keptGallery && geometryExceeds(e->box,bound.centre,bound.radius)) {" in pass_fn and \
    "Decision{Treat::remove,e->piece == Piece::object ? Bucket::removedObject : Bucket::removedWhole};" in pass_fn
for key in ("iCutStyle", "bCutKeepFarGallery", "bCutStyleShadows"):
    assert gate.count('"FixedCameraSpike","' + key + '"') == 1 and ("\n" + key + "=") in ini, key
assert "\niCutStyle=1\n" in ini and "\nbCutKeepFarGallery=0\n" in ini and "\nbCutStyleShadows=1\n" in ini
assert "if (c.style > stageOneStyle) c.style = stageOneStyle;" in cut_policy and "unsigned style{1};" in cut_policy
# The policy decides nothing for a cell that is not a kit building, and slices only kit pieces.
assert "if (!f.kitCell) return {Treat::flat,Bucket::flat};" in style_policy and "constexpr Constant removeConstant{-1.0f,-1.0e9f,0.0f,0.0f};" in style_policy
# Review MINOR-3 of 8f1248f: an Effect pass is clipped whole only when its bound lies wholly above the feet.
assert "bound.centre.z-bound.radius > view.feet.z;" in sky_policy
# Fix pass after the review of 30503e0.
assert shadow.index("if (!shadowAudited()) { ++s.window.shadowSkips; return; }") < shadow.index("shaderFor(0)")  # MINOR-7: no FCS-75 from a shadow pass
audit_fn = style_body("inline bool shadowAudited() {", "// A Utility pass of another view")
assert "refuse(" not in audit_fn and "auditedFor(0,codeHash(object+offset::shaderCode,size))" in audit_fn and "size <= largestBlob" in audit_fn
assert "immutable.usage = 1; immutable.access = 0;" in install_style and "static_cast<const BufferDescription*>(&immutable),static_cast<const void*>(&initial),&remove)" in install_style
assert "geometryExceeds(e->box,bound.centre,bound.radius))" in pass_fn  # MINOR-3 as corrected after 2f5d9f6
assert "if (s.shadowAccepted.back() && s.shadowRejected.back()) return false;" in audit_fn and "std::array<const std::uint8_t*,256> shadowRejected{},shadowAccepted{};" in style
decide_fn = style_policy[style_policy.index("constexpr Decision decide(const Facts& f) {"):style_policy.index("// The height a slicing treatment cuts at.")]
assert decide_fn.index("const Decision d = rules(f);") < decide_fn.index("if (f.walkable) return {Treat::flat,Bucket::fallback};") < \
    decide_fn.index("if (f.interact) return {Treat::flat,Bucket::interactCut};")  # MINOR-4, MINOR-5
assert "f.interact = kind == Kind::object && base && base->GetFormType() != RE::FormType::Tree;" in style
assert '"shadowSkips=%llu fxFloorKept=%llu fxWholeRadius=%.1f interactCut=%llu sample=window",' in style_policy
print("PASS shader fourth-pass wiring: style runtime only with iCutStyle=1; per-reference buffer choice inside the existing hooks after the "
      "flat cut's own selection; audited bytecode and budget before any bind; remove buffer never Mapped; shadow rule on Utility other-view "
      "passes of removed geometry only; no new hook or engine write; one site per FCS-91..92")
