// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// P5.5-02 wave 2 package "clip" (DYL-217). Disposable spike policy for
// [FixedCameraSpike] iSeeThroughMode=2: a near plane moved to just in front of
// the player for selected draws. No engine headers, no engine pointers. The
// native adapter is src/fixed_spike_clip.h; the binary audit behind every
// number here is docs/evidence/p5.5/spike-wave2-clip.md.
#include "fixed_spike_policy.h"
#include <bit>
#include <cstdint>

namespace skycrpg::camera::fixed::clip {
// ---- audited engine facts (SkyrimSE.exe 1.7.104) ----------------------------
struct Address { std::uint64_t id{}; std::uintptr_t rva{}; };
constexpr Address stateAddress{388819,0x20cfc00};    // BSGraphics::RendererShadowState
constexpr Address uploadAddress{77258,0x100ae80};    // per-frame constant buffer upload(this,flags)
constexpr Address rendererAddress{411393,0x3331f40}; // the RCX every caller passes; the function never reads it
constexpr Address gateAddress{411490,0x3336400};     // byte: 1 from frame begin to present, the upload returns at once on 0
constexpr Address biasAddress{411359,0x3330200};     // 12 floats indexed by the raster depth-bias mode
constexpr unsigned biasModes = 12;
// First .pdata chunk of the upload function and its first 21 bytes: three
// whole instructions in 14 bytes, none RIP-relative, then sub rsp,320h.
constexpr CodeRange uploadExtent{0x100ae80,0x100b03c};
constexpr std::size_t uploadCopied = 14;
constexpr std::array<std::uint8_t,21> uploadPrologue{0x48,0x89,0x5c,0x24,0x08,0x55,0x48,0x8d,0xac,0x24,0xe0,0xfd,0xff,0xff,
    0x48,0x81,0xec,0x20,0x03,0x00,0x00};
// BSShader vtable slots 6 and 7: SetupGeometry(pass,flags) and
// RestoreGeometry(pass,flags). Every draw of a pass happens between the two.
constexpr std::size_t setupSlot = 6, restoreSlot = 7;
struct ShaderClass {
    const char* name{};
    Address table{};
    std::uintptr_t setupRva{},restoreRva{};
    std::array<std::uint8_t,16> setupBytes{},restoreBytes{};
};
constexpr std::array<ShaderClass,3> shaderClasses{{
    {"lighting",{255053,0x1b47420},0x1549550,0x154a900,
        {0x48,0x8b,0xc4,0x44,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55},
        {0x80,0x7a,0x1c,0x0a,0x75,0x3a,0x83,0x3d,0x83,0x53,0xb8,0x00,0x00,0x75,0x0c,0x81}},
    {"utility",{255302,0x1b4ab58},0x15673f0,0x15689c0,
        {0x48,0x8b,0xc4,0x48,0x89,0x48,0x08,0x41,0x56,0x48,0x81,0xec,0x40,0x01,0x00,0x00},
        {0x8b,0x81,0x90,0x00,0x00,0x00,0x45,0x33,0xc0,0x8b,0xc8,0x81,0xe1,0x00,0x12,0x00}},
    {"effect",{255194,0x1b48ab0},0x1557090,0x1557f50,
        {0x48,0x8b,0xc4,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55,0x53,0x56,0x48,0x8d},
        {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18,0x0f}}}};
// Byte offsets. State = RendererShadowState; camera = its cameraData (ViewData).
namespace offset {
constexpr std::size_t stateFlags = 0x0, viewportMinDepth = 0x80, viewportMaxDepth = 0x84, depthBiasMode = 0xa0, posAdjust = 0x35c, camera = 0x380;
constexpr std::size_t depthTarget = 0x38, currentVertexShader = 0x348;
constexpr std::size_t previousPosAdjust = 0x368;
constexpr std::size_t cameraSize = 0x250, viewDir = 0x20, projection = 0x70, viewProjection = 0xb0;
constexpr std::size_t viewProjectionUnjittered = 0x130, projectionUnjittered = 0x1b0, depthRange = 0x240;
// What the upload reads from the camera data: view, projection, view-projection
// at 30h..F0h and the three unjittered matrices at 130h..1F0h.
constexpr std::size_t uploadedFirst = 0x30, uploadedSecond = 0x130, uploadedSize = 0xc0;
constexpr std::size_t passGeometry = 0x10;                         // BSRenderPass
constexpr std::size_t objectParent = 0x30, objectBound = 0xe4, objectUser = 0xf8; // NiAVObject
constexpr std::size_t geometrySkin = 0x130;                        // BSGeometry
constexpr std::size_t shaderCodeSize = 0x10, shaderCode = 0x68;    // BSGraphics::VertexShader: DXBC length and bytes
}
constexpr std::uint32_t dirtyViewport = 0x2;   // stateUpdateFlags bit that makes the next draw call RSSetViewports
constexpr std::uint32_t dirtyDepthBias = 0x40; // and the bit that makes it rebuild the viewport's depth range first
constexpr unsigned maxOwnerHops = 32;

// One code per refusal site. FCS-40 and FCS-41 belong to the wave 1 snap keys.
enum class Refusal : unsigned { settings = 42, module = 43, address = 44, table = 45, prologue = 46, thread = 47, bracket = 48, state = 49 };

// ---- [FixedCameraSpike] clip keys ------------------------------------------
constexpr unsigned traceLines = 6000;
struct Config {
    float height{128.0f};    // plane anchor above the player's feet (top of the head)
    float margin{24.0f};     // view depth by which the plane sits in front of the anchor
    float corridor{384.0f};  // only bounds within this of the camera-anchor segment; 0 = the whole view
    float floorBand{24.0f};  // a reference wholly at or below feet + this is never clipped
    unsigned maxPerFrame{512};
    unsigned shaders{3};     // bit 0 lighting, bit 1 utility (depth), bit 2 effect
    bool exterior{};         // 0 = interior cells only
    bool trace{};
};
inline Config validate(Config c,bool* adjusted = nullptr) {
    const Config in = c;
    c.height = bounded(c.height,128.0f,0.0f,512.0f);
    c.margin = bounded(c.margin,24.0f,0.0f,256.0f);
    c.corridor = bounded(c.corridor,384.0f,0.0f,4096.0f);
    c.floorBand = bounded(c.floorBand,24.0f,0.0f,128.0f);
    c.maxPerFrame = std::clamp(c.maxPerFrame,1u,4096u);
    if (!c.shaders || c.shaders > 7u) c.shaders = 3;
    if (adjusted) *adjusted = c.height != in.height || c.margin != in.margin || c.corridor != in.corridor ||
        c.floorBand != in.floorBand || c.maxPerFrame != in.maxPerFrame || c.shaders != in.shaders;
    return c;
}
constexpr bool shaderEnabled(unsigned mask,std::size_t index) { return index < shaderClasses.size() && ((mask >> index) & 1u) != 0; }
constexpr bool clipMode(unsigned seeThroughMode) { return seeThroughMode == 2; }
constexpr bool cellAllowed(bool interiorCell,bool exteriorAllowed) { return interiorCell || exteriorAllowed; }

// ---- published snapshot -----------------------------------------------------
constexpr float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
constexpr Vec minus(Vec a,Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
constexpr float minPlaneDepth = 64.0f;  // game units in front of the camera; nearer than this nothing is clipped
constexpr double staleSeconds = .25;    // a snapshot the camera service stopped renewing is not used
// Immutable copy published by the main thread after the camera update. The
// plane passes through anchor - margin * forward and faces the camera.
struct View {
    unsigned long long frame{};
    double seconds{};
    Vec eye{};         // rendered camera position (world)
    Vec forward{};     // unit view direction (world)
    Vec anchor{};      // player's feet + height
    float anchorDepth{}; // (anchor - eye) . forward
    float planeDepth{};  // anchorDepth - margin
    float corridor{};
    float feetZ{};
    float floorBand{};
};
inline std::optional<View> makeView(const Config& c,Vec eye,Vec forward,Vec feet,unsigned long long frame,double seconds) {
    const float length = std::sqrt(dot(forward,forward));
    if (!finite(eye) || !finite(feet) || !std::isfinite(length) || length < .5f || length > 2.0f || !std::isfinite(seconds)) return {};
    View v;
    v.frame = frame; v.seconds = seconds; v.eye = eye;
    v.forward = {forward.x/length,forward.y/length,forward.z/length};
    v.anchor = {feet.x,feet.y,feet.z+c.height};
    v.anchorDepth = dot(minus(v.anchor,eye),v.forward);
    v.planeDepth = v.anchorDepth-c.margin;
    v.corridor = c.corridor; v.feetZ = feet.z; v.floorBand = c.floorBand;
    if (!std::isfinite(v.planeDepth) || v.planeDepth < minPlaneDepth) return {};
    return v;
}

// ---- the plane as a depth interval -----------------------------------------
// Engine matrices are stored for row vectors: clip = (x,y,z,1) * M with the
// point relative to posAdjust, so clip z is column 2 and clip w is column 3
// (the world matrix builder at RVA 15170C0 puts the translation in row 3).
using Matrix = std::array<std::array<float,4>,4>;
enum : std::size_t { projectionMatrix, viewProjectionMatrix, viewProjectionUnjitteredMatrix, projectionUnjitteredMatrix, matrixCount };
constexpr std::array<std::size_t,matrixCount> matrixOffsets{offset::projection,offset::viewProjection,
    offset::viewProjectionUnjittered,offset::projectionUnjittered};
struct Camera {
    Vec posAdjust{},viewDir{};
    std::array<Matrix,matrixCount> matrices{};
};
// stale..unknownEpoch are expected and only counted; from gate on, the render
// state disagreed with the audit and the first occurrence is logged (FCS-49).
enum class Reject : unsigned { none, stale, otherView, tooNear, unknownEpoch, gate, drift, layout, published, depthRange, convention, bias, count };
constexpr std::array rejectNames{"none","stale","other-view","too-near","unknown-epoch","gate","drift","layout","published",
    "depth-range","convention","bias"};
static_assert(rejectNames.size() == static_cast<std::size_t>(Reject::count));
constexpr bool audited(Reject why) { return why >= Reject::gate && why < Reject::count; }
constexpr float sameView = .9995f;       // cosine between the state's view direction and the published one
constexpr float axisTolerance = .002f;
constexpr float publishedTolerance = 96.0f; // game units between the state's and the published anchor depth
constexpr double depthAgreement = 1e-5;
constexpr double minDepthRange = 1e-4;   // 1 - d0: what is left of the depth range behind the plane
struct Plan {
    Reject reject{Reject::unknownEpoch};
    Vec eye{},forward{};
    float depthOffset{}; // view depth of a point = (point - eye) . forward + depthOffset
    float planeDepth{};
    double d0{};  // depth-buffer value of the plane under the unmodified projection
    std::array<std::array<float,4>,matrixCount> columns{}; // replacement for column 2 of each matrix
};
inline bool finiteMatrix(const Matrix& m) {
    for (const auto& row : m) for (const float value : row) if (!std::isfinite(value)) return false;
    return true;
}
// Depth-buffer value (z/w) of the point on the matrix's w axis whose clip w is `depth`.
inline double depthAt(const Matrix& m,Vec axis,double depth) {
    const double x = axis.x,y = axis.y,z = axis.z;
    const double along = (depth-static_cast<double>(m[3][3]))/(x*x+y*y+z*z);
    return (along*(x*static_cast<double>(m[0][2])+y*static_cast<double>(m[1][2])+z*static_cast<double>(m[2][2]))+static_cast<double>(m[3][2]))/depth;
}
constexpr Vec wAxis(const Matrix& m) { return {m[0][3],m[1][3],m[2][3]}; }
// A draw is clipped by drawing it with z' = (z - d0*w) / (1 - d0) and the
// bottom of the viewport depth range raised to match (viewportMinDepth below):
// hardware near clipping (z' >= 0) removes what the unmodified projection puts
// nearer than d0, and every surviving fragment gets exactly the depth it would
// have had. x, y and w are untouched, so screen position, jitter and motion
// vectors are unchanged. This is the only plane a matrix can express without
// changing depth: perpendicular to the view direction.
inline Plan evaluate(const Camera& cam,const View& view,double now) {
    Plan plan;
    if (!(now-view.seconds <= staleSeconds) || !(now-view.seconds >= -staleSeconds)) { plan.reject = Reject::stale; return plan; }
    for (const auto& m : cam.matrices) if (!finiteMatrix(m)) { plan.reject = Reject::layout; return plan; }
    if (!finite(cam.posAdjust) || !finite(cam.viewDir)) { plan.reject = Reject::layout; return plan; }
    const Matrix& vp = cam.matrices[viewProjectionMatrix];
    const Vec axis = wAxis(vp);
    // A perspective view along the published direction: w = distance along the axis (+ a constant).
    if (std::abs(std::sqrt(dot(axis,axis))-1.0f) > axisTolerance || dot(axis,view.forward) < sameView) {
        plan.reject = Reject::otherView; return plan;
    }
    if (std::abs(axis.x-cam.viewDir.x) > axisTolerance || std::abs(axis.y-cam.viewDir.y) > axisTolerance ||
        std::abs(axis.z-cam.viewDir.z) > axisTolerance) { plan.reject = Reject::layout; return plan; }
    const float anchorDepth = dot(minus(view.anchor,cam.posAdjust),axis)+vp[3][3];
    if (!(std::abs(anchorDepth-view.anchorDepth) <= publishedTolerance)) { plan.reject = Reject::published; return plan; }
    const float planeDepth = anchorDepth-(view.anchorDepth-view.planeDepth);
    if (!(planeDepth >= minPlaneDepth)) { plan.reject = Reject::tooNear; return plan; }
    // Depth must grow with distance (0 near, 1 far) and all four matrices must agree on it.
    const double d0 = depthAt(vp,axis,planeDepth),further = depthAt(vp,axis,static_cast<double>(planeDepth)+64.0);
    if (!std::isfinite(d0) || !std::isfinite(further) || !(d0 > 0.0) || !(d0 < 1.0-minDepthRange) || !(further > d0)) {
        plan.reject = Reject::convention; return plan;
    }
    for (std::size_t i = 0;i < matrixCount;++i) {
        const Matrix& m = cam.matrices[i];
        const Vec own = wAxis(m);
        if (std::abs(std::sqrt(dot(own,own))-1.0f) > axisTolerance || !(std::abs(depthAt(m,own,planeDepth)-d0) <= depthAgreement)) {
            plan.reject = Reject::convention; return plan;
        }
    }
    const double scale = 1.0/(1.0-d0);
    for (std::size_t i = 0;i < matrixCount;++i)
        for (std::size_t row = 0;row < 4;++row)
            plan.columns[i][row] = static_cast<float>((static_cast<double>(cam.matrices[i][row][2])-d0*cam.matrices[i][row][3])*scale);
    plan.reject = Reject::none; plan.eye = cam.posAdjust; plan.forward = axis; plan.depthOffset = vp[3][3];
    plan.planeDepth = planeDepth; plan.d0 = d0;
    return plan;
}
inline Matrix clipped(Matrix m,const std::array<float,4>& column) {
    for (std::size_t row = 0;row < 4;++row) m[row][2] = column[row];
    return m;
}
// The depth range a draw gets is decided by the engine's flush (RVA
// 101106C..10110D0). With the depth-bias dirty bit set it rebuilds the range
// from cameraData.viewDepthRange and lowers the top by the bias of the current
// mode; otherwise the viewport keeps the range it has. The opaque pass runs
// with a top just under 1 (set at RVA 15377F0), so (0,1) is not assumed.
struct DepthRange { float low{},high{}; };
inline std::optional<DepthRange> flushedRange(bool biasDirty,float cameraMin,float cameraMax,float viewportMin,float viewportMax,float bias) {
    const DepthRange range = biasDirty ? DepthRange{cameraMin,cameraMax-bias} : DepthRange{viewportMin,viewportMax};
    if (!std::isfinite(bias) || bias < 0.0f || bias > .01f || !std::isfinite(range.low) || !std::isfinite(range.high) ||
        range.low < 0.0f || range.low >= range.high || range.high > 1.0f) return {};
    return range;
}
// Raising only the bottom of that range to low + (high - low) * d0 puts every
// fragment of the clipped draw exactly where the unclipped draw would have
// written it, for every range and bias.
inline std::optional<float> viewportMinDepth(double d0,DepthRange range) {
    if (!std::isfinite(d0) || !(d0 > 0.0) || !(d0 < 1.0)) return {};
    return static_cast<float>(static_cast<double>(range.low)+(static_cast<double>(range.high)-static_cast<double>(range.low))*d0);
}
inline std::optional<float> biasOf(unsigned mode,const float* table) {
    if (mode >= biasModes || !table) return {};
    return table[mode];
}
// Only undo our own write: the value is still bit for bit what we stored.
inline bool ownWrite(float current,float written) { return std::bit_cast<std::uint32_t>(current) == std::bit_cast<std::uint32_t>(written); }

// ---- selection --------------------------------------------------------------
enum class Skip : unsigned { none, skinned, noBound, behind, outside, ownerless, actor, kind, floor, shader, cap, count };
constexpr std::array skipNames{"none","skinned","no-bound","behind","outside","ownerless","actor","kind","floor","shader","cap"};
static_assert(skipNames.size() == static_cast<std::size_t>(Skip::count));
struct Bound { Vec centre{}; float radius{}; };
inline float segmentDistance(Vec point,Vec from,Vec to) {
    const Vec along = minus(to,from),offsetFrom = minus(point,from);
    const float length = dot(along,along);
    const float t = length > 0 ? std::clamp(dot(offsetFrom,along)/length,0.0f,1.0f) : 0.0f;
    return span(point,mix(from,to,t));
}
// Pointer-free part, evaluated for every pass of a hooked shader in the main
// view. A bound wholly at or behind the plane cannot lose a fragment, so the
// floor under and beyond the player, the far walls of the player's room and
// everything behind them are never selected; skinned geometry never is.
inline Skip reach(const Plan& plan,const View& view,Bound bound,bool skinned) {
    if (skinned) return Skip::skinned;
    if (!finite(bound.centre) || !std::isfinite(bound.radius) || bound.radius <= 0) return Skip::noBound;
    if (dot(minus(bound.centre,plan.eye),plan.forward)+plan.depthOffset-bound.radius >= plan.planeDepth) return Skip::behind;
    if (view.corridor > 0 && segmentDistance(bound.centre,plan.eye,view.anchor) > view.corridor+bound.radius) return Skip::outside;
    return Skip::none;
}
// Owner part, for the passes `reach` let through. Geometry without an owning
// reference (terrain, sky, water) is never clipped; actors, the player and
// everything they carry never are; lights and other excluded kinds never are;
// a reference wholly at or below the player's feet is floor and stays whole.
inline Skip ownerRule(bool owned,bool actor,Kind kind,const Extent& box,float feetZ,float floorBand) {
    if (!owned) return Skip::ownerless;
    if (actor) return Skip::actor;
    if (kind == Kind::excluded) return Skip::kind;
    if (box.valid && box.maxZ <= feetZ+floorBand) return Skip::floor;
    return Skip::none;
}
// The clip reaches a draw only through the per-frame buffer: its vertex shader
// must take all four rows of ViewProj (c8..c11 of cb12) from there. All 127
// vanilla Lighting vertex shaders do, 286 of 339 Utility and 599 of 606 Effect
// ones; the others position their vertices some other way and are left alone.
// `code` is a DXBC container; only its SHEX/SHDR token stream is scanned for
// constant-buffer operands with two immediate indices.
inline std::uint32_t word(const std::uint8_t* at) {
    std::uint32_t value{};
    std::memcpy(&value,at,sizeof(value));
    return value;
}
inline bool readsViewProjection(const std::uint8_t* code,std::size_t size) {
    constexpr std::size_t header = 32, largest = std::size_t{1} << 20;
    if (!code || size < header || size > largest || std::memcmp(code,"DXBC",4) != 0) return false;
    const std::size_t total = word(code+24),chunks = word(code+28);
    if (total > size || chunks > 32 || header+chunks*4 > total) return false;
    unsigned rows = 0;
    for (std::size_t i = 0;i < chunks;++i) {
        const std::size_t at = word(code+header+i*4);
        if (at > total || total-at < 16) continue;
        if (std::memcmp(code+at,"SHEX",4) != 0 && std::memcmp(code+at,"SHDR",4) != 0) continue;
        const std::size_t bytes = word(code+at+4);
        if (bytes > total-at-8) continue;
        const std::size_t tokens = (std::min)(static_cast<std::size_t>(word(code+at+12)),bytes/4);
        const std::uint8_t* stream = code+at+8;
        for (std::size_t k = 2;k+2 < tokens;++k) {
            const std::uint32_t operand = word(stream+k*4);
            if (((operand >> 12) & 0xffu) != 8u || ((operand >> 20) & 0xffu) != 2u || (operand >> 31) != 0u) continue;
            if (word(stream+(k+1)*4) != 12u) continue;
            const std::uint32_t row = word(stream+(k+2)*4);
            if (row >= 8u && row <= 11u) rows |= 1u << (row-8u);
        }
    }
    return rows == 0xfu;
}
struct Budget {
    unsigned limit{},used{},peak{};
    void frame() { used = 0; }
    bool take() {
        if (used >= limit) return false;
        if (++used > peak) peak = used;
        return true;
    }
};
// A clip is open from SetupGeometry to RestoreGeometry of one pass. Finding it
// still open anywhere else means a draw path the audit did not cover: it is
// closed at once and the mode stays native for the rest of the session.
enum class Fault : unsigned { setup = 1, upload = 2, frame = 3 };
constexpr const char* faultName(Fault where) {
    return where == Fault::setup ? "the next SetupGeometry" : where == Fault::upload ? "an engine per-frame upload" : "the next camera frame";
}

// ---- trace ------------------------------------------------------------------
struct Window {
    unsigned long long passes{},clipped{},uploads{},biased{},conflicts{};
    std::array<unsigned long long,shaderClasses.size()> byClass{}; // clipped passes per hooked shader class
    std::array<unsigned long long,static_cast<std::size_t>(Skip::count)> skips{};
    std::array<unsigned long long,static_cast<std::size_t>(Reject::count)> rejects{};
    double seconds{}; // spent applying and restoring
};
struct Sample {
    double seconds{};
    unsigned long long frame{},offMain{};
    bool installed{},active{},latched{},haveView{},havePlan{};
    View view{};
    Plan plan{};
    unsigned peak{},cap{};
    int depthTarget{-1}; // depth-stencil target index of the last clipped pass
    Window window{};
};
inline bool format(char* out,std::size_t size,const Sample& s) {
    const auto& w = s.window;
    const auto skip = [&](Skip which) { return w.skips[static_cast<std::size_t>(which)]; };
    const auto reject = [&](Reject which) { return w.rejects[static_cast<std::size_t>(which)]; };
    const double none = static_cast<double>(std::numeric_limits<float>::quiet_NaN());
    const auto of = [&](bool have,float value) { return have ? static_cast<double>(value) : none; };
    const int written = std::snprintf(out,size,
        "FixedClip t=%.3f frame=%llu installed=%u active=%u latched=%u eye=%.2f,%.2f,%.2f anchor=%.2f,%.2f,%.2f "
        "anchorDepth=%.2f planeDepth=%.2f statePlane=%.2f d0=%.7f target=%d passes=%llu clipped=%llu byClass=%llu,%llu,%llu peak=%u cap=%u "
        "uploads=%llu biased=%llu conflicts=%llu offMain=%llu micros=%.1f skipSkin=%llu skipBound=%llu skipBehind=%llu skipOut=%llu "
        "skipOwner=%llu skipActor=%llu skipKind=%llu skipFloor=%llu skipShader=%llu skipCap=%llu rejStale=%llu rejOther=%llu rejNear=%llu "
        "rejEpoch=%llu rejGate=%llu rejDrift=%llu rejLayout=%llu rejPub=%llu rejRange=%llu rejConv=%llu rejBias=%llu "
        "sample=window",
        s.seconds,s.frame,static_cast<unsigned>(s.installed),static_cast<unsigned>(s.active),static_cast<unsigned>(s.latched),
        of(s.haveView,s.view.eye.x),of(s.haveView,s.view.eye.y),of(s.haveView,s.view.eye.z),
        of(s.haveView,s.view.anchor.x),of(s.haveView,s.view.anchor.y),of(s.haveView,s.view.anchor.z),
        of(s.haveView,s.view.anchorDepth),of(s.haveView,s.view.planeDepth),
        of(s.havePlan,s.plan.planeDepth),s.havePlan ? s.plan.d0 : none,s.havePlan ? s.depthTarget : -1,w.passes,w.clipped,
        w.byClass[0],w.byClass[1],w.byClass[2],s.peak,s.cap,
        w.uploads,w.biased,w.conflicts,s.offMain,w.seconds*1e6,skip(Skip::skinned),skip(Skip::noBound),skip(Skip::behind),skip(Skip::outside),
        skip(Skip::ownerless),skip(Skip::actor),skip(Skip::kind),skip(Skip::floor),skip(Skip::shader),skip(Skip::cap),reject(Reject::stale),
        reject(Reject::otherView),reject(Reject::tooNear),reject(Reject::unknownEpoch),reject(Reject::gate),reject(Reject::drift),
        reject(Reject::layout),reject(Reject::published),reject(Reject::depthRange),reject(Reject::convention),reject(Reject::bias));
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed::clip
