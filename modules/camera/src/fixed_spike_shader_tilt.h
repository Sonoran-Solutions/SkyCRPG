// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included by fixed_spike_shader.h. P5.5-02 wave 2 package "shader", Stage 3
// step 3.2: the tilted cut (include/fixed_spike_shader_tilt_policy.h has the
// plane). Exists only with iSeeThroughMode=3, iCutStyle=1 and iCutTilt=1; with
// iCutTilt=0 nothing here is allocated, no tilted shader is made and every
// pass is cut exactly as before (the flat shaders' bytes are unchanged).
//
// Mechanism. A pass the flat cut would draw with the plane buffer (Stage 1's
// Treat::flat, and the Effect passes cut at the plane) in a cell Stage 1's
// census calls flat is drawn instead with a second patched copy of the same
// audited vertex shader (patchVertexShader Cut::tilted) and a 48-byte dynamic
// buffer in the same slot 13 holding the three planes of TiltConstant,
// Mapped only when they change. Kit and unknown cells, removed-whole draws,
// the storey line, Effect passes clipped whole and every shadow pass are
// untouched. Every refusal (FCS-102..105) turns the tilt off for the session:
// every pass is cut flat from then on (a refusal for one class's shader with
// its twin in the other class tilted would cut depth and colour apart).
//
// Agreement with the other readers of the plane. Lighting and Utility (the
// depth pre-pass) of one geometry take the same tilted shader and the same
// rows (one posAdjust per view). The skip rules that keep a draw native when
// it lies wholly under the flat plane stay valid: the tilted cut keeps
// everything the flat cut keeps (max with the low plane). The Effect whole
// rule reads the tilted height above the pass's bound centre. The frame
// pass's band is off in a tilted cell (BandState::tilt). The fill never sees a
// tilted pass (openFill stays false; counted fillSkipped). The void colour,
// the sky and the player fade hold do not read the plane.

namespace skycrpg::camera::fixed::shader {
// A vertex shader object, the identity of the bytecode its tilted copy was made from, and the copy.
// `sum` is the DXBC container checksum (bytes 4..20, an MD5-based digest of the whole container,
// verified against all 16,044 archive blobs by the policy test) and `size` the bytecode size: both are
// read in O(1) on every lookup, so an engine object freed and reused for other bytecode at the same
// address is noticed without hashing kilobytes per pass. `hash` (FNV-1a-64, the audit lists' key) is
// kept for the log lines.
struct TiltEntry {
    const std::uint8_t* object{};
    std::array<std::uint8_t,16> sum{};
    std::uint32_t size{};
    std::uint64_t hash{};
    void* shader{};
};
struct TiltRuntime {
    Feature state{Feature::pending};
    void* buffer{};
    TiltConstant uploaded{};
    bool haveUploaded{};
    std::optional<TiltPlanes> planes; // this frame's, from publish; none: no tilted pass this frame
    std::array<TiltEntry,2048> shaders{};
    unsigned made{},refused{};
    TiltWindow window{};
};
inline Feature tiltState() { return runtime && runtime->tilt ? runtime->tilt->state : Feature::off; }
// The tilt is drawing: installed, Stage 1 drawing, not latched.
inline bool tiltOn() { return runtime && runtime->tilt && runtime->tilt->state == Feature::installed && styled(); }
// This frame's cell takes the tilted cut: Stage 1's census called it flat and the frame has planes.
inline bool tiltCell() { return tiltOn() && runtime->style->cellStyle == CellStyle::flat && runtime->tilt->planes.has_value(); }
// This frame's planes; only after tiltCell() said true.
inline const TiltPlanes& tiltPlanesNow() { return *runtime->tilt->planes; }
// An Effect pass of a tilted cell clipped whole (the whole rule at the tilted height).
inline void tiltNoteWhole() { ++runtime->tilt->window.effectWhole; }
// The bytecode an entry was made from is still the object's: same size, same container checksum.
inline bool sameCode(const TiltEntry& slot,std::uint32_t size,const std::uint8_t* code) {
    return slot.size == size && std::memcmp(slot.sum.data(),code+checksumAt,slot.sum.size()) == 0;
}
// The tilted copy of an audited, already patched vertex shader object (entry.patched is non-null).
// An entry whose object now holds other bytecode is dropped (its copy released) and rebuilt in place
// (counted `rebuilt`). A refusal (FCS-102/103) ends the tilt for the session: one class tilted and its
// twin flat would cut the depth pre-pass and the colour pass of one geometry at different planes.
inline TiltEntry* tiltEntryFor(const Entry& entry,std::size_t group) {
    auto& r = *runtime;
    auto& t = *r.tilt;
    const std::uint32_t size = peek<std::uint32_t>(entry.object,offset::shaderCodeSize);
    const std::uint8_t* code = entry.object+offset::shaderCode;
    if (size < containerHeader || size > largestBlob) return nullptr; // prepare accepted it; never expected
    const std::size_t first = (reinterpret_cast<std::uintptr_t>(entry.object) >> 4) % t.shaders.size();
    for (std::size_t probe = 0;probe < 16;++probe) {
        auto& slot = t.shaders[(first+probe) % t.shaders.size()];
        if (slot.object == entry.object && sameCode(slot,size,code)) return &slot;
        if (slot.object == entry.object) { releaseObject(slot.shader); ++t.window.rebuilt; }
        else if (slot.object) continue;
        slot = {};
        slot.object = entry.object;
        slot.size = size;
        std::memcpy(slot.sum.data(),code+checksumAt,slot.sum.size());
        slot.hash = codeHash(code,size);
        char text[256]{};
        const Patched tilted = patchVertexShader(code,size,Cut::tilted);
        if (tilted.result != Patch::ok) {
            ++t.refused; t.state = Feature::refused;
            std::snprintf(text,sizeof(text),"%s bytecode %016llx has no tilted copy (%s); the flat cut for the rest of the session",
                group ? "Effect" : "audited",static_cast<unsigned long long>(slot.hash),patchNames[static_cast<std::size_t>(tilted.result)]);
            refuse(Refusal::tiltPatch,text); return &slot;
        }
        void* created = nullptr;
        const std::int32_t answer = invoke<std::int32_t>(r.device,method::createVertexShader,static_cast<const void*>(tilted.code.data()),
            tilted.code.size(),static_cast<void*>(nullptr),&created);
        if (answer < 0 || !created) {
            ++t.refused; t.state = Feature::refused;
            std::snprintf(text,sizeof(text),"the device refused a tilted vertex shader (hash %016llx, result %08X); the flat cut for the rest of the session",
                static_cast<unsigned long long>(slot.hash),static_cast<unsigned>(answer));
            refuse(Refusal::tiltCreate,text); return &slot;
        }
        slot.shader = created;
        ++t.made; ++t.window.created;
        return &slot;
    }
    return nullptr;
}
// The rows Mapped only when they change (the plane buffer's rule). A failure ends the tilt for the
// session (FCS-105); the flat cut goes on.
inline bool uploadTilt(const TiltConstant& wanted) {
    auto& r = *runtime;
    auto& t = *r.tilt;
    if (t.haveUploaded && std::memcmp(&wanted,&t.uploaded,sizeof(wanted)) == 0) return true;
    MappedBuffer mapped{};
    const std::int32_t answer = invoke<std::int32_t>(r.context,method::map,t.buffer,0u,mapWriteDiscard,0u,&mapped);
    if (answer < 0 || !mapped.data) {
        t.state = Feature::refused;
        char text[192]{};
        std::snprintf(text,sizeof(text),"the tilt buffer could not be mapped (result %08X); the flat cut for the rest of the session",
            static_cast<unsigned>(answer));
        refuse(Refusal::tiltMap,text); return false;
    }
    std::memcpy(mapped.data,&wanted,sizeof(wanted));
    invoke<void>(r.context,method::unmap,t.buffer,0u);
    t.uploaded = wanted; t.haveUploaded = true;
    ++t.window.uploads;
    return true;
}
// A pass the flat cut would draw at the plane: drawn tilted when its cell takes the tilt. False = the
// caller draws the flat cut (apply), as without iCutTilt. Binds as apply does: buffer, then shader;
// closed by finish at RestoreGeometry.
inline bool tiltPass(const Entry& entry,Vec eye,std::size_t shaderClass) {
    auto& r = *runtime;
    if (!r.tilt || !styled()) return false;
    auto& t = *r.tilt;
    auto& tw = t.window;
    // After a refusal (FCS-102..105) every plane pass of a flat cell is drawn flat and counted.
    if (t.state != Feature::installed) { if (t.state == Feature::refused && r.style->cellStyle == CellStyle::flat) ++tw.refusedFlat; return false; }
    const std::size_t group = groupOf(shaderClass);
    if (r.style->cellStyle != CellStyle::flat) { ++(r.style->cellStyle == CellStyle::kit ? tw.kitFlat : tw.unknownFlat); return false; }
    if (!t.planes) { ++tw.unknownFlat; return false; }
    const TiltEntry* slot = tiltEntryFor(entry,group);
    if (!slot) { ++tw.fullFlat; return false; }
    if (!slot->shader || t.state != Feature::installed) { ++tw.refusedFlat; return false; }
    if (!uploadTilt(tiltConstant(*t.planes,eye))) { ++tw.refusedFlat; return false; }
    void* buffers[1]{t.buffer};
    invoke<void>(r.context,method::vsSetConstantBuffers,cutBuffer,1u,static_cast<void* const*>(buffers));
    invoke<void>(r.context,method::vsSetShader,slot->shader,static_cast<void* const*>(nullptr),0u);
    r.openObject = entry.object;
    r.openFill = false; // Stage 3.1's fill never draws under a tilted plane
    if (r.fill && shaderClass == 0) ++tw.fillSkipped;
    r.opened.store(true,std::memory_order_release);
    auto& w = r.windows[group];
    ++w.cut; ++w.byClass[shaderClass];
    ++(group ? tw.effectTilted : tw.tilted);
    return true;
}
// publish, after styleFrame: this frame's planes (feet x, y of the published view, the held floor, the
// locked heading's quadrant; a snap switches the heading on the next published frame).
inline void tiltFrame() {
    auto& r = *runtime;
    if (!r.tilt) return;
    auto& t = *r.tilt;
    t.planes.reset();
    if (!tiltOn()) return;
    t.planes = tiltPlanes(r.view,r.config.height,r.config.tiltSlope,r.config.tiltMax);
    if (!t.planes) ++t.window.noPlanes;
}
// configure, after validate (which keeps iCutTilt only with iCutStyle=1).
inline void configureTilt() {
    auto& r = *runtime;
    if (r.config.tilt == tiltFlatCells) r.tilt = new TiltRuntime;
}
// commit, after the fill. A failure leaves the flat cut as it is (FCS-104).
inline void installTilt() {
    auto& r = *runtime;
    if (!r.tilt) return;
    auto& t = *r.tilt;
    BufferDescription description;
    description.bytes = static_cast<std::uint32_t>(sizeof(TiltConstant));
    void* created = nullptr;
    const std::int32_t answer = invoke<std::int32_t>(r.device,method::createBuffer,static_cast<const BufferDescription*>(&description),
        static_cast<const void*>(nullptr),&created);
    if (answer < 0 || !created) {
        t.state = Feature::refused;
        char text[192]{};
        std::snprintf(text,sizeof(text),"result %08X: the device refused the tilt buffer; the flat cut is drawn (iCutTilt=0 behaviour)",
            static_cast<unsigned>(answer));
        refuse(Refusal::tiltBuffer,text); return;
    }
    t.buffer = created;
    t.state = Feature::installed;
}
inline void traceTilt(double now) {
    auto& r = *runtime;
    if (!r.tilt) return;
    auto& t = *r.tilt;
    TiltSample sample;
    sample.seconds = now; sample.frame = r.frame; sample.mode = r.config.tilt; sample.state = t.state;
    sample.cell = r.style ? r.style->cellStyle : CellStyle::unknown;
    sample.haveView = r.active.load(std::memory_order_acquire) && r.haveView;
    sample.havePlanes = t.planes.has_value();
    if (t.planes) sample.planes = *t.planes;
    sample.slope = r.config.tiltSlope; sample.maxAbove = r.config.tiltMax;
    sample.made = t.made; sample.refused = t.refused;
    sample.window = t.window;
    t.window = {};
    char line[sizeof(TraceRow)]{};
    if (formatTilt(line,sizeof(line),sample)) r.tiltEmitter(line);
}
} // namespace skycrpg::camera::fixed::shader
