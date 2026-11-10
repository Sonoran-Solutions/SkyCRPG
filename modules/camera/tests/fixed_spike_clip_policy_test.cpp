// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "fixed_spike_clip_policy.h"
#include <cassert>
#include <set>
#include <string>
#include <vector>
using namespace skycrpg::camera;
using namespace skycrpg::camera::fixed::clip;
using fixed::Extent;
using fixed::Kind;
using fixed::TraceRow;
using fixed::Vec;
using fixed::hideScanned;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
bool close(double a,double b,double tolerance) { return std::abs(a-b) <= tolerance; }
Vec cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec unit(Vec v) { const float length = std::sqrt(dot(v,v)); return {v.x/length,v.y/length,v.z/length}; }
Vec add(Vec a,Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec times(Vec v,float s) { return {v.x*s,v.y*s,v.z*s}; }
Matrix multiply(const Matrix& a,const Matrix& b) {
    Matrix out{};
    for (std::size_t i = 0;i < 4;++i) for (std::size_t j = 0;j < 4;++j) for (std::size_t k = 0;k < 4;++k) out[i][j] += a[i][k]*b[k][j];
    return out;
}
// Row-vector matrices, as the engine stores them. The view matrix takes a point
// relative to the eye to (right, up, depth); the eye itself is the origin.
Matrix viewMatrix(Vec forward) {
    const Vec right = unit(cross(forward,{0,0,1})),up = cross(right,forward);
    return {{{right.x,up.x,forward.x,0},{right.y,up.y,forward.y,0},{right.z,up.z,forward.z,0},{0,0,0,1}}};
}
Matrix perspective(float near,float far,float jitterX = 0,float jitterY = 0) {
    const float a = far/(far-near);
    return {{{1.2f,0,0,0},{0,2.1f,0,0},{jitterX,jitterY,a,1},{0,0,-near*a,0}}};
}
Matrix reversed(float near,float far) {
    const float a = -near/(far-near);
    return {{{1.2f,0,0,0},{0,2.1f,0,0},{0,0,a,1},{0,0,far*near/(far-near),0}}};
}
Matrix orthographic() { return {{{.001f,0,0,0},{0,.001f,0,0},{0,0,.0001f,0},{0,0,.1f,1}}}; }
struct Clip { double x{},y{},z{},w{}; };
Clip project(const Matrix& m,Vec p) {
    const std::array<double,4> v{p.x,p.y,p.z,1.0};
    std::array<double,4> out{};
    for (std::size_t column = 0;column < 4;++column) for (std::size_t row = 0;row < 4;++row) out[column] += v[row]*m[row][column];
    return {out[0],out[1],out[2],out[3]};
}
// The wave 1 rig: camera offset (237.3, 237.3, 683.8) from the player at heading 225, pitch 55.
const Vec feet{2048,-9216,-2556};
const Vec eye{feet.x+237.3f,feet.y+237.3f,feet.z+683.8f};
const float pitch = radians(55.0f),heading = radians(225.0f);
const Vec forward{std::sin(heading)*std::cos(pitch),std::cos(heading)*std::cos(pitch),-std::sin(pitch)};
Camera cameraAt(Vec at,Vec facing,const Matrix& projection,const Matrix& unjittered) {
    Camera cam;
    cam.posAdjust = at; cam.viewDir = facing;
    const Matrix view = viewMatrix(facing);
    cam.matrices[projectionMatrix] = projection;
    cam.matrices[viewProjectionMatrix] = multiply(view,projection);
    cam.matrices[viewProjectionUnjitteredMatrix] = multiply(view,unjittered);
    cam.matrices[projectionUnjitteredMatrix] = unjittered;
    return cam;
}
Camera mainCamera() { return cameraAt(eye,forward,perspective(15.0f,353840.0f,.0007f,-.0004f),perspective(15.0f,353840.0f)); }
View rigView(const Config& c = {}) { return *makeView(c,eye,forward,feet,7,100.0); }

void audit() {
    // Codes stay inside the brief's range and off the two wave 1 snap codes.
    const std::array codes{Refusal::settings,Refusal::module,Refusal::address,Refusal::table,Refusal::prologue,Refusal::thread,
        Refusal::bracket,Refusal::state};
    std::set<unsigned> seen;
    for (const auto code : codes) {
        const auto value = static_cast<unsigned>(code);
        assert(value >= 42 && value <= 49 && seen.insert(value).second);
    }
    assert(seen.size() == 8);
    // The 14 copied bytes end on an instruction boundary: sub rsp,320h follows.
    assert(uploadCopied == 14 && uploadPrologue[14] == 0x48 && uploadPrologue[15] == 0x81 && uploadPrologue[16] == 0xec);
    assert(uploadExtent.begin == uploadAddress.rva && uploadExtent.holds(uploadAddress.rva+uploadCopied));
    assert(setupSlot == 6 && restoreSlot == 7 && biasModes == 12 && dirtyViewport == 2);
    std::set<std::uintptr_t> addresses;
    for (const auto& c : shaderClasses) {
        assert(c.setupRva != c.restoreRva && c.setupBytes != c.restoreBytes);
        assert(addresses.insert(c.table.rva).second && addresses.insert(c.setupRva).second && addresses.insert(c.restoreRva).second);
        assert(c.table.rva >= 0x17c8000 && c.setupRva < 0x17c8000 && c.restoreRva < 0x17c8000); // .rdata table, .text functions
    }
    assert(std::string(shaderClasses[0].name) == "lighting" && std::string(shaderClasses[1].name) == "utility");
    // Offsets: z column of each matrix lies inside what the drift check compares.
    for (const auto at : matrixOffsets)
        assert((at >= offset::uploadedFirst && at+64 <= offset::uploadedFirst+offset::uploadedSize) ||
            (at >= offset::uploadedSecond && at+64 <= offset::uploadedSecond+offset::uploadedSize));
    assert(offset::depthRange+8 <= offset::cameraSize && offset::camera+offset::cameraSize == 0x5d0);
    assert(sizeof(Matrix) == 64 && sizeof(Vec) == 12);
}
void settings() {
    bool adjusted = true;
    const Config defaults = validate(Config{},&adjusted);
    assert(!adjusted && defaults.height == 128.0f && defaults.margin == 24.0f && defaults.corridor == 384.0f && defaults.floorBand == 24.0f);
    assert(defaults.maxPerFrame == 512 && defaults.shaders == 3 && !defaults.exterior && !defaults.trace);
    Config c;
    c.height = nan; c.margin = -5; c.corridor = 1e9f; c.floorBand = 500; c.maxPerFrame = 0; c.shaders = 9;
    c = validate(c,&adjusted);
    assert(adjusted && c.height == 128.0f && c.margin == 0.0f && c.corridor == 4096.0f && c.floorBand == 128.0f && c.maxPerFrame == 1 && c.shaders == 3);
    c = {}; c.shaders = 0; assert(validate(c).shaders == 3);
    c = {}; c.maxPerFrame = 100000; assert(validate(c).maxPerFrame == 4096);
    assert(shaderEnabled(3,0) && shaderEnabled(3,1) && !shaderEnabled(3,2) && shaderEnabled(4,2) && !shaderEnabled(7,3));
    assert(clipMode(2) && !clipMode(0) && !clipMode(1) && !clipMode(3));
    assert(cellAllowed(true,false) && !cellAllowed(false,false) && cellAllowed(false,true));
    // Mode 2 survives the spike's validation and leaves the hide scan off.
    const Settings base;
    fixed::Config spike;
    spike.seeThroughMode = 2;
    assert(fixed::validate(spike,base,48).seeThroughMode == 2 && !hideScanned(2,true));
    spike.seeThroughMode = 3;
    assert(fixed::validate(spike,base,48).seeThroughMode == 0);
}
void views() {
    const View v = rigView();
    assert(v.frame == 7 && v.seconds == 100.0 && close(dot(v.forward,v.forward),1.0,1e-5));
    assert(close(v.anchor.z,feet.z+128.0f,1e-3) && close(v.planeDepth,v.anchorDepth-24.0f,1e-3) && v.feetZ == feet.z);
    // Camera 761.7 from the feet; the head is 0.819 * 128 nearer in view depth.
    assert(close(v.anchorDepth,dot(minus(feet,eye),forward)-128.0f*std::sin(pitch),.05));
    assert(v.anchorDepth > 600.0f && v.anchorDepth < 700.0f);
    // An unnormalised forward is normalised; nonsense is refused.
    const auto doubled = makeView({},eye,times(forward,1.5f),feet,1,0.0);
    assert(doubled && close(doubled->planeDepth,v.planeDepth,1e-2));
    assert(!makeView({},eye,{0,0,0},feet,1,0.0) && !makeView({},{nan,0,0},forward,feet,1,0.0) && !makeView({},eye,forward,{0,nan,0},1,0.0));
    assert(!makeView({},eye,forward,feet,1,std::numeric_limits<double>::infinity()));
    // A camera closer than the minimum plane depth publishes nothing.
    assert(!makeView({},add(feet,{0,0,200}),{0,0,-1},feet,1,0.0));
    Config flat; flat.height = 0; flat.margin = 0;
    assert(close(makeView(flat,eye,forward,feet,1,0.0)->planeDepth,dot(minus(feet,eye),forward),1e-3));
}
// Window depth of a point drawn through `m` with the viewport range [low, high].
double window(const Matrix& m,Vec relative,double low,double high) {
    const Clip c = project(m,relative);
    return low+(high-low)*(c.z/c.w);
}
void plane() {
    const Camera cam = mainCamera();
    const View v = rigView();
    const Plan plan = evaluate(cam,v,100.05);
    assert(plan.reject == Reject::none && close(plan.planeDepth,v.planeDepth,1e-2));
    assert(close(plan.forward.x,forward.x,1e-5) && close(plan.eye.z,eye.z,0));
    // Plane depth 15 near, far 353840: d0 = far/(far-near) * (1 - near/depth).
    const double expected = 353840.0/(353840.0-15.0)*(1.0-15.0/plan.planeDepth);
    assert(close(plan.d0,expected,1e-6) && plan.d0 > .97 && plan.d0 < .98);
    const Vec right = unit(cross(forward,{0,0,1})),up = cross(right,forward);
    unsigned kept = 0,cut = 0;
    for (const std::size_t which : {viewProjectionMatrix,viewProjectionUnjitteredMatrix}) {
        const Matrix& original = cam.matrices[which];
        const Matrix changed = clipped(original,plan.columns[which]);
        for (int depthStep = 0;depthStep < 60;++depthStep) for (int side = -2;side <= 2;++side) for (int rise = -1;rise <= 1;++rise) {
            const float depth = 20.0f+static_cast<float>(depthStep)*35.0f;
            const Vec p = add(add(times(forward,depth),times(right,static_cast<float>(side)*90.0f)),times(up,static_cast<float>(rise)*70.0f));
            const Clip before = project(original,p),after = project(changed,p);
            // x, y and w are untouched: same pixel, same jitter, same motion vector.
            assert(before.x == after.x && before.y == after.y && before.w == after.w && close(before.w,depth,1e-2));
            if (depth < plan.planeDepth-.05f) { assert(after.z < 0); ++cut; }       // hardware near clipping removes it
            else if (depth > plan.planeDepth+.05f) {
                assert(after.z > 0 && after.z <= after.w);
                // Every surviving fragment keeps its depth-buffer value, for the full range and the opaque pass's range.
                assert(close(window(changed,p,*viewportMinDepth(plan.d0,{0.0f,1.0f}),1.0),window(original,p,0.0,1.0),2e-6));
                const float top = .999998f;
                assert(close(window(changed,p,*viewportMinDepth(plan.d0,{0.0f,top}),top),window(original,p,0.0,top),2e-6));
                // Decals: the engine lowers the top of the range by the bias; the result is still exact,
                for (const float bias : {1.5e-5f,2.6e-5f,3.0e-5f}) {
                    const DepthRange biased = *flushedRange(true,0.0f,top,0.0f,top,bias);
                    assert(biased.high == top-bias && close(window(changed,p,*viewportMinDepth(plan.d0,biased),biased.high),
                        window(original,p,0.0,biased.high),2e-6));
                    // and the decal stays in front of its surface as it does without the clip.
                    assert(window(changed,p,*viewportMinDepth(plan.d0,biased),biased.high) < window(changed,p,*viewportMinDepth(plan.d0,{0.0f,top}),top));
                }
                ++kept;
            }
        }
    }
    assert(kept > 500 && cut > 500);
    // The projection matrices get the same treatment (inverse matrices in the buffer stay consistent).
    const Matrix projection = clipped(cam.matrices[projectionMatrix],plan.columns[projectionMatrix]);
    assert(project(projection,{0,0,plan.planeDepth-1.0f}).z < 0 && project(projection,{0,0,plan.planeDepth+1.0f}).z > 0);
    assert(projection[2][0] == cam.matrices[projectionMatrix][2][0] && projection[2][3] == 1.0f && projection[3][3] == 0.0f);
    // What the flush will leave in the viewport: rebuilt from the camera data when the bias bit is dirty, kept otherwise.
    const auto rebuilt = flushedRange(true,0.0f,.999998f,.5f,.75f,1.5e-5f),kept2 = flushedRange(false,0.0f,.999998f,.25f,.75f,1.5e-5f);
    assert(rebuilt && rebuilt->low == 0.0f && rebuilt->high == .999998f-1.5e-5f && kept2 && kept2->low == .25f && kept2->high == .75f);
    assert(close(*viewportMinDepth(.5,*kept2),.5,1e-6) && close(*viewportMinDepth(plan.d0,{0.0f,1.0f}),plan.d0,1e-7));
    // Ranges, bias values and plane depths that cannot be used give no value.
    assert(!flushedRange(true,0,1,0,1,nan) && !flushedRange(true,0,1,0,1,-1e-5f) && !flushedRange(true,0,1,0,1,.5f));
    assert(!flushedRange(true,.5f,.5f,0,1,0) && !flushedRange(true,-.1f,1,0,1,0) && !flushedRange(true,0,1.5f,0,1,0) && !flushedRange(true,nan,1,0,1,0));
    assert(!flushedRange(false,0,1,.5f,.5f,0) && !flushedRange(false,0,1,0,nan,0) && !flushedRange(false,0,1,.6f,.5f,0) && flushedRange(false,nan,nan,0,1,0));
    assert(!viewportMinDepth(0.0,{0,1}) && !viewportMinDepth(1.0,{0,1}) && !viewportMinDepth(nan,{0,1}));
    const std::array<float,biasModes> table{0,1.5e-5f,0,0,0,0,2.6e-5f,1.5e-5f,2.8e-5f,1.7e-5f,3.0e-5f,1.9e-5f};
    assert(*biasOf(0,table.data()) == 0.0f && *biasOf(1,table.data()) == 1.5e-5f && *biasOf(11,table.data()) == 1.9e-5f);
    assert(!biasOf(12,table.data()) && !biasOf(0,nullptr));
    assert(ownWrite(.5f,.5f) && !ownWrite(.5f,.25f) && !ownWrite(0.0f,-0.0f) && ownWrite(nan,nan));
}
void rejects() {
    const View v = rigView();
    const Matrix p = perspective(15.0f,353840.0f);
    assert(evaluate(mainCamera(),v,100.2).reject == Reject::none);
    assert(evaluate(mainCamera(),v,100.3).reject == Reject::stale && evaluate(mainCamera(),v,99.0).reject == Reject::stale &&
        evaluate(mainCamera(),v,std::numeric_limits<double>::quiet_NaN()).reject == Reject::stale);
    // Other cameras: a shadow or local-map orthographic view, a reflection, a cube face, a view from elsewhere along another axis.
    assert(evaluate(cameraAt(eye,forward,orthographic(),orthographic()),v,100).reject == Reject::otherView);
    assert(evaluate(cameraAt(eye,{forward.x,forward.y,-forward.z},p,p),v,100).reject == Reject::otherView);
    assert(evaluate(cameraAt(eye,{1,0,0},p,p),v,100).reject == Reject::otherView);
    const Vec tilted = unit({forward.x+.05f,forward.y,forward.z});
    assert(evaluate(cameraAt(eye,tilted,p,p),v,100).reject == Reject::otherView);
    // Same direction, different place along it: not the published camera.
    assert(evaluate(cameraAt(add(eye,times(forward,-150.0f)),forward,p,p),v,100).reject == Reject::published);
    assert(evaluate(cameraAt(add(eye,times(forward,-40.0f)),forward,p,p),v,100).reject == Reject::none); // the state's own depth is used
    // A sideways shift does not change any depth, so the plane is still right.
    const Vec right = unit(cross(forward,{0,0,1}));
    assert(evaluate(cameraAt(add(eye,times(right,3.0f)),forward,p,p),v,100).reject == Reject::none);
    // A view matrix that carries part of the eye position (posAdjust 5 units ahead of the eye) gives the same plane.
    {
        Camera shifted = mainCamera();
        shifted.posAdjust = add(eye,times(forward,5.0f));
        for (const std::size_t which : {viewProjectionMatrix,viewProjectionUnjitteredMatrix}) {
            Matrix& m = shifted.matrices[which];
            for (std::size_t column = 0;column < 4;++column)
                m[3][column] += 5.0f*(forward.x*m[0][column]+forward.y*m[1][column]+forward.z*m[2][column]);
        }
        const Plan moved = evaluate(shifted,v,100),plain = evaluate(mainCamera(),v,100);
        assert(moved.reject == Reject::none && close(moved.depthOffset,5.0,1e-3) && close(moved.planeDepth,plain.planeDepth,1e-2) &&
            close(moved.d0,plain.d0,1e-6));
        const Vec overhead = add(feet,{0,0,300}),underfoot = add(feet,{0,0,-8});
        assert(reach(moved,v,{overhead,64},false) == Skip::none && reach(moved,v,{underfoot,64},false) == Skip::behind);
        const Matrix changed = clipped(shifted.matrices[viewProjectionMatrix],moved.columns[viewProjectionMatrix]);
        assert(project(changed,minus(overhead,shifted.posAdjust)).z < 0 && project(changed,minus(underfoot,shifted.posAdjust)).z > 0);
    }
    // The state's own view direction must be the matrix's w axis, or the layout is not the audited one.
    Camera cam = mainCamera();
    cam.viewDir = {forward.x+.01f,forward.y,forward.z};
    assert(evaluate(cam,v,100).reject == Reject::layout);
    cam = mainCamera(); cam.matrices[projectionMatrix][1][1] = nan;
    assert(evaluate(cam,v,100).reject == Reject::layout);
    cam = mainCamera(); cam.posAdjust.x = nan;
    assert(evaluate(cam,v,100).reject == Reject::layout);
    // Reversed depth, or matrices that disagree about depth, are not the audited convention.
    assert(evaluate(cameraAt(eye,forward,reversed(15.0f,353840.0f),reversed(15.0f,353840.0f)),v,100).reject == Reject::convention);
    assert(evaluate(cameraAt(eye,forward,p,perspective(30.0f,353840.0f)),v,100).reject == Reject::convention);
    cam = mainCamera(); cam.matrices[projectionMatrix] = orthographic();
    assert(evaluate(cam,v,100).reject == Reject::convention);
    // A camera almost on top of the anchor: the state's plane would sit before the minimum depth.
    Config tight; tight.margin = 256.0f;
    const Vec nearEye = minus(add(feet,{0,0,128.0f}),times(forward,300.0f));
    const auto nearView = makeView({},nearEye,forward,feet,1,100.0);
    assert(nearView && !makeView(tight,nearEye,forward,feet,1,100.0));
    View forced = *nearView;
    forced.planeDepth = forced.anchorDepth-250.0f;
    assert(evaluate(cameraAt(nearEye,forward,p,p),forced,100).reject == Reject::tooNear);
    assert(!audited(Reject::none) && !audited(Reject::stale) && !audited(Reject::otherView) && !audited(Reject::tooNear) &&
        !audited(Reject::unknownEpoch) && audited(Reject::gate) && audited(Reject::drift) && audited(Reject::layout) &&
        audited(Reject::published) && audited(Reject::depthRange) && audited(Reject::convention) && audited(Reject::bias) && !audited(Reject::count));
    assert(std::string(rejectNames[static_cast<std::size_t>(Reject::drift)]) == "drift" &&
        std::string(rejectNames[static_cast<std::size_t>(Reject::bias)]) == "bias");
}
void selection() {
    const View v = rigView();
    const Plan plan = evaluate(mainCamera(),v,100);
    assert(plan.reject == Reject::none);
    const Vec toCamera = unit({eye.x-feet.x,eye.y-feet.y,0});
    const auto ground = [&](float towardCamera,float height) { return add(feet,add(times(toCamera,towardCamera),{0,0,height})); };
    // Never skinned geometry, never a bound that cannot be trusted.
    assert(reach(plan,v,{ground(0,300),200},true) == Skip::skinned);
    assert(reach(plan,v,{ground(0,300),0},false) == Skip::noBound && reach(plan,v,{{nan,0,0},50},false) == Skip::noBound &&
        reach(plan,v,{ground(0,300),nan},false) == Skip::noBound);
    // The floor under the player, the far wall of the player's room and everything beyond are behind the plane.
    assert(reach(plan,v,{ground(0,-8),64},false) == Skip::behind);
    assert(reach(plan,v,{ground(-300,128),128},false) == Skip::behind);
    assert(reach(plan,v,{ground(-900,400),300},false) == Skip::behind);
    // A ceiling over the player and a wall between camera and player are in front of it.
    assert(reach(plan,v,{ground(0,300),64},false) == Skip::none);
    assert(reach(plan,v,{ground(200,200),64},false) == Skip::none);
    // A one-piece cave mesh around everything reaches in front of the plane whatever its centre.
    assert(reach(plan,v,{ground(-2000,-500),6000},false) == Skip::none);
    // The corridor: the same piece far to the side is left alone, unless the corridor is the whole view.
    const Vec side = unit(cross(toCamera,{0,0,1}));
    const Bound aside{add(ground(150,350),times(side,700.0f)),64};
    assert(reach(plan,v,aside,false) == Skip::outside);
    Config whole; whole.corridor = 0;
    assert(reach(plan,rigView(whole),aside,false) == Skip::none);
    assert(close(segmentDistance(add(eye,{0,0,50}),eye,v.anchor),50.0,1e-2) && close(segmentDistance(eye,eye,eye),0,0)); // before the start
    assert(close(segmentDistance(add(fixed::mix(eye,v.anchor,.5f),times(side,30.0f)),eye,v.anchor),30.0,1e-2));
    assert(close(segmentDistance(add(v.anchor,times(forward,100.0f)),eye,v.anchor),100.0,1e-2)); // past the end: distance to the anchor
    // Where the plane meets a flat floor on the camera side: (0.819 * 128 + 24) / 0.574 = 224.5 units from the player.
    const auto depthOf = [&](Vec point) { return dot(minus(point,plan.eye),plan.forward); };
    assert(depthOf(ground(215,0)) > plan.planeDepth && depthOf(ground(235,0)) < plan.planeDepth);
    assert(close((std::sin(pitch)*128.0f+24.0f)/std::cos(pitch),224.6,.2));
    // and it clears the top of the head by the margin
    assert(close(depthOf(ground(0,128))-plan.planeDepth,24.0,.05));

    const Extent wall{true,feet.z,feet.z+300.0f,256,32},floorTile{true,feet.z-16.0f,feet.z+8.0f,256,256},step{true,feet.z,feet.z+24.0f,128,64};
    const Extent raised{true,feet.z+25.0f,feet.z+40.0f,64,64},unknown{};
    assert(ownerRule(false,false,Kind::architecture,wall,feet.z,24) == Skip::ownerless);
    assert(ownerRule(true,true,Kind::architecture,wall,feet.z,24) == Skip::actor);
    assert(ownerRule(true,false,Kind::excluded,wall,feet.z,24) == Skip::kind);
    assert(ownerRule(true,false,Kind::architecture,floorTile,feet.z,24) == Skip::floor && ownerRule(true,false,Kind::object,step,feet.z,24) == Skip::floor);
    assert(ownerRule(true,false,Kind::architecture,wall,feet.z,24) == Skip::none && ownerRule(true,false,Kind::object,raised,feet.z,24) == Skip::none);
    assert(ownerRule(true,false,Kind::architecture,unknown,feet.z,24) == Skip::none); // no bounds: the plane alone decides
    assert(ownerRule(true,false,Kind::architecture,floorTile,feet.z-100.0f,24) == Skip::none); // above the player's level it is not floor
    Budget budget; budget.limit = 2;
    assert(budget.take() && budget.take() && !budget.take() && budget.peak == 2);
    budget.frame();
    assert(budget.take() && budget.used == 1 && budget.peak == 2);
    assert(std::string(skipNames[static_cast<std::size_t>(Skip::cap)]) == "cap" && std::string(skipNames[static_cast<std::size_t>(Skip::floor)]) == "floor" &&
        std::string(skipNames[static_cast<std::size_t>(Skip::shader)]) == "shader");
    assert(std::string(faultName(Fault::setup)) != faultName(Fault::upload) && std::string(faultName(Fault::frame)) != faultName(Fault::upload));
    assert(static_cast<unsigned>(Fault::setup) == 1 && static_cast<unsigned>(Fault::upload) == 2 && static_cast<unsigned>(Fault::frame) == 3);
}
// A DXBC container with one SHEX chunk holding `tokens` after its version and length words.
std::vector<std::uint8_t> shaderBlob(const std::vector<std::uint32_t>& tokens,const char* tag = "SHEX") {
    std::vector<std::uint8_t> blob(36);                         // "DXBC", checksum, 1, total size, chunk count, one offset
    const std::uint32_t chunkBytes = static_cast<std::uint32_t>((tokens.size()+2)*4);
    const auto put = [&](std::size_t at,std::uint32_t value) { std::memcpy(blob.data()+at,&value,4); };
    std::memcpy(blob.data(),"DXBC",4);
    put(20,1); put(28,1); put(32,36);
    blob.resize(36+8+chunkBytes);
    std::memcpy(blob.data()+36,tag,4);
    put(40,chunkBytes); put(44,0x00010050); put(48,static_cast<std::uint32_t>(tokens.size()+2));
    for (std::size_t i = 0;i < tokens.size();++i) put(52+i*4,tokens[i]);
    put(24,static_cast<std::uint32_t>(blob.size()));
    return blob;
}
void shaders() {
    // dp4 against cb12[row]: operand type 8 (constant buffer), two immediate indices.
    const std::uint32_t cb = (8u << 12) | (2u << 20) | 0x46u;
    const auto rows = [&](std::initializer_list<std::uint32_t> list) {
        std::vector<std::uint32_t> tokens{0x0100086a,0x08000011};
        for (const auto row : list) { tokens.insert(tokens.end(),{0x08000011,cb,12u,row,0x00101e46}); }
        return tokens;
    };
    const auto reads = [](const std::vector<std::uint32_t>& tokens,const char* tag = "SHEX") {
        const auto made = shaderBlob(tokens,tag);
        return readsViewProjection(made.data(),made.size());
    };
    const auto blob = shaderBlob(rows({8,9,10,11}));
    assert(readsViewProjection(blob.data(),blob.size()) && reads(rows({11,10,9,8,12,41}),"SHDR"));
    // The depth row (c10) missing, another buffer, or an operand with a relative index: not this draw's ViewProj.
    assert(!reads(rows({8,9,11})) && !reads(rows({8,9,10})) && !reads(rows({})) && !reads(rows({0,1,2,3})));
    auto other = rows({8,9,10,11});
    for (auto& token : other) if (token == 12u) token = 2u;       // cb2[8..11]
    assert(!reads(other));
    other = rows({8,9,10,11});
    for (auto& token : other) if (token == cb) token = cb | (1u << 22); // index 0 not immediate
    assert(!reads(other));
    other = rows({8,9,10,11});
    for (auto& token : other) if (token == cb) token = cb | (1u << 31); // extended operand
    assert(!reads(other));
    // Malformed or truncated input is never read past its size.
    assert(!readsViewProjection(nullptr,100) && !readsViewProjection(blob.data(),31) && !readsViewProjection(blob.data(),blob.size()-1));
    auto bad = blob; bad[0] = 'X';
    assert(!readsViewProjection(bad.data(),bad.size()));
    bad = blob; const std::uint32_t far = 0x7fffffff; std::memcpy(bad.data()+32,&far,4);  // chunk offset outside
    assert(!readsViewProjection(bad.data(),bad.size()));
    bad = blob; std::memcpy(bad.data()+40,&far,4);                                        // chunk size outside
    assert(!readsViewProjection(bad.data(),bad.size()));
    bad = blob; std::memcpy(bad.data()+28,&far,4);                                        // chunk count
    assert(!readsViewProjection(bad.data(),bad.size()));
    bad = blob; std::memcpy(bad.data()+48,&far,4);                                        // token count is clamped to the chunk
    assert(readsViewProjection(bad.data(),bad.size()));
    bad = blob; std::memcpy(bad.data()+36,"ISGN",4);
    assert(!readsViewProjection(bad.data(),bad.size()));
    std::vector<std::uint8_t> huge((std::size_t{1} << 20)+4);
    std::memcpy(huge.data(),blob.data(),blob.size());
    assert(!readsViewProjection(huge.data(),huge.size()));
}
void trace() {
    Sample s;
    s.seconds = 12.5; s.frame = 42; s.installed = true; s.active = true; s.haveView = true; s.havePlan = true;
    s.view = rigView(); s.plan = evaluate(mainCamera(),s.view,100);
    s.peak = 37; s.cap = 512; s.depthTarget = 0;
    s.window.passes = 1000; s.window.clipped = 37; s.window.uploads = 12; s.window.biased = 3; s.window.byClass = {30,7,0};
    s.window.skips[static_cast<std::size_t>(Skip::behind)] = 800; s.window.skips[static_cast<std::size_t>(Skip::floor)] = 9;
    s.window.rejects[static_cast<std::size_t>(Reject::otherView)] = 120; s.window.seconds = .000125;
    char line[768]{};
    assert(format(line,sizeof(line),s));
    const std::string text = line;
    for (const char* field : {"FixedClip t=12.500 frame=42 installed=1 active=1 latched=0 "," passes=1000 clipped=37 byClass=30,7,0 peak=37 cap=512 uploads=12 biased=3 ",
        " micros=125.0 "," skipBehind=800 "," skipFloor=9 "," target=0 "," rejOther=120 "," planeDepth=6"," d0=0.97"," sample=window"})
        assert(text.find(field) != std::string::npos);
    // Nothing published or clipped in the window: the fields read nan, never a stale value.
    Sample empty;
    assert(format(line,sizeof(line),empty));
    assert(std::string(line).find(" eye=nan,nan,nan anchor=nan,nan,nan ") != std::string::npos && std::string(line).find(" statePlane=nan d0=nan target=-1 ") != std::string::npos);
    // The longest plausible row still fits one trace row (768 bytes with its terminator).
    Sample big = s;
    big.frame = 99999999; big.seconds = 99999.999; big.offMain = 999999; big.peak = 4096; big.cap = 4096;
    big.view.eye = big.view.anchor = {-123456.78f,-123456.78f,-12345.67f};
    big.view.anchorDepth = big.view.planeDepth = big.plan.planeDepth = 12345.67f;
    big.window.passes = big.window.clipped = big.window.uploads = big.window.biased = big.window.conflicts = 9999999;
    big.window.skips.fill(9999999); big.window.rejects.fill(9999999); big.window.byClass.fill(9999999); big.window.seconds = 9.999999;
    assert(format(line,sizeof(line),big) && std::string(line).size() < std::tuple_size<TraceRow>::value);
    char small[64]{};
    assert(!format(small,sizeof(small),s));
}
} // namespace
int main() {
    audit();
    settings();
    views();
    plane();
    rejects();
    selection();
    shaders();
    trace();
    return 0;
}
