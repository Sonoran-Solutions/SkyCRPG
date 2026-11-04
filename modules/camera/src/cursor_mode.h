// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// Included in plugin.cpp after context(); all engine/UI mutation is main-thread only.
namespace cursorMode {
namespace api = skycrpg::selection::api;
using Exit = cursor::Exit;
static_assert(offsetof(RE::TESCamera,cameraRoot) == 0x20);
static_assert(offsetof(RE::IMenu,uiMovie) == 0x10);
static_assert(offsetof(RE::IMenu,menuFlags) == 0x1C);
static_assert(offsetof(RE::NiCamera::RUNTIME_DATA2,port) == 0x24);
static_assert(offsetof(RE::GViewport,width) == 0x10);
static_assert(offsetof(RE::GViewport,height) == 0x14);
static_assert(offsetof(RE::NiAVObject,worldBound) == 0xE4);
static_assert(offsetof(RE::NiBound,center) == 0);
static_assert(offsetof(RE::UI,processMessagesLock) == 0x158);
constexpr char menuName[] = "SkyCRPGCameraWorldPointer";
constexpr auto flags = static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kRequiresUpdate) | static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kAllowSaving);
const api::Interface* selection = nullptr;
HMODULE selectionModule = nullptr;
cursor::Session session;
std::atomic<bool> dataReady{false}, worldLoaded{false}, padInput{false};
std::atomic<unsigned> stop{0}, toggle{0}, held{0};
bool registered = false, pending = false, resolving = false;
double pendingSince = 0;
cursor::Freshness freshness;
cursor::HoverAttempts hoverAttempts;
std::uint32_t servicedFrame = 0;
bool serviced = false;
api::CursorFrame cachedFrame;
std::array<cursor::Candidate,16> projected{};
std::size_t projectedCount = 0;
bool waiting = false;
std::uint32_t submittedTarget = 0;
std::uint64_t submittedReplies = 0;
std::uint64_t refresh = 0, cycles = 0, projections = 0, pushes = 0;
std::uint32_t sentTarget = 0;
RE::FormID cell = 0;
unsigned traceLines = 0;
std::uint64_t framePickMicros = 0;
std::uint64_t refusalBits = 0;
class Overlay;
Overlay* overlay = nullptr;
RE::GPtr<RE::IMenu>& overlayOwner = *new RE::GPtr<RE::IMenu>; // process lifetime; replace only on main
bool mainThread() {
    const auto main = RE::Main::GetSingleton();
    return main && main->threadID == GetCurrentThreadId();
}
void refuse(Exit reason) {
    const auto bit = std::uint64_t{1} << static_cast<unsigned>(reason);
    if (refusalBits & bit) return;
    refusalBits |= bit;
    char line[128]{};
    constexpr std::array names{"none","toggle","disabled","lifecycle","menu","load","death","cell","first-person","special","precision","lock","F6","F8","selection","overlay","thread","stale"};
    std::snprintf(line,sizeof(line),"Cursor refused/exit reason=%s",names[static_cast<unsigned>(reason)]);
    log(line);
}
void service();
void leave(Exit reason);
bool ownMenu(RE::IMenu* menu);
bool nativeMenu();
Exit captureExit();
class Overlay final : public RE::IMenu {
public:
    RE::GFxValue pointer, reticle;
    bool usable = false;
    float width = 0, height = 0, left = 0, top = 0, bufferWidth = 0, bufferHeight = 0;
    Overlay() {
        if (!mainThread()) { stop.store(static_cast<unsigned>(Exit::thread)); return; }
        overlay = this;
        menuFlags.set(RE::UI_MENU_FLAGS::kRequiresUpdate,RE::UI_MENU_FLAGS::kAllowSaving);
        depthPriority = 2;
        const auto manager = RE::BSScaleformManager::GetSingleton();
        if (!manager || !manager->LoadMovie(this,uiMovie,"CursorMenu",RE::GFxMovieView::ScaleModeType::kShowAll)) return;
        hide();
        RE::GFxValue listener, result, root;
        // The vanilla frame-one listener tracks _xmouse/_ymouse. Remove it;
        // ProcessMessage below never forwards OS/menu input into this movie.
        if (!uiMovie->GetVariable(&listener,"_root.mouseListener") ||
            !uiMovie->Invoke("Mouse.removeListener",&result,&listener,1) || !result.IsBool() || !result.GetBool() ||
            !uiMovie->GetVariable(&pointer,"_root.mc_Cursor") || !pointer.IsDisplayObject() ||
            !uiMovie->GetVariable(&root,"_root")) return;
        const std::array<RE::GFxValue,2> createArgs{RE::GFxValue("skycrpgReticle"),RE::GFxValue(100.0)};
        if (!root.Invoke("createEmptyMovieClip",&reticle,createArgs) || !reticle.IsDisplayObject()) return;
        const std::array<RE::GFxValue,3> style{RE::GFxValue(2.0),RE::GFxValue(16777215.0),RE::GFxValue(100.0)};
        if (!reticle.Invoke("lineStyle",style)) return;
        for (const auto segment : std::array<std::array<double,4>,4>{{{-9,0,-3,0},{3,0,9,0},{0,-9,0,-3},{0,3,0,9}}}) {
            const std::array<RE::GFxValue,2> from{RE::GFxValue(segment[0]),RE::GFxValue(segment[1])};
            const std::array<RE::GFxValue,2> to{RE::GFxValue(segment[2]),RE::GFxValue(segment[3])};
            if (!reticle.Invoke("moveTo",from) || !reticle.Invoke("lineTo",to)) return;
        }
        uiMovie->SetMouseCursorCount(0);
        usable = dimensions();
        hide();
    }
    ~Overlay() override { if (mainThread() && overlay == this) overlay = nullptr; }
    static RE::IMenu* create() {
        auto menu = new Overlay;
        if (mainThread()) overlayOwner.reset(menu);
        return menu;
    }
    bool dimensions() {
        if (!uiMovie) return false;
        RE::GViewport viewport;
        uiMovie->GetViewport(&viewport);
        width = static_cast<float>(viewport.width); height = static_cast<float>(viewport.height);
        left = static_cast<float>(viewport.left); top = static_cast<float>(viewport.top);
        bufferWidth = static_cast<float>(viewport.bufferWidth); bufferHeight = static_cast<float>(viewport.bufferHeight);
        return width > 0 && height > 0 && left >= 0 && top >= 0 &&
            bufferWidth >= left+width && bufferHeight >= top+height;
    }
    void hide() {
        if (uiMovie) static_cast<void>(uiMovie->SetVariable("_root._visible",RE::GFxValue(false)));
    }
    bool paint() {
        if (!usable || !dimensions()) return false;
        session.pointer.move({},cursorSettings.sensitivity,width,height);
        const auto rect = uiMovie->GetVisibleFrameRect();
        const float x = rect.left+session.pointer.point.x/width*(rect.right-rect.left);
        const float y = rect.top+session.pointer.point.y/height*(rect.bottom-rect.top);
        RE::GFxValue::DisplayInfo info;
        info.SetPosition(x,y); info.SetVisible(!padInput.load());
        if (!pointer.SetDisplayInfo(info)) return false;
        info.SetVisible(padInput.load());
        if (!reticle.SetDisplayInfo(info)) return false;
        return uiMovie->SetVariable("_root._visible",RE::GFxValue(true));
    }
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& message) override {
        if (!mainThread()) { stop.store(static_cast<unsigned>(Exit::thread)); return RE::UI_MESSAGE_RESULTS::kPassOn; }
        if (message.type == RE::UI_MESSAGE_TYPE::kHide || message.type == RE::UI_MESSAGE_TYPE::kForceHide) {
            hide(); stop.store(static_cast<unsigned>(Exit::overlay));
        }
        // Never take native Activate, mouse buttons or a Scaleform input event.
        return RE::UI_MESSAGE_RESULTS::kPassOn;
    }
    void AdvanceMovie(float, std::uint32_t) override {
        if (!mainThread()) { stop.store(static_cast<unsigned>(Exit::thread)); return; }
    }
    void PostDisplay() override {
        if (!mainThread()) { stop.store(static_cast<unsigned>(Exit::thread)); return; }
        if (!session.active) return;
        if (const auto reason = captureExit(); reason != Exit::none) { leave(reason); return; }
        if (!ownMenu(this) || !paint()) { leave(Exit::overlay); return; }
        if (uiMovie) uiMovie->Display();
    }
};
bool ownMenu(RE::IMenu* menu) {
    if (!cursorSettings.enabled || !mainThread() || !menu || menu != overlay) return false;
    const auto ui = RE::UI::GetSingleton();
    const auto own = ui ? ui->GetMenu(menuName) : RE::GPtr<RE::IMenu>{};
    const auto allowed = static_cast<std::uint32_t>(flags) | static_cast<std::uint32_t>(RE::UI_MENU_FLAGS::kOnStack);
    return ui && ui->menuSystemVisible && own.get() == menu && (menu->menuFlags.underlying() & ~allowed) == 0 &&
        menu->menuFlags.all(RE::UI_MENU_FLAGS::kRequiresUpdate,RE::UI_MENU_FLAGS::kAllowSaving,RE::UI_MENU_FLAGS::kOnStack);
}
bool nativeMenu() {
    const auto ui = RE::UI::GetSingleton();
    if (!ui || focused()) return true;
    const auto hud = ui->GetMenu("HUD Menu"), fader = ui->GetMenu("Fader Menu"), mist = ui->GetMenu("Mist Menu");
    for (const auto& menu : ui->menuStack) {
        if (ownMenu(menu.get())) continue;
        // These passive movies are base gameplay, never an input-focus exemption.
        if (!menu || (menu.get() != hud.get() && menu.get() != fader.get() && menu.get() != mist.get())) return true;
    }
    return false;
}
bool apiLive() { return selection && selectionModule; } // process-lifetime DLL; no live unload contract
Exit captureExit() {
    if (const auto reason = stop.exchange(0)) return static_cast<Exit>(reason);
    if (nativeMenu()) return Exit::menu;
    if (!apiLive() || selection->setCursorMode(2) != api::Result::ok) return Exit::selection;
    if (!freshness.usable(lockClock(),false)) return Exit::stale;
    return Exit::none;
}
void resolve() {
    if (resolving || !dataReady.load()) return;
    resolving = true;
    selectionModule = GetModuleHandleW(L"SkyCRPG_Selection.dll");
    if (!selectionModule) { log("Cursor unavailable: SkyCRPG_Selection.dll absent."); return; }
    const auto proc = GetProcAddress(selectionModule,api::exportName);
    api::GetAPI get = nullptr;
    static_assert(sizeof(get) == sizeof(proc));
    std::memcpy(&get,&proc,sizeof(get));
    const auto candidate = get ? get(api::version) : nullptr;
    if (candidate && candidate->apiVersion == api::version && candidate->size >= sizeof(api::Interface) &&
        candidate->getSnapshot && candidate->copyCandidates && candidate->setTarget && candidate->clearTarget && candidate->setCursorMode && candidate->setCursorTarget && candidate->getCursorFrame)
        selection = candidate;
    else log("Cursor unavailable: Selection v2 cursor extension missing or invalid size/function table.");
}
void leave(Exit reason) {
    const bool wasActive = session.active || pending;
    session.exit(); pending = false; waiting = false; projectedCount = 0;
    if (wasActive && apiLive()) { static_cast<void>(selection->setCursorMode(0)); }
    if (overlay) overlay->hide();
    if (wasActive) {
        const auto queue = RE::UIMessageQueue::GetSingleton();
        if (queue) queue->AddMessage(menuName,RE::UI_MESSAGE_TYPE::kHide,nullptr);
        refuse(reason);
    }
    cell = 0; sentTarget = 0; hoverAttempts = {};
}
void requestExit(Exit reason) {
    if (!cursorSettings.enabled || !dataReady.load()) return;
    stop.store(static_cast<unsigned>(reason));
    if (mainThread()) { stop.store(0); leave(reason); }
}
RE::NiCamera* projectionCamera() {
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!camera || !camera->cameraRoot) return nullptr;
    const auto& children = camera->cameraRoot->GetChildren();
    const REL::Relocation<std::uintptr_t> table{RE::VTABLE_NiCamera[0]};
    std::size_t inspected = 0;
    for (const auto& child : children) {
        if (++inspected > 32) break;
        if (child && *reinterpret_cast<const std::uintptr_t*>(child.get()) == table.address()) return static_cast<RE::NiCamera*>(child.get());
    }
    return nullptr;
}
bool centre(RE::TESObjectREFR* ref, RE::NiPoint3& result) {
    if (!ref || ref->IsDeleted() || ref->IsDisabled()) return false;
    const auto root = ref->Get3D();
    if (!root) return false;
    if (ref->GetFormType() == RE::FormType::ActorCharacter && ref->IsDead()) {
        result = root->worldBound.center;
        return std::isfinite(result.x) && std::isfinite(result.y) && std::isfinite(result.z);
    }
    const auto base = ref->GetBaseObject();
    if (!base) return false;
    const auto lo = base->boundData.boundMin, hi = base->boundData.boundMax;
    if (lo.x > hi.x || lo.y > hi.y || lo.z > hi.z) return false;
    const auto angle = ref->GetAngle();
    const float scale = ref->GetScale()*.5f;
    if (!std::isfinite(scale) || scale <= 0) return false;
    const float x = (static_cast<float>(lo.x)+hi.x)*scale, y = (static_cast<float>(lo.y)+hi.y)*scale, z = (static_cast<float>(lo.z)+hi.z)*scale;
    const float sx=std::sin(angle.x),cx=std::cos(angle.x),sy=std::sin(angle.y),cy=std::cos(angle.y),sz=std::sin(angle.z),cz=std::cos(angle.z);
    const auto position = ref->GetPosition();
    result = {position.x+cy*cz*x+cy*sz*y-sy*z,
        position.y+(sx*sy*cz-cx*sz)*x+(sx*sy*sz+cx*cz)*y+sx*cy*z,
        position.z+(cx*sy*cz+sx*sz)*x+(cx*sy*sz-sx*cz)*y+cx*cy*z};
    return std::isfinite(result.x) && std::isfinite(result.y) && std::isfinite(result.z);
}
std::span<const cursor::Candidate> points() { return {projected.data(),projectedCount}; }
bool retainLatch() {
    const bool valid = std::any_of(points().begin(),points().end(),[](const auto& c) { return c.id == session.latch.id; });
    return session.latch.retain(session.pointer.point,cursorSettings.latchPixels,valid);
}
void clearAway() {
    if (!session.active || retainLatch()) return;
    const auto target = waiting ? submittedTarget : sentTarget;
    if (target && !cursor::within(points(),target,session.pointer.point,cursorSettings.radius)) {
        // This gate-free revocation also cancels a queued/in-flight hover. The
        // Selection activation resolver checks it before resolving native E.
        static_cast<void>(selection->setCursorTarget(0,cycles)); ++pushes;
        sentTarget = 0; waiting = false;
    }
}
void pick(const api::CursorFrame& frame, bool fresh) {
    if (!overlay) return;
    const auto start = std::chrono::steady_clock::now();
    const auto& snapshot = frame.snapshot;
    { // Re-project cached IDs once per engine update, including while walking.
        if (frame.count > projected.size()) { leave(Exit::selection); return; }
        const auto camera = projectionCamera();
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!camera || !player || !overlay->dimensions()) { leave(Exit::lifecycle); return; }
        projectedCount = 0;
        for (std::uint32_t i = 0; i < frame.count; ++i) {
            const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(frame.ids[i]);
            RE::NiPoint3 world;
            if (!centre(ref,world)) continue;
            // The native projector divides by abs(w). Test signed w first;
            // this remains correct with reversed Z and uses no depth range.
            if (!cursor::inFront(camera->GetRuntimeData().worldToCam[3],world.x,world.y,world.z)) continue;
            float x = 0,y = 0,z = 0;
            ++projections;
            if (!camera->WorldPtToScreenPt3(world,x,y,z,1e-5f) || !std::isfinite(x) || !std::isfinite(y)) continue;
            const cursor::Point screen{x*overlay->bufferWidth-overlay->left,(1-y)*overlay->bufferHeight-overlay->top};
            if (screen.x < 0 || screen.x > overlay->width || screen.y < 0 || screen.y > overlay->height) continue;
            const auto delta = world-player->GetPosition();
            projected[projectedCount++] = {frame.ids[i],screen,delta.SqrLength()};
        }
        if (fresh && cycles != snapshot.cyclesTotal) {
            cycles = snapshot.cyclesTotal;
            session.latch.cycle(snapshot.latched ? snapshot.target : 0,session.pointer.point);
            waiting = false; hoverAttempts = {};
        }
    }
    if (fresh) {
        sentTarget = frame.authorizedTarget;
        if (waiting && frame.replies != submittedReplies) {
            // queued is not applied; retry rejection only after another scan attempt.
            waiting = false;
            if (snapshot.externalTarget != submittedTarget || snapshot.externalResult != static_cast<std::uint32_t>(api::Result::ok))
                sentTarget = 0;
        }
    }
    clearAway();
    const auto id = retainLatch() ? session.latch.id : cursor::choose(points(),session.pointer.point,cursorSettings.radius);
    if (id != sentTarget && hoverAttempts.claim(id,frame.aliveRefreshes)) {
        const auto result = selection->setCursorTarget(id,cycles); ++pushes;
        if (result == api::Result::queued) {
            waiting = true; submittedTarget = id; submittedReplies = frame.replies; sentTarget = 0;
        } else if (result == api::Result::ok) { sentTarget = 0; waiting = false; }
        // Busy/invalid requests retry once per scan attempt; only session query or
        // bounded stale refreshes end capture, never a transient mailbox result.
    }
    framePickMicros += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-start).count());
}
bool registerOverlay() {
    if (!mainThread()) { refuse(Exit::thread); return false; }
    const auto ui = RE::UI::GetSingleton();
    if (!ui) {
        static bool deferred = false;
        if (!deferred) { log("Cursor overlay registration deferred: UI unavailable; retry on main service."); deferred = true; }
        return false;
    }
    // UI+0x158 is reentrant; first main service precedes every cursor session.
    const RE::BSSpinLockGuard lock(ui->processMessagesLock);
    if (ui->menuMap.find(std::string_view(menuName)) != ui->menuMap.end()) { refuse(Exit::overlay); return false; }
    ui->Register(menuName,Overlay::create);
    return true;
}
void traceFrame() {
    if (!mainThread()) return;
    if (session.active && traceLines < cursorSettings.trace) {
        ++traceLines;
        char line[384]{};
        std::snprintf(line,sizeof(line),"Cursor sample=%u refresh=%llu target=%08X latch=%08X x=%.2f y=%.2f projections=%llu pushes=%llu cost_us=%llu viable=%u available=%u count=%u replies=%llu auth=%08X",
            traceLines,static_cast<unsigned long long>(refresh),sentTarget,session.latch.id,session.pointer.point.x,session.pointer.point.y,
            static_cast<unsigned long long>(projections),static_cast<unsigned long long>(pushes),static_cast<unsigned long long>(framePickMicros),
            cachedFrame.viable,cachedFrame.snapshot.available,cachedFrame.count,static_cast<unsigned long long>(cachedFrame.replies),cachedFrame.authorizedTarget);
        log(line);
    }
    framePickMicros = 0;
}
void service() {
    if (!cursorSettings.enabled || !mainThread() || !dataReady.load()) return;
    if (!cursor::prepareOverlay(registered,session.active,pending,registerOverlay)) return;
    if (const auto reason = stop.exchange(0)) { leave(static_cast<Exit>(reason)); return; }
    const auto graphics = RE::BSGraphics::State::GetSingleton();
    const auto frameState = graphics ? graphics->GetFrameState1799() : nullptr;
    if (!frameState) { leave(Exit::lifecycle); return; }
    if (serviced && servicedFrame == frameState->gUpdateCounter) { clearAway(); return; }
    serviced = true; servicedFrame = frameState->gUpdateCounter;
    const unsigned request = toggle.exchange(0);
    if (request && (session.active || pending)) { leave(Exit::toggle); return; }
    if (!request && !session.active && !pending) return;
    resolve();
    const auto c = context();
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto parent = player ? player->GetParentCell() : nullptr;
    const auto now = lockClock();
    api::CursorFrame frame;
    const bool fresh = apiLive() && selection->getCursorFrame(&frame) == api::Result::ok;
    if (fresh) { cachedFrame = frame; freshness.observe(frame.aliveRefreshes,now,frame.refreshSeconds); }
    const auto& snapshot = cachedFrame.snapshot;
    refresh = cachedFrame.aliveRefreshes;
    const bool lost = (session.active && (!apiLive() || selection->setCursorMode(2) != api::Result::ok));
    cursor::Eligibility e{settings.enabled,c.prerequisites && c.world,nativeMenu(),worldLoaded.load() && !loading,player && player->IsDead(),
        !cell || (parent && parent->GetFormID() == cell),c.firstPerson,c.special || c.slide,
        player && player->IsWeaponDrawn(),c.targetLock,apiLive() && !lost,true,true};
    // Entry interrupts through the existing lease path; wait for TDM's unlocked
    // acknowledgement, never capture a frame while the lock still owns aim.
    if (request && e.locked) { interruptTargetLock(); e.locked = false; }
    if (pending && e.locked && now-pendingSince < .5) e.locked = false;
    auto reason = cursor::refusal(e);
    if (reason == Exit::none && !freshness.usable(now,false)) reason = Exit::stale;
    if (reason != Exit::none) { refuse(reason); leave(reason); return; }
    if (request) {
        const auto map = RE::ControlMap::GetSingleton();
        const bool pad = (request & 2u) != 0;
        const auto key = pad ? cursorSettings.toggleGamepad : cursorSettings.toggleKey;
        if (key < 0 || !map || !map->GetUserEventName(static_cast<std::uint32_t>(key),pad ? RE::INPUT_DEVICE::kGamepad : RE::INPUT_DEVICE::kKeyboard).empty()) {
            refuse(Exit::lifecycle); return;
        }
        const auto ui = RE::UI::GetSingleton();
        const auto queue = RE::UIMessageQueue::GetSingleton();
        if (!ui || !queue) { refuse(Exit::overlay); return; }
        if (!registered) { refuse(Exit::overlay); return; }
        pending = true; pendingSince = now; cell = parent ? parent->GetFormID() : 0;
        interruptTargetLock();
        queue->AddMessage(menuName,RE::UI_MESSAGE_TYPE::kShow,nullptr);
    }
    if (pending) {
        if (now-pendingSince >= .5) { leave(Exit::overlay); return; }
        if (c.targetLock || !overlay || !ownMenu(overlay)) return;
        if (!overlay->usable) { leave(Exit::overlay); return; }
        const auto result = selection->setCursorMode(1);
        if (result != api::Result::ok) {
            if (result == api::Result::disabled && !(refusalBits & (std::uint64_t{1} << static_cast<unsigned>(Exit::selection))))
                log("Cursor unavailable: Selection bAllowExternalTargets=0 (opt-in required).");
            leave(Exit::selection); return;
        }
        static_cast<void>(session.toggle(e,overlay->width,overlay->height));
        pending = false; recenter = false; cycles = snapshot.cyclesTotal;
        sentTarget = 0; hoverAttempts = {};
    }
    if (!session.active) return;
    if (!overlay || !ownMenu(overlay) || !overlay->paint()) { leave(Exit::overlay); return; }
    pick(cachedFrame,fresh);
}
bool look(const RE::NiPoint2& input) {
    if (!cursorSettings.enabled) return false;
    if (!mainThread()) { requestExit(Exit::thread); return false; }
    if (!session.active || !overlay) return false;
    if (toggle.exchange(0)) { leave(Exit::toggle); return false; }
    if (const auto reason = captureExit(); reason != Exit::none) { leave(reason); return false; }
    if (!ownMenu(overlay)) { leave(Exit::overlay); return false; }
    session.pointer.move({input.x,input.y},cursorSettings.sensitivity,overlay->width,overlay->height);
    clearAway();
    if (!overlay->paint()) { leave(Exit::overlay); return false; }
    return true;
}
void input(RE::InputEvent* events) {
    if (!cursorSettings.enabled || !dataReady.load()) return;
    for (auto event = events; event; event = event->next) {
        if (event->AsMouseMoveEvent()) padInput.store(false);
        if (const auto stick = event->AsThumbstickEvent(); stick && stick->IsRight()) padInput.store(true);
        const auto button = event->AsButtonEvent();
        if (!button) continue;
        const auto device = event->GetDevice();
        const bool pad = device == RE::INPUT_DEVICE::kGamepad;
        if (!pad && device != RE::INPUT_DEVICE::kKeyboard) continue;
        const int id = static_cast<int>(button->GetIDCode());
        if (button->IsDown()) {
            const auto reason = cursor::interruptKey(pad,id,button->QUserEvent() == "Toggle POV");
            if (reason != Exit::none) requestExit(reason);
            else if (id == (pad ? settings.toggleGamepad : settings.toggleKey)) requestExit(Exit::f6);
            else if (id == (pad ? settings.recenterGamepad : settings.recenterKey)) requestExit(Exit::lifecycle);
        }
        if (id != (pad ? cursorSettings.toggleGamepad : cursorSettings.toggleKey)) continue;
        const unsigned mask = pad ? 2u : 1u;
        const unsigned old = button->IsPressed() ? held.fetch_or(mask) : held.fetch_and(~mask);
        if (button->IsDown() && !(old & mask) && button->QUserEvent().empty()) toggle.fetch_or(mask);
    }
}
} // namespace cursorMode
