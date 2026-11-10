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
               "inline void deactivate() {\n    if (runtime) ",
               "inline void frameStart() {\n    if (!runtime) return;",
               "inline void publish(Vec eye,Vec forward,Vec feet,bool interiorCell,unsigned long long frame,double now) {\n    if (!runtime) return;",
               "inline void trace(double now) {\n    if (!runtime) return;",
               "inline bool tracing() { return runtime && ",
               "inline const char* status() { return !runtime ? "):
    assert helper in cut, helper
for key in ("fShaderCutHeight", "fShaderCutRadius", "iShaderCutMaxPerFrame", "iShaderCutShaders", "iShaderCutTrace"):
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
table_guard = cut_body("inline unsigned tableGuard(", "template<std::size_t Index>\nvoid hookClass() {")
assert "slots[setupSlot] != base+c.setupRva" in table_guard and "slots[restoreSlot] != base+c.restoreRva" in table_guard
assert "c.setupBytes.size()) != 0" in table_guard and "c.restoreBytes.size()) != 0" in table_guard
assert "REL::ID(data[i].id).address() != base+data[i].rva" in cut and "REL::ID(shaderClasses[i].table.id).address() != base+shaderClasses[i].table.rva" in cut
# Each hook calls the engine function exactly once; a pass is considered only after its own SetupGeometry.
setup_hook = cut_body("std::uintptr_t hookSetup(", "template<std::size_t Index>\nstd::uintptr_t hookRestore(")
assert setup_hook.count("r.setup[Index](self,pass,flags,extra)") == 1 and cut.count("r.setup[Index](") == 1
assert setup_hook.index("fault(Fault::setup)") < setup_hook.index("r.setup[Index](self,pass,flags,extra)") < \
    setup_hook.index("if (pass && r.active.load(std::memory_order_acquire)) {") < setup_hook.index("consider(pass,Index);")
restore_hook = cut_body("std::uintptr_t hookRestore(", "// ---- install")
assert restore_hook.count("r.restore[Index](self,pass,flags,extra)") == 1 and cut.count("r.restore[Index](") == 1
assert restore_hook.index("r.restore[Index](self,pass,flags,extra)") < restore_hook.index("r.opened.load(") < \
    restore_hook.index("if (!thin::onMain()) r.offMain.fetch_add(") < restore_hook.index("else if (!finish()) fault(Fault::shaderChanged);")
# Selection order, cheapest first; nothing is read from engine objects off the main thread or outside the published view.
consider = cut_body("inline void consider(", "// ---- hooks")
assert consider.index("r.latched.load(") < consider.index("if (!thin::onMain()) { r.offMain.fetch_add(1ull,std::memory_order_relaxed); return; }") < \
    consider.index("++r.window.passes;") < consider.index("r.opened.load(") < consider.index("!fresh(r.view,r.clock)") < \
    consider.index("peek<Vec>(r.state,offset::posAdjust)") < consider.index("if (!sameView(r.view,eye,direction)) {") < \
    consider.index("peek<const std::uint8_t*>(pass,offset::passGeometry)") < consider.index("skip = reach(r.view,bound,") < \
    consider.index("skip = ownerSkip(geometry);") < consider.index("shaderFor()") < consider.index("!r.budget.take()") < \
    consider.index("apply(*entry,wanted,shaderClass);")
assert "geometry && usable(wanted)" in consider and "offset::geometrySkin) != nullptr" in consider and cut.count("apply(*entry,") == 1
off_thread = consider[consider.index("if (!thin::onMain())"):consider.index("++r.window.passes;")]
assert off_thread.count(";") == 2  # one counter, then return: nothing else is touched off the main thread
# The owner is found as the engine finds it: a user only on a fade node, past carried ammunition; actors and excluded kinds stay.
owner = cut_body("inline RE::TESObjectREFR* ownerOf(", "// The pass is drawn with the patched shader")
assert owner.index("offset::asFadeNode") < owner.index("peek<RE::TESObjectREFR*>(fade,offset::objectUser)") < owner.index("!= ammoForm) break;") < \
    owner.index("offset::objectParent")
assert "hop < maxOwnerHops" in owner and "RE::FormType::ActorCharacter" in owner and "kindOf(ref,ref->GetBaseObject())" in owner
# Only audited bytecode is patched, and a shader is used only after the device accepted it.
prepare = cut_body("inline void prepare(", "// The vertex shader the engine bound")
assert prepare.index("size < containerHeader || size > largestBlob") < prepare.index("codeHash(code,size)") < prepare.index("if (!audited(hash)) {") < \
    prepare.index("patchVertexShader(code,size)") < prepare.index("if (made.result != Patch::ok) {") < prepare.index("method::createVertexShader") < \
    prepare.index("if (answer < 0 || !created) {") < prepare.index("entry.patched = created;")
assert prepare.count("return;") == 4 and cut.count("entry.patched = ") == 1 and cut.count("method::createVertexShader") == 1
# A cut pass: the two calls that can fail come first; buffer, then shader; closed by the engine's own shader and an empty slot.
apply = cut_body("inline void apply(", "// A pass of a hooked shader class")
assert apply.index("method::map") < apply.index("if (answer < 0 || !mapped.data) {") < apply.index("latch();") < apply.index("Refusal::map") < \
    apply.index("std::memcpy(mapped.data,&wanted,sizeof(wanted));") < apply.index("method::unmap") < \
    apply.index("method::vsSetConstantBuffers,cutBuffer,1u,") < apply.index("method::vsSetShader,entry.patched,") < \
    apply.index("r.opened.store(true,") < apply.index("++r.window.cut;")
finish = cut_body("inline bool finish() {", "inline void fault(")
assert finish.index("peek<const std::uint8_t*>(r.state,offset::currentVertexShader) == r.openObject") < \
    finish.index("if (same) invoke<void>(r.context,method::vsSetShader,peek<void*>(r.openObject,offset::shaderObject),") < \
    finish.index("void* none[1]{};") < finish.index("method::vsSetConstantBuffers,cutBuffer,1u,static_cast<void* const*>(none)") < \
    finish.index("r.opened.store(false,")
assert cut.count("method::vsSetShader") == 2 and cut.count("method::vsSetConstantBuffers") == 2 and cut.count("std::memcpy(") == 2
fault = cut_body("inline void fault(", "// The patched device shader")
assert fault.index("finish()") < fault.index("latch();") < fault.index("Refusal::bracket")
publish = cut_body("inline void publish(", "inline void trace(double now) {")
assert publish.index("if (r.published+1u != frame) r.plane.reset();") < publish.index("interiorCell && std::isfinite(feet.z)") < \
    publish.index("makeView(r.config,eye,forward,feet,r.plane.update(feet.z,now),frame,now)") < \
    publish.index("if (!next) { r.active.store(false,std::memory_order_release); return; }") < publish.index("r.active.store(true,")
assert "!r.latched.load(" in publish and "r.installed &&" in publish
cut_codes = dict(re.findall(r"(\w+) = (\d+)", cut_policy[cut_policy.index("enum class Refusal : unsigned {"):cut_policy.index("constexpr unsigned firstRefusal")]))
assert len(cut_codes) == 14 and sorted(int(v) for v in cut_codes.values()) == list(range(66, 80))
for name in cut_codes:
    assert cut.count("Refusal::" + name) == 1, name  # one site per code
assert "std::uint32_t{1}" not in cut and "refusalBit(code)" in cut_body("inline void refuse(", "inline void latch()")  # own once-mask: codes above 63
for constant in ("constexpr Address stateAddress{388819,0x20cfc00};", "constexpr Address deviceAddress{411348,0x3330190};",
                 "constexpr Address contextAddress{411391,0x3331f30};", '{"lighting",{255053,0x1b47420},0x1549550,0x154a900,',
                 '{"utility",{255302,0x1b4ab58},0x15673f0,0x15689c0,', "constexpr std::size_t setupSlot = 6, restoreSlot = 7;",
                 "currentVertexShader = 0x348, posAdjust = 0x35c, viewDirection = 0x3a0;", "objectParent = 0x30, objectBound = 0xe4, objectUser = 0xf8;",
                 "asFadeNode = 0x28;", "geometrySkin = 0x130;", "shaderObject = 0x08, shaderCodeSize = 0x10, shaderCode = 0x68;",
                 "createBuffer = 0x18, createVertexShader = 0x60;", "vsSetConstantBuffers = 0x38, vsSetShader = 0x58, map = 0x70, unmap = 0x78;",
                 "constexpr std::uint32_t cutBuffer = 13, cutRegister = 16;", "constexpr std::uint32_t mapWriteDiscard = 4;",
                 "constexpr std::uint32_t ammoForm = 0x2a;"):
    assert constant in cut_policy, constant
print("PASS shader cut wiring: no runtime outside mode 3; main-thread install after every guard; no code or engine-state write; "
      "engine function called once per hook; audited bytecode only; fail-before-bind; own shader and empty slot restored; one site per FCS-66..79")
