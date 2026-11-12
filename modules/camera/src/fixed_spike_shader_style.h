// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader.h. P5.5-02 wave 2 package "shader", fourth
// pass: Stage 1 of the seamless cutaway (include/fixed_spike_shader_style_policy.h
// has the rules). Exists only with iSeeThroughMode=3 and iCutStyle=1; with
// iCutStyle=0 nothing here is allocated and the flat cut runs exactly as the
// third pass.
//
// Mechanism. The same hooks, the same audited patched vertex shaders and the
// same slot 13 as the flat cut; per draw only the constant buffer bound in
// slot 13 differs: the plane buffer (the flat cut's, Mapped as before), a
// second dynamic buffer for the storey line, or a third holding
// removeConstant from creation (never Mapped). A draw kept whole is left to
// the engine. No engine code, object or state is written; nothing new is
// hooked. Shadows: a Utility pass of another view whose geometry the main
// view removed whole in this frame or the last is drawn with removeConstant.
//
// Engine reads, all on the main thread and all already made by the wave 1 hide
// (fixed_spike.h scanHide/extentOf/modelOf, run in game): the owning
// reference (ownerOf, as the flat cut), its base object, its 3D root's world
// transform and bound, the base OBND, the base's TESModel path; and once per
// cell TESObjectCELL::ForEachReference for the census.

namespace skycrpg::camera::fixed::shader {
struct StyleEntry {
    const RE::TESObjectREFR* ref{};
    std::uint32_t formId{},baseId{};
    unsigned long long frame{};      // last refresh; 0 = never
    bool used{},lane{},roof{},interact{},walkable{};
    int quadrant{-1};
    Family family{Family::other};
    Piece piece{Piece::flat};
    Thin thin{Thin::none};
    Side side{Side::across};
    Box box{};
    Decision decision{};
};
// D3D11_SUBRESOURCE_DATA.
struct InitialData { const void* data{}; std::uint32_t pitch{},slicePitch{}; };
static_assert(sizeof(InitialData) == 16);
constexpr std::size_t styleEntries = 4096, removedSlots = 8192, styleProbes = 16;
struct StyleRuntime {
    Feature state{Feature::pending};
    void* storeyBuffer{};
    void* removeBuffer{};
    Constant storeyUploaded{};
    bool haveStorey{};
    std::uint32_t cell{};
    CellStyle cellStyle{CellStyle::unknown};
    Census census{};
    unsigned censusTries{},refsNow{},refsLast{};
    Storey storey;
    float found{std::numeric_limits<float>::infinity()}; // lowest storey candidate of the frame being drawn
    float base{},lineZ{},capZ{};
    float headroomZ{}; // headroomFloor(base, feet): the overhead fact and the storey candidates (fix after batch T2)
    int quadrant{-1};
    unsigned long long lastFrame{};
    Budget shadowBudget;
    // Vertex shader objects of shadow passes whose bytecode is not on the audited list: skipped
    // without preparing them, so no FCS-75 line comes from a shadow pass (review of 30503e0, MINOR-7).
    std::array<const std::uint8_t*,256> shadowRejected{},shadowAccepted{};
    std::array<StyleEntry,styleEntries> entries{};
    std::array<std::array<const void*,removedSlots>,2> removed{}; // geometry removed whole: [current] this frame, the other the last
    std::size_t current{};
    StyleWindow window{};
};
inline Feature styleState() { return runtime && runtime->style ? runtime->style->state : Feature::off; }
inline bool styled() {
    return runtime && runtime->style && runtime->style->state == Feature::installed && !runtime->latched.load(std::memory_order_relaxed);
}
inline const char* modelPath(RE::TESBoundObject* base) {
    const auto model = base ? base->As<RE::TESModel>() : nullptr;
    return model ? model->GetModel() : nullptr;
}
// ---- geometry removed whole (for the shadow passes) ---------------------------
inline std::size_t removedSlot(const void* geometry,std::size_t probe) {
    return ((reinterpret_cast<std::uintptr_t>(geometry) >> 4)+probe) % removedSlots;
}
inline void noteRemoved(const void* geometry) {
    auto& set = runtime->style->removed[runtime->style->current];
    for (std::size_t probe = 0;probe < styleProbes;++probe) {
        auto& slot = set[removedSlot(geometry,probe)];
        if (slot == geometry) return;
        if (!slot) { slot = geometry; return; }
    }
}
inline bool wasRemoved(const void* geometry) {
    for (const auto& set : runtime->style->removed)
        for (std::size_t probe = 0;probe < styleProbes;++probe) {
            const auto slot = set[removedSlot(geometry,probe)];
            if (slot == geometry) return true;
            if (!slot) break;
        }
    return false;
}
// ---- one reference per frame --------------------------------------------------
inline StyleEntry* entryFor(const RE::TESObjectREFR* ref) {
    auto& s = *runtime->style;
    const std::uint32_t id = ref->GetFormID();
    const std::size_t first = (reinterpret_cast<std::uintptr_t>(ref) >> 4) % styleEntries;
    for (std::size_t probe = 0;probe < styleProbes;++probe) {
        auto& e = s.entries[(first+probe) % styleEntries];
        if (e.used && e.ref == ref && e.formId == id) return &e;
        if (!e.used) { e = {}; e.used = true; e.ref = ref; e.formId = id; return &e; }
    }
    return nullptr;
}
inline void refresh(StyleEntry& e,RE::TESObjectREFR* ref,Kind kind) {
    auto& r = *runtime;
    auto& s = *r.style;
    const auto base = ref->GetBaseObject();
    const std::uint32_t baseId = base ? base->GetFormID() : 0u;
    if (!e.frame || e.baseId != baseId) {
        e.baseId = baseId;
        const char* path = modelPath(base);
        e.family = modelFamily(path); e.roof = roofModel(path);
    }
    Box box;
    bool obnd = false;
    if (const auto root = ref->Get3D()) {
        if (base) {
            const auto& bounds = base->boundData;
            const auto& transform = root->world;
            const auto& m = transform.rotate.entry;
            box = worldBox({static_cast<float>(bounds.boundMin.x),static_cast<float>(bounds.boundMin.y),static_cast<float>(bounds.boundMin.z)},
                {static_cast<float>(bounds.boundMax.x),static_cast<float>(bounds.boundMax.y),static_cast<float>(bounds.boundMax.z)},
                {Vec{m[0][0],m[0][1],m[0][2]},Vec{m[1][0],m[1][1],m[1][2]},Vec{m[2][0],m[2][1],m[2][2]}},transform.scale,
                {transform.translate.x,transform.translate.y,transform.translate.z});
            obnd = box.valid;
        }
        if (!box.valid) box = sphereBox({root->worldBound.center.x,root->worldBound.center.y,root->worldBound.center.z},root->worldBound.radius);
    }
    const Vec feet = r.view.feet,eye = r.view.eye;
    e.box = box;
    e.piece = pieceOf(kind,e.family,box,obnd);
    e.thin = obnd ? thinAxis(box) : Thin::none;
    e.side = settledSide(box,e.thin,feet,eye,e.side,e.frame != 0 && e.quadrant == s.quadrant);
    e.lane = box.valid && settledLane(box,feet,eye,e.lane);
    e.quadrant = s.quadrant;
    if (storeyCandidate(e.piece,box,feet,s.headroomZ) && box.low.z < s.found) s.found = box.low.z;
    Facts f;
    f.piece = e.piece; f.side = e.side; f.lane = e.lane; f.gallery = r.config.keepFarGallery; f.kitCell = s.cellStyle == CellStyle::kit;
    f.above = box.valid && box.low.z >= s.lineZ-aboveTolerance;
    f.overhead = box.valid && box.low.z >= s.headroomZ+storeyHeadroom;
    f.around = box.valid && feet.x >= box.low.x && feet.x <= box.high.x && feet.y >= box.low.y && feet.y <= box.high.y;
    f.low = box.valid && box.high.z <= feet.z+laneHigh;
    // Every object form type the cursor can activate (all of kindOf's objects but Tree).
    f.interact = kind == Kind::object && base && base->GetFormType() != RE::FormType::Tree;
    // W5 fixes: roofs and ceilings never kept; a back piece kept only standing on this storey and not far above its cap.
    f.roof = e.roof;
    f.walkable = box.valid && box.high.z <= feet.z+floorTop;
    f.standing = box.valid && box.low.z <= s.base+standSlack;
    f.overCap = box.valid && box.high.z > s.capZ+capSlack;
    e.interact = f.interact; e.walkable = f.walkable;
    e.decision = decide(f);
    e.frame = r.frame;
    ++s.refsNow;
}
// ---- binding ----------------------------------------------------------------
// The storey line's constant, Mapped only when it changes (the plane buffer's rule).
inline bool uploadStorey(Constant wanted) {
    auto& r = *runtime;
    auto& s = *r.style;
    if (s.haveStorey && std::memcmp(&wanted,&s.storeyUploaded,sizeof(wanted)) == 0) return true;
    MappedBuffer mapped{};
    const std::int32_t answer = invoke<std::int32_t>(r.context,method::map,s.storeyBuffer,0u,mapWriteDiscard,0u,&mapped);
    if (answer < 0 || !mapped.data) {
        latch();
        char text[192]{};
        std::snprintf(text,sizeof(text),"the storey-line buffer could not be mapped (result %08X); native rendering for the rest of the session",
            static_cast<unsigned>(answer));
        refuse(Refusal::styleMap,text); return false;
    }
    std::memcpy(mapped.data,&wanted,sizeof(wanted));
    invoke<void>(r.context,method::unmap,s.storeyBuffer,0u);
    s.storeyUploaded = wanted; s.haveStorey = true;
    ++s.window.storeyUploads;
    return true;
}
// As the end of apply: buffer, then shader; closed by finish at RestoreGeometry.
// `mainView`: counted on the FixedShader row (shadow passes are not).
inline void bindStyle(const Entry& entry,void* buffer,std::size_t shaderClass,bool mainView) {
    auto& r = *runtime;
    void* buffers[1]{buffer};
    invoke<void>(r.context,method::vsSetConstantBuffers,cutBuffer,1u,static_cast<void* const*>(buffers));
    invoke<void>(r.context,method::vsSetShader,entry.patched,static_cast<void* const*>(nullptr),0u);
    r.openObject = entry.object;
    r.opened.store(true,std::memory_order_release);
    if (!mainView) return;
    auto& w = r.windows[0];
    ++w.cut; ++w.byClass[shaderClass];
}
// ---- the passes -------------------------------------------------------------
// A Lighting or Utility pass of the published view, iCutStyle=1. Counts on the
// FixedShader row as the flat cut counts them; a draw kept whole is counted
// only on the FixedShaderStyle row.
inline void styledPass(void* pass,std::size_t shaderClass,Vec eye) {
    auto& r = *runtime;
    auto& s = *r.style;
    auto& w = r.windows[0];
    auto& sw = s.window;
    const auto skipped = [&](Skip why) { ++w.skips[static_cast<std::size_t>(why)]; };
    const auto geometry = peek<const std::uint8_t*>(pass,offset::passGeometry);
    const Constant plane = cutConstant(r.view.planeZ,eye.z);
    if (!geometry || !usable(plane)) { skipped(Skip::noBound); return; }
    const Bound bound{peek<Vec>(geometry,offset::objectBound),peek<float>(geometry,offset::objectBound+sizeof(Vec))};
    Skip skip = reach(r.view,bound,peek<const void*>(geometry,offset::geometrySkin) != nullptr);
    // A draw under the plane is native unless its reference is removed whole, which only a kit cell does.
    const bool below = skip == Skip::below;
    if (below && r.view.radius > 0 && groundReach(r.view.feet,bound.centre,bound.radius) > r.view.radius) { skipped(Skip::below); return; }
    if (skip != Skip::none && !(below && s.cellStyle == CellStyle::kit)) { skipped(skip); return; }
    const auto ref = ownerOf(geometry);
    const bool actor = ref && ref->GetFormType() == RE::FormType::ActorCharacter;
    const Kind kind = ref && !actor ? kindOf(ref,ref->GetBaseObject()) : Kind::excluded;
    skip = ownerRule(ref != nullptr,actor,kind);
    if (skip != Skip::none) { skipped(below ? Skip::below : skip); return; }
    StyleEntry* e = entryFor(ref);
    Decision d{Treat::flat,s.cellStyle == CellStyle::kit ? Bucket::fallback : Bucket::flat};
    if (!e) ++sw.tableFull;
    else {
        if (e->frame != r.frame) refresh(*e,ref,kind);
        d = e->decision;
        // A back-side keep rests on the base OBND; a pass whose geometry the box does not account
        // for gets the flat cut instead (review of 30503e0, MINOR-3, as corrected after 2f5d9f6, MAJOR-1).
        // W5 gallery fix: a gallery keep whose geometry exceeds its box is removed like the rest of the
        // storey above (an interactable gets the flat cut, the floor under the player stays), never
        // flat-cut as a fallback (that sliced gallery objects).
        // A back piece cut at its cap rests on the box as well (review of 0b738ab, MINOR-2).
        if ((d.bucket == Bucket::keptFar || d.bucket == Bucket::cappedFar) && geometryExceeds(e->box,bound.centre,bound.radius))
            d = {Treat::flat,Bucket::fallback};
        else if (d.bucket == Bucket::keptGallery && geometryExceeds(e->box,bound.centre,bound.radius)) {
            d = e->walkable ? Decision{Treat::flat,Bucket::fallback} : e->interact ? Decision{Treat::flat,Bucket::interactCut} :
                Decision{Treat::remove,e->piece == Piece::object ? Bucket::removedObject : Bucket::removedWhole};
            ++sw.galleryRemoved;
        }
    }
    if (below && d.treat != Treat::remove) { skipped(Skip::below); return; }
    const bool sliced = d.treat == Treat::flat || d.treat == Treat::slice || d.treat == Treat::low || d.treat == Treat::cap;
    const float height = sliced ? sliceHeight(d.treat,r.view.planeZ,s.lineZ,s.capZ) : r.view.planeZ;
    if (d.treat == Treat::keep) { ++sw.buckets[static_cast<std::size_t>(d.bucket)]; return; }
    if (sliced && bound.centre.z+bound.radius <= height) { ++sw.underLine; return; }
    const Entry* entry = shaderFor(0);
    if (!entry || !entry->patched) { skipped(Skip::shader); return; }
    if (!r.budgets[0].take()) { skipped(Skip::cap); return; }
    ++sw.buckets[static_cast<std::size_t>(d.bucket)];
    ++sw.notes[static_cast<std::size_t>(d.note)];
    if (d.treat == Treat::remove) {
        noteRemoved(geometry);
        bindStyle(*entry,s.removeBuffer,shaderClass,true);
        return;
    }
    if (d.treat == Treat::low) ++sw.slicedLow;
    // A back piece cut at its cap is counted on FixedShaderStyle2 cappedFar, not as slicedFar.
    // ... nor an interactable on the back side given the flat cut (review of 82056db: it counted them as sliced walls).
    if (d.bucket != Bucket::cappedFar && d.bucket != Bucket::interactCut && (e ? e->side == Side::back : false)) ++sw.slicedFar;
    // An interactable given the flat cut instead of a removal is counted on interactCut only.
    // ... and a tall object around the player given the flat cut, on fallback (aroundCut).
    if (d.bucket != Bucket::interactCut && d.bucket != Bucket::aroundCut && (e ? e->piece == Piece::object : kind == Kind::object)) ++sw.slicedObjects;
    if (height == r.view.planeZ) { apply(*entry,plane,shaderClass); return; } // the plane buffer: the flat cut itself
    if (!uploadStorey(cutConstant(height,eye.z))) return;
    bindStyle(*entry,s.storeyBuffer,shaderClass,true);
}
// The vertex shader current for a shadow pass is on the Lighting/Utility audited
// list (checked once per shader object, 256 remembered each way; once both are
// full an unknown one is skipped without hashing). Only then may shaderFor prepare it.
inline bool shadowAudited() {
    auto& r = *runtime;
    auto& s = *r.style;
    const auto object = peek<const std::uint8_t*>(r.state,offset::currentVertexShader);
    if (!object) return false;
    for (const auto known : s.shadowAccepted) if (known == object) return true;
    for (const auto known : s.shadowRejected) if (known == object) return false;
    // Both caches full: skipped without hashing (counted on shadowSkips by the caller).
    if (s.shadowAccepted.back() && s.shadowRejected.back()) return false;
    const std::uint32_t size = peek<std::uint32_t>(object,offset::shaderCodeSize);
    const bool ok = size >= containerHeader && size <= largestBlob && auditedFor(0,codeHash(object+offset::shaderCode,size));
    auto& list = ok ? s.shadowAccepted : s.shadowRejected;
    for (auto& slot : list) if (!slot) { slot = object; break; }
    return ok;
}
// A Utility pass of another view (shadow maps among them): a geometry the main
// view removed whole this frame or the last casts nothing either.
inline void shadowPass(void* pass,std::size_t shaderClass) {
    auto& r = *runtime;
    auto& s = *r.style;
    if (!r.config.styleShadows || shaderClass != 1 || s.cellStyle != CellStyle::kit) return;
    const auto geometry = peek<const std::uint8_t*>(pass,offset::passGeometry);
    if (!geometry || !wasRemoved(geometry)) return;
    ++s.window.shadowPasses;
    if (!shadowAudited()) { ++s.window.shadowSkips; return; }
    const Entry* entry = shaderFor(0);
    if (!entry || !entry->patched || !s.shadowBudget.take()) { ++s.window.shadowSkips; return; }
    ++s.window.shadowRemoved;
    bindStyle(*entry,s.removeBuffer,shaderClass,false);
}
// ---- frame, cell, install, trace ------------------------------------------------
// publish, after the view is set: the storey line from the frame just drawn.
inline void styleFrame() {
    if (!styled()) return;
    auto& r = *runtime;
    auto& s = *r.style;
    const bool resumed = s.lastFrame+1u != r.frame;
    s.lastFrame = r.frame;
    s.base = r.plane.base;
    s.headroomZ = headroomFloor(s.base,r.view.feet.z); // fix after batch T2: never above the feet
    // After a gap (menu, load, any ineligible frame) the last frame drew nothing: the held line stays.
    if (!resumed) s.storey.update(s.found,s.base,r.view.planeZ,r.view.seconds);
    s.found = std::numeric_limits<float>::infinity();
    s.lineZ = s.storey.line(r.view.planeZ);
    s.capZ = capHeight(s.storey.known,s.lineZ,s.base);
    s.quadrant = quadrantOf(r.view.feet,r.view.eye);
    s.refsLast = s.refsNow; s.refsNow = 0;
    s.current = 1u-s.current;
    s.removed[s.current].fill(nullptr);
    s.shadowBudget.frame();
    ++s.window.frames;
}
// endFrame, eligible frames, before publish: the census of a newly entered cell.
inline void noteCell(RE::TESObjectCELL* cell) {
    if (!runtime || !runtime->style || !cell || !thin::onMain()) return;
    auto& s = *runtime->style;
    const std::uint32_t id = cell->GetFormID();
    if (id != s.cell) {
        s.cell = id; s.cellStyle = CellStyle::unknown; s.census = {}; s.censusTries = 0;
        s.storey.reset(); s.found = std::numeric_limits<float>::infinity();
        s.entries.fill(StyleEntry{});
        for (auto& set : s.removed) set.fill(nullptr);
    }
    if (s.cellStyle != CellStyle::unknown || !cell->IsInteriorCell()) return;
    if (s.censusTries++ >= censusRetries) { s.cellStyle = CellStyle::flat; return; }
    Census c;
    cell->ForEachReference([&](RE::TESObjectREFR* ref) {
        const auto base = ref ? ref->GetBaseObject() : nullptr;
        if (!base) return RE::BSContainer::ForEachResult::kContinue;
        const auto type = base->GetFormType();
        if (type != RE::FormType::Static && type != RE::FormType::MovableStatic) return RE::BSContainer::ForEachResult::kContinue;
        ++c.statics;
        const Family family = modelFamily(modelPath(base));
        if (family == Family::architecture) ++c.kit;
        else if (family == Family::dungeons) ++c.dungeon;
        return RE::BSContainer::ForEachResult::kContinue;
    });
    s.census = c;
    s.cellStyle = cellStyle(c);
}
// configure, after validate: only iCutStyle=1 has a style runtime.
inline void configureStyle() {
    auto& r = *runtime;
    if (r.config.style != stageOneStyle) return;
    r.style = new StyleRuntime;
    r.style->shadowBudget.limit = r.config.maxPerFrame;
}
// commit, after the sky. Both buffers are made before the style is used; a
// failure leaves the flat cut as it is (FCS-91).
inline void installStyle() {
    auto& r = *runtime;
    if (!r.style) return;
    auto& s = *r.style;
    BufferDescription description;
    description.bytes = static_cast<std::uint32_t>(sizeof(Constant));
    const InitialData initial{&removeConstant,0u,0u};
    // The remove buffer never changes: immutable (usage 1), no CPU access (review of 30503e0, MINOR-7 option).
    BufferDescription immutable = description;
    immutable.usage = 1; immutable.access = 0;
    void* storey = nullptr;
    void* remove = nullptr;
    const std::int32_t first = invoke<std::int32_t>(r.device,method::createBuffer,static_cast<const BufferDescription*>(&description),
        static_cast<const void*>(nullptr),&storey);
    const std::int32_t second = first >= 0 && storey ? invoke<std::int32_t>(r.device,method::createBuffer,
        static_cast<const BufferDescription*>(&immutable),static_cast<const void*>(&initial),&remove) : -1;
    if (first < 0 || !storey || second < 0 || !remove) {
        s.state = Feature::refused;
        char text[224]{};
        std::snprintf(text,sizeof(text),"results %08X,%08X: the device refused a style buffer; the flat cut is drawn (iCutStyle=0 behaviour)",
            static_cast<unsigned>(first),static_cast<unsigned>(second));
        refuse(Refusal::styleBuffers,text); return;
    }
    s.storeyBuffer = storey; s.removeBuffer = remove;
    s.state = Feature::installed;
}
inline void traceStyle(double now,const VoidWindow& effect) {
    auto& r = *runtime;
    if (!r.style) return;
    auto& s = *r.style;
    StyleSample sample;
    sample.seconds = now; sample.frame = r.frame; sample.style = r.config.style; sample.state = s.state; sample.cell = s.cellStyle;
    sample.census = s.census; sample.gallery = r.config.keepFarGallery; sample.shadows = r.config.styleShadows;
    sample.haveView = r.active.load(std::memory_order_acquire) && r.haveView;
    sample.storeyKnown = s.storey.known; sample.storeyZ = s.storey.z; sample.base = s.base; sample.lineZ = s.lineZ;
    sample.refs = s.refsLast; sample.moves = s.storey.moves; sample.window = s.window;
    sample.fxFloorKept = effect.floorKept; sample.fxWholeRadius = effect.wholeRadius;
    Style2Sample second;
    second.seconds = now; second.frame = r.frame; second.haveView = sample.haveView; second.capZ = s.capZ; second.window = s.window;
    second.headroomZ = s.headroomZ; second.heldMinusFeet = s.base-r.view.feet.z;
    s.window = {};
    char line[sizeof(TraceRow)]{};
    if (formatStyle(line,sizeof(line),sample)) r.styleEmitter(line);
    if (formatStyle2(line,sizeof(line),second)) r.style2Emitter(line);
}
} // namespace skycrpg::camera::fixed::shader
