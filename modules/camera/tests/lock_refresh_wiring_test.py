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
light = (Path(__file__).resolve().parents[1] / "src/fixed_spike_light.h").read_text()


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
             "fixed::configure(", "fixed::runtime", "fixed::light::Config l;")
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
assert hook_room.index("if (caller != RoomCaller::camera) {") < hook_room.index("r.counters.viewerCalls") < hook_room.index("seedClaim(caller,*view,asked,")
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
assert len(codes) == 26 and len(set(codes.values())) == len(codes)
for name in codes:
    assert (spike + light + source).count("Refusal::" + name) == 1, name  # one site per code
print("PASS spike on wiring: frame-scoped suppression; shared caster detour; sealed gateway; caller-gated room hook; hook-thread isolation; "
      "interior-only radius-bounded nearest-first hide; one site per refusal code")

# Wave 2 package "light" (DYL-217). Off unless a light key is on; no hook at
# all; engine light arrays read only under their lock; the fill light is a
# plain AddLight/RemoveLight client, hidden at once on every ineligible frame.
def light_body(start, end):
    return light[light.index(start):light.index(end, light.index(start))]


assert spike.count('#include "fixed_spike_light.h"') == 1
assert light.count("lights = new LightRuntime;") == 1
configure_light = light_body("inline void configureLight(", "inline void installLight(")
assert configure_light.index("if (!configured(requested)) return;") < configure_light.index("lights = new LightRuntime;")
configure = spike_body("inline void configure(", "} // namespace")
assert configure.index("runtime = new Runtime;") < configure.index("light::configureLight(lightRequested);") < configure.index("r.writer.start(")
assert "fixed::configure(f,l,settings," in loader
for entry in ("inline void installLight(std::uintptr_t base) {\n    if (!lights) return;",
              "inline void frameLight(bool eligible,bool hold,RE::TESObjectCELL* parent,Vec feet,Vec at,Vec camera,const char* cause) {\n    if (!lights || hold) return;",
              "inline RE::ShadowSceneNode* scene() {\n    if (!lights || !lights->ready) return nullptr;"):
    assert entry in light, entry
assert spike.count("light::installLight(base);") == 1 and spike.count("light::frameLight(") == 2
assert end_frame.index("light::frameLight(false,!exiting,parent,{},{},{},") < end_frame.index("traceRig(false);") < \
    end_frame.index("light::frameLight(true,false,parent,")
assert end_frame.index("if (!thin::onMain())") < end_frame.index("light::frameLight(")  # main thread only
for forbidden in ("write_vfunc", "safe_write", "write_call", "write_branch", "VirtualProtect", "_ReturnAddress", "BSFixedString",
                  "GetMenu(", "IsMenuOpen(", "AddTask(", "RequestCameraControl"):
    assert forbidden not in light, forbidden
install_light = light_body("inline void installLight(", "// endFrame(), main thread")
assert install_light.index("REL::ID(390951).address() != base+stateRva") < install_light.index("l.ready = true;") < \
    install_light.index("REL::ID(106326).address() != base+addLightRva") < install_light.index("l.fillReady = true;")
for check in ("REL::ID(70967).address() != base+pointCtorRva", "REL::ID(237259).address() != base+pointVtableRva",
              "REL::ID(106331).address() != base+removeLightRva", "!sameBytes(base+pointCtorRva,pointCtorPrologue)",
              "!sameBytes(base+addLightRva,addLightPrologue)", "!sameBytes(base+removeLightRva,removeLightPrologue)"):
    assert install_light.index(check) < install_light.index("l.fillReady = true;"), check
scene = light_body("inline RE::ShadowSceneNode* scene() {", "inline LightView view(")
assert "base()+sceneVtableRva" in scene and "Refusal::sceneReplaced" in scene
# Engine light arrays: read only under the light lock, with no engine call while held.
for name, end in (("inline Presence presenceNow(", "inline void census("), ("inline void census(", "// ---- fill light")):
    reader = light_body(name, end)
    assert "RE::BSSpinLockGuard guard(data.lightQueueLock);" in reader
    assert reader.index("RE::BSSpinLockGuard guard(data.lightQueueLock);") < reader.index("data.active")
    assert not any(call in reader for call in ("AddLight(", "RemoveLight(", "SetAppCulled(", "emit", "refuse("))
assert light.count("RE::BSSpinLockGuard") == 2
assert light.count("->AddLight(") == 1 and "->AddLight(" in light_body("inline void addFill(", "// Main thread, every frame")
assert light.count("->RemoveLight(") == 1 and "->RemoveLight(" in light_body("inline void serviceFill(", "// ---- entry points")
add_fill = light_body("inline void addFill(", "// Main thread, every frame")
for param in ("params.dynamic = true;", "params.shadowLight = false;", "params.portalStrict = false;", "params.neverFades = false;",
              "params.restrictedNode = nullptr;"):
    assert param in add_fill, param
assert light.count("SetAppCulled(") == 2  # created hidden; show() is the only other writer
service = light_body("inline void serviceFill(", "// ---- entry points")
assert service.index("if (!l.fillReady) { show(false,\"refused\"); return; }") < service.index("createFill()") < service.index("show(false,cause);") < \
    service.index("presenceNow(node)") < service.index("fillStep(wanted,l.presence,l.track,runtime->now)")
frame_light = light_body("inline void frameLight(", "} // namespace")
assert frame_light.index("serviceFill(node,wanted,feet,cause);") < frame_light.index("if (!l.config.trace || !node || !eligible")
assert "fillWanted(l.config.fillMode,eligible && finite(feet),interior,l.fillReady && node != nullptr)" in frame_light
# Hook-silent frames inside wave 1's grace hold the fill light as it is.
assert end_frame.index("const bool exiting = restoreNow(cause,r.silentFrames);") < end_frame.index("if (exiting) restoreAll(cause);") < \
    end_frame.index("light::frameLight(false,!exiting,")
# The census include follows the vanilla room test it uses; player rooms are
# found before the light lock, and the census itself never runs a room query.
assert spike.index("inline bool within(") < spike.index('#include "fixed_spike_light.h"') < spike.index("inline void endFrame() {")
census_body = light_body("inline void census(", "// ---- fill light")
assert "within(" not in census_body and "playerRooms(" not in census_body
assert frame_light.index("const PlayerRooms player = playerRooms(parent,at);") < frame_light.index("census(node,player,at,s,shadows);")
assert "fillStep(wanted,l.presence,l.track,runtime->now)" in service
print("PASS light wiring: off unless configured; no hook; audited IDs and prologues before use; locked read-only census; "
      "one AddLight/RemoveLight site; hidden on every ineligible frame; one site per refusal code")
