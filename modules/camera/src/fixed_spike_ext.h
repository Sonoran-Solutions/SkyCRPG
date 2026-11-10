// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included at the end of fixed_spike.h. P5.5-02 wave 2 package "ext" (DYL-217):
// see-through for roofs, overhangs and canopies in exterior cells. Nothing here
// runs unless [FixedCameraSpike] bEnabled=1 and iExteriorSeeThrough is 1 or 2;
// with 0, endFrame takes the wave 1 branch (nothing hidden outdoors, FCS-31).
// Main thread only (endFrame). Reuses the wave 1 hide table, budget, dwell,
// restore causes and trace; the rule is include/fixed_spike_ext_policy.h.
#include "fixed_spike_ext_policy.h"

namespace skycrpg::camera::fixed {
using Each = RE::BSContainer::ForEachResult;
// Own mask: the base word already uses FCS-60/61 and cannot hold bit 64+.
inline void refuseExterior(ExtRefusal code,const char* text) {
    const unsigned bit = extRefusalBit(code);
    if (runtime->ext.refusals.fetch_or(bit,std::memory_order_acq_rel) & bit) return;
    char line[320]{};
    std::snprintf(line,sizeof(line),"FixedCameraSpike refused code=FCS-%02u %s",static_cast<unsigned>(code),text);
    runtime->logger(line);
}
// loadSettings, inside the bEnabled gate, right after configure().
inline void configureExterior(const ExtConfig& requested) {
    if (!runtime) return;
    auto& x = runtime->ext;
    bool adjusted = false;
    x.config = validateExterior(requested,&adjusted);
    if (adjusted) refuseExterior(ExtRefusal::settings,"one or more [FixedCameraSpike] exterior values were out of range; safe values applied");
    char line[256]{};
    std::snprintf(line,sizeof(line),"exterior see-through mode=%u maxSpan=%.0f maxHidden=%u (0 = wave 1 behaviour outdoors)",
        x.config.mode,static_cast<double>(x.config.maxSpan),x.config.maxHidden);
    note(line);
}
// Plain placed references only: statics and movable statics are structures,
// trees are trees. Actors, doors, containers, activators, furniture, flora,
// items and lights are never touched outdoors. Terrain is not a reference.
inline ExtKind extKindOf(const RE::TESObjectREFR* ref,const RE::TESBoundObject* base) {
    if (!base || ref->GetFormType() != RE::FormType::Reference) return ExtKind::excluded;
    switch (base->GetFormType()) {
    case RE::FormType::Static:
    case RE::FormType::MovableStatic:
        return ExtKind::structure;
    case RE::FormType::Tree:
        return ExtKind::tree;
    default:
        return ExtKind::excluded;
    }
}
inline Obb obbOf(const RE::NiAVObject* root,const RE::TESBoundObject* base) {
    const auto& bounds = base->boundData;
    const auto& w = root->world;
    const auto& m = w.rotate.entry;
    return makeObb(
        {static_cast<float>(bounds.boundMin.x),static_cast<float>(bounds.boundMin.y),static_cast<float>(bounds.boundMin.z)},
        {static_cast<float>(bounds.boundMax.x),static_cast<float>(bounds.boundMax.y),static_cast<float>(bounds.boundMax.z)},
        {m[0][0],m[0][1],m[0][2],m[1][0],m[1][1],m[1][2],m[2][0],m[2][1],m[2][2]},
        {w.translate.x,w.translate.y,w.translate.z},w.scale);
}
// Once per exterior cell entry (the flag is reset by endFrame on a cell change).
inline void noteExteriorActive(RE::FormID cellId) {
    auto& r = *runtime;
    if (r.exteriorNoted) return;
    r.exteriorNoted = true;
    if (!r.config.hideTrace) return;
    char line[256]{};
    std::snprintf(line,sizeof(line),"FixedHide t=%.3f frame=%llu event=ext-active cell=%08X mode=%u maxHidden=%u maxSpan=%.0f hidden=%u",
        r.now,static_cast<unsigned long long>(r.frame),cellId,r.ext.config.mode,r.ext.config.maxHidden,
        static_cast<double>(r.ext.config.maxSpan),r.budget.hidden);
    emit(r.hideLines,line);
}
inline void noteExtSkip(const RE::TESObjectREFR* ref,RE::TESBoundObject* base,ExtSkip skip,const Obb& box) {
    auto& r = *runtime;
    ++r.ext.counters.skips[static_cast<std::size_t>(skip)];
    if (!r.config.hideTrace) return;
    const auto id = ref->GetFormID();
    for (const auto logged : r.skipLog) if (logged == id) return;
    r.skipLog[r.skipNext] = id;
    r.skipNext = (r.skipNext+1u)%static_cast<unsigned>(r.skipLog.size());
    const Extent e = obbExtent(box);
    char line[640]{};
    std::snprintf(line,sizeof(line),"FixedHide t=%.3f frame=%llu event=skip ref=%08X base=%08X reason=ext cause=%s minZ=%.2f maxZ=%.2f spanX=%.2f spanY=%.2f feetZ=%.2f model=%.200s",
        r.now,static_cast<unsigned long long>(r.frame),id,base ? base->GetFormID() : 0u,extSkipNames[static_cast<std::size_t>(skip)],
        static_cast<double>(e.minZ),static_cast<double>(e.maxZ),static_cast<double>(e.spanX),static_cast<double>(e.spanY),
        static_cast<double>(r.lastFeet),modelOf(base));
    emit(r.hideLines,line);
}
// The exterior cap first (nearest kept, as indoors), then the shared table.
inline Hidden* claimExterior(RE::TESObjectREFR* ref,RE::NiAVObject* root,const RE::TESBoundObject* base,float reach) {
    auto& r = *runtime;
    if (r.tracked >= r.ext.config.maxHidden) {
        const int farthest = farthestSlot(r.hidden,[](const Hidden& slot) { return slot.used; },[](const Hidden& slot) { return slot.reach; });
        if (farthest < 0 || !evicts(reach,r.hidden[static_cast<std::size_t>(farthest)].reach)) { ++r.ext.counters.cap; return nullptr; }
        release(r.hidden[static_cast<std::size_t>(farthest)],Exit::evicted);
    }
    return claimSlot(ref,root,base,reach);
}
// The loaded exterior cells that can hold a qualifying reference near the fan,
// the player's own cell first. 0 when the grid cannot be read, or when it does
// not hold the player's cell where the engine's position-to-cell step puts it
// (TES::GetCell, ID 13322): the slot mapping is then not trusted.
inline unsigned exteriorCells(RE::TESObjectCELL* parent,Vec feet,Vec eye,std::array<RE::TESObjectCELL*,extMaxCells>& cells) {
    const auto tes = RE::TES::GetSingleton();
    const auto grid = tes ? tes->gridCells : nullptr;
    const auto range = cellsAround(feet,eye);
    const auto ownX = cellIndex(feet.x),ownY = cellIndex(feet.y);
    if (!tes || tes->interiorCell || !grid || !grid->cells || !range.valid || !ownX || !ownY) return 0;
    const unsigned length = grid->length;
    const auto own = gridSlot(*ownX,*ownY,tes->currentGridX,tes->currentGridY,length);
    if (!own.valid() || grid->GetCell(static_cast<std::uint32_t>(own.x),static_cast<std::uint32_t>(own.y)) != parent) return 0;
    unsigned count = 0;
    cells[count++] = parent;
    for (int cellX = range.x0;cellX <= range.x1;++cellX) {
        for (int cellY = range.y0;cellY <= range.y1;++cellY) {
            const auto slot = gridSlot(cellX,cellY,tes->currentGridX,tes->currentGridY,length);
            if (!slot.valid() || count == cells.size()) continue;
            const auto cell = grid->GetCell(static_cast<std::uint32_t>(slot.x),static_cast<std::uint32_t>(slot.y));
            if (cell && cell != parent && cell->IsAttached()) cells[count++] = cell;
        }
    }
    return count;
}
// What one scan's picks found. Reference pointers are only compared, never
// dereferenced: the cell walk turns a pointer into a live reference.
struct ExtHits {
    std::array<const RE::TESObjectREFR*,extMaxHits> refs{};
    std::array<Vec,extMaxHits> top{};   // highest point where a sightline ray hit each
    std::array<bool,extMaxHits> open{}; // its open-below probes found a clear one
    unsigned count{};
    std::array<const RE::TESObjectREFR*,maxFloorProbes> floor{};
    unsigned floorCount{};
};
inline int hitIndex(const ExtHits& hits,const RE::TESObjectREFR* ref) {
    for (unsigned i = 0;i < hits.count;++i) if (hits.refs[i] == ref) return static_cast<int>(i);
    return -1;
}
// Sightline rays from the rendered camera to the exterior targets, each marched
// past up to extRaySegments hits; open-below probes for the hits that could be
// an overhang; the wave 1 floor probes. The one shared bhkWorld of all loaded
// exterior cells (GetbhkWorld, 2BE9B0) returns hits in any of them.
inline bool castExteriorRays(RE::PlayerCharacter* player,RE::TESObjectCELL* parent,Vec feet,Vec eye,
    const std::array<Vec,extRayTargets>& targets,ExtHits& hits) {
    auto& r = *runtime;
    auto& c = r.ext.counters;
    const auto world = parent->GetbhkWorld();
    const auto controller = player->GetCharController();
    const float scale = RE::bhkWorld::GetWorldScale();
    if (!world || !controller || !std::isfinite(scale) || scale <= 0) { ++c.noWorld; return false; }
    if (!r.pickChecked && !r.pickRefused) {
        // The audit castRays applies: CommonLib slot 0x33 must be the PickObject read in the file.
        const auto slots = *reinterpret_cast<const std::uintptr_t* const*>(world);
        if (slots[0x33] != REL::Module::get().base()+0x104be80) r.pickRefused = true;
        else r.pickChecked = true;
    }
    if (r.pickRefused) {
        refuseExterior(ExtRefusal::pickSlot,"bhkWorld slot 0x33 is not the audited PickObject; no exterior rays, nothing hidden outdoors");
        return false;
    }
    RE::CFilter filter{};
    controller->GetCollisionFilterInfo(filter);
    const std::uint32_t layer = (filter.filter & 0xffff0000u) | static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);
    RE::BSReadLockGuard lock(world->worldLock);
    // failed: the pick itself was refused (never read as clear).
    const auto pick = [&](Vec from,Vec to,float& fraction,bool& hit,bool& failed) -> const RE::TESObjectREFR* {
        RE::bhkPickData ray{};
        ray.rayInput.from = RE::hkVector4(from.x*scale,from.y*scale,from.z*scale,0.0f);
        ray.rayInput.to = RE::hkVector4(to.x*scale,to.y*scale,to.z*scale,0.0f);
        ray.rayInput.filterInfo.filter = layer;
        ray.rayInput.enableShapeCollectionFilter = false;
        ray.rayOutput.Reset();
        ++c.rays;
        world->PickObject(ray);
        failed = ray.pickFailed;
        if (failed) ++c.rayFailed;
        hit = !failed && ray.rayOutput.rootCollidable != nullptr;
        if (!hit) return nullptr;
        fraction = ray.rayOutput.hitFraction;
        ++c.rayHits;
        const auto ref = RE::TESHavokUtilities::FindCollidableRef(*ray.rayOutput.rootCollidable);
        if (!ref) ++c.rayNoRef; // terrain and other collision without a reference
        return ref;
    };
    const RE::TESObjectREFR* self = player;
    for (const auto target : targets) {
        Vec from = eye;
        for (unsigned segment = 0;segment < extRaySegments;++segment) {
            float fraction = 1.0f; bool hit = false,failed = false;
            const auto ref = pick(from,target,fraction,hit,failed);
            if (!hit) break;
            const Vec point = mix(from,target,fraction);
            if (ref && ref != self) {
                const int at = hitIndex(hits,ref);
                if (at >= 0) { if (point.z > hits.top[static_cast<std::size_t>(at)].z) hits.top[static_cast<std::size_t>(at)] = point; }
                else if (hits.count == extMaxHits) { ++c.rayCap; break; }
                else { hits.refs[hits.count] = ref; hits.top[hits.count] = point; ++hits.count; }
            }
            const auto next = rayContinue(from,target,fraction);
            if (!next) break;
            from = *next;
        }
    }
    // Probed with the looser (hidden) thresholds; classify applies the right one.
    unsigned probed = 0;
    for (unsigned i = 0;i < hits.count;++i) {
        if (!overhangHit(hits.top[i],feet,true)) continue;
        if (probed == maxOverhangProbed) { ++c.probeCap; break; }
        ++probed; ++c.probed;
        const auto probes = openProbes(feet,hits.top[i]);
        std::array<bool,openProbeCount> clear{};
        for (unsigned k = 0;k < openProbeCount;++k) {
            if (!probes[k].valid) continue;
            float fraction = 1.0f; bool hit = false,failed = false;
            static_cast<void>(pick(probes[k].from,probes[k].to,fraction,hit,failed));
            clear[k] = !hit && !failed;
        }
        hits.open[i] = openBelow(probes,clear);
        if (hits.open[i]) ++c.open;
    }
    for (const auto probe : floorProbes(feet)) {
        float fraction = 1.0f; bool hit = false,failed = false;
        const auto ref = pick({probe.x,probe.y,probe.z+48.0f},{probe.x,probe.y,probe.z-160.0f},fraction,hit,failed);
        if (ref && !listed(hits.floor,hits.floorCount,ref)) hits.floor[hits.floorCount++] = ref;
    }
    return true;
}
// Every 0.1 s in eligible frames in an exterior cell with iExteriorSeeThrough
// 1 or 2. Candidates come only from the ray hits (structures) and, in mode 2,
// from trees whose box the fan crosses; the cells are walked only to turn a hit
// pointer into a live reference, as the interior scan does.
inline void scanExterior(RE::PlayerCharacter* player,RE::TESObjectCELL* parent) {
    auto& r = *runtime;
    auto& x = r.ext;
    auto& c = x.counters;
    if (r.now < r.nextScan) return;
    r.nextScan = r.now+hideScanSeconds;
    noteExteriorActive(parent->GetFormID());
    const auto camera = RE::PlayerCamera::GetSingleton();
    if (!camera || !camera->cameraRoot) return;
    const auto feetPoint = player->GetPosition();
    const auto eyePoint = camera->cameraRoot->world.translate;
    const Vec feet{feetPoint.x,feetPoint.y,feetPoint.z},eye{eyePoint.x,eyePoint.y,eyePoint.z};
    if (!finite(feet) || !finite(eye)) return;
    r.budget.frame();
    ++c.scans;
    r.lastFeet = feet.z; r.lastCut = feet.z+headClear; // traced as cutZ: where the cover column starts outdoors
    x.lastEyeZ = eye.z;
    const auto targets = exteriorTargets(feet);
    ExtHits hits;
    std::array<RE::TESObjectCELL*,extMaxCells> cells{};
    unsigned matched{},visited{};
    const unsigned cellCount = exteriorCells(parent,feet,eye,cells);
    x.gridMisses = cellCount ? 0u : x.gridMisses+1u;
    if (!cellCount) {
        ++c.gridSkips;
        if (x.gridMisses >= gridRefuseScans)
            refuseExterior(ExtRefusal::grid,"loaded cell grid unreadable, not holding the player's cell at its slot, or the fan spans more than 3 x 3 cells for 1 s; those scans skipped (gridSkips), hidden references expire");
    } else if (castExteriorRays(player,parent,feet,eye,targets,hits) && (hits.count || x.config.mode >= 2)) {
        c.cells += cellCount;
        bool full = false;
        for (unsigned i = 0;i < cellCount && !full;++i) {
            cells[i]->ForEachReference([&](RE::TESObjectREFR* ref) {
                // Mode 1 needs only the hit pointers: stop once every one is found.
                if (x.config.mode < 2 && matched == hits.count) { full = true; return Each::kStop; }
                if (++visited > maxVisited) { ++c.visitCap; full = true; return Each::kStop; }
                if (ref == player) return Each::kContinue;
                const int at = hitIndex(hits,ref);
                const bool onRay = at >= 0;
                if (onRay) ++matched;
                else if (x.config.mode < 2) return Each::kContinue; // structures come only from ray hits
                const auto base = ref->GetBaseObject();
                const ExtKind kind = extKindOf(ref,base);
                if (!onRay && kind != ExtKind::tree) return Each::kContinue;
                const auto root = ref->Get3D();
                if (!root) return Each::kContinue;
                const auto& bound = root->worldBound;
                const Vec centre{bound.center.x,bound.center.y,bound.center.z};
                if (!onRay) {
                    ++c.trees;
                    if (!nearFan(centre,bound.radius,feet,eye)) return Each::kContinue;
                    ++c.treeTests;
                }
                ExtCandidate candidate;
                candidate.kind = kind;
                if (kind != ExtKind::excluded) candidate.box = obbOf(root,base);
                candidate.rayHit = onRay;
                candidate.floorContact = listed(hits.floor,hits.floorCount,ref);
                if (onRay) { candidate.hit = hits.top[static_cast<std::size_t>(at)]; candidate.openBelow = hits.open[static_cast<std::size_t>(at)]; }
                candidate.pierced = kind == ExtKind::tree && piercedByFan(candidate.box,eye,targets,feet.z);
                const float reach = groundReach(feet,centre,bound.radius);
                // The looser exit thresholds first, as indoors; a table lookup only for qualifiers.
                ExtVerdict verdict = classifyExterior(x.config,candidate,feet,eye.z,true);
                Hidden* slot = verdict.reason != Reason::none ? findSlot(ref->GetFormID(),root) : nullptr;
                if (verdict.reason != Reason::none && (!slot || !slot->track.hidden))
                    verdict = classifyExterior(x.config,candidate,feet,eye.z,false);
                if (verdict.reason == Reason::none) {
                    if (verdict.skip != ExtSkip::none) noteExtSkip(ref,base,verdict.skip,candidate.box);
                    return Each::kContinue;
                }
                if (!slot) slot = claimExterior(ref,root,base,reach);
                if (slot) { slot->reason = verdict.reason; slot->reach = reach; slot->track.seen(r.now); }
                return Each::kContinue;
            });
        }
        c.visited += visited; c.matched += matched;
        c.unmatched += hits.count > matched ? hits.count-matched : 0u; // persistent or unloaded owners: never hidden
    }
    std::array<Near,maxHidden> due{};
    unsigned dueCount{},index{};
    for (auto& slot : r.hidden) {
        const unsigned at = index++;
        if (!slot.used) continue;
        const auto live = slot.handle.get();
        if (!live || live->Get3D() != slot.root.get()) { release(slot,Exit::unload); continue; }
        // Now under the player's feet: back in this scan, not after the dwell.
        if (slot.track.hidden && slot.track.last != r.now && listed(hits.floor,hits.floorCount,live.get())) { release(slot,Exit::underfoot); continue; }
        if (slot.track.shouldHide(r.now)) due[dueCount++] = {slot.reach,at};
        else if (slot.track.shouldRestore(r.now) || slot.track.expired(r.now)) release(slot,Exit::expired);
    }
    // Nearest first, so the per-scan cap spends itself on what is over the player.
    nearestFirst(due,dueCount);
    for (unsigned i = 0;i < dueCount;++i) {
        auto& slot = r.hidden[due[i].slot];
        // A root that is already hidden belongs to the engine or another mod.
        if (slot.root->GetAppCulled()) { ++r.totals.foreign; release(slot,Exit::expired); continue; }
        if (!r.budget.take()) break;
        slot.root->SetAppCulled(true);
        slot.track.hidden = true;
        ++r.totals.hides; ++c.hides;
        const auto live = slot.handle.get();
        traceHide("hide",slot,"-",live ? modelOf(live->GetBaseObject()) : "");
    }
}
inline void traceExteriorSummary(RE::FormID cellId) {
    auto& r = *runtime;
    if (!r.config.hideTrace || r.now < r.ext.nextSummary) return;
    r.ext.nextSummary = r.now+1.0;
    ExtSummary s;
    s.seconds = r.now; s.frame = r.frame; s.cell = cellId; s.mode = r.ext.config.mode;
    s.hidden = r.budget.hidden; s.tracked = r.tracked; s.maxHidden = r.ext.config.maxHidden;
    s.feetZ = r.lastFeet; s.eyeZ = r.ext.lastEyeZ; s.restores = r.totals.restores;
    s.c = r.ext.counters; s.exits = r.totals.exits;
    char line[768]{};
    if (formatExtSummary(line,sizeof(line),s)) emit(r.hideLines,line);
}
} // namespace skycrpg::camera::fixed
