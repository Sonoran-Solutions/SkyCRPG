// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// Included at the end of fixed_spike_shader_policy.h (after the fill policy). P5.5-02 wave 2 package
// "shader", Stage 3 step 3.2: the tilted cut, iCutTilt. In a cell Stage 1's census calls flat (one-piece
// caves and ruins) the cut is no longer one level plane: it rises with ground distance behind the player
// (away from the camera along the locked heading), so the far wall stays tall while the near side stays
// low. Kit cells keep Stage 1 unchanged. No engine headers, no engine pointers; the native side is
// src/fixed_spike_shader_tilt.h, the audit docs/evidence/p5.5/spike-wave2-shader.md ("Stage 3.2").
//
// The plane. With F the feet (x, y), Z0 the held floor (CutPlane, the flat cut's reference), H
// fShaderCutHeight, s fCutTiltSlope, M fCutTiltMax and h the unit ground vector from the feet towards the
// camera, quantised to the locked heading's diagonal (+-1/sqrt 2, +-1/sqrt 2), a point p is kept when
//     p.z <= max(Z0 + H, min(Z0 + H + s * ((F - p.xy) . h), Z0 + M))
// i.e. the near side (towards the camera) at the flat cut's height, rising by s per unit behind the
// feet, capped at Z0 + M. M >= H (validated), so the cap never cuts under the flat plane; the flat cut
// is the special case s = 0 (exactly: see tiltClip).
//
// Shader form (Cut::tilted in patchVertexShader): o16.x = max(dot(P, row0), min(dot(P, row1), dot(P, row2)))
// with P = (world - posAdjust, 1) and the three rows of TiltConstant in vertex constant buffer 13. Every
// row is relative to the engine's position adjustment exactly as the flat cut's offset is.

namespace skycrpg::camera::fixed::shader {
constexpr unsigned tiltOff = 0, tiltFlatCells = 1;
constexpr float tiltDiagonal = 0.70710678f;
// The three planes, posAdjust-relative: low (the flat cut), tilted, cap. 48 bytes, cb13[0..2].
struct TiltConstant { std::array<std::array<float,4>,3> rows{}; };
static_assert(sizeof(TiltConstant) == 48);
// The locked heading as the tilt sees it: the quadrant of the camera around the feet (quantised; a 90
// degree snap is a hard switch to the next quadrant, no blend) and the unit ground vector towards it.
struct Heading { int quadrant{-1}; float x{},y{}; };
inline Heading tiltHeading(Vec feet,Vec eye) {
    const int q = quadrantOf(feet,eye);
    if (q < 0) return {};
    return {q,(q & 1) ? tiltDiagonal : -tiltDiagonal,(q & 2) ? tiltDiagonal : -tiltDiagonal};
}
// One frame's planes in world heights. Anchors: the feet x, y of the published view (they move with
// the player every frame) and the held floor Z0 = planeZ - H (CutPlane: a jump or a step does not move it).
struct TiltPlanes {
    Heading heading{};
    Vec feet{};
    float lowZ{},capZ{},slope{};
};
inline std::optional<TiltPlanes> tiltPlanes(const View& view,float height,float slope,float maxAbove) {
    if (!finite(view.feet) || !finite(view.eye) || !std::isfinite(view.planeZ) || !std::isfinite(height) || !std::isfinite(slope) ||
        !std::isfinite(maxAbove) || slope < 0.0f) return {};
    const Heading h = tiltHeading(view.feet,view.eye);
    if (h.quadrant < 0) return {};
    TiltPlanes p;
    p.heading = h; p.feet = view.feet; p.slope = slope;
    p.lowZ = view.planeZ;
    p.capZ = (std::max)(view.planeZ-height+maxAbove,view.planeZ);
    if (!std::isfinite(p.capZ)) return {};
    return p;
}
// Ground distance of (x, y) behind the feet: positive away from the camera.
inline float behind(const TiltPlanes& p,float x,float y) { return (p.feet.x-x)*p.heading.x+(p.feet.y-y)*p.heading.y; }
// The world height of the cut above the ground point (x, y): what the shader keeps under.
inline float tiltHeight(const TiltPlanes& p,float x,float y) {
    return (std::max)(p.lowZ,(std::min)(p.lowZ+p.slope*behind(p,x,y),p.capZ));
}
// The rows for one pass's position adjustment.
inline TiltConstant tiltConstant(const TiltPlanes& p,Vec posAdjust) {
    const float low = p.lowZ-posAdjust.z,sx = p.slope*p.heading.x,sy = p.slope*p.heading.y;
    const float anchor = sx*(p.feet.x-posAdjust.x)+sy*(p.feet.y-posAdjust.y);
    TiltConstant k;
    k.rows[0] = {0.0f,0.0f,-1.0f,low};
    k.rows[1] = {-sx,-sy,-1.0f,low+anchor};
    k.rows[2] = {0.0f,0.0f,-1.0f,p.capZ-posAdjust.z};
    return k;
}
// What the patched shader computes for a posAdjust-relative world position (x*r0 + y*r1 + z*r2 + r3 in
// that order, as dp4 is specified to the precision D3D allows): the clip distance, kept when >= 0.
inline float rowDot(const std::array<float,4>& row,Vec q) { return q.x*row[0]+q.y*row[1]+q.z*row[2]+row[3]; }
inline float tiltClip(const TiltConstant& k,Vec q) {
    return (std::max)(rowDot(k.rows[0],q),(std::min)(rowDot(k.rows[1],q),rowDot(k.rows[2],q)));
}
// A copy of the view whose planeZ is the tilted plane's height over (x, y): for the rules that read one
// height (the Effect whole/upper tests) at a pass's bound centre.
inline View tiltedView(const View& view,const TiltPlanes& p,float x,float y) {
    View v = view;
    v.planeZ = tiltHeight(p,x,y);
    return v;
}

// ---- trace: FixedShaderTilt -------------------------------------------------------
// Per window: passes drawn with the tilted shader (Lighting/Utility, Effect), plane passes of the main view
// left on the flat cut because the cell is a kit cell or not yet known, because the tilted shader for the
// pass's vertex shader is refused (FCS-102/103) or the tilt table is full, Effect passes clipped whole,
// fill candidates the tilt took away from the fill, frames without planes, uploads; after a refusal
// (FCS-102..105, the tilt off for the session) refusedFlat counts the flat cell's plane passes drawn flat.
struct TiltWindow {
    unsigned long long tilted{},effectTilted{},kitFlat{},unknownFlat{},refusedFlat{},fullFlat{},effectWhole{},fillSkipped{},
        noPlanes{},uploads{};
    unsigned created{},rebuilt{}; // tilted shaders created in the window (first use, a first-frame hitch); entries rebuilt for new bytecode
};
struct TiltSample {
    double seconds{};
    unsigned long long frame{};
    unsigned mode{};
    Feature state{Feature::off};
    CellStyle cell{CellStyle::unknown};
    bool haveView{},havePlanes{};
    TiltPlanes planes{};
    float slope{},maxAbove{};
    unsigned made{},refused{};
    TiltWindow window{};
};
constexpr float tiltProbe = 500.0f; // the trace's sample distances behind and in front of the feet
inline bool formatTilt(char* out,std::size_t size,const TiltSample& s) {
    const auto& w = s.window;
    const double none = static_cast<double>(std::numeric_limits<float>::quiet_NaN());
    const bool on = s.haveView && s.havePlanes;
    const TiltPlanes& p = s.planes;
    // 500 behind: the feet moved away from the camera along -h; 500 in front: towards it.
    const auto at = [&](float distance) {
        return on ? static_cast<double>(tiltHeight(p,p.feet.x-distance*p.heading.x,p.feet.y-distance*p.heading.y)) : none;
    };
    const int written = std::snprintf(out,size,
        "FixedShaderTilt t=%.3f frame=%llu tilt=%u state=%s cell=%s quadrant=%d heading=%.3f,%.3f slope=%.3f max=%.1f lowZ=%.2f capZ=%.2f "
        "atFeet=%.2f behind500=%.2f front500=%.2f tilted=%llu effectTilted=%llu kitFlat=%llu unknownFlat=%llu refusedFlat=%llu fullFlat=%llu "
        "effectWhole=%llu fillSkipped=%llu noPlanes=%llu uploads=%llu made=%u refused=%u rebuilt=%u created=%u sample=window",
        s.seconds,s.frame,s.mode,featureNames[static_cast<std::size_t>(s.state)],cellNames[static_cast<std::size_t>(s.cell)],
        on ? p.heading.quadrant : -1,on ? static_cast<double>(p.heading.x) : none,on ? static_cast<double>(p.heading.y) : none,
        static_cast<double>(s.slope),static_cast<double>(s.maxAbove),on ? static_cast<double>(p.lowZ) : none,on ? static_cast<double>(p.capZ) : none,
        at(0.0f),at(tiltProbe),at(-tiltProbe),w.tilted,w.effectTilted,w.kitFlat,w.unknownFlat,w.refusedFlat,w.fullFlat,w.effectWhole,
        w.fillSkipped,w.noPlanes,w.uploads,s.made,s.refused,w.rebuilt,w.created);
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed::shader
