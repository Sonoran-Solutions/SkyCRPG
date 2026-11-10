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
             "fixed::configure(", "fixed::runtime", "fixed::clip::Config k;")
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

# Wave 2 clip (DYL-217): iSeeThroughMode=2. Off, or in any other mode, there is
# no clip runtime and every entry point is a no-op; on, the plane is published
# by the main thread, a pass is changed only between its own SetupGeometry and
# RestoreGeometry, and every engine write is undone by the code that made it.
clip = (Path(__file__).resolve().parents[1] / "src/fixed_spike_clip.h").read_text()
clip_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_clip_policy.h").read_text()
ini = (Path(__file__).resolve().parents[1] / "config/SkyCRPG_Camera.ini").read_text()


def clip_body(start, end):
    return clip[clip.index(start):clip.index(end, clip.index(start))]


assert clip.count("runtime = new Runtime;") == 1 and "runtime = new Runtime;" in clip_body("inline void configure(", "} // namespace")
assert spike.count("clip::configure(") == 1 and "clip::configure(" not in source and source.count("fixed::clip::") == 1
configure_spike = spike_body("inline void configure(", "} // namespace")
assert configure_spike.index("r.config = validate(requested,base,cursorKey,&adjusted);") < \
    configure_spike.index("if (clip::clipMode(r.config.seeThroughMode))\n        clip::configure(clipRequested,logger,") < \
    configure_spike.index("r.traceReady = r.writer.start(tracePath,rows);")
assert "constexpr bool clipMode(unsigned seeThroughMode) { return seeThroughMode == 2; }" in clip_policy
assert "if (c.seeThroughMode > 2) c.seeThroughMode = 0;" in (Path(__file__).resolve().parents[1] / "include/fixed_spike_policy.h").read_text()
for helper in ("inline void install(std::uintptr_t base) {\n    if (!runtime) return;",
               "inline void deactivate() {\n    if (runtime) ",
               "inline void frameStart() {\n    if (!runtime) return;",
               "inline void publish(Vec eye,Vec forward,Vec feet,bool interiorCell,unsigned long long frame,double now) {\n    if (!runtime) return;",
               "inline void trace(double now) {\n    if (!runtime) return;",
               "inline bool tracing() { return runtime && ",
               "inline const char* status() { return !runtime ? "):
    assert helper in clip, helper
for key in ("fClipHeight", "fClipMargin", "fClipCorridor", "fClipFloorBand", "iClipMaxPerFrame", "iClipShaders", "iClipExterior", "iClipTrace"):
    assert gate.count('"FixedCameraSpike","' + key + '"') == 1 and ("\n" + key + "=") in ini, key
assert "\niSeeThroughMode=0\n" in ini and "\niClipExterior=0\n" in ini and "\niClipTrace=0\n" in ini
# The spike's bracket drives it: any thread may only switch it off.
assert "clip::deactivate(); // wave 2 clip" in spike_body("inline void lifecycle(Exit cause) {", "// Plain placed references only.")
begin_frame = spike_body("inline void beginFrame() {", "inline void clearFrame() {")
assert begin_frame.index("!thin::onMain()) return;") < begin_frame.index("clip::frameStart();")
assert end_frame.index("clip::deactivate(); clip::trace(r.now);") < end_frame.index("r.previousEligible = false;") < \
    end_frame.index("publishView(viewer,eye);") < end_frame.index("clip::publish(viewer,") < end_frame.index("clip::trace(r.now);\n}")
assert spike.count("clip::publish(") == 1 and spike.count("clip::frameStart(") == 1 and spike.count("clip::install(") == 1
install_spike = spike_body("inline void install(bool prerequisites,bool service) {", "// loadSettings, only when")
assert install_spike.index("Refusal::service") < install_spike.index("clip::install(base);") < install_spike.index("r.active.store(true,")
# kDataLoaded only asks; the guards and every write run in the main-thread frame service.
assert "commit" not in clip_body("inline void install(std::uintptr_t base) {", "// Any thread.")
assert clip.count("commit();") == 1 and "if (r.pending.exchange(false,std::memory_order_acq_rel)) commit();" in \
    clip_body("inline void frameStart() {", "// Main thread, after the camera update")
commit = clip_body("inline void commit() {", "// ---- called by the spike's frame bracket")
assert commit.index("foreignRenderer()") < commit.index("addressGuard(base)") < commit.index("tableGuard(base,r.config.shaders)") < \
    commit.index("buildGateway(target,base,gateway)") < commit.index("r.upload = gateway;") < commit.index("REL::safe_write(target,") < \
    commit.index("hookClass<0>(); hookClass<1>(); hookClass<2>();") < commit.index("r.installed = true;")
assert commit.count("return;") == 4 and clip.count("REL::safe_write(") == 1 and clip.count("write_vfunc(") == 2
gateway = clip_body("inline unsigned buildGateway(", "template<std::size_t Index>\nvoid hookClass()")
assert gateway.index("uploadPrologue.size()) != 0) return 1;") < gateway.index("RtlLookupFunctionEntry(target,&image,nullptr)") < \
    gateway.index("PAGE_READWRITE)") < gateway.index("std::memcpy(gateway,uploadPrologue.data(),uploadCopied);") < \
    gateway.index("VirtualProtect(gateway,64,PAGE_EXECUTE_READ,&previous)") < gateway.index("FlushInstructionCache(GetCurrentProcess(),gateway,") < \
    gateway.index("out = reinterpret_cast<Upload>(gateway);")
assert "PAGE_EXECUTE_READWRITE" not in clip and "AllocTrampoline" not in clip
hook_class = clip_body("template<std::size_t Index>\nvoid hookClass()", "// Main thread, inside the camera frame service")
assert hook_class.index("r.setup[Index] = ") < hook_class.index("r.restore[Index] = ") < hook_class.index("write_vfunc(restoreSlot,") < \
    hook_class.index("write_vfunc(setupSlot,")
guards = clip_body("inline unsigned addressGuard(", "inline unsigned buildGateway(")
assert "REL::ID(data[i].id).address() != base+data[i].rva" in guards and "REL::ID(shaderClasses[i].table.id).address() != base+shaderClasses[i].table.rva" in guards
assert "slots[setupSlot] != base+c.setupRva" in guards and "slots[restoreSlot] != base+c.restoreRva" in guards and guards.count("std::memcmp(") == 2
# The audited numbers the guards compare against (binary audit in the evidence file).
for fact in ("stateAddress{388819,0x20cfc00}", "uploadAddress{77258,0x100ae80}", "rendererAddress{411393,0x3331f40}",
             "gateAddress{411490,0x3336400}", "biasAddress{411359,0x3330200}", "uploadExtent{0x100ae80,0x100b03c}",
             '{"lighting",{255053,0x1b47420},0x1549550,0x154a900,', '{"utility",{255302,0x1b4ab58},0x15673f0,0x15689c0,',
             '{"effect",{255194,0x1b48ab0},0x1557090,0x1557f50,', "setupSlot = 6, restoreSlot = 7", "dirtyViewport = 0x2",
             "stateFlags = 0x0, viewportMinDepth = 0x80, viewportMaxDepth = 0x84, depthBiasMode = 0xa0, posAdjust = 0x35c, camera = 0x380",
             "dirtyDepthBias = 0x40",
             "passGeometry = 0x10", "objectParent = 0x30, objectBound = 0xe4, objectUser = 0xf8", "geometrySkin = 0x130",
             "depthTarget = 0x38, currentVertexShader = 0x348", "shaderCodeSize = 0x10, shaderCode = 0x68",
             "previousPosAdjust = 0x368", "uploadedFirst = 0x30, uploadedSecond = 0x130, uploadedSize = 0xc0"):
    assert fact in clip_policy, fact
# Hooks: the engine function always runs exactly once; a pass is considered only
# after its own SetupGeometry and only while a snapshot is active.
setup_hook = clip_body("std::uintptr_t hookSetup(", "template<std::size_t Index>\nstd::uintptr_t hookRestore(")
assert setup_hook.count("r.setup[Index](shader,pass,flags,extra)") == 1 and setup_hook.index("fault(Fault::setup)") < \
    setup_hook.index("r.setup[Index](shader,pass,flags,extra)") < setup_hook.index("r.active.load(std::memory_order_acquire)) consider(pass,Index);")
restore_hook = clip_body("std::uintptr_t hookRestore(", "// Every engine upload of the per-frame buffer")
assert restore_hook.count("r.restore[Index](shader,pass,flags,extra)") == 1 and \
    restore_hook.index("r.restore[Index](shader,pass,flags,extra)") < restore_hook.index("if (thin::onMain()) finish();") < \
    restore_hook.index("else offMainCall();")
upload_hook = clip_body("inline std::uintptr_t hookUpload(", "// ---- install")
assert upload_hook.count("r.upload(renderer,flags,third,fourth)") == 1 and upload_hook.index("fault(Fault::upload)") < \
    upload_hook.index("r.upload(renderer,flags,third,fourth)") < upload_hook.index("if (r.active.load(std::memory_order_acquire)) offMainCall();") < \
    upload_hook.index("else r.epochStale.store(true,std::memory_order_release);") < upload_hook.index("r.epochStale.store(false,") < \
    upload_hook.index("e.known = *r.gate == 1;") < upload_hook.index("std::memcpy(e.camera.data(),")
off_branch = upload_hook[upload_hook.index("if (!main) {"):upload_hook.index("r.epochStale.store(false,")]
assert "return result;" in off_branch and not any(touch in off_branch for touch in ("r.epoch.", "auto& e", "peek", "poke", "r.window", "r.state", "logger"))
# Off the main thread nothing but atomics is touched, and nothing is logged.
off_main = clip_body("inline void offMainCall() {", "// A pass in a view the plan cannot be used for.")
assert off_main.count(";") == 2 and "runtime->offMain.fetch_add(1ull,std::memory_order_relaxed);" in off_main and \
    "runtime->epochStale.store(true,std::memory_order_release);" in off_main
for engine in ("logger", "refuse(", "window", "latch", "state", "peek", "poke", "auto& r"):
    assert engine not in off_main, engine
consider = clip_body("inline void consider(void* pass,std::size_t shaderClass) {", "// ---- hooks")
assert consider.index("r.latched.load(") < consider.index("if (!thin::onMain()) { offMainCall(); return; }") < consider.index("++r.window.passes;") < \
    consider.index("if (r.opened.load(std::memory_order_relaxed)) return;") < consider.index("if (r.epochStale.exchange(false,std::memory_order_acq_rel)) e.known = false;") < consider.index("if (!e.known)") < consider.index("evaluate(cameraOf(e),*e.view,thin::seconds())") < \
    consider.index("if (e.plan.reject != Reject::none)") < consider.index("peek<const std::uint8_t*>(pass,offset::passGeometry)") < \
    consider.index("reach(e.plan,*e.view,bound,") < consider.index("ownerSkip(geometry,*e.view)") < consider.index("if (skip == Skip::none && !shaderReads()) skip = Skip::shader;") < \
    consider.index("apply(e.plan,shaderClass);")
owner = clip_body("inline Skip ownerSkip(", "// Whether the vertex shader bound for this pass")
assert "hop < maxOwnerHops" in owner and "RE::FormType::ActorCharacter" in owner and "kindOf(ref,form)" in owner and "extentOf(" in owner
# One clipped pass: every refusal is decided before the first engine write; the
# state matrices are put back as soon as the upload returns; the depth range is
# the only thing left changed while the pass draws.
shader_reads = clip_body("inline bool shaderReads() {", "// A pass of a hooked shader class")
assert "peek<const std::uint8_t*>(r.state,offset::currentVertexShader)" in shader_reads and \
    "readsViewProjection(shader+offset::shaderCode,peek<std::uint32_t>(shader,offset::shaderCodeSize))" in shader_reads
apply_clip = clip_body("inline void apply(const Plan& plan,std::size_t shaderClass) {", "// The reference that owns a geometry")
first_write = apply_clip.index("poke(camera,zColumn(i,row),plan.columns[i][row]);")
assert apply_clip.index("biasOf(peek<std::uint32_t>(r.state,offset::depthBiasMode),r.bias)") < \
    apply_clip.index("if (*r.gate != 1) why = Reject::gate;") < apply_clip.index("else if (drifted()) why = Reject::drift;") < \
    apply_clip.index("else if (!bias) why = Reject::bias;") < apply_clip.index("flushedRange(biasDirty,cameraMin,") < \
    apply_clip.index("if (why != Reject::none) { rejectPass(why); return; }") < apply_clip.index("if (!r.budget.take())") < first_write < \
    apply_clip.index("r.upload(r.renderer,r.epoch.flags,0,0);") < \
    apply_clip.index("poke(camera,zColumn(i,row),peek<float>(r.epoch.camera.data(),zColumn(i,row)));") < \
    apply_clip.index("r.applied = {cameraMin,biasDirty ? cameraMin : viewportMin,*minDepth};") < apply_clip.index("poke(camera,offset::depthRange,*minDepth);") < \
    apply_clip.index("r.opened.store(true,")
assert apply_clip.count("return;") == 2 and "poke(" not in apply_clip[:first_write]
assert clip.count("poke(camera,zColumn(") == 2 and clip.count("r.upload(") == 3 and clip.count("r.opened.store(true,") == 1
finish = clip_body("inline void finish() {", "inline void fault(Fault where) {")
assert finish.index("r.upload(r.renderer,r.epoch.flags,0,0);") < finish.index("restoreDepth();") < finish.index("r.opened.store(false,")
restore_depth = clip_body("inline void restoreDepth() {", "// RestoreGeometry of the clipped pass has run")
assert restore_depth.count("ownWrite(") == 2 and "r.applied.depthMin" in restore_depth and "r.applied.viewportMin" in restore_depth and \
    "|dirtyViewport" in restore_depth
fault = clip_body("inline void fault(Fault where) {", "// The engine's last upload must still describe the state")
assert fault.index("finish();") < fault.index("latch();") < fault.index("refuse(Refusal::bracket,text);")
assert clip.count("fault(Fault::") == 3 and "fault(Fault::frame)" in clip_body("inline void frameStart() {", "// Main thread, after the camera update")
publish = clip_body("inline void publish(", "inline void trace(double now) {")
assert publish.index("r.installed && !r.latched.load(") < publish.index("cellAllowed(interiorCell,r.config.exterior)") < \
    publish.index("makeView(r.config,eye,forward,feet,frame,now)") < publish.index("r.view.store(") < publish.index("r.budget.frame();") < \
    publish.index("r.active.store(true,")
assert clip.count("r.active.store(true,") == 1
for forbidden in ("BSFixedString", "GetMenu(", "IsMenuOpen(", "RequestCameraControl", "AddTask(", "SetAppCulled", "PlayerCharacter", "PlayerCamera"):
    assert forbidden not in clip, forbidden
clip_codes = dict(re.findall(r"(\w+) = (\d+)", clip_policy[clip_policy.index("enum class Refusal : unsigned {"):clip_policy.index("};", clip_policy.index("enum class Refusal : unsigned {"))]))
assert len(clip_codes) == 8 and sorted(int(value) for value in clip_codes.values()) == list(range(42, 50))
assert not set(clip_codes.values()) & set(codes.values())  # FCS-40 and FCS-41 stay with the wave 1 snap keys
for name in clip_codes:
    assert clip.count("Refusal::" + name) == 1, name  # one site per code
print("PASS clip wiring: no runtime outside mode 2; install deferred to the main-thread frame; guarded gateway and vtable slots; "
      "pass-scoped clip with all refusals before the first write; own-write restore; off-thread calls touch only atomics; one site per FCS-42..49")
