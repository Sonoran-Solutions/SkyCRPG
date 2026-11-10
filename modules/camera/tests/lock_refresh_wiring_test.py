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
             "fixed::configure(", "fixed::runtime", "fixed::ExtConfig e;", "fixed::configureExterior(")
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

# Wave 2 package "ext" (exterior see-through). With iExteriorSeeThrough 0 the
# exterior branch is never taken and the wave 1 FCS-31 branch runs unchanged;
# with it on, candidates come only from ray hits (and, mode 2, fan-crossed
# trees), hides go through the shared table, caps and restore causes.
ext = (Path(__file__).resolve().parents[1] / "src/fixed_spike_ext.h").read_text()
ext_policy = (Path(__file__).resolve().parents[1] / "include/fixed_spike_ext_policy.h").read_text()


def ext_body(start, end):
    return ext[ext.index(start):ext.index(end, ext.index(start))]


assert source.count("fixed::configureExterior(e);") == 1 and source.count("fixed::ExtConfig e;") == 1
assert gate.index("fixed::configure(") < gate.index("fixed::ExtConfig e;") < gate.index("fixed::configureExterior(e);")
for key in ("iExteriorSeeThrough", "fExteriorMaxSpan", "iExteriorMaxHidden", "fExteriorCoverFootprint", "fExteriorDownStart"):
    assert gate.count('"' + key + '"') == 1, key
assert "inline void configureExterior(const ExtConfig& requested) {\n    if (!runtime) return;" in ext
assert spike.count('#include "fixed_spike_ext_policy.h"') == 1 and spike.rstrip().endswith('#include "fixed_spike_ext.h" // wave 2 package "ext": exterior see-through, iExteriorSeeThrough')
assert "    ExtState ext;" in spike_body("struct Runtime {", "// Process lifetime, like the thin runtime")
# Dispatch: interior hide first, the exterior scan only when configured, the wave 1 notice otherwise.
dispatch_hide = "if (hideScanned(r.config.seeThroughMode,parent->IsInteriorCell())) { scanHide(player,parent); traceSummary(); }"
dispatch_ext = "else if (exteriorScanned(r.ext.config.mode,parent->IsInteriorCell())) { scanExterior(player,parent); traceExteriorSummary(cellId); }"
dispatch_old = "else if (r.config.seeThroughMode == 1) { restoreAll(Exit::cell); noteExterior(cellId); }"
assert end_frame.index(dispatch_hide) < end_frame.index(dispatch_ext) < end_frame.index(dispatch_old)
# Leaving a cell restores first, except exterior to exterior with the exterior rule on (review m4).
cell_change = end_frame[end_frame.index("if (cellId != r.cell) {"):end_frame.index(dispatch_hide)]
assert "if (!keepAcrossCells(r.ext.config.mode,r.cell != 0 && !r.cellInterior,parent && !interiorNow)) restoreAll(Exit::cell);" in cell_change
assert cell_change.index("restoreAll(Exit::cell);") < cell_change.index("r.cell = cellId; r.cellInterior = interiorNow;")
assert "constexpr bool keepAcrossCells(unsigned mode,bool wasExterior,bool isExterior) { return mode != 0 && wasExterior && isExterior; }" in ext_policy
assert "constexpr bool exteriorScanned(unsigned mode,bool interiorCell) { return mode != 0 && !interiorCell; }" in ext_policy
assert ext.count("scanExterior(player,parent)") == 0 and spike.count("scanExterior(player,parent)") == 1
# Hides only through the shared table and budget; restores only through the shared release().
scan_ext = ext_body("inline void scanExterior(", "inline void traceExteriorSummary(")
assert ext.count("SetAppCulled(true)") == 1 and "SetAppCulled(false)" not in ext and "SetAppCulled(true)" in scan_ext
assert scan_ext.index("release(slot,Exit::unload)") < scan_ext.index("release(slot,Exit::underfoot)") < \
    scan_ext.index("nearestFirst(due,dueCount);") < scan_ext.index("if (!r.budget.take()) break;") < scan_ext.index("SetAppCulled(true)")
assert scan_ext.index("if (slot.root->GetAppCulled()) { ++r.totals.foreign;") < scan_ext.index("SetAppCulled(true)")
# A scan that could not read the grid, the world or the floor claims and hides nothing (review m2).
assert scan_ext.index("} else if (hits.floorFailed) {") < scan_ext.index("ForEachReference(") < \
    scan_ext.index("if (!claims) dueCount = 0;") < scan_ext.index("nearestFirst(due,dueCount);")
assert scan_ext.count("claims = false") == 3
# Cover is a collision fact; ground-standing pieces never cover (review M1); the summary has its own budget (m5).
assert "candidate.overhead = listed(hits.over,hits.overCount,ref);" in scan_ext
assert "const bool cover = x.overhead && covers(x.box,feet,topZ,hiddenNow);" in ext_policy
# Both hide branches are guarded by the footprint and the kit rule (re-review M1), before either can return a hide.
guard_fp = "if ((cover || overhang) && x.kind == ExtKind::structure && groundStanding(e,feet.z,c.coverFootprint))"
guard_kit = "if ((cover || overhang) && x.kind == ExtKind::structure && x.kit && e.minZ <= feet.z+floorReach)"
assert guard_fp in ext_policy and guard_kit in ext_policy
assert ext_policy.index("if (largeStructure(") < ext_policy.index(guard_fp) < ext_policy.index(guard_kit) < \
    ext_policy.index("if (cover) return {Reason::extCover") < ext_policy.index("if (overhang) return {Reason::extOverhang")
assert "candidate.kit = kind == ExtKind::structure && kitModel(modelOf(base));" in scan_ext
# The down pick start is the INI key (n1); floor contact band asserted with the pick order below (n2).
assert "downProbe(feet,hits.top[i],x.config.downStart)" in ext
assert '"fExteriorDownStart"' in gate
summary_ext = ext_body("inline void traceExteriorSummary(", "} // namespace")
assert summary_ext.count("emit(r.ext.summaryLines,line);") == 2 and "hideLines" not in summary_ext
assert "(r.config.hideTrace ? hideTraceLines+extSummaryLines : 0u)" in spike
claim_ext = ext_body("inline Hidden* claimExterior(", "inline unsigned exteriorCells(")
assert claim_ext.index("if (r.tracked >= r.ext.config.maxHidden) {") < claim_ext.index("release(r.hidden[static_cast<std::size_t>(farthest)],Exit::evicted);") < \
    claim_ext.index("return claimSlot(ref,root,base,reach);")
# Candidates: a structure must be a ray hit; only trees are taken from the walk, and only near the fan.
assert scan_ext.index("const bool onRay = at >= 0;") < scan_ext.index("else if (x.config.mode < 2) return Each::kContinue;") < \
    scan_ext.index("if (!onRay && kind != ExtKind::tree) return Each::kContinue;") < scan_ext.index("nearFan(centre,bound.radius,feet,eye)")
assert "if (x.kind == ExtKind::structure && !x.rayHit) return {};" in ext_policy
assert scan_ext.count("ForEachReference(") == 1 and ext.count("ForEachReference(") == 1
assert "if (++visited > maxVisited) { ++c.visitCap; full = true; return Each::kStop; }" in scan_ext
assert scan_ext.index("x.gridMisses = cellCount ? 0u : x.gridMisses+1u;") < scan_ext.index("if (x.gridMisses >= gridRefuseScans)") < \
    scan_ext.index("ExtRefusal::grid")  # a one-scan border mismatch is counted, not refused
# Cells: the loaded grid, trusted only when it holds the player's own cell at its computed slot.
cells_ext = ext_body("inline unsigned exteriorCells(", "// What one scan's picks found.")
assert cells_ext.index("tes->interiorCell") < cells_ext.index("!= parent) return 0;") < cells_ext.index("cells[count++] = parent;")
assert "cell->IsAttached()" in cells_ext and "gridSlot(" in cells_ext and "GetCell(" in cells_ext
assert "(TES::GetCell, ID 13322" in ext_policy and "ID 13319 (1A3A00)" in ext_policy
# Rays: the audited pick slot, the read lock, pointers only compared until the walk.
rays_ext = ext_body("inline bool castExteriorRays(", "// Every 0.1 s in eligible frames in an exterior cell")
# One audit per bhkWorld vtable pointer, shared with the interior castRays, each refusing with its own code (m3).
audit = spike_body("inline bool pickAudited(const RE::bhkWorld* world) {", "// Sightline rays (camera to player")
assert "if (slots == r.pickTable) return true;" in audit and "slots[0x33] != REL::Module::get().base()+0x104be80" in audit
assert audit.index("return false;") < audit.index("r.pickTable = slots;")
assert spike.count("pickAudited(world)") == 1 and ext.count("pickAudited(world)") == 1 and "pickRefused" not in spike + ext
interior_rays = spike_body("inline void castRays(", "inline void traceSummary()")
assert interior_rays.index("if (!pickAudited(world)) {") < interior_rays.index("refuse(Refusal::pickSlot,") < interior_rays.index("RE::BSReadLockGuard")
assert rays_ext.index("if (!pickAudited(world)) {") < rays_ext.index("ExtRefusal::pickSlot") < rays_ext.index("RE::BSReadLockGuard lock(world->worldLock);") < \
    rays_ext.index("world->PickObject(ray);")
assert rays_ext.index("ExtRefusal::world") < rays_ext.index("if (!pickAudited(world))")
# Order of picks: floor first (a failure stops before anything else), upward, sightline, then probes.
assert rays_ext.index("const auto floorStarts = floorProbes(feet);") < rays_ext.index("if (!done) { hits.floorFailed = true; return true; }") < \
    rays_ext.index("{feet.x,feet.y,feet.z+headClear},{feet.x,feet.y,eye.z},extOverheadHits") < rays_ext.index("for (const auto target : targets) {") < \
    rays_ext.index("const auto probes = openProbes(feet,hits.top[i]);") < rays_ext.index("const auto down = downProbe(feet,hits.top[i],x.config.downStart);")
assert "if (!done) downSame = true;" in rays_ext and "floorClaimHit(k,walkableFace(normal))" in rays_ext and "floorNearHit(k,point.z,feet.z)" in rays_ext
assert "candidate.floorContact = listed(hits.floor,hits.floorCount,ref);" in scan_ext  # claims: under the feet, or a walkable face beside them
# The hit normal is read from the pick output and handed to every march callback (third re-review).
assert "std::memcpy(normal.data(),&ray.rayOutput.normal,sizeof(float)*4);" in rays_ext and "if (!seen(ref,mix(from,to,fraction),surface)) return true;" in rays_ext
assert "inline bool walkableFace(Vec normal) {" in ext_policy and "constexpr float floorBand = 24.0f, walkableNormal = 0.7f;" in ext_policy
assert "listed(hits.nearby,hits.nearbyCount,live.get())) { release(slot,Exit::underfoot);" in scan_ext  # at-once restore: near band and "hits.open[i] = openBelow(probes,clear,down.valid,downSame);" in rays_ext
assert "->Get" not in rays_ext.replace("controller->GetCollisionFilterInfo", "").replace("parent->GetbhkWorld", "").replace("player->GetCharController", "")
assert "clear[k] = !hit && !failed;" in rays_ext  # a refused probe is never read as open
for forbidden in ("BSFixedString", "GetMenu(", "IsMenuOpen(", "RequestCameraControl", "AddTask(", "AllocTrampoline", "write_vfunc", "safe_write"):
    assert forbidden not in ext, forbidden
# Exterior refusal codes: own range above the base 60/61, own mask, one site each.
ext_codes = dict(re.findall(r"(\w+) = (\d+)", ext_policy[ext_policy.index("enum class ExtRefusal : unsigned {"):]
                            .split("};")[0]))
assert ext_codes and all(62 <= int(v) <= 69 for v in ext_codes.values()) and len(set(ext_codes.values())) == len(ext_codes)
assert not set(ext_codes.values()) & set(codes.values())
for name in ext_codes:
    assert ext.count("ExtRefusal::" + name) == 1, name
print("PASS ext wiring: off equals wave 1 (FCS-31 branch); in-gate INI; ray-hit structures and fan-bounded trees only; "
      "cover needs the upward pick; cover and overhang never take ground-standing or ground-level kit pieces; "
      "floor first, no claims on a failed floor; "
      "per-vtable pick audit; exterior-to-exterior keep; shared table, caps and restores; grid trusted by the player's cell; "
      "own FCS-62..69 codes, one site each")
