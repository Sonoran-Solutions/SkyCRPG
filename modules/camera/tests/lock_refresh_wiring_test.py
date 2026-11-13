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
               "inline void loadingMenu() {\n    if (runtime) ",
               "inline void install(bool prerequisites,bool service) {\n    if (!runtime) return;"):
    assert helper in spike, helper
unguarded = ("fixed::engaged()", "fixed::dropSnaps()", "fixed::lifecycle(", "fixed::loadingMenu()", "fixed::install(", "fixed::Exit::", "fixed::Config f;",
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
assert len(codes) == 20 and len(set(codes.values())) == len(codes) and codes["fadeSetting"] == "15"
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
    end_frame.index("publishView(viewer,eye);") < end_frame.index("shader::publish(viewer,") < end_frame.index("shader::trace(r.now);\n    observeFade(); settleFade(true); traceFade();")
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
    publish.index("if (!next) { r.active.store(false,std::memory_order_release); settleSky(false); doorLightsFrame(false); return; }") < \
    publish.index("r.active.store(true,")
assert "inline bool publishes(bool installed,bool latched,bool interiorCell,float feetZ) {\n    return installed && !latched && interiorCell && " \
    "std::isfinite(feetZ);" in cut_policy
void = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_void.h").read_text()
cut_codes = dict(re.findall(r"(\w+) = (\d+)", cut_policy[cut_policy.index("enum class Refusal : unsigned {"):cut_policy.index("constexpr unsigned firstRefusal")]))
assert len(cut_codes) == 45 and sorted(int(v) for v in cut_codes.values()) == list(range(66, 111))
sky = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_sky.h").read_text()
sky_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_sky_policy.h").read_text()
style = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_style.h").read_text()
style_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_style_policy.h").read_text()
frame = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_frame.h").read_text()
frame_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_frame_policy.h").read_text()
fill = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_fill.h").read_text()
fill_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_fill_policy.h").read_text()
tilt = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_tilt.h").read_text()
door_light = (Path(__file__).resolve().parents[1] / "src/fixed_spike_shader_door_light.h").read_text()
door_light_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_door_light_policy.h").read_text()
tilt_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_tilt_policy.h").read_text()
prepare_table = cut_policy[cut_policy.index("constexpr std::array<PrepareCodes,2> prepareCodes{{"):cut_policy.index("// ---- [FixedCameraSpike] shader-cut keys")]
for name in cut_codes:
    sites = cut.count("Refusal::" + name) + void.count("Refusal::" + name) + sky.count("Refusal::" + name) + style.count("Refusal::" + name) + \
        frame.count("Refusal::" + name) + fill.count("Refusal::" + name) + tilt.count("Refusal::" + name) + prepare_table.count("Refusal::" + name) + \
        door_light.count("Refusal::" + name)
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
      "engine function called once per hook; audited bytecode only; fail-before-bind; own shader and empty slot restored; one site per FCS-66..105")

# Wave 2 shader, second pass (owner decision 2026-11-10): the Effect class and
# the void colour. Both exist only inside the shader runtime (mode 3), are
# installed by its main-thread commit after every cut guard (so ENB and
# Community Shaders refuse them with the cut, FCS-67), and are native whenever
# the cut's snapshot is not published.
assert cut.endswith('#include "fixed_spike_shader_void.h" // second pass: the void colour\n'
                    '#include "fixed_spike_shader_sky.h"  // third pass: the sky dome over the void\n'
                    '#include "fixed_spike_shader_style.h" // fourth pass: Stage 1 of the seamless cutaway\n'
                    '#include "fixed_spike_shader_frame.h" // fifth pass: Stage 2, the frame pass\n'
                    '#include "fixed_spike_shader_fill.h" // Stage 3.1: the back-face fill\n'
                    '#include "fixed_spike_shader_tilt.h" // Stage 3.2: the tilted cut\n'
                    '#include "fixed_spike_shader_door_light.h" // the door void\'s lights (iDoorVoidLights=1)\n')
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
    "            [](const char* text) { emit(runtime->shaderStyleLines,text); },[](const char* text) { emit(runtime->shaderStyle2Lines,text); },\n" \
    "            [](const char* text) { emit(runtime->shaderFrameLines,text); },[](const char* text) { emit(runtime->shaderFillLines,text); },\n" \
    "            [](const char* text) { emit(runtime->shaderTiltLines,text); });" \
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
# Stage 3.2: the whole rule reads the published view, or in a tilted cell (Effect only) the tilted height at the bound centre.
assert "const bool whole = group && r.config.effectWhole && wholeEffect(local,bound);" in consider and \
    "const View local = tilt ? tiltedView(r.view,tiltPlanesNow(),bound.centre.x,bound.centre.y) : r.view;" in consider and \
    consider.index("bound = {peek<Vec>(geometry,offset::objectBound),") < consider.index("skip = reach(r.view,bound,") < \
    consider.index("const bool tilt = group && tiltCell();") < \
    consider.index("const bool whole = group && r.config.effectWhole && wholeEffect(local,bound);") < \
    consider.index("if (!whole && group && tiltPass(*entry,eye,shaderClass)) return;") < \
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
# (The door void's census copies one pointer, ExtraTeleport's presence bits, out of the extra list: the second memcpy.)
assert style.count("method::map") == 1 and "method::map,s.storeyBuffer," in style and style.count("std::memcpy(") == 2 and \
    "std::memcpy(mapped.data,&wanted,sizeof(wanted));" in style and "std::memcpy(&presence,extra+0x10,sizeof(presence));" in style
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
assert "    return (runtime && runtime->config.style == stageOneStyle ? 5u : 3u)+(runtime && runtime->framePass ? 1u : 0u)+(runtime && runtime->fill ? 2u : 0u)+\n" \
    "        (runtime && runtime->tilt ? 1u : 0u);" in cut
assert "LineBudget shaderStyle2Lines{shader::traceLines};" in spike
# W5 fixes: FixedShaderStyle2 after FixedShaderStyle; the style row's format is unchanged; roofs never kept; kit overhead removed;
# back pieces kept only standing and within the cap, else cut at the cap and not counted as slicedFar; gallery keeps that exceed
# their box are removed, never flat-cut.
trace_style = style_body("inline void traceStyle(", "} // namespace")
assert trace_style.index("if (formatStyle(line,sizeof(line),sample)) r.styleEmitter(line);") < trace_style.index("if (formatStyle2(line,sizeof(line),second)) r.style2Emitter(line);")
assert '"FixedShaderStyle2 t=%.3f frame=%llu capZ=%.2f cappedFar=%llu roofRemoved=%llu overheadRemoved=%llu galleryRemoved=%llu headroomZ=%.2f "\n        "heldMinusFeet=%.2f stackRemoved=%llu stackKept=%llu stacks=%u openStacks=%u sample=window",' in style_policy
# Stage 3.0 (iCutStacks): the census builds the uprights once per cell (only with the key on), the per-pass decision only
# reads them; the stack rule comes right after the roof rule and before the storey/overhead rule.
census = style_body("inline void noteCell(", "// configure, after validate")
assert "if (family == Family::architecture && runtime->config.stacks != noStacks) noteUpright(ref,base);" in census and \
    census.index("s.cellStyle = cellStyle(c);") < census.index("linkCellStacks();") and "s.uprightCount = 0;" in census
assert "f.openStack = r.config.stacks != noStacks && e.piece == Piece::member && openStackOf(ref->GetFormID());" in style
assert "memberCut(s.lineZ,s.headroomZ),s.headroomZ+standSlack);" in style_body("inline bool openStackOf(", "inline void refresh(")
stack_rules = style_policy[style_policy.index("constexpr Decision rules(const Facts& f) {"):style_policy.index("constexpr Decision decide(const Facts& f) {")]
assert stack_rules.index("Note::roof};") < stack_rules.index("if (f.openStack && f.piece == Piece::member && f.stackMode != noStacks) {") < \
    stack_rules.index("if (f.above || (!object && f.overhead)) {")
assert gate.count('"FixedCameraSpike","iCutStacks"') == 1 and "\niCutStacks=1\n" in ini and "unsigned stacks{1};" in cut_policy
assert '#include "fixed_spike_shader_stack_audit.h"' in style_policy
# Fix after batch T2: the head-height rules measure from the lower of the held floor and the feet, set once per frame.
assert "f.overhead = box.valid && box.low.z >= s.headroomZ+storeyHeadroom;" in style and \
    "storeyCandidate(e.piece,box,feet,s.headroomZ)" in style and "s.base+storeyHeadroom" not in style and \
    "s.headroomZ = headroomFloor(s.base,r.view.feet.z);" in style and \
    "constexpr float headroomFloor(float heldZ,float feetZ) { return feetZ < heldZ ? feetZ : heldZ; }" in style_policy
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

# Base rig: the engine's own player fade (bHoldPlayerFade). One Setting value is held while the rig
# holds the view and put back on the first frame it does not and on every main-thread lifecycle exit;
# no code is written; the Setting is found by ID and verified by address and name before any write.
fade_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_fade_policy.h").read_text()
install_fade = spike_body("inline void installFade(std::uintptr_t base) {", "// Main thread: endFrame")
assert install_fade.index("if (!r.config.holdFade) return;") < install_fade.index("REL::ID(fadeSettingId).address()") < \
    install_fade.index("fadeSettingCheck(base,at,name)") < install_fade.index("Refusal::fadeSetting") < install_fade.index("r.fadeValue = ")
assert "at == base+fadeSettingRva ? *reinterpret_cast<const char* const*>(at+settingName) : nullptr" in install_fade  # the name is read only at the audited address
settle_fade = spike_body("inline void settleFade(bool eligible) {", "// Main thread, every camera frame")
assert "if (!r.fadeValue || !thin::onMain()) return;" in settle_fade and "float& value = *r.fadeValue;" in settle_fade and "value = fadeHoldValue;" in settle_fade and "value = r.fadeSaved;" in settle_fade
assert "constexpr std::uint64_t fadeSettingId = 382599;" in fade_policy and "constexpr std::uintptr_t fadeSettingRva = 0x20ba460;" in fade_policy and \
    "constexpr float fadeHoldValue = -5000.0f;" in fade_policy
assert end_frame.count("observeFade(); settleFade(false); traceFade();") == 1 and end_frame.count("observeFade(); settleFade(true); traceFade();") == 1
assert end_frame.index("observeFade(); settleFade(false); traceFade();") < end_frame.index("r.previousEligible = false;") < \
    end_frame.index("shader::publish(viewer,") < end_frame.index("observeFade(); settleFade(true); traceFade();")
assert "if (runtime->active.load(std::memory_order_acquire) && thin::onMain()) { restoreAll(cause); settleFade(false); }" in \
    spike_body("inline void lifecycle(Exit cause) {", "// Plain placed references only.")
assert install_spike.index("installFade(base);") < install_spike.index("r.active.store(true,")
assert gate.count('"FixedCameraSpike","bHoldPlayerFade"') == 1 and "\nbHoldPlayerFade=1\n" in ini
for forbidden in ("write_call", "write_branch", "safe_write(", "VirtualProtect"):
    assert forbidden not in install_fade + settle_fade, forbidden
print("PASS player fade hold: one Setting found by ID, checked by address and name, held only while the rig holds, put back on the first "
      "frame it does not and on every main-thread lifecycle exit; no code written; FCS-15")

# Wave 2 shader, fifth pass (owner go 2026-11-11): Stage 2 of the seamless cutaway, the frame pass
# (bFramePass=1). Its runtime exists only with the key; it is installed by the cut's main-thread commit after
# every earlier pass; the only engine write is the displacement of the call at 656A78, after every check and
# every device object; the hook calls ID 36558 exactly once; the draw happens only on the main thread while the
# cut is active, and every piece of device state it changes is read before and set back after.
def frame_body(start, end):
    return frame[frame.index(start):frame.index(end, frame.index(start))]
assert "inline void installFrame(std::uintptr_t base);" in cut and cut.count("installFrame(base);") == 1 and \
    "inline void captureFrame(Vec eye);" in cut and cut.count("configureFrame();") == 2 and cut.count("frameStartFrame();") == 2 and \
    cut.count("traceFrame(now);") == 1 and "inline void traceFrame(double now);" in cut
assert commit.index('"FixedCameraSpike shader style installed style=%u state=%s gallery=%u shadows=%u"') < commit.index("if (r.framePass) {") < \
    commit.index("installFrame(base);") < commit.index('"FixedCameraSpike shader frame installed pass=%s band=%.1f headroom=%.1f radii=%.1f,%.1f vignette=%.3f "')
configure_frame = frame_body("inline void configureFrame() {", "inline void traceFrame(")
assert "if (r.config.frame.enabled) r.framePass = new FrameRuntime;" in configure_frame and frame.count("new FrameRuntime") == 1 and "runtime = " not in frame
# Captured during a pass of the published view, before the style branch, once per render, only when installed.
assert consider.index("if (!group) ++r.frameMatched;") < consider.index("if (!group) captureFrame(eye);") < \
    consider.index("if (!group && styled()) { styledPass(pass,shaderClass,eye); return; }")
capture = frame_body("inline void captureFrame(Vec eye) {", "// The engine's main colour target")
assert "if (!r.framePass || r.framePass->captured || r.framePass->state != Feature::installed) return;" in capture and \
    "c.viewFrame = r.view.frame;" in capture and "peek<Matrix>(r.state,frame_offset::viewProjection)" in capture
install_frame = frame_body("inline void installFrame(std::uintptr_t base) {", "inline Feature frameState()")
order = ["if (!r.framePass) return;", "frameSiteCheck(base,REL::ID(postWorldAddress.id).address(),imageChain,renderer,", "Refusal::frameSite",
         "method::createVertexShader", "frame_method::createPixelShader", "method::createBuffer", "frame_method::createBlendState",
         "frame_method::createRasterizerState", "Refusal::frameDevice", "relayPage(site)", "relay(reinterpret_cast<std::uintptr_t>(&hookImageSpace))",
         "VirtualProtect(page,relaySize,PAGE_EXECUTE_READ,&previous)", "Refusal::frameMemory", "FlushInstructionCache(GetCurrentProcess(),page,relaySize);",
         "f.imageChain = reinterpret_cast<ImageChainFunction>(imageChain);", "f.state = Feature::installed;", "REL::safe_write(site+1u,&value,sizeof(value));",
         "FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(site),5);"]
assert all(install_frame.index(a) < install_frame.index(b) for a, b in zip(order, order[1:]))
assert install_frame.count("return;") == 4 and "const std::uintptr_t site = base+frameCallRva;" in install_frame and frame.count("safe_write(") == 1
for forbidden in ("write_vfunc", "write_call", "write_branch", "AllocTrampoline", "GetTrampoline", "PAGE_EXECUTE_READWRITE", "GetSingleton", "AddTask(",
                  "BSFixedString", "SetAppCulled", "ClearState", "_ReturnAddress"):
    assert forbidden not in frame, forbidden
# The hook (fix pass after the review of 32605a5): inside ID 107148, the draw first and only for the normal-play destination
# 0 from target 1, then ID 105674 exactly once with every register argument passed on.
hook_chain = frame_body("inline std::uintptr_t hookImageSpace(", "// Main thread, from frameStart.")
assert frame.count("->imageChain(") == 1 and frame.count("drawFrame();") == 1 and \
    hook_chain.index("if (static_cast<std::uint32_t>(destination) == playDestination && static_cast<std::uint32_t>(source) == chainSource) drawFrame();") < \
    hook_chain.index("return runtime->framePass->imageChain(manager,source,destination,flags);")
assert "hookMainView" not in frame and "constexpr std::uintptr_t playDestination = 0, chainSource = 1;" in frame_policy
# The draw: thread, gate, targets, constants, map before anything is bound; the depth-stencil view is unbound before the depth
# is bound for reading; everything saved is set back and released; nothing is allocated.
draw = frame_body("inline void drawFrame() {", "// The relay's target")
order = ["if (!thin::onMain()) { f.offMain.fetch_add(1ull,std::memory_order_relaxed); return; }", "f.captured = false;", "frameGate(",
         "targetsReady(colourView,depthView)", "if (skip == FrameSkip::none && colourShared()) skip = FrameSkip::shared;", "frameConstants(r.config.frame,", "method::map,f.constants,", "refuseRun(text); return;",
         "std::memcpy(mapped.data,&*constants,sizeof(FrameConstants));", "method::unmap,f.constants,", "save(c,saved);",
         "frame_method::omSetRenderTargets,1u,static_cast<void* const*>(targets),static_cast<void*>(nullptr));",
         "frame_method::psSetShaderResources,0u,1u,static_cast<void* const*>(resources));", "frame_method::draw,3u,0u);", "restore(c,saved);",
         "restoredAsSaved(c,saved)", "releaseSaved(saved);", "++w.drawn;"]
assert all(draw.index(a) < draw.index(b) for a, b in zip(order, order[1:]))
for forbidden in ("new ", "std::vector", "malloc", "make_unique", "std::string"):
    assert forbidden not in draw and forbidden not in frame_body("inline void save(", "// A cheap read-back"), forbidden
assert "const bool kitCell = styled() && r.style->cellStyle == CellStyle::kit;" in draw and "r.active.load(std::memory_order_acquire)" in draw
restore_fn = frame_body("inline void restore(void* c,const SavedState& s) {", "inline void releaseSaved(")
assert restore_fn.index("frame_method::psSetShaderResources,0u,1u,static_cast<void* const*>(none));") < \
    restore_fn.index("frame_method::omSetRenderTargets,renderTargetSlots,") < restore_fn.index("frame_method::psSetShaderResources,0u,pixelSlots,")
save_fn = frame_body("inline void save(void* c,SavedState& s) {", "// The depth's view leaves")
for getter in ("omGetRenderTargets", "omGetBlendState", "rsGetState", "rsGetViewports", "iaGetInputLayout", "iaGetPrimitiveTopology", "vsGetShader",
               "psGetShader", "psGetConstantBuffers", "psGetShaderResources"):
    assert "frame_method::" + getter in save_fn, getter
for setter in ("omSetRenderTargets", "omSetBlendState", "rsSetState", "rsSetViewports", "iaSetInputLayout", "iaSetPrimitiveTopology",
               "psSetShader", "psSetConstantBuffers", "psSetShaderResources"):
    assert "frame_method::" + setter in restore_fn and "frame_method::" + setter in draw, setter
assert "method::vsSetShader,s.vertexShader," in restore_fn and "method::vsSetShader,f.vertexShader," in draw
release_fn = frame_body("inline void releaseSaved(SavedState& s) {", "// A cheap read-back")
for field in ("s.targets", "s.resources", "s.depthStencil", "s.blend", "s.rasterizer", "s.layout", "s.vertexShader", "s.pixelShader", "s.pixelConstants"):
    assert field in release_fn, field
assert frame_start.index("Refusal::voidThread") < frame_start.index("frameStartFrame();") < frame_start.index("Refusal::context")
assert "runtime->framePass->captured = false; // each frame's render captures its own main view" in frame_body("inline void frameStartFrame() {", "// Main thread, from commit")
shared_fn = frame_body("inline bool colourShared() {", "// Inside ID 107148")
for getter in ("csGetShaderResources", "csGetUnorderedAccessViews", "vsGetShaderResources"):
    assert "frame_method::" + getter in shared_fn, getter
holds = frame_body("inline bool viewsHold(", "// A view of the main colour texture")
assert holds.index("frame_method::viewResource,&resource") < holds.index("releaseObject(resource);") < holds.index("releaseObject(views[i]);")
assert trace_body.index("traceStyle(now,voids.window);") < trace_body.index("traceFrame(now);")
assert "LineBudget shaderFrameLines{shader::traceLines};" in spike
# The audited facts the pass stands on.
for constant in ("constexpr Address postWorldAddress{107148,0x153ac00};", "constexpr Address imageChainAddress{105674,0x14edbf0};",
                 "constexpr Address rendererAddress{411393,0x3331f40};",
                 "constexpr std::uintptr_t frameCallRva = 0x153ade7;", "constexpr std::size_t frameGuardedBefore = 0x2b;",
                 "constexpr std::size_t targets = 0xa58, targetStride = 0x30, targetView = 0x10;",
                 "constexpr std::size_t depths = 0x2018, depthStride = 0x98, depthResource = 0x88;",
                 "constexpr std::size_t mainTarget = 1, mainDepth = 0;", "constexpr std::size_t viewport = 0x70, viewProjection = 0x430, depthRange = 0x5c0;",
                 "constexpr FrameSkip frameGate(bool latched,bool active,bool freshView,bool captured) {"):
    assert constant in frame_policy, constant
assert '#include "fixed_spike_shader_frame_shaders.h"' in frame_policy and \
    '#include "fixed_spike_shader_frame_policy.h" // fifth pass: Stage 2, the frame pass (bFramePass=1)' in cut_policy
for key, default in (("bFramePass", "1"), ("fFrameBand", "96"), ("fFrameBandHeadroom", "136"), ("fFrameRadiusInner", "500"),
                     ("fFrameRadiusOuter", "900"), ("fFrameVignette", "0.25")):
    assert gate.count('"FixedCameraSpike","' + key + '"') == 1 and ("\n" + key + "=" + default + "\n") in ini, key
assert (Path(__file__).resolve().parents[1] / "shaders/fixed_spike_frame_vs.hlsl").exists() and \
    (Path(__file__).resolve().parents[1] / "shaders/fixed_spike_frame_ps.hlsl").exists()
print("PASS shader fifth-pass wiring: frame runtime only with bFramePass=1; installed after every earlier pass, every guard and device "
      "object before the one 4-byte call write; drawn inside ID 107148 before ID 105674, which is called once per hook; main thread, "
      "normal-play destination and active cut only; no view of the colour target on other stages; targets, constants and "
      "map before any bind; depth-stencil view unbound before the depth is read; all touched state saved, set back and released; "
      "no allocation per frame; one site per FCS-93..97")

# Stage 3.1 (owner go "Start solid caps"): the back-face fill (iCutFill=1). Its runtime exists only with the key;
# it is installed by the cut's commit after every earlier pass, writes no engine code and adds no hook; it draws
# only inside the Lighting RestoreGeometry hook, before the engine's RestoreGeometry, on the main thread, for a
# pass the cut opened with a plane in the main view; it changes four pieces of device state and sets each back.
def fill_body(start, end):
    return fill[fill.index(start):fill.index(end, fill.index(start))]
configure_fill = fill_body("inline void configureFill() {", "inline void traceFill(")
assert "if (r.config.fill) r.fill = new FillRuntime;" in configure_fill and fill.count("new FillRuntime") == 1 and "runtime = " not in fill
assert cut.count("configureFill();") == 2 and cut.count("installFill(base);") == 1 and cut.count("traceFill(now);") == 1
assert commit.index("installFrame(base);") < commit.index("if (r.fill) {") < commit.index("installFill(base);") < \
    commit.index('"FixedCameraSpike shader fill installed fill=%s colour=%.3f,%.3f,%.3f cap=%u"')
assert restore_hook.index("if (r.fill) cutFill(pass,Index);") < restore_hook.index("r.restore[Index](self,pass,flags,extra)")
assert cut.count("cutFill(") == 2  # declaration and the one call
# Review MINOR-1 of the fill: the fill draws only for the pass whose SetupGeometry opened the cut, and never after a latch.
assert setup_hook.index("consider(pass,Index);") < \
    setup_hook.index("if (r.opened.load(std::memory_order_relaxed) && thin::onMain()) r.openPass = pass;")
assert "r.openFill = false; r.openPass = nullptr;" in finish
assert "if (f.writeMask != allTargetsMask && !(f.mode == widenedFill && f.writeMask == colourRgbMask)) return FillSkip::mask;" in fill_policy and \
    "constexpr std::uint32_t allTargetsMask = 2;" in fill_policy and "constexpr std::uint32_t colourRgbMask = 1;" in fill_policy
assert "if (f.alphaTest != 0 && !(f.mode == widenedFill && f.alphaRef <= 0.0f)) return FillSkip::alpha;" in fill_policy
trace_fill = fill_body("inline void traceFill(double now) {", "} // namespace")
assert trace_fill.index("if (formatFill(line,sizeof(line),sample)) r.fillEmitter(line);") < trace_fill.index("if (formatFill2(line,sizeof(line),sample)) r.fillEmitter(line);")
assert "if (c.fill > 2u) c.fill = 0u;" in cut_policy
assert "    r.openFill = usable(wanted); // Stage 3.1: a plane cut, not the whole-draw constant" in apply and "r.openFill = false;" in finish
assert "r.openFill = mainView && buffer != runtime->style->removeBuffer;" in style
draw_fill = fill_body("inline void cutFill(void* pass,std::size_t shaderClass) {", "inline Feature fillState()")
order = ["if (!thin::onMain() || f.state != Feature::installed || f.broken || !r.openFill || !r.opened.load(std::memory_order_acquire) ||\n"
         "        r.openPass != pass || r.latched.load(std::memory_order_acquire)) return;",
         "++w.sliced;", "x.lighting = shaderClass == 0;", "x.flatCell = styled() && r.style->cellStyle == CellStyle::flat;",
         "x.writeMask = peek<std::uint32_t>(r.state,fill_offset::writeMask);", "x.mode = r.config.fill;",
         "if (x.lighting && x.flatCell) w.diagnostics.note(", "const FillSkip skip = fillGate(x);", "fillFrameConstants()",
         "if (!f.guard.on) {", "++w.skips[static_cast<std::size_t>(FillSkip::guard)];", "frame_method::rsGetState,&savedRaster",
         "fill_method::omGetDepthStencilState,&savedDepth,&savedRef", "frame_method::omGetBlendState,&savedBlend,",
         "frame_method::psGetShader,&savedPixel,", "frame_method::psGetConstantBuffers,fillBuffer,1u,",
         "frame_method::rsSetState,raster", "fill_method::omSetDepthStencilState,depth,", "frame_method::omSetBlendState,f.blend,",
         "frame_method::psSetShader,f.cap,", "frame_method::psSetConstantBuffers,fillBuffer,1u,static_cast<void* const*>(constants)",
         "fill_method::drawIndexed,3u*static_cast<std::uint32_t>(triangles),0u,0", "static_cast<void* const*>(savedConstants)",
         "frame_method::psSetShader,savedPixel,", "frame_method::omSetBlendState,savedBlend,", "fill_method::omSetDepthStencilState,savedDepth,savedRef",
         "frame_method::rsSetState,savedRaster", "if (checkRaster != savedRaster || checkPixel != savedPixel) ++f.mismatch;",
         "releaseObject(object);", "++w.filled; ++w.restores;"]
assert all(draw_fill.index(a) < draw_fill.index(b) for a, b in zip(order, order[1:]))
assert "for (void* object : {checkRaster,checkPixel,savedRaster,savedDepth,savedBlend,savedPixel,savedConstants[0]}) releaseObject(object);" in draw_fill
for forbidden in ("new ", "std::vector", "malloc", "make_unique", "std::string"):
    assert forbidden not in draw_fill, forbidden
for forbidden in ("safe_write", "VirtualAlloc", "VirtualProtect", "write_vfunc", "write_call", "write_branch", "AllocTrampoline", "GetSingleton",
                  "AddTask(", "BSFixedString", "ClearState", "OMSetRenderTargets", "omSetRenderTargets", "iaSet"):
    assert forbidden not in fill, forbidden
install_fill = fill_body("inline void installFill(std::uintptr_t base) {", "// configure, after validate")
order = ["if (!r.fill) return;", "fillSiteCheck(base,depthTable,rasterizerTable,image)", "Refusal::fillSite", "frame_method::createPixelShader",
         "frame_method::createBlendState", "method::createBuffer", "Refusal::fillDevice", "f.depthTable = ", "f.state = Feature::installed;"]
assert all(install_fill.index(a) < install_fill.index(b) for a, b in zip(order, order[1:]))
map_fill = fill_body("inline bool fillFrameConstants() {", "// RestoreGeometry hook")
assert map_fill.index("method::map,f.constants,") < map_fill.index("Refusal::fillMap") < map_fill.index("std::memcpy(mapped.data,&k,sizeof(FillConstants));")
for constant in ("constexpr Address depthStencilTableAddress{411362,0x3330230};", "constexpr Address rasterizerTableAddress{411363,0x33309b0};",
                 "constexpr std::uint32_t cullBack = 1, cullFront = 2, depthTestWrite = 3, depthEqual = 4;",
                 "constexpr std::size_t geometryType = 0x150, geometryTriangles = 0x158, geometryData = 0x138;", "constexpr unsigned fillBuffer = 13;"):
    assert constant in fill_policy, constant
assert "LineBudget shaderFillLines{2*shader::traceLines};" in spike and gate.count('"FixedCameraSpike","iCutFill"') == 1 and \
    gate.count('"FixedCameraSpike","sCutFillColor"') == 1 and "\niCutFill=0\n" in ini and "\nsCutFillColor=0,0,0\n" in ini
assert "unsigned fill{0};" in cut_policy and "std::uint64_t refusalBit(Refusal code)" in cut_policy and "std::atomic<std::uint64_t> refusals{};" in cut
assert (Path(__file__).resolve().parents[1] / "shaders/fixed_spike_cut_fill_ps.hlsl").exists()
print("PASS shader Stage 3.1 fill wiring: fill runtime only with iCutFill=1; installed after every earlier pass with its guards and objects "
      "first; no code write, no new hook; drawn in the Lighting RestoreGeometry hook before the engine's, main thread, plane-cut passes "
      "only; rasteriser, depth-stencil, blend, pixel shader and pixel slot 13 saved, set back in reverse and released; no allocation "
      "per frame; one site per FCS-98..101")

# Stage 3.2 (iCutTilt): the tilted cut. Its runtime exists only with iCutTilt=1 (validate keeps it only with
# iCutStyle=1); it adds no hook, no code write and no engine-state write; a pass reaches it only where the flat
# cut or Stage 1 would bind the plane buffer, and every refusal leaves that flat bind. The flat patch's bytes
# do not depend on it (the archive digest in fixed_spike_shader_policy_test.cpp).
def tilt_body(start, end):
    return tilt[tilt.index(start):tilt.index(end, tilt.index(start))]
assert "if (c.tilt > 1u || c.style != stageOneStyle) c.tilt = 0u;" in cut_policy and "unsigned tilt{0};" in cut_policy
assert "inline void configureTilt() {\n    auto& r = *runtime;\n    if (r.config.tilt == tiltFlatCells) r.tilt = new TiltRuntime;\n}" in tilt
assert cut.count("    configureTilt();") == 1 and cut.count("        installTilt();") == 1 and cut.count("    tiltFrame();") == 1 and cut.count("    traceTilt(now);") == 1
commit = cut_body("inline void commit() {", "// ---- called by the spike's frame bracket")
assert commit.index("installFill(base);") < commit.index("if (r.tilt) {") < commit.index("installTilt();")
publish = cut_body("inline void publish(", "inline void trace(double now) {")
assert publish.index("styleFrame();") < publish.index("tiltFrame();") < publish.index("r.active.store(true,")
assert "inline bool tiltOn() { return runtime && runtime->tilt && runtime->tilt->state == Feature::installed && styled(); }" in tilt
assert "inline bool tiltCell() { return tiltOn() && runtime->style->cellStyle == CellStyle::flat && runtime->tilt->planes.has_value(); }" in tilt
# Stage 1: only the plane buffer's bind is replaced; the storey line and removals are untouched.
assert "        if (!tiltPass(*entry,eye,shaderClass)) apply(*entry,plane,shaderClass);\n        return;\n    }" in style and style.count("tiltPass(") == 1
assert cut.count("tiltPass(") == 2  # the declaration and the Effect path
tilt_pass = tilt_body("inline bool tiltPass(", "// publish, after styleFrame")
order = ["if (!r.tilt || !styled()) return false;",
         "if (t.state != Feature::installed) { if (t.state == Feature::refused && r.style->cellStyle == CellStyle::flat) ++tw.refusedFlat; return false; }",
         "if (r.style->cellStyle != CellStyle::flat)", "if (!t.planes)", "tiltEntryFor(entry,group)",
         "if (!slot) { ++tw.fullFlat; return false; }", "if (!slot->shader || t.state != Feature::installed) { ++tw.refusedFlat; return false; }",
         "if (!uploadTilt(tiltConstant(*t.planes,eye)))", "method::vsSetConstantBuffers,cutBuffer,1u,", "method::vsSetShader,slot->shader,",
         "r.openObject = entry.object;", "r.openFill = false;", "r.opened.store(true,", "++w.cut;"]
assert all(tilt_pass.index(a) < tilt_pass.index(b) for a, b in zip(order, order[1:]))
upload = tilt_body("inline bool uploadTilt(", "// A pass the flat cut would draw")
assert upload.index("method::map,t.buffer,") < upload.index("Refusal::tiltMap") < upload.index("std::memcpy(mapped.data,&wanted,sizeof(wanted));") < \
    upload.index("method::unmap,t.buffer,")
assert "latch()" not in tilt  # a tilt refusal never ends the flat cut
entry_for = tilt_body("inline TiltEntry* tiltEntryFor(", "// The rows Mapped only")
assert entry_for.index("patchVertexShader(code,size,Cut::tilted)") < entry_for.index("Refusal::tiltPatch") < \
    entry_for.index("method::createVertexShader") < entry_for.index("Refusal::tiltCreate") < entry_for.index("slot.shader = created;")
# A one-sided refusal never leaves one class tilted and its twin flat: FCS-102 and 103 end the tilt for the session.
assert entry_for.count("++t.refused; t.state = Feature::refused;") == 2 and \
    entry_for.index("++t.refused; t.state = Feature::refused;") < entry_for.index("Refusal::tiltPatch")
assert tilt_body("inline bool uploadTilt(", "// A pass the flat cut").count("t.state = Feature::refused;") == 1 and \
    tilt_body("inline void installTilt(", "inline void traceTilt(").count("t.state = Feature::refused;") == 1
# Cache identity: an entry is reused only for the same object with the same bytecode (size and container checksum);
# otherwise its copy is released and the entry rebuilt in place.
assert "    return slot.size == size && std::memcmp(slot.sum.data(),code+checksumAt,slot.sum.size()) == 0;" in tilt
assert entry_for.index("if (slot.object == entry.object && sameCode(slot,size,code)) return &slot;") < \
    entry_for.index("if (slot.object == entry.object) { releaseObject(slot.shader); ++t.window.rebuilt; }") < \
    entry_for.index("else if (slot.object) continue;") < entry_for.index("slot = {};") < \
    entry_for.index("std::memcpy(slot.sum.data(),code+checksumAt,slot.sum.size());") < entry_for.index("patchVertexShader(code,size,Cut::tilted)")
assert entry_for.count("++t.window.created;") == 1
for forbidden in ("safe_write", "VirtualAlloc", "VirtualProtect", "write_vfunc", "write_call", "write_branch", "AllocTrampoline", "std::vector"):
    assert forbidden not in tilt, forbidden
# The frame pass's band is off in a tilted cell; its shader bytes are unchanged.
assert "const bool tilted = tiltCell();" in frame and "kitCell,band,bandFloor,tilted);" in frame
assert "    if (kitCell) return BandState::kit;\n    if (tiltCell) return BandState::tilt;" in frame_policy
assert "LineBudget shaderTiltLines{shader::traceLines};" in spike and gate.count('"FixedCameraSpike","iCutTilt"') == 1 and \
    gate.count('"FixedCameraSpike","fCutTiltSlope"') == 1 and gate.count('"FixedCameraSpike","fCutTiltMax"') == 1 and \
    "\niCutTilt=1\n" in ini and "\nfCutTiltSlope=0.5\n" in ini and "\nfCutTiltMax=400\n" in ini
assert cut_policy.endswith('#include "fixed_spike_shader_tilt_policy.h"  // Stage 3.2: the tilted cut (iCutTilt=1)\n') or \
    '#include "fixed_spike_shader_tilt_policy.h"' in cut_policy
print("PASS shader Stage 3.2 tilt wiring: tilt runtime only with iCutTilt=1 and iCutStyle=1; installed after every earlier pass; no hook, "
      "code or engine-state write; only the plane buffer's bind replaced, in flat cells, buffer then shader as apply; every refusal "
      "ends the tilt for the session (flat cut, no latch); cache entries tied to their bytecode; frame band off in tilted cells; one site per FCS-102..105")

# The door void (iDoorVoid=1, include/fixed_spike_shader_door_policy.h): rides on the frame pass and Stage 1's
# census. Off (the default) reads nothing new and leaves the frame pass's constants and shader term unread; on, the
# census notes table-model Door references (ExtraTeleport read only for those), the draw picks the nearest doors the
# player is in front of in flat cells only, writes them into the same constant buffer before the one Map, and the
# trace writes FixedDoorVoid rows through the frame row's budget, on change only. No hook, code or engine-state write.
door_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_door_policy.h").read_text()
door_table = (Path(__file__).resolve().parents[1] / "include/fixed_spike_shader_door_table.h").read_text()
frame_ps = (Path(__file__).resolve().parents[1] / "shaders/fixed_spike_frame_ps.hlsl").read_text()
assert "unsigned doorVoid{0};" in cut_policy and \
    "if (c.frame.enabled && c.frame.doorVoid && c.style != stageOneStyle) { c.frame.doorVoid = 0u; c.frame.doorLights = 0u; frameChanged = true; }" in cut_policy
assert "    if (f.doorVoid > 1u) f.doorVoid = 0u;\n    if (f.doorVoid) {" in cut_policy
for key, default in (("iDoorVoid", "1"), ("fDoorVoidDepth", "900"), ("fDoorVoidEdge", "48"), ("fDoorVoidRadius", "1500"), ("fDoorVoidSide", "320")):
    assert gate.count('"FixedCameraSpike","' + key + '"') == 1 and ("\n" + key + "=" + default + "\n") in ini, key
assert '#include "fixed_spike_shader_door_policy.h"' in frame_policy and '#include "fixed_spike_shader_door_table.h"' in door_policy
assert "GENERATED by modules/camera/tools/load_doors.py" in door_table and "static_assert(doorTableSorted());" in door_policy
assert "static_assert(sizeof(FrameConstants) == 304 && sizeof(FrameConstants)%16 == 0);" in frame_policy and \
    "static_assert(offsetof(FrameConstants,doorShape) == 160 && offsetof(FrameConstants,door) == 176);" in frame_policy
# The shader and its C++ copy read the door rows only when a door is drawn; the term is a max (never brighter).
assert "    float4 doorShape;" in frame_ps and "    float4 door[8];" in frame_ps and \
    "        if (doorShape.x > 0.5)\n            amount = max(amount, doorFade(world));" in frame_ps
assert "        if (k.doorShape[0] > .5f) amount = (std::max)(amount,doorFade(k,out.world));" in frame_policy
# Census: one branch in Stage 1's walk, before the statics filter, only with the key; the extra list is read only
# for a table model that is not disabled or deleted; the cell's doors are replaced as a whole and reset on a new cell.
note_cell = style_body("inline void noteCell(", "// configure, after validate")
assert note_cell.index("const bool doorVoid = runtime->config.frame.enabled && runtime->config.frame.doorVoid != 0u;") < \
    note_cell.index("if (type == RE::FormType::Door && doorVoid) { noteDoorRef(doors,ref,base); return RE::BSContainer::ForEachResult::kContinue; }") < \
    note_cell.index("if (type != RE::FormType::Static") < note_cell.index("s.doors = doors;") < note_cell.index("formatDoorCensus(line,sizeof(line),id,")
assert "        s.doors = {};\n" in note_cell and style.count("noteDoorRef(") == 2 and "HasType" not in style and style.count("teleportBit(") == 2
note_door = style_body("inline void noteDoorRef(", "// Stage 3.0, after the census")
assert "const Teleport teleport = model && !off ? teleportBit(ref) : Teleport::absent;" in note_door and \
    note_door.index("doorModel(modelPath(base))") < note_door.index("ref->IsDisabled() || ref->IsDeleted()") < note_door.index("teleportBit(ref)")
# ExtraTeleport's bit is read as callbackMount reads ExtraInteraction: a CAS on the list's read count (at most 16 tries), never a wait.
bit = style_body("inline Teleport teleportBit(", "// The door void, census")
assert bit.index("static_assert(sizeof(RE::ExtraDataList) == 0x20);") < bit.index("static_assert(offsetof(RE::TESObjectREFR,extraList) == 0x70);") < \
    bit.index("static_assert(teleport == 0x2b);") < bit.index("for (int attempt = 0;attempt < 16;++attempt) {") < \
    bit.index("const PrecisionMountReadLock lock(*reinterpret_cast<std::uint32_t*>(extra+0x1c));") < bit.index("if (!lock) continue;") < \
    bit.index("std::memcpy(&presence,extra+0x10,sizeof(presence));") < bit.index("return Teleport::busy;")
assert "const PrecisionMountReadLock mountLock(*reinterpret_cast<std::uint32_t*>(extra+0x1c));" in source and "LockForRead" not in style
# Draw: after the constants and every skip, before the Map; only in a flat cell of the census; no allocation.
draw = frame_body("inline void drawFrame() {", "// The relay's target")
assert draw.index("frameConstants(r.config.frame,") < draw.index("if (r.config.frame.doorVoid) {") < \
    draw.index("const bool flatCell = styled() && r.style->cellStyle == CellStyle::flat;") < \
    draw.index("selectDoors(r.style->doors,r.view.feet,r.config.frame.doorRadius)") < \
    draw.index("applyDoors(*constants,f.doors,f.capture.posAdjust,r.config.frame.doorEdge,r.config.frame.doorDepth);") < draw.index("method::map,f.constants,")
assert frame.count("applyDoors(") == 1 and frame.count("selectDoors(") == 1 and "new " not in draw
# Player side: a door the feet are not strictly in front of is skipped; the void starts at least doorBodyClear behind the feet.
assert "if (!door.plane.valid || !(side > 0.0f)) { ++out.behind; continue; }" in door_policy and \
    "(std::max)(door.plane.start,doorBodyClear-side)" in door_policy and "constexpr float doorBodyClear = 48.0f" in door_policy
assert "else if (wrongFacing(id)) why = DoorNote::wrongFacing;" in door_policy and "else if (teleport != Teleport::present) why = DoorNote::noTeleport;" in door_policy
# Trace: after the frame row, on change only, through the frame row's emitter and budget (no new line kind).
trace_frame = frame_body("inline void traceFrame(double now) {", "} // namespace")
assert trace_frame.index("if (formatFrame(line,sizeof(line),sample)) r.frameEmitter(line);") < \
    trace_frame.index("if (!r.config.frame.doorVoid || sameDoors(f.doors,f.doorsTraced)) return;") < trace_frame.index("formatDoorVoid(line,sizeof(line),")
assert '"FixedDoorVoid t=%.3f frame=%llu cell=%08X active=%zu slot=%zu door=%s model=%s plane=%.1f,%.1f,%.1f normal=%.3f,%.3f half=%.1f "' in door_policy
assert commit.index('"FixedCameraSpike shader frame installed pass=%s') < commit.index("if (r.config.frame.doorVoid) {") < \
    commit.index('"FixedCameraSpike shader door void installed pass=%s') < commit.index("if (r.fill) {")
for forbidden in ("safe_write", "VirtualAlloc", "VirtualProtect", "write_vfunc", "write_call", "write_branch", "AllocTrampoline", "std::vector", "GetSingleton"):
    assert forbidden not in door_policy and forbidden not in note_door, forbidden
print("PASS shader door void wiring: only with iDoorVoid=1 (INI default 1, struct default 0), bFramePass=1 and iCutStyle=1; one census branch (ExtraTeleport's bit read only for "
      "table models, one CAS, never a wait); doors chosen in flat cells only, the player never behind a chosen plane; rows written before the one Map; shader "
      "term read only with a door drawn; FixedDoorVoid on change through the frame budget; no hook, code or engine-state write")

# The door void's lights (iDoorVoidLights=1, include/fixed_spike_shader_door_light_policy.h, src/fixed_spike_shader_door_light.h):
# off by default and only with the door void. One more census branch (LIGH references, position and form flags only);
# served from publish on eligible frames with the frame pass's own door choice; restored on every other path; the
# only engine writes are bit 0 of a listed NiLight's flags and lightListChanged on its geometries' light data, both
# made under the scene node's light lock with no engine call; no hook and no code write.
def door_light_body(start, end):
    return door_light[door_light.index(start):door_light.index(end, door_light.index(start))]


assert "unsigned doorLights{0};" in cut_policy and "    if (f.doorLights > 1u || !f.doorVoid) f.doorLights = 0u;" in cut_policy
assert gate.count('"FixedCameraSpike","iDoorVoidLights"') == 1 and "\niDoorVoidLights=0\n" in ini
assert '#include "fixed_spike_shader_door_light_policy.h"' in frame_policy and "lastRefusal = 110" in cut_policy
configure_lights = door_light_body("inline void configureDoorLights() {", "// Main thread, publish")
assert "if (r.config.frame.enabled && r.config.frame.doorVoid && r.config.frame.doorLights) r.doorLights = new DoorLightRuntime;" in configure_lights
assert door_light.count("= new ") == 1 and cut.count("\n    configureDoorLights();\n") == 1
# Install: inside the frame pass's block of commit, after the door void, before the fill; every check before the state.
assert commit.index('"FixedCameraSpike shader door void installed pass=%s') < commit.index("installDoorLights(base);") < \
    commit.index('"FixedCameraSpike shader door lights installed lights=%s') < commit.index("if (r.fill) {")
install_lights = door_light_body("inline void installDoorLights(std::uintptr_t base) {", "inline Feature doorLightState()")
assert install_lights.index("l.state = Feature::refused;") < install_lights.index("frameState() == Feature::installed") < \
    install_lights.index("REL::ID(ids[i].id).address() != base+ids[i].rva") < install_lights.index("std::memcmp(reinterpret_cast<const void*>(base+lightCode[i].rva)") < \
    install_lights.index("Refusal::doorLightSite") < install_lights.index("l.state = Feature::installed;")
for constant in ("constexpr Address lightStateAddress{390951,0x20d68b0};", "constexpr Address sceneVtableAddress{254646,0x1b3dba0};",
                 "constexpr Address lightVtableAddress{255404,0x1b4ca30};", "constexpr Address lightUpdateAddress{106335,0x150d410};",
                 "constexpr Address lightListAddress{107784,0x1569550};", "constexpr Address lightCullAddress{108278,0x1575d20};",
                 "constexpr std::size_t lightGeometry = 0x18, lightObject = 0x48, lightDynamic = 0x46;", "constexpr std::size_t itemNext = 0x0, itemElement = 0x10;",
                 "constexpr std::size_t objectFlags = 0xf4, worldTranslate = 0xa0, lightRadius = 0x128;", "constexpr std::uint32_t appCulled = 1u;",
                 "constexpr std::size_t geometryShader = 0x128, shaderLightData = 0x70, lightListChanged = 0x20;",
                 "{0x150da8e,updateBytes.data(),updateBytes.size()},{0x150d439,lockBytes.data(),lockBytes.size()}",
                 "{0x1569713,listBytes.data(),listBytes.size()},{0x1575e8a,markBytes.data(),markBytes.size()},{0x151a528,rebuildBytes.data(),rebuildBytes.size()}"):
    assert constant in door_light, constant
# Census: one branch after the door's, before the statics filter, only with the key; reset with the cell; logged once per cell.
assert note_cell.index("const bool doorLights = doorVoid && runtime->config.frame.doorLights != 0u;") < \
    note_cell.index("if (type == RE::FormType::Door && doorVoid)") < \
    note_cell.index("if (type == RE::FormType::Light && doorLights) { noteLightRef(lights,ref); return RE::BSContainer::ForEachResult::kContinue; }") < \
    note_cell.index("if (type != RE::FormType::Static") < note_cell.index("s.lights = lights;") < note_cell.index("formatDoorLightCensus(line,sizeof(line),id,")
assert "        s.lights = {};\n" in note_cell and style.count("noteLightRef(") == 2
note_light = style_body("inline void noteLightRef(", "// Stage 3.0, after the census")
assert "ref->IsDisabled() || ref->IsDeleted()" in note_light and "ref->GetPosition()" in note_light and "Get3D" not in note_light
# Serve: the arrays are read and every write is made under the light lock, with no engine call inside it. A reference
# is held on every NiLight in the book (taken under the lock, an atomic increment); every held NiLight is read and
# restored through it whether or not an active entry lists it; dropped references are released after the lock.
serve = door_light_body("inline void serveDoorLights(", "// Main thread: an off pass for a lifecycle request")
locked = serve[serve.index("if (data) guard.emplace(data->lightQueueLock);"):serve.index("} // the light lock ends here")]
assert serve.index("if (!want && !l.book.count)") < serve.index("lightScene();") < serve.index("f.want = want && node;") < \
    serve.index("std::array<RE::NiPointer<RE::NiLight>,maxDoorLights> released{};") < serve.index("if (data) guard.emplace(data->lightQueueLock);") < \
    serve.index("if (data) for (const auto& entry : data->activeLights) {") < \
    serve.index("seen[n++] = readLight(nullptr,static_cast<const std::uint8_t*>(l.book.held[i].light),true,l.lightVtable);") < \
    serve.index("planLights(l.book,seen.data(),n,f)") < serve.index("std::memcpy(light+light_offset::objectFlags,&flags,sizeof(flags));") < \
    serve.index("if (s.entry) markLinked(s.entry);") < serve.index("next[i] = RE::NiPointer<RE::NiLight>(") < \
    serve.index("for (auto& ref : l.refs) if (ref) released[dropped++] = std::move(ref);") < serve.index("l.book = plan.book;") < \
    serve.index("} // the light lock ends here")
assert "const bool held = l.book.holds(light);" in serve and "if (!held && n >= maxSeenLights-maxDoorLights)" in serve
assert "std::array<RE::NiPointer<RE::NiLight>,maxDoorLights> refs{};" in door_light
read_light = door_light_body("inline LeightSeen".replace("Leight","Light") + " readLight(", "// ID 108278's marking walk")
assert "peek<std::uintptr_t>(entry,0) == lightVtable" in read_light and "s.orphan = peek<std::int32_t>(light,light_offset::refCount) <= (held ? 2 : 1);" in read_light and \
    "if (!entry) { s.orphan = true; return s; }" in read_light and "constexpr std::size_t refCount = 0x8;" in door_light
locked_code = "\n".join(line.split("//")[0] for line in locked.splitlines())
for call in ("GetSingleton", "invoke<", "RemoveLight", "AddLight", "SetAppCulled", "->Update", "REL::", "logger", "refuse("):
    assert call not in locked_code, call
assert serve.index("} // the light lock ends here") < serve.index("Refusal::doorLightGone") < serve.index("Refusal::doorLightConflict") < \
    serve.index("Refusal::doorLightUnlinked")
mark = door_light_body("inline void markLinked(", "// Main thread. want:")
assert "data[light_offset::lightListChanged] = 1u;" in mark and "hops < maxLinkedGeometry" in mark
# The policy only asks for writes on entries it read this frame, and only puts out lights that are wanted, plain and lit.
assert "return slot >= 0 && placed && l.plain && !l.orphan;" in door_light_policy and "if (l.culled) { ++p.counts.foreign; continue; }" in door_light_policy and \
    "if (!f.want) return p;" in door_light_policy and "const LightSeen* l = findLight(seen,n,h.light,at);" in door_light_policy and \
    "if (!l->entry || l->orphan) {" in door_light_policy and "if (!l->dynamic && !l->linked) ++p.counts.unlinked;" in door_light_policy
# Hysteresis: only for a held light, only while wanted, 0.5 s and the radius plus 128.
assert "constexpr double lightKeepSeconds = .5;" in door_light_policy and "constexpr float lightKeepMargin = 128.0f;" in door_light_policy and \
    "const bool grace = f.want && l->plain && std::isfinite(f.now) && std::isfinite(h.chosenAt) && f.now >= h.chosenAt &&" in door_light_policy
for forbidden in ("RE::", "REL::", "GetSingleton", "#include <windows", "std::vector"):
    assert forbidden not in door_light_policy, forbidden
# Paths: served by publish (eligible, after the sky), off when publish has no view, a lifecycle load/cell, the Loading Menu,
# a latch or an off-thread request at frame start; an ineligible frame holds or restores by the fill light's rule.
assert publish.index("settleSky(true);") < publish.index("doorLightsFrame(true);") and \
    "settleSky(false); doorLightsFrame(false); return; }" in publish
assert frame_start.index("frameStartFrame();") < frame_start.index("doorLightsStart();")
assert trace_body.index("traceTilt(now);") < trace_body.index("traceDoorLights(now);")
lifecycle_spike = spike_body("inline void lifecycle(Exit cause) {", "// Plain placed references only.")
assert "shader::doorLightsExit(cause == Exit::load ? shader::lightAskLoad : cause == Exit::cell ? shader::lightAskCell : 0u);" in lifecycle_spike
assert "inline void loadingMenu() {\n    if (runtime) shader::doorLightsExit(shader::lightAskLoading);\n}" in lifecycle_spike
assert end_frame.index("const bool exiting = restoreNow(cause,r.silentFrames);") < end_frame.index("if (exiting) restoreAll(cause);") < \
    end_frame.index("shader::doorLightsIdle(exiting,cause,cellId,r.now);") < end_frame.index("traceRig(false);")
assert source.count("fixed::loadingMenu()") == 1 and \
    'if (event && event->opening && event->menuName == "Loading Menu") fixed::loadingMenu();' in source
assert "constexpr LightDuty lightDuty(bool eligible,bool exiting,Exit cause,bool silenceHeld,bool loadingMenu) {" in door_light_policy and \
    "return d == LightDuty::hold && (cellChanged || loaded) ? LightDuty::off : d;" in door_light_policy
idle = door_light_body("inline void doorLightsIdle(", "// Any thread: a lifecycle exit.")
assert idle.index("takeDoorLightRequests();") < idle.index("if (!l.book.count)") < idle.index("l.silence.step(paused,tween,now)") < \
    idle.index("settleLightDuty(lightDuty(false,exiting,cause,held,false),cellId != l.cell,l.loadPending)") < idle.index("serveDoorLights(false,{},\"off\",")
start_lights = door_light_body("inline void doorLightsStart() {", "// FixedDoorLight, through")
assert "takeDoorLightRequests();" in start_lights and 'if (r.latched.load(std::memory_order_acquire) && r.doorLights->book.count) serveDoorLights(false,{},"off","latched");' in start_lights
frame_lights = door_light_body("inline void doorLightsFrame(bool active) {", "// Main thread, the camera frame service's ineligible path")
assert frame_lights.index("takeDoorLightRequests();") < frame_lights.index("if (cell != l.cell || l.loadPending) {") < \
    frame_lights.index("selectDoors(r.style->doors,r.view.feet,r.config.frame.doorRadius)")
assert "r.style->cellStyle == CellStyle::flat" in frame_lights and "!r.framePass->broken" in frame_lights
trace_lights = door_light_body("inline void traceDoorLights(double now) {", "} // namespace")
assert trace_lights.index("if (sameHeld(l.book,l.traced) && !l.window.any()) return;") < trace_lights.index("r.frameEmitter(line);")
door_light_code = "\n".join(line.split("//")[0] for line in door_light.splitlines())  # code only, comments name what is rejected
for forbidden in ("safe_write", "VirtualAlloc", "VirtualProtect", "write_vfunc", "write_call", "write_branch", "AllocTrampoline", "std::vector",
                  "RemoveLight", "AddLight", "SetAppCulled"):
    assert forbidden not in door_light_code, forbidden
print("PASS shader door void lights wiring: only with iDoorVoidLights=1 (INI 0) and the door void; one census branch (LIGH position and form flags); "
      "IDs and five engine instruction runs checked before use; arrays read and the app-culled bit and lightListChanged written only under the "
      "light lock with no engine call; served from publish with the frame pass's door choice; restored on an inactive publish, lifecycle load/cell, "
      "the Loading Menu, a latch and ineligible frames by the fill light's menu rule (at once; 0.5 s hysteresis only for an unchosen door); a reference held on "
      "every NiLight put out, the bit cleared through it even after its BSLight left the list, released after the lock; FixedDoorLight on change through "
      "the frame budget; one site per FCS-106..110")
