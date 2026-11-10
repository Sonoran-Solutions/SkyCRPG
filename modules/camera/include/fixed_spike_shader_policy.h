// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// P5.5-02 wave 2 package "shader" (DYL-217). Disposable spike policy for
// [FixedCameraSpike] iSeeThroughMode=3: a world-space horizontal cut made in
// the vertex stage. A selected draw is drawn with a patched copy of its own
// vertex shader that also writes SV_ClipDistance = cut height - world height,
// so the rasteriser removes what lies above the plane in every pass of that
// draw, depth-only passes included; no pixel shader is touched. No engine
// headers, no engine pointers. The native adapter is src/fixed_spike_shader.h;
// the audit behind every number here is docs/evidence/p5.5/spike-wave2-shader.md.
#include "fixed_spike_policy.h"
#include "fixed_spike_shader_audit.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>

namespace skycrpg::camera::fixed::shader {
// ---- audited engine facts (SkyrimSE.exe 1.7.104) ----------------------------
struct Address { std::uint64_t id{}; std::uintptr_t rva{}; };
constexpr Address stateAddress{388819,0x20cfc00};   // BSGraphics::RendererShadowState
constexpr Address deviceAddress{411348,0x3330190};  // ID3D11Device* the engine creates its shaders and buffers with
constexpr Address contextAddress{411391,0x3331f30}; // ID3D11DeviceContext* every engine bind and draw goes through
// BSShader vtable slots 6 and 7: SetupGeometry(pass,flags) and
// RestoreGeometry(pass,flags). Every draw of a pass happens between the two
// (per-pass renderer RVA 1560340), and the technique's vertex shader is bound
// before the first of them (BeginTechnique, RVA 1579120).
constexpr std::size_t setupSlot = 6, restoreSlot = 7;
struct ShaderClass {
    const char* name{};
    Address table{};
    std::uintptr_t setupRva{},restoreRva{};
    std::array<std::uint8_t,16> setupBytes{},restoreBytes{};
};
constexpr std::array<ShaderClass,2> shaderClasses{{
    {"lighting",{255053,0x1b47420},0x1549550,0x154a900,
        {0x48,0x8b,0xc4,0x44,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55},
        {0x80,0x7a,0x1c,0x0a,0x75,0x3a,0x83,0x3d,0x83,0x53,0xb8,0x00,0x00,0x75,0x0c,0x81}},
    {"utility",{255302,0x1b4ab58},0x15673f0,0x15689c0,
        {0x48,0x8b,0xc4,0x48,0x89,0x48,0x08,0x41,0x56,0x48,0x81,0xec,0x40,0x01,0x00,0x00},
        {0x8b,0x81,0x90,0x00,0x00,0x00,0x45,0x33,0xc0,0x8b,0xc8,0x81,0xe1,0x00,0x12,0x00}}}};
namespace offset {
constexpr std::size_t currentVertexShader = 0x348, posAdjust = 0x35c, viewDirection = 0x3a0; // RendererShadowState
constexpr std::size_t passGeometry = 0x10;                                 // BSRenderPass
constexpr std::size_t objectParent = 0x30, objectBound = 0xe4, objectUser = 0xf8; // NiAVObject
constexpr std::size_t asFadeNode = 0x28;                                   // NiObject vtable: the engine reads the user only on fade nodes (RVA 2EED25)
constexpr std::size_t geometrySkin = 0x130;                                // BSGeometry
constexpr std::size_t shaderObject = 0x08, shaderCodeSize = 0x10, shaderCode = 0x68; // BSGraphics::VertexShader
}
// COM vtable byte offsets, each one as the engine itself calls it.
namespace method {
constexpr std::size_t createBuffer = 0x18, createVertexShader = 0x60; // ID3D11Device (RVA 1012884, 100FD8A)
constexpr std::size_t vsSetConstantBuffers = 0x38, vsSetShader = 0x58, map = 0x70, unmap = 0x78; // ID3D11DeviceContext (1009E9E, 100FF05, 101118A, 10111C0)
}
// D3D11_BUFFER_DESC as the engine fills it for its own dynamic constant
// buffers (RVA 1012841..1012884): usage 2, bind 4, CPU access 10000h.
struct BufferDescription { std::uint32_t bytes{},usage{2},bind{4},access{0x10000},misc{},stride{}; };
struct MappedBuffer { void* data{}; std::uint32_t rowPitch{},depthPitch{}; };
constexpr std::uint32_t mapWriteDiscard = 4;
constexpr unsigned maxOwnerHops = 32;
constexpr std::uint32_t ammoForm = 0x2a; // the engine looks past an ammunition reference for the actor carrying it (RVA 2EED5E)

// One code per refusal site; codes above 63 need their own once-mask.
enum class Refusal : unsigned { settings = 66, foreign = 67, address = 68, table = 69, device = 70, thread = 71, bracket = 72,
    create = 73, patch = 74, unaudited = 75, view = 76, map = 77, object = 78, context = 79 };
constexpr unsigned firstRefusal = 66, lastRefusal = 79;
constexpr std::uint32_t refusalBit(Refusal code) { return 1u << (static_cast<unsigned>(code)-firstRefusal); }

// ---- [FixedCameraSpike] shader-cut keys ------------------------------------
constexpr unsigned traceLines = 6000;
struct Config {
    float height{160.0f};       // cut plane above the player's feet
    float radius{0.0f};         // 0 = every piece in view; otherwise only pieces reaching within this ground distance of the feet
    unsigned maxPerFrame{4096}; // cut draws per frame
    unsigned shaders{3};        // bit 0 lighting, bit 1 utility (depth passes)
    bool trace{};
};
// The camera stands about 336 ground units from the feet, so a radius under
// 384 could leave a piece between camera and player whole.
constexpr float minRadius = 384.0f;
inline Config validate(Config c,bool* adjusted = nullptr) {
    const Config in = c;
    c.height = bounded(c.height,160.0f,64.0f,1024.0f);
    c.radius = !std::isfinite(c.radius) || c.radius == 0.0f ? 0.0f : std::clamp(c.radius,minRadius,8192.0f);
    c.maxPerFrame = std::clamp(c.maxPerFrame,1u,16384u);
    if (!c.shaders || c.shaders > 3u) c.shaders = 3;
    if (adjusted) *adjusted = c.height != in.height || c.radius != in.radius || c.maxPerFrame != in.maxPerFrame || c.shaders != in.shaders;
    return c;
}
constexpr bool shaderEnabled(unsigned mask,std::size_t index) { return index < shaderClasses.size() && ((mask >> index) & 1u) != 0; }
constexpr bool shaderMode(unsigned seeThroughMode) { return seeThroughMode == 3; }

// ---- published snapshot -----------------------------------------------------
constexpr float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
constexpr double staleSeconds = .25;   // a snapshot the camera service stopped renewing is not used
// Immutable copy published by the main thread after the camera update.
struct View {
    unsigned long long frame{};
    double seconds{};
    Vec eye{};      // rendered camera position (world)
    Vec forward{};  // unit view direction (world)
    Vec feet{};
    float planeZ{}; // world height above which selected draws are not drawn
    float radius{};
};
// `cutBase` is the held reference height of the feet (CutPlane), so a jump or
// one stair step does not move the plane.
inline std::optional<View> makeView(const Config& c,Vec eye,Vec forward,Vec feet,float cutBase,unsigned long long frame,double seconds) {
    const float length = std::sqrt(dot(forward,forward));
    if (!finite(eye) || !finite(feet) || !std::isfinite(cutBase) || !std::isfinite(length) || length < .5f || length > 2.0f ||
        !std::isfinite(seconds)) return {};
    View v;
    v.frame = frame; v.seconds = seconds; v.eye = eye;
    v.forward = {forward.x/length,forward.y/length,forward.z/length};
    v.feet = feet; v.planeZ = cutBase+c.height; v.radius = c.radius;
    if (!std::isfinite(v.planeZ)) return {};
    return v;
}
constexpr bool fresh(const View& view,double now) { return now-view.seconds <= staleSeconds && now-view.seconds >= -staleSeconds; }
// A pass belongs to the published view when the render state's eye (its
// position adjustment, the NiCamera's world translation) and view direction
// are the published ones. Shadow maps, reflections, the local map and the
// inventory scene set another camera and are never cut. The tolerance on the
// eye is the one the wave 1 room hook ran with in game.
constexpr float eyeTolerance = 96.0f, sameDirection = .99f, unitTolerance = .01f;
inline bool sameView(const View& view,Vec stateEye,Vec stateDirection) {
    if (!finite(stateEye) || !finite(stateDirection)) return false;
    if (std::abs(std::sqrt(dot(stateDirection,stateDirection))-1.0f) > unitTolerance) return false;
    return span(stateEye,view.eye) <= eyeTolerance && dot(stateDirection,view.forward) >= sameDirection;
}
// The patched shader computes clip = worldZ * scale + offset from the vertex's
// height relative to the position adjustment, and the rasteriser drops what is
// negative. All zero (the buffer not bound) never cuts.
struct Constant { float scale{},offset{},unusedZ{},unusedW{}; };
static_assert(sizeof(Constant) == 16);
constexpr Constant cutConstant(float planeZ,float posAdjustZ) { return {-1.0f,planeZ-posAdjustZ,0.0f,0.0f}; }
constexpr float clipDistance(Constant c,float relativeZ) { return relativeZ*c.scale+c.offset; }
inline bool usable(Constant c) { return c.scale == -1.0f && std::isfinite(c.offset); }

// ---- selection --------------------------------------------------------------
enum class Skip : unsigned { none, skinned, noBound, below, outside, ownerless, actor, kind, shader, cap, count };
constexpr std::array skipNames{"none","skinned","no-bound","below","outside","ownerless","actor","kind","shader","cap"};
static_assert(skipNames.size() == static_cast<std::size_t>(Skip::count));
struct Bound { Vec centre{}; float radius{}; };
// Pointer-free part, evaluated for every pass of a hooked shader in the
// published view. A bound that does not reach above the plane cannot lose a
// fragment, so floors and everything at or under head height are not even
// touched; skinned geometry never is.
inline Skip reach(const View& view,Bound bound,bool skinned) {
    if (skinned) return Skip::skinned;
    if (!finite(bound.centre) || !std::isfinite(bound.radius) || bound.radius <= 0) return Skip::noBound;
    if (bound.centre.z+bound.radius <= view.planeZ) return Skip::below;
    if (view.radius > 0 && groundReach(view.feet,bound.centre,bound.radius) > view.radius) return Skip::outside;
    return Skip::none;
}
// Owner part, for the passes `reach` let through. Geometry without an owning
// reference (terrain, sky, water) is never cut; actors, the player and what
// they carry never are; lights and the other kinds wave 1 excluded never are.
constexpr Skip ownerRule(bool owned,bool actor,Kind kind) {
    return !owned ? Skip::ownerless : actor ? Skip::actor : kind == Kind::excluded ? Skip::kind : Skip::none;
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
// A cut is open from SetupGeometry to RestoreGeometry of one pass. Finding it
// still open anywhere else means a draw path the audit did not cover: it is
// closed at once and the mode stays native for the rest of the session.
enum class Fault : unsigned { setup = 1, frame = 2, shaderChanged = 3 };
constexpr const char* faultName(Fault where) {
    return where == Fault::setup ? "the next SetupGeometry" : where == Fault::frame ? "the next camera frame" :
        "its RestoreGeometry with another vertex shader current";
}

// ---- DXBC -------------------------------------------------------------------
inline std::uint32_t word(const std::uint8_t* at) {
    std::uint32_t value{};
    std::memcpy(&value,at,sizeof(value));
    return value;
}
inline void store(std::uint8_t* at,std::uint32_t value) { std::memcpy(at,&value,sizeof(value)); }
// The container checksum is MD5's block function (RFC 1321) run over
// everything after the checksum field, with DXBC's own last block in place of
// MD5's padding. Checked against the stored checksum of every blob in the
// shipped shader archive (tests, when the archive is present).
namespace md5 {
constexpr std::array<std::uint32_t,64> sines{
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
constexpr std::array<int,16> turns{7,12,17,22,5,9,14,20,4,11,16,23,6,10,15,21};
inline void block(std::array<std::uint32_t,4>& state,const std::uint8_t* data) {
    std::array<std::uint32_t,16> m{};
    for (std::size_t i = 0;i < m.size();++i) m[i] = word(data+i*4u);
    std::uint32_t a = state[0],b = state[1],c = state[2],d = state[3];
    for (std::uint32_t i = 0;i < 64u;++i) {
        std::uint32_t f{},g{};
        if (i < 16u) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32u) { f = (d & b) | (~d & c); g = (5u*i+1u)%16u; }
        else if (i < 48u) { f = b ^ c ^ d; g = (3u*i+5u)%16u; }
        else { f = c ^ (b | ~d); g = (7u*i)%16u; }
        f += a+sines[i]+m[g];
        a = d; d = c; c = b;
        b += std::rotl(f,turns[(i/16u)*4u+i%4u]);
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}
}
constexpr std::size_t containerHeader = 32, checksumAt = 4, checksumEnd = 20, largestBlob = std::size_t{1} << 20;
inline std::array<std::uint32_t,4> checksum(const std::uint8_t* blob,std::size_t size) {
    std::array<std::uint32_t,4> state{0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u};
    if (!blob || size < checksumEnd) return state;
    const std::uint8_t* data = blob+checksumEnd;
    const std::size_t length = size-checksumEnd,whole = length/64u*64u,left = length-whole;
    for (std::size_t at = 0;at < whole;at += 64u) md5::block(state,data+at);
    const std::uint32_t bits = static_cast<std::uint32_t>(length*8u),last = (bits >> 2) | 1u;
    std::array<std::uint8_t,64> tail{};
    if (left >= 56u) {
        std::memcpy(tail.data(),data+whole,left);
        tail[left] = 0x80;
        md5::block(state,tail.data());
        tail = {};
        store(tail.data(),bits);
    } else {
        store(tail.data(),bits);
        std::memcpy(tail.data()+4u,data+whole,left);
        tail[4u+left] = 0x80;
    }
    store(tail.data()+60u,last);
    md5::block(state,tail.data());
    return state;
}
inline bool checksumMatches(const std::uint8_t* blob,std::size_t size) {
    if (!blob || size < containerHeader) return false;
    const auto sum = checksum(blob,size);
    for (std::size_t i = 0;i < sum.size();++i) if (word(blob+checksumAt+i*4u) != sum[i]) return false;
    return true;
}
// Identity of a vertex shader's bytecode (FNV-1a, 64 bit). Only bytecode on
// the audited list (fixed_spike_shader_audit.h) is ever patched.
inline std::uint64_t codeHash(const std::uint8_t* code,std::size_t size) {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i = 0;i < size;++i) { hash ^= code[i]; hash *= 1099511628211ull; }
    return hash;
}
inline bool audited(std::uint64_t hash) { return std::binary_search(auditedVertexShaders.begin(),auditedVertexShaders.end(),hash); }

// What the patch adds, and where it may go. The shipped vertex shaders declare
// constant buffers 0, 1, 2, 7-12 at most and the engine binds no other vertex
// slot; every Lighting and Utility pixel shader takes its inputs from
// registers 0-15.
constexpr std::uint32_t cutBuffer = 13, cutRegister = 16;
constexpr std::uint32_t perFrameBuffer = 12, perGeometryBuffer = 2, firstViewProjectionRow = 8, lastViewProjectionRow = 11;
constexpr std::uint32_t vertexShader50 = 0x00010050u, mostTemps = 4096;
namespace op {
constexpr std::uint32_t add = 0, call = 4, callc = 5, dp4 = 17, endif_ = 21, endloop = 22, endswitch = 23, if_ = 31, label = 44, loop = 48;
constexpr std::uint32_t customData = 53, mov = 54, mul = 56, ret = 62, retc = 63, switch_ = 76;
constexpr std::uint32_t firstDeclaration = 88, constantBuffer = 89, output = 101, outputSgv = 102, outputSiv = 103, temps = 104, lastDeclaration = 106;
constexpr std::uint32_t last = 142; // swapc; everything above is a Shader Model 5 declaration no vertex shader here has
}
namespace token {
constexpr std::uint32_t precise = 0x00780000u;     // per-component "precise" bits of an instruction token
constexpr std::uint32_t tempAll = 0x001000f2u;     // rN.xyzw written
constexpr std::uint32_t tempX = 0x00100012u;       // rN.x written
constexpr std::uint32_t outputX = 0x00102012u;     // oN.x written
constexpr std::uint32_t tempRead = 0x00100e46u;    // rN.xyzw read
constexpr std::uint32_t tempReadX = 0x0010000au;   // rN.x read as a scalar
constexpr std::uint32_t row = 0x00208e46u;         // cbS[row].xyzw
constexpr std::uint32_t rowZ = 0x00208aa6u;        // cbS[row].zzzz
constexpr std::uint32_t rowX = 0x0020800au, rowY = 0x0020801au; // cbS[row].x and .y read as scalars
constexpr std::uint32_t mulRows = 0x09000038u, addTemps = 0x07000000u, dot4Temps = 0x07000011u; // matched without the precise bits
constexpr std::uint32_t dot4Row = 0x08000011u, madScalars = 0x0b000032u;
constexpr std::uint32_t declareBuffer = 0x04000059u, declareOutputValue = 0x04000067u, declareTemps = 0x02000068u;
constexpr std::uint32_t outputOperand = 0x00102000u, outputOperandMask = 0xfffff000u; // an output register with one immediate index
constexpr std::uint32_t written = 0x00100002u, writtenMask = 0xffffdf0fu;            // a temp or output written through a mask
constexpr std::uint32_t clipDistanceName = 2;      // D3D10_SB_NAME_CLIP_DISTANCE, also D3D_NAME_CLIP_DISTANCE in the signature
}
enum class Patch : unsigned { ok, container, checksum, chunks, version, skinned, signature, declarations, pattern, flow, count };
constexpr std::array patchNames{"ok","container","checksum","chunks","version","skinned","signature","declarations","pattern","flow"};
static_assert(patchNames.size() == static_cast<std::size_t>(Patch::count));
struct Patched {
    Patch result{Patch::container};
    std::uint32_t worldRow{},position{},temp{}; // cb2 row holding World's z row, register holding the model position, the temp added
    std::vector<std::uint8_t> code;
};
struct Chunk { std::size_t at{},size{}; }; // payload offset and length
struct Instruction { std::size_t at{}; std::uint32_t code{},length{}; };
inline bool decode(const std::vector<std::uint32_t>& t,std::size_t at,Instruction& out) {
    if (at >= t.size()) return false;
    out.at = at; out.code = t[at] & 0x7ffu;
    out.length = out.code == op::customData ? (at+1u < t.size() ? t[at+1u] : 0u) : (t[at] >> 24) & 0x7fu;
    return out.length != 0 && out.length <= t.size()-at;
}
constexpr bool declaration(std::uint32_t code) { return (code >= op::firstDeclaration && code <= op::lastDeclaration) || code == op::customData; }
// Where the additions go, found in one pass over the token stream.
struct Scan {
    Patch result{Patch::declarations};
    std::size_t afterBuffers{},afterOutputs{},temps{},insert{};
    std::uint32_t tempCount{},worldRow{},position{};
};
// Every shipped rigid vertex shader that positions its vertices with the
// per-frame view-projection builds each row of World-View-Projection as
//   mul rA, cb12[8+r].xxxx, cb2[k]   mul rB, cb12[8+r].yyyy, cb2[k+1]   add
//   mul rB, cb12[8+r].zzzz, cb2[k+2] add   mul rB, cb12[8+r].wwww, ...  add rW
// and then takes dp4 rW, rP with the model position rP. The row multiplied by
// the .zzzz column is World's z row, so world height (relative to the position
// adjustment) is dp4 cb2[k+2], rP. The two added instructions go immediately
// before that first dp4, where rP is known to hold the position.
inline Scan scan(const std::vector<std::uint32_t>& t) {
    Scan s;
    const auto fail = [&](Patch why) { s.result = why; return s; };
    bool code = false,haveTemps = false,rowKnown = false,armed = false,found = false,haveSum = false;
    std::uint32_t depth = 0,geometryRows = 0,sum = 0;
    Instruction ins;
    for (std::size_t at = 2;at < t.size();at += ins.length) {
        if (!decode(t,at,ins) || ins.code > op::last) return fail(Patch::declarations);
        if (declaration(ins.code)) {
            if (code) return fail(Patch::declarations);
            if (ins.code == op::constantBuffer) {
                if (ins.length != 4u || t[at+1u] != token::row || t[at+2u] == cutBuffer) return fail(Patch::declarations);
                if (t[at+2u] == perGeometryBuffer) geometryRows = t[at+3u];
                s.afterBuffers = at+ins.length;
            } else if (ins.code == op::output || ins.code == op::outputSgv || ins.code == op::outputSiv) {
                if (ins.length < 3u || (t[at+1u] & token::outputOperandMask) != token::outputOperand || t[at+2u] >= cutRegister)
                    return fail(Patch::declarations);
                s.afterOutputs = at+ins.length;
            } else if (ins.code == op::temps) {
                if (haveTemps || ins.length != 2u) return fail(Patch::declarations);
                haveTemps = true; s.temps = at; s.tempCount = t[at+1u];
            }
            continue;
        }
        code = true;
        if (ins.code == op::if_ || ins.code == op::loop || ins.code == op::switch_) ++depth;
        else if (ins.code == op::endif_ || ins.code == op::endloop || ins.code == op::endswitch) { if (!depth) return fail(Patch::flow); --depth; }
        else if (ins.code == op::call || ins.code == op::callc || ins.code == op::label) return fail(Patch::flow);
        else if ((ins.code == op::ret || ins.code == op::retc) && !found) return fail(Patch::pattern);
        const std::uint32_t head = t[at] & ~token::precise;
        if (head == token::mulRows && ins.length == 9u && t[at+1u] == token::tempAll) {
            for (const std::size_t view : {at+3u,at+6u}) {
                const std::size_t world = view == at+3u ? at+6u : at+3u;
                if (t[view] != token::rowZ || t[view+1u] != perFrameBuffer || t[view+2u] < firstViewProjectionRow ||
                    t[view+2u] > lastViewProjectionRow || t[world] != token::row || t[world+1u] != perGeometryBuffer) continue;
                if (rowKnown && s.worldRow != t[world+2u]) return fail(Patch::pattern);
                if (!rowKnown) { rowKnown = armed = true; s.worldRow = t[world+2u]; }
            }
        }
        if (!armed || found) continue;
        if (head == token::addTemps && ins.length == 7u && t[at+1u] == token::tempAll) { haveSum = true; sum = t[at+2u]; }
        else if (head == token::dot4Temps && ins.length == 7u && (t[at+1u] & token::writtenMask) == token::written &&
            t[at+3u] == token::tempRead && t[at+5u] == token::tempRead) {
            const std::uint32_t first = t[at+4u],second = t[at+6u];
            if (!haveSum || first == second || (first != sum && second != sum)) return fail(Patch::pattern);
            if (depth) return fail(Patch::flow);
            s.position = first == sum ? second : first; s.insert = at; found = true;
        } else if (ins.code != op::mul && ins.code != op::mov && ins.code != op::add) return fail(Patch::pattern);
    }
    if (!code || !found) return fail(Patch::pattern);
    if (depth) return fail(Patch::flow);
    if (!haveTemps || !s.afterBuffers || !s.afterOutputs || s.tempCount >= mostTemps || s.position >= s.tempCount ||
        s.worldRow >= geometryRows) return fail(Patch::declarations);
    s.result = Patch::ok;
    return s;
}
// Signature chunk: count, 8, then 24-byte elements (name offset, semantic
// index, system value, component type, register, mask, unused mask), then the
// names. Offsets are relative to the chunk payload.
constexpr std::size_t signatureHeader = 8, elementSize = 24;
constexpr const char clipSemantic[16] = "SV_ClipDistance";
inline bool signatureCount(const std::uint8_t* chunk,std::size_t size,std::size_t& count) {
    if (size < signatureHeader || size%4u) return false;
    count = word(chunk);
    return word(chunk+4u) == signatureHeader && count <= 32u && signatureHeader+count*elementSize <= size;
}
inline bool named(const std::uint8_t* chunk,std::size_t size,std::size_t element,const char* name) {
    const std::size_t at = word(chunk+signatureHeader+element*elementSize),length = std::strlen(name)+1u;
    return at <= size && size-at >= length && std::memcmp(chunk+at,name,length) == 0;
}
// A patched copy of `code`, or the reason there is none. Everything is
// checked before anything is built: exactly the three chunks the shipped
// shaders have, a matching checksum, vs_5_0, no skinning inputs, no clip or
// cull distance already, room in the registers, and the one arithmetic shape
// described above. The copy differs only by: constant buffer 13 declared (one
// row), output register 16.x declared and listed as SV_ClipDistance0, one more
// temp, and two instructions before the first World-View-Projection product.
inline Patched patchVertexShader(const std::uint8_t* code,std::size_t size) {
    Patched out;
    constexpr std::size_t chunkCount = 3,firstChunk = containerHeader+chunkCount*4u;
    if (!code || size < firstChunk || size > largestBlob || std::memcmp(code,"DXBC",4) != 0 || word(code+20u) != 1u ||
        word(code+24u) != size || word(code+28u) != chunkCount) return out;
    if (!checksumMatches(code,size)) { out.result = Patch::checksum; return out; }
    out.result = Patch::chunks;
    constexpr std::array<const char*,chunkCount> tags{"ISGN","OSGN","SHEX"};
    std::array<Chunk,chunkCount> chunks{};
    std::size_t expected = firstChunk;
    for (std::size_t i = 0;i < chunkCount;++i) {
        const std::size_t at = word(code+containerHeader+i*4u);
        if (at != expected || size-at < 8u || std::memcmp(code+at,tags[i],4) != 0) return out;
        chunks[i] = {at+8u,word(code+at+4u)};
        if (chunks[i].size%4u || chunks[i].size > size-chunks[i].at) return out;
        expected = chunks[i].at+chunks[i].size;
    }
    if (expected != size) return out;
    const auto& [inputs,outputs,body] = chunks;
    if (body.size < 8u || word(code+body.at) != vertexShader50 || word(code+body.at+4u) != body.size/4u) { out.result = Patch::version; return out; }
    std::size_t inputCount{},outputCount{};
    out.result = Patch::signature;
    if (!signatureCount(code+inputs.at,inputs.size,inputCount) || !signatureCount(code+outputs.at,outputs.size,outputCount) ||
        !outputCount || outputCount >= 32u) return out;
    for (std::size_t i = 0;i < inputCount;++i)
        if (named(code+inputs.at,inputs.size,i,"BLENDWEIGHT") || named(code+inputs.at,inputs.size,i,"BLENDINDICES")) { out.result = Patch::skinned; return out; }
    const std::size_t names = signatureHeader+outputCount*elementSize;
    for (std::size_t i = 0;i < outputCount;++i) {
        const std::uint8_t* element = code+outputs.at+signatureHeader+i*elementSize;
        const std::uint32_t name = word(element),systemValue = word(element+8u),reg = word(element+16u);
        if (name < names || name >= outputs.size || systemValue == token::clipDistanceName || systemValue == token::clipDistanceName+1u ||
            reg >= cutRegister) return out;
    }
    std::vector<std::uint32_t> t(body.size/4u);
    std::memcpy(t.data(),code+body.at,body.size);
    const Scan plan = scan(t);
    if (plan.result != Patch::ok) { out.result = plan.result; return out; }
    out.worldRow = plan.worldRow; out.position = plan.position; out.temp = plan.tempCount;
    // The token stream with the additions.
    std::vector<std::uint32_t> body2;
    body2.reserve(t.size()+27u);
    body2.push_back(t[0]); body2.push_back(0u);
    Instruction ins;
    for (std::size_t at = 2;at < t.size();at += ins.length) {
        static_cast<void>(decode(t,at,ins)); // the scan accepted every instruction
        if (at == plan.insert) {
            // dp4 rT.x, cb2[worldRow].xyzw, rP.xyzw
            for (const std::uint32_t value : {token::dot4Row,token::tempX,plan.tempCount,token::row,perGeometryBuffer,plan.worldRow,token::tempRead,plan.position})
                body2.push_back(value);
            // mad o16.x, rT.x, cb13[0].x, cb13[0].y
            for (const std::uint32_t value : {token::madScalars,token::outputX,cutRegister,token::tempReadX,plan.tempCount,token::rowX,cutBuffer,0u,token::rowY,cutBuffer,0u})
                body2.push_back(value);
        }
        body2.insert(body2.end(),t.begin()+static_cast<std::ptrdiff_t>(at),t.begin()+static_cast<std::ptrdiff_t>(at+ins.length));
        if (at == plan.temps) body2.back() = plan.tempCount+1u;
        if (at+ins.length == plan.afterBuffers)
            for (const std::uint32_t value : {token::declareBuffer,token::row,cutBuffer,1u}) body2.push_back(value);
        if (at+ins.length == plan.afterOutputs)
            for (const std::uint32_t value : {token::declareOutputValue,token::outputX,cutRegister,token::clipDistanceName}) body2.push_back(value);
    }
    body2[1] = static_cast<std::uint32_t>(body2.size());
    // The output signature with one more element; its name goes after the old names.
    const std::size_t oldNames = outputs.size-names,outputs2 = outputs.size+elementSize+sizeof(clipSemantic);
    const std::size_t body2Bytes = body2.size()*4u,total = firstChunk+8u+inputs.size+8u+outputs2+8u+body2Bytes;
    out.code.assign(total,std::uint8_t{0});
    std::uint8_t* p = out.code.data();
    std::memcpy(p,"DXBC",4);
    store(p+20u,1u); store(p+24u,static_cast<std::uint32_t>(total)); store(p+28u,static_cast<std::uint32_t>(chunkCount));
    std::size_t at = firstChunk;
    store(p+containerHeader,static_cast<std::uint32_t>(at));
    std::memcpy(p+at,code+inputs.at-8u,8u+inputs.size);
    at += 8u+inputs.size;
    store(p+containerHeader+4u,static_cast<std::uint32_t>(at));
    std::memcpy(p+at,"OSGN",4); store(p+at+4u,static_cast<std::uint32_t>(outputs2));
    std::uint8_t* sig = p+at+8u;
    store(sig,static_cast<std::uint32_t>(outputCount+1u)); store(sig+4u,static_cast<std::uint32_t>(signatureHeader));
    std::memcpy(sig+signatureHeader,code+outputs.at+signatureHeader,outputCount*elementSize);
    for (std::size_t i = 0;i < outputCount;++i) {
        std::uint8_t* element = sig+signatureHeader+i*elementSize;
        store(element,word(element)+static_cast<std::uint32_t>(elementSize));
    }
    std::uint8_t* added = sig+names;
    store(added,static_cast<std::uint32_t>(names+elementSize+oldNames)); // name
    store(added+4u,0u);                       // semantic index
    store(added+8u,token::clipDistanceName);  // system value
    store(added+12u,3u);                      // float32
    store(added+16u,cutRegister);
    added[20] = 0x1; added[21] = 0xe;         // .x used, .yzw never written
    std::memcpy(added+elementSize,code+outputs.at+names,oldNames);
    std::memcpy(added+elementSize+oldNames,clipSemantic,sizeof(clipSemantic));
    at += 8u+outputs2;
    store(p+containerHeader+8u,static_cast<std::uint32_t>(at));
    std::memcpy(p+at,"SHEX",4); store(p+at+4u,static_cast<std::uint32_t>(body2Bytes));
    std::memcpy(p+at+8u,body2.data(),body2Bytes);
    const auto sum = checksum(p,total);
    for (std::size_t i = 0;i < sum.size();++i) store(p+checksumAt+i*4u,sum[i]);
    out.result = Patch::ok;
    return out;
}

// ---- trace ------------------------------------------------------------------
struct Window {
    unsigned long long passes{},cut{},uploads{},otherView{},stale{},created{},unbalanced{};
    std::array<unsigned long long,shaderClasses.size()> byClass{}; // cut passes per hooked shader class
    std::array<unsigned long long,static_cast<std::size_t>(Skip::count)> skips{};
    double seconds{}; // spent selecting, binding and restoring
};
struct Sample {
    double seconds{};
    unsigned long long frame{},offMain{};
    bool installed{},active{},latched{},haveView{},haveOther{};
    View view{};
    Vec otherEye{},otherDirection{}; // render state of the last pass that was not the published view
    float offset{};                  // last constant uploaded: plane height relative to the position adjustment
    unsigned peak{},cap{},patched{},rejected{},unaudited{};
    Window window{};
};
inline bool format(char* out,std::size_t size,const Sample& s) {
    const auto& w = s.window;
    const auto skip = [&](Skip which) { return w.skips[static_cast<std::size_t>(which)]; };
    const double none = static_cast<double>(std::numeric_limits<float>::quiet_NaN());
    const auto of = [&](bool have,float value) { return have ? static_cast<double>(value) : none; };
    const int written = std::snprintf(out,size,
        "FixedShader t=%.3f frame=%llu installed=%u active=%u latched=%u eye=%.1f,%.1f,%.1f feet=%.1f,%.1f,%.1f planeZ=%.2f radius=%.1f "
        "relPlane=%.2f passes=%llu cut=%llu byClass=%llu,%llu peak=%u cap=%u uploads=%llu shaders=%u,%u,%u created=%llu open=%llu "
        "offMain=%llu micros=%.1f otherView=%llu otherEye=%.1f,%.1f,%.1f otherDir=%.2f,%.2f,%.2f stale=%llu skipSkin=%llu "
        "skipBound=%llu skipBelow=%llu skipOut=%llu skipOwner=%llu skipActor=%llu skipKind=%llu skipShader=%llu skipCap=%llu sample=window",
        s.seconds,s.frame,static_cast<unsigned>(s.installed),static_cast<unsigned>(s.active),static_cast<unsigned>(s.latched),
        of(s.haveView,s.view.eye.x),of(s.haveView,s.view.eye.y),of(s.haveView,s.view.eye.z),
        of(s.haveView,s.view.feet.x),of(s.haveView,s.view.feet.y),of(s.haveView,s.view.feet.z),
        of(s.haveView,s.view.planeZ),of(s.haveView,s.view.radius),of(w.cut != 0,s.offset),w.passes,w.cut,w.byClass[0],w.byClass[1],
        s.peak,s.cap,w.uploads,s.patched,s.rejected,s.unaudited,w.created,w.unbalanced,s.offMain,w.seconds*1e6,w.otherView,
        of(s.haveOther,s.otherEye.x),of(s.haveOther,s.otherEye.y),of(s.haveOther,s.otherEye.z),
        of(s.haveOther,s.otherDirection.x),of(s.haveOther,s.otherDirection.y),of(s.haveOther,s.otherDirection.z),w.stale,
        skip(Skip::skinned),skip(Skip::noBound),skip(Skip::below),skip(Skip::outside),skip(Skip::ownerless),skip(Skip::actor),
        skip(Skip::kind),skip(Skip::shader),skip(Skip::cap));
    return written > 0 && static_cast<std::size_t>(written) < size;
}
} // namespace skycrpg::camera::fixed::shader
