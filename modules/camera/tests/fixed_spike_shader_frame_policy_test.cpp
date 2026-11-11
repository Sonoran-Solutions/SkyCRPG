// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader", fifth pass (Stage 2 of the seamless cutaway): portable checks of the
// frame pass. The maths (band, falloff, their combination, the vignette, world position from depth against a
// known view-projection), the settings and their defaults, the eligibility gate, the install checks against a
// synthetic image, the target checks, the embedded shader bytes against their sources, and the trace row.
#include "fixed_spike_shader_policy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
using namespace skycrpg::camera::fixed::shader;
using skycrpg::camera::fixed::Vec;
namespace {
const float nan = std::numeric_limits<float>::quiet_NaN();
const float infinity = std::numeric_limits<float>::infinity();
bool close(float a,float b,float tolerance) { return std::abs(a-b) <= tolerance; }

void codes() {
    // The fifth pass continues at 93 without a gap; 97 fills the once-mask.
    constexpr std::array frame{Refusal::frameSite,Refusal::frameMemory,Refusal::frameDevice,Refusal::frameThread,Refusal::frameRun};
    for (std::size_t i = 0;i < frame.size();++i) assert(static_cast<unsigned>(frame[i]) == 93u+i);
    assert(lastRefusal == 97 && lastRefusal-firstRefusal == 31 && refusalBit(Refusal::frameRun) == 0x80000000u);
    assert(frameSkipNames.size() == 8 && std::string(frameSkipNames[static_cast<std::size_t>(FrameSkip::capture)]) == "capture" &&
        std::string(frameSkipNames[static_cast<std::size_t>(FrameSkip::shared)]) == "shared");
    assert(std::string(bandNames[static_cast<std::size_t>(BandState::kit)]) == "kit");
}

void settings() {
    bool adjusted = true;
    const Config defaults = validate(Config{},&adjusted);
    const FrameConfig& f = defaults.frame;
    assert(!adjusted && f.enabled && f.band == 96.0f && f.headroom == 136.0f && f.radiusInner == 900.0f && f.radiusOuter == 1400.0f && f.vignette == .25f);
    assert(falloffOn(f));
    Config c;
    c.frame.band = 3.0f; c.frame.headroom = -1.0f; c.frame.radiusInner = nan; c.frame.radiusOuter = -5.0f; c.frame.vignette = 2.0f;
    c = validate(c,&adjusted);
    assert(adjusted && c.frame.band == 8.0f && c.frame.headroom == 0.0f && c.frame.radiusInner == 900.0f && c.frame.radiusOuter == 0.0f &&
        c.frame.vignette == 1.0f);
    assert(!falloffOn(c.frame)); // outer <= inner is the documented off value, not an error
    c = {}; c.frame.band = -4.0f; c = validate(c,&adjusted);
    assert(adjusted && c.frame.band == 0.0f);
    c = {}; c.frame.band = 0.0f; c.frame.vignette = 0.0f; c.frame.radiusOuter = 900.0f; c = validate(c,&adjusted);
    assert(!adjusted && c.frame.band == 0.0f && c.frame.vignette == 0.0f && !falloffOn(c.frame)); // every part off is valid
    c = {}; c.frame.band = infinity; c.frame.headroom = 9000.0f; c.frame.radiusOuter = 1e9f; c = validate(c,&adjusted);
    assert(adjusted && c.frame.band == 96.0f && c.frame.headroom == 512.0f && c.frame.radiusOuter == 16384.0f);
    c = {}; c.frame.band = 600.0f; assert(validate(c).frame.band == 512.0f);
    // bFramePass=0: nothing is validated or reported, so the rest of the shader settings behave exactly as before.
    c = {}; c.frame.enabled = false; c.frame.band = nan; c.frame.vignette = 7.0f; c = validate(c,&adjusted);
    assert(!adjusted && !c.frame.enabled && std::isnan(c.frame.band) && c.frame.vignette == 7.0f);
}

// ---- a view-projection built the way the engine's is used: row vectors, clip = p M, p relative to the eye ---
using Rows = std::array<double,16>;
Rows multiply(const Rows& a,const Rows& b) {
    Rows out{};
    for (int i = 0;i < 4;++i) for (int j = 0;j < 4;++j) for (int k = 0;k < 4;++k) out[i*4+j] += a[i*4+k]*b[k*4+j];
    return out;
}
struct Camera { Vec eye; float headingDegrees,pitchDegrees,distance; float fovY,aspect,nearPlane,farPlane,jitterX,jitterY; };
Matrix viewProjection(const Camera& cam,Vec target) {
    // Look-at, left-handed, z up (Skyrim world); positions relative to the eye (the position adjustment).
    const double fx = target.x-cam.eye.x,fy = target.y-cam.eye.y,fz = target.z-cam.eye.z;
    const double fl = std::sqrt(fx*fx+fy*fy+fz*fz);
    const double zx = fx/fl,zy = fy/fl,zz = fz/fl;
    double xx = 0.0*zz-1.0*zy,xy = 1.0*zx-0.0*zz,xz = 0.0;
    const double xl = std::sqrt(xx*xx+xy*xy+xz*xz); xx /= xl; xy /= xl;
    const double yx = zy*xz-zz*xy,yy = zz*xx-zx*xz,yz = zx*xy-zy*xx;
    const Rows view{xx,yx,zx,0, xy,yy,zy,0, xz,yz,zz,0, 0,0,0,1};
    const double ys = 1.0/std::tan(cam.fovY/2.0),xs = ys/cam.aspect,q = cam.farPlane/(cam.farPlane-cam.nearPlane);
    const Rows projection{xs,0,0,0, 0,ys,0,0, cam.jitterX,cam.jitterY,q,1, 0,0,-q*cam.nearPlane,0};
    const Rows m = multiply(view,projection);
    Matrix out;
    for (int i = 0;i < 16;++i) out.m[static_cast<std::size_t>(i)] = static_cast<float>(m[static_cast<std::size_t>(i)]);
    return out;
}
struct Projected { float px,py,depth; bool visible; };
Projected project(const Matrix& m,const FrameCapture& cap,Vec world) {
    const double p[4]{world.x-cap.posAdjust.x,world.y-cap.posAdjust.y,world.z-cap.posAdjust.z,1.0};
    double clip[4]{};
    for (int j = 0;j < 4;++j) for (int i = 0;i < 4;++i) clip[j] += p[i]*m.m[static_cast<std::size_t>(i*4+j)];
    const double nx = clip[0]/clip[3],ny = clip[1]/clip[3],nz = clip[2]/clip[3];
    const Viewport& v = cap.viewport;
    Projected out;
    out.px = static_cast<float>(v.x+(nx+1.0)/2.0*v.width);
    out.py = static_cast<float>(v.y+(1.0-ny)/2.0*v.height);
    out.depth = static_cast<float>(cap.depthMin+nz*(cap.depthMax-cap.depthMin));
    out.visible = clip[3] > 0 && std::abs(nx) <= 1 && std::abs(ny) <= 1 && nz >= 0 && nz <= 1;
    return out;
}
// The fixed rig's geometry (wave 1: pitch 55, ground distance 335.6, height 683.8 above the feet at fZoom 0.65),
// at the Embershard arrival (player 2487,1485,9241; plane 9420.25 in W3 with feet+160 at a held 9260.25).
struct Scene { FrameCapture capture; View view; Matrix matrix; };
Scene scene(Vec feet,float heldFeetZ,float jitterX = 0.0f,float jitterY = 0.0f) {
    Scene s;
    const Vec eye{feet.x-237.3f,feet.y-237.3f,feet.z+683.8f};
    const Camera cam{eye,45.0f,55.0f,761.7f,1.2217305f,16.0f/9.0f,15.0f,353840.0f,jitterX,jitterY};
    s.matrix = viewProjection(cam,{feet.x,feet.y,feet.z+80.0f});
    s.capture.viewProjection = s.matrix;
    s.capture.posAdjust = eye;
    s.capture.viewport = {0.0f,0.0f,1920.0f,1080.0f,0.0f,1.0f};
    s.capture.depthMin = 0.0f; s.capture.depthMax = 1.0f;
    s.capture.viewFrame = 7;
    s.view.frame = 7; s.view.eye = eye; s.view.feet = feet; s.view.planeZ = heldFeetZ+160.0f; s.view.forward = {0,0,-1};
    return s;
}
float expectAmount(const FrameConstants& k,const Projected& at,float band,float fall) {
    const float ndcX = at.px/960.0f-1.0f,ndcY = 1.0f-at.py/540.0f;
    const float edge = k.guard[2]*smooth(vignetteStart,1.0f,std::hypot(ndcX,ndcY)*.70710678f);
    const float inner = 1.0f-(1.0f-band)*(1.0f-fall);
    return 1.0f-(1.0f-inner)*(1.0f-edge);
}

void reconstruction() {
    const Vec feet{2487.0f,1485.0f,9241.0f};
    for (const float jitter : {0.0f,.0007f}) {
        Scene s = scene(feet,9260.25f,jitter,-jitter);
        BandState band{};
        float floor{};
        const auto k = frameConstants(FrameConfig{},160.0f,s.view,s.capture,Color{},false,band,floor);
        assert(k && band == BandState::on && floor == 9260.25f+136.0f);
        // World position from depth, relative to the position adjustment, at points across the room.
        for (const Vec p : {feet,Vec{feet.x+300,feet.y,feet.z},Vec{feet.x-150,feet.y+400,feet.z+20},Vec{feet.x+500,feet.y+500,feet.z-40},
                            Vec{feet.x+60,feet.y-90,9410.0f}}) {
            const Projected at = project(s.matrix,s.capture,p);
            assert(at.visible);
            const FramePixel px = framePixel(*k,at.px,at.py,at.depth);
            assert(!px.voidPixel);
            const Vec world{px.world.x+s.capture.posAdjust.x,px.world.y+s.capture.posAdjust.y,px.world.z+s.capture.posAdjust.z};
            assert(close(world.x,p.x,.75f) && close(world.y,p.y,.75f) && close(world.z,p.z,.75f));
        }
    }
    // The same scene shifted far across the world reconstructs the same relative positions: the circle sits on the feet.
    Scene a = scene(feet,9260.25f);
    Scene b = scene({feet.x+60000.0f,feet.y-45000.0f,feet.z-3000.0f},9260.25f-3000.0f);
    BandState band{};
    float floor{};
    const auto ka = frameConstants(FrameConfig{},160.0f,a.view,a.capture,Color{},false,band,floor);
    const auto kb = frameConstants(FrameConfig{},160.0f,b.view,b.capture,Color{},false,band,floor);
    const Projected pa = project(a.matrix,a.capture,{feet.x+1100,feet.y+200,feet.z});
    const Projected pb = project(b.matrix,b.capture,{feet.x+60000.0f+1100,feet.y-45000.0f+200,feet.z-3000.0f});
    assert(close(pa.px,pb.px,.01f) && close(pa.py,pb.py,.01f));
    assert(close(framePixel(*ka,pa.px,pa.py,pa.depth).amount,framePixel(*kb,pb.px,pb.py,pb.depth).amount,1e-4f));
    // The engine's viewport depth range is honoured: a [0.1, 0.9] range gives the same world point.
    Scene r = scene(feet,9260.25f);
    r.capture.depthMin = .1f; r.capture.depthMax = .9f;
    const auto kr = frameConstants(FrameConfig{},160.0f,r.view,r.capture,Color{},false,band,floor);
    const Projected pr = project(r.matrix,r.capture,{feet.x+200,feet.y+100,feet.z});
    const FramePixel wr = framePixel(*kr,pr.px,pr.py,pr.depth);
    assert(close(wr.world.x+r.capture.posAdjust.x,feet.x+200,.75f) && close(wr.world.z+r.capture.posAdjust.z,feet.z,.75f));
}

void shading() {
    const Vec feet{2487.0f,1485.0f,9241.0f};
    const float held = 9260.25f,plane = held+160.0f;
    Scene s = scene(feet,held);
    BandState band{};
    float floor{};
    const Color voidColor{.02f,.03f,.04f};
    const auto k = frameConstants(FrameConfig{},160.0f,s.view,s.capture,voidColor,false,band,floor);
    assert(k && band == BandState::on);
    // Void pixels are exactly the void colour, whatever was drawn there, at the centre and in a corner.
    for (const auto& [x,y] : {std::pair{960.5f,540.5f},std::pair{.5f,.5f},std::pair{1919.5f,1079.5f}}) {
        const FramePixel v = framePixel(*k,x,y,1.0f);
        assert(v.voidPixel && v.amount == 1.0f);
        assert(blended(5.0f,voidColor.red,v.amount) == voidColor.red && blended(.3f,voidColor.blue,v.amount) == voidColor.blue);
    }
    assert(framePixel(*k,960.5f,540.5f,.9999995f).voidPixel && !framePixel(*k,960.5f,540.5f,.99999f).voidPixel);
    // The floor at the feet: no band (under the band floor), no falloff, only the vignette at that screen position.
    const Projected atFeet = project(s.matrix,s.capture,feet);
    assert(close(framePixel(*k,atFeet.px,atFeet.py,atFeet.depth).amount,expectAmount(*k,atFeet,0,0),1e-4f));
    // Radial falloff: 1200 from the feet on the floor; beyond the outer radius everything is the void colour.
    // (With this test camera the top of the frame is about 1,150 ground units ahead of the feet.)
    const Vec farFloor{feet.x+1000.0f*.7071f,feet.y+1000.0f*.7071f,feet.z};
    const Projected atFar = project(s.matrix,s.capture,farFloor);
    assert(atFar.visible && close(framePixel(*k,atFar.px,atFar.py,atFar.depth).amount,expectAmount(*k,atFar,0,smooth(900,1400,1000)),2e-3f));
    FrameConfig tight; tight.radiusInner = 300.0f; tight.radiusOuter = 600.0f;
    const auto ks = frameConstants(tight,160.0f,s.view,s.capture,voidColor,false,band,floor);
    const Vec beyond{feet.x+800.0f*.7071f,feet.y+800.0f*.7071f,feet.z};
    const Projected atBeyond = project(s.matrix,s.capture,beyond);
    assert(atBeyond.visible && framePixel(*ks,atBeyond.px,atBeyond.py,atBeyond.depth).amount == 1.0f);
    assert(close(framePixel(*ks,atFar.px,atFar.py,atFar.depth).amount,1.0f,0.0f) && framePixel(*k,atBeyond.px,atBeyond.py,atBeyond.depth).amount < .5f);
    // Soft cut edge: the band in force runs from the held feet + 136 to the plane (24 units); 10 under the plane, 300 away.
    const Vec wall{feet.x+300.0f,feet.y,plane-10.0f};
    const Projected atWall = project(s.matrix,s.capture,wall);
    assert(atWall.visible);
    const float kWall = smooth(held+136.0f,plane,plane-10.0f);
    assert(close(framePixel(*k,atWall.px,atWall.py,atWall.depth).amount,expectAmount(*k,atWall,kWall,0),2e-2f));
    // Nothing above the plane is darkened by the band (actors are never cut; their heads may stand above it).
    const Vec above{feet.x+300.0f,feet.y,plane+30.0f};
    const Projected atAbove = project(s.matrix,s.capture,above);
    assert(close(framePixel(*k,atAbove.px,atAbove.py,atAbove.depth).amount,expectAmount(*k,atAbove,0,0),1e-3f));
    // The player's own head, even inside the band on a stair where the plane lags (feet 20 above the held floor):
    // the guard around the feet keeps the band off it.
    const Vec head{feet.x+10.0f,feet.y,held+20.0f+128.0f};
    const Projected atHead = project(s.matrix,s.capture,head);
    assert(close(framePixel(*k,atHead.px,atHead.py,atHead.depth).amount,expectAmount(*k,atHead,0,0),1e-3f));
    // Another actor on the player's floor 300 away: its head (feet + 128) is under the band floor (feet + 136).
    const Vec npcHead{feet.x+300.0f,feet.y,held+128.0f};
    const Projected atNpc = project(s.matrix,s.capture,npcHead);
    assert(close(framePixel(*k,atNpc.px,atNpc.py,atNpc.depth).amount,expectAmount(*k,atNpc,0,0),1e-3f));
    // Headroom 0 gives the full band: N under the plane.
    FrameConfig wide; wide.headroom = 0.0f;
    const auto kw = frameConstants(wide,160.0f,s.view,s.capture,voidColor,false,band,floor);
    assert(band == BandState::on && floor == plane-96.0f);
    assert(close(framePixel(*kw,atNpc.px,atNpc.py,atNpc.depth).amount,expectAmount(*kw,atNpc,smooth(plane-96.0f,plane,held+128.0f),0),2e-2f));
    // Combination 1-(1-k)(1-f): a wall point 450 away inside the band, with radii 300/600.
    const Vec farWall{feet.x+450.0f*.7071f,feet.y+450.0f*.7071f,plane-12.0f};
    const Projected atFarWall = project(s.matrix,s.capture,farWall);
    assert(atFarWall.visible);
    assert(close(framePixel(*ks,atFarWall.px,atFarWall.py,atFarWall.depth).amount,
            expectAmount(*ks,atFarWall,smooth(held+136.0f,plane,plane-12.0f),smooth(300,600,450)),2e-2f));
    // Each part off on its own.
    FrameConfig none; none.band = 0; none.radiusOuter = 0; none.vignette = 0;
    const auto k0 = frameConstants(none,160.0f,s.view,s.capture,voidColor,false,band,floor);
    assert(band == BandState::off && k0->shape[0] == 0.0f && k0->shape[3] == 0.0f && k0->guard[2] == 0.0f);
    for (const Projected& at : {atFeet,atWall,atFar,atBeyond}) assert(at.visible && framePixel(*k0,at.px,at.py,at.depth).amount == 0.0f);
    assert(framePixel(*k0,.5f,.5f,1.0f).amount == 1.0f); // the void stays exact with everything else off
    // Kit cells with Stage 1: no band (back walls stand above the plane on purpose); the falloff and vignette stay.
    const auto kk = frameConstants(FrameConfig{},160.0f,s.view,s.capture,voidColor,true,band,floor);
    assert(band == BandState::kit && kk->shape[0] == 0.0f && kk->shape[3] == 1.0f);
    assert(close(framePixel(*kk,atWall.px,atWall.py,atWall.depth).amount,expectAmount(*kk,atWall,0,0),1e-3f));
    // A plane lower than feet + headroom leaves no band at all (off, not inverted).
    assert(bandState(FrameConfig{},100.0f,held+100.0f,false,&floor) == BandState::off);
    // Never brighter: the amount stays in [0,1] and the blend lies between the scene and the void colour.
    for (float y = .5f;y < 1080.0f;y += 37.0f)
        for (float x = .5f;x < 1920.0f;x += 53.0f)
            for (const float depth : {.95f,.99f,.999f,.9999f}) {
                const float a = framePixel(*k,x,y,depth).amount;
                assert(a >= 0.0f && a <= 1.0f);
                const float out = blended(.8f,voidColor.red,a);
                assert(out <= .8f+1e-6f && out >= voidColor.red-1e-6f);
            }
    // The vignette: none at the centre, the configured strength in the corners.
    assert(framePixel(*k0,960.0f,540.0f,1.0f).amount == 1.0f);
    FrameConfig vignetteOnly = none; vignetteOnly.vignette = .25f;
    const auto kv = frameConstants(vignetteOnly,160.0f,s.view,s.capture,voidColor,false,band,floor);
    assert(framePixel(*kv,960.0f,540.0f,.99f).amount == 0.0f && close(framePixel(*kv,0.0f,0.0f,.99f).amount,.25f,1e-4f));
    // Unusable captures: no constants, the frame is skipped (FrameSkip::matrix).
    FrameCapture bad = s.capture; bad.viewProjection = {};
    assert(!frameConstants(FrameConfig{},160.0f,s.view,bad,voidColor,false,band,floor));
    bad = s.capture; bad.viewport.width = 0; assert(!frameConstants(FrameConfig{},160.0f,s.view,bad,voidColor,false,band,floor));
    bad = s.capture; bad.depthMax = bad.depthMin; assert(!frameConstants(FrameConfig{},160.0f,s.view,bad,voidColor,false,band,floor));
    bad = s.capture; bad.viewProjection.m[5] = nan; assert(!frameConstants(FrameConfig{},160.0f,s.view,bad,voidColor,false,band,floor));
    View badView = s.view; badView.planeZ = infinity; assert(!frameConstants(FrameConfig{},160.0f,badView,s.capture,voidColor,false,band,floor));
    // The constants as the shader reads them: cut relative to the position adjustment, the void colour, the guard.
    assert(close(k->cut[0],feet.x-s.capture.posAdjust.x,1e-3f) && close(k->cut[2],plane-s.capture.posAdjust.z,1e-3f) &&
        close(k->cut[3],held+136.0f-s.capture.posAdjust.z,1e-3f));
    assert(k->voidColor[0] == .02f && k->voidColor[3] == 1.0f && k->guard[0] == guardInner && k->guard[1] == guardOuter && k->depthRange[2] == voidDepth);
}

void gate() {
    using skycrpg::camera::State;
    using skycrpg::camera::fixed::rigEligible;
    constexpr std::array states{State::disabled,State::invalidWorld,State::special,State::menu,State::firstPerson,State::precision,
        State::targeting,State::obstruction,State::normal,State::targetLock};
    for (const State state : states)
        for (const bool interior : {false,true})
            for (const bool installed : {false,true})
                for (const bool latched : {false,true})
                    for (const bool freshView : {false,true})
                        for (const bool captured : {false,true}) {
                            // endFrame publishes only for an eligible frame; every other frame and every lifecycle exit clears `active`.
                            const bool active = rigEligible(true,state) && publishes(installed,latched,interior,100.0f);
                            const FrameSkip skip = frameGate(latched,active,freshView,captured);
                            const bool expected = (state == State::normal || state == State::targetLock) && interior && installed && !latched &&
                                freshView && captured;
                            assert((skip == FrameSkip::none) == expected);
                            if (latched) assert(skip == FrameSkip::latched);
                            else if (!active) assert(skip == FrameSkip::inactive);
                        }
    // Menus, first person, the rig off and outdoors never draw; a lifecycle exit clears `active` before the next render.
    assert(frameGate(false,false,true,true) == FrameSkip::inactive && frameGate(false,true,false,true) == FrameSkip::stale &&
        frameGate(false,true,true,false) == FrameSkip::capture && frameGate(true,true,true,true) == FrameSkip::latched);
}

void targets() {
    TextureDescription colour{1920,1080,1,1,10,1,0,0,0x28,0,0},depth{1920,1080,1,1,44,1,0,0,0x48,0,0};
    const Viewport view{0,0,1920,1080,0,1};
    assert(targetCheck(true,colour,depth,view) == 0 && targetCheck(false,colour,depth,view) == 1);
    TextureDescription d = depth; d.bind = 0x40; assert(targetCheck(true,colour,d,view) == 2);
    d = depth; d.arraySize = 2; assert(targetCheck(true,colour,d,view) == 2);
    d = depth; d.samples = 4; assert(targetCheck(true,colour,d,view) == 2);
    TextureDescription cl = colour; cl.samples = 2; assert(targetCheck(true,cl,depth,view) == 3);
    cl = colour; cl.width = 1280; assert(targetCheck(true,cl,depth,view) == 4);
    assert(targetCheck(true,colour,depth,Viewport{0,0,2560,1440,0,1}) == 5 && targetCheck(true,colour,depth,Viewport{0,0,0,1080,0,1}) == 5);
    assert(targetCheck(true,colour,depth,Viewport{0,0,1280,720,0,1}) == 0); // a smaller main view inside the targets is fine
    // The descriptions the pass creates its two states from.
    const BlendDescription b = frameBlend();
    assert(!b.alphaToCoverage && !b.independent && b.targets[0].enable == 1 && b.targets[0].source == 5 && b.targets[0].destination == 6 &&
        b.targets[0].operation == 1 && b.targets[0].mask == 0x7 && b.targets[0].sourceAlpha == 1 && b.targets[0].destinationAlpha == 2);
    for (std::size_t i = 1;i < b.targets.size();++i) assert(!b.targets[i].enable);
    const RasterizerDescription r = frameRasterizer();
    assert(r.fill == 3 && r.cull == 1 && r.depthClip == 1 && !r.scissor && !r.multisample && !r.depthBias);
    assert(triangleList == 4 && pixelSlots == 16 && viewportSlots == 16 && renderTargetSlots == 8);
}

// ---- the install checks against a synthetic image -----------------------------
std::int32_t displacementAt(const std::uint8_t* at) { std::int32_t d{}; std::memcpy(&d,at,4); return d; }
std::uintptr_t ripTarget(std::uintptr_t rva,const std::uint8_t* bytes,std::size_t operand,std::size_t length) {
    return static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rva+length)+displacementAt(bytes+operand));
}
void site() {
    // The evidence windows encode exactly the offsets and indices the pass uses.
    // The site: ID 107148 calls the scene composite (ID 105675), then the image-space chain (ID 105674) with
    // (manager, source 1, destination ebp, 1).
    assert(callTarget(frameCallRva,frameSiteBytes.data()+frameGuardedBefore) == imageChainAddress.rva);
    assert(callTarget(0x153adbc,frameSiteBytes.data()) == 0x152ea20 && callTarget(0x153add0,frameSiteBytes.data()+0x14) == 0x14edfd0);
    assert(ripTarget(0x153adc9,frameSiteBytes.data()+0x0d,3,7) == 0x33d40a0 && ripTarget(0x153ade0,frameSiteBytes.data()+0x24,3,7) == 0x33d40a0);
    assert(frameSiteBytes[0x1c] == 0x44 && frameSiteBytes[0x1d] == 0x8b && frameSiteBytes[0x1e] == 0xc5); // mov r8d,ebp: the destination
    assert(frameSiteBytes[0x1f] == 0xba && displacementAt(frameSiteBytes.data()+0x20) == static_cast<std::int32_t>(chainSource));
    assert(playDestination == 0 && imageChainPrologue[0] == 0x48 && imageChainPrologue[15] == 0x44);
    // The destination's way down: 36558 esi = edx, edx = esi into 36559, stored at [rax+0x10], rbp = rax-0x5F,
    // ecx = [rbp+0x6F] = [rax+0x10] into 107148 (the call at 6578D1 goes to 153AC00), ebp = ecx there.
    assert(destinationKeptBytes == (std::array<std::uint8_t,2>{0x8b,0xf2}) && destinationHeldBytes == (std::array<std::uint8_t,2>{0x8b,0xe9}));
    assert(destinationStoredBytes[3] == 0x89 && destinationStoredBytes[5] == 0x10 && static_cast<std::int8_t>(destinationStoredBytes[20]) == -0x5f);
    assert(destinationLoadedBytes[2] == 0x6f && -0x5f+0x6f == 0x10 && callTarget(0x6578d1,destinationLoadedBytes.data()+3) == postWorldAddress.rva);
    assert(callTarget(0x656e2d,destinationPassedBytes.data()+5) == 0x656e60 && destinationPassedBytes[0] == 0x8b && destinationPassedBytes[1] == 0xd6);
    assert(callTarget(0x1537f7b,mainTargetsBytes.data()+0x1b) == 0x10155b0 && mainTargetsBytes[0x17] == 0x45 && mainTargetsBytes[0x1a] == 0x01 &&
        mainTargetsBytes[0x0e] == 0x33 && mainTargetsBytes[0x0f] == 0xd2); // SetRenderTarget(slot 0, target r9+1 = 1)
    assert(callTarget(0x1537f97,mainTargetsBytes.data()+0x37) == 0x1015790 && callTarget(0x1537fb0,mainTargetsBytes.data()+0x50) == 0x1015670);
    assert(mainDepthIndexBytes == (std::array<std::uint8_t,3>{0x33,0xc0,0xc3}) && frame_offset::mainDepth == 0 && frame_offset::mainTarget == 1);
    assert(displacementAt(flushTargetBytes.data()+20) == 0xa58 && flushTargetBytes[15] == 0xc9); // [r8+rcx*8+0xA58], rcx = 6*index
    assert(displacementAt(createTargetBytes.data()+13) == 0xa58 && displacementAt(createTargetBytes.data()+30) == 0xa68 &&
        createTargetBytes[19] == 0x28); // CreateTexture2D -> +0xA58, CreateRenderTargetView -> +0xA68
    assert(displacementAt(createDepthViewBytes.data()+10) == 0x20a0 && displacementAt(createDepthViewBytes.data()+17) == 0x2018 &&
        createDepthViewBytes[30] == 0x38); // CreateShaderResourceView(texture +0x2018) -> +0x20A0
    assert(displacementAt(bindDepthViewBytes.data()+6) == 0x98 && displacementAt(bindDepthViewBytes.data()+14) == 0x20a0);
    assert(frame_offset::targets+frame_offset::targetView-0xa58 == frame_offset::targetView && frame_offset::depthResource == 0x20a0-0x2018);
    constexpr std::uintptr_t state = 0x20cfc00;
    assert(ripTarget(0x100af6e,viewProjectionBytes.data()+0x27,3,7) == state+frame_offset::viewProjection); // movaps xmm4,[state+430]
    assert(ripTarget(0x100af7c,viewProjectionBytes.data()+0x35,3,8) == state+frame_offset::viewProjection+0x10);
    assert(ripTarget(0x100af47,viewProjectionBytes.data(),3,7) == state+frame_offset::viewProjection+0x20);
    assert(ripTarget(0x100af55,viewProjectionBytes.data()+0x0e,3,8) == state+frame_offset::viewProjection+0x30);
    assert(ripTarget(0x10110e4,viewportBytes.data()+7,3,7) == state+frame_offset::viewport);
    assert(ripTarget(0x101106c,depthRangeBytes.data(),4,8) == state+frame_offset::depthRange &&
        ripTarget(0x101107f,depthRangeBytes.data()+19,4,8) == state+frame_offset::depthRange+4);
    // frameSiteCheck at three image bases: exact bytes pass; every guarded byte flipped, a moved ID, a
    // redirected call or a detoured ID 36558 refuse with their own check number.
    for (const std::uintptr_t base : {std::uintptr_t{0x140000000},std::uintptr_t{0x7ff6a0000000},std::uintptr_t{0x10000}}) {
        std::map<std::uintptr_t,std::vector<std::uint8_t>> image;
        for (const auto& e : frameEvidence) image[e.rva].assign(e.bytes,e.bytes+e.size);
        std::vector<std::uint8_t> window(frameSiteBytes.begin(),frameSiteBytes.end());
        std::vector<std::uint8_t> callee(imageChainPrologue.begin(),imageChainPrologue.end());
        const auto at = [&](std::uintptr_t rva) { return image.at(rva).data(); };
        const auto check = [&](std::uintptr_t function,std::uintptr_t main,std::uintptr_t renderer) {
            return frameSiteCheck(base,function,main,renderer,window.data(),callee.data(),at);
        };
        const std::uintptr_t f = base+postWorldAddress.rva,m = base+imageChainAddress.rva,rd = base+rendererAddress.rva;
        assert(check(f,m,rd) == 0);
        assert(check(f+1,m,rd) == 1 && check(f,m+16,rd) == 2 && check(f,m,rd+8) == 3);
        for (std::size_t i = 0;i < window.size();++i) { window[i] ^= 0x01; assert(check(f,m,rd) == 4); window[i] ^= 0x01; }
        for (std::size_t i = 0;i < callee.size();++i) { callee[i] ^= 0x80; assert(check(f,m,rd) == 6); callee[i] ^= 0x80; }
        std::size_t index = 0;
        for (const auto& e : frameEvidence) {
            for (std::size_t i = 0;i < e.size;++i) { image[e.rva][i] ^= 0x10; assert(check(f,m,rd) == 10+index); image[e.rva][i] ^= 0x10; }
            ++index;
        }
        assert(check(f,m,rd) == 0);
    }
    assert(frameEvidence.size() == 14 && std::string(frameEvidence[0].what).find("107131") != std::string::npos);
    assert(postWorldAddress.id == 107148 && postWorldAddress.rva == 0x153ac00 && imageChainAddress.id == 105674 && imageChainAddress.rva == 0x14edbf0);
    assert(rendererAddress.id == 411393 && rendererAddress.rva == 0x3331f40);
    assert(frameCallRva == 0x153ade7 && frameCallRva != voidCallRva && sizeof(Viewport) == 24 && sizeof(TextureDescription) == 44);
    assert(computeResourceSlots == 16 && computeViewSlots == 8 && vertexResourceSlots == 16);
}

// ---- the embedded shaders against their sources ---------------------------------
std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    assert(in && "shader source not found: run the tests from the repository root");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
struct Chunks { std::vector<std::string> tags; std::vector<const std::uint8_t*> payload; std::vector<std::uint32_t> sizes; };
Chunks chunks(const std::uint8_t* blob,std::size_t size) {
    Chunks c;
    assert(size > 32 && std::memcmp(blob,"DXBC",4) == 0 && word(blob+20) == 1 && word(blob+24) == size);
    for (std::uint32_t i = 0;i < word(blob+28);++i) {
        const std::uint32_t at = word(blob+32+4*i);
        c.tags.emplace_back(reinterpret_cast<const char*>(blob+at),4);
        c.payload.push_back(blob+at+8);
        c.sizes.push_back(word(blob+at+4));
    }
    return c;
}
bool hasElement(const std::uint8_t* sig,std::uint32_t size,const char* name,std::uint32_t systemValue) {
    for (std::uint32_t i = 0;i < word(sig);++i) {
        const std::uint8_t* e = sig+8+24*i;
        const std::uint32_t at = word(e);
        if (at < size && std::string(reinterpret_cast<const char*>(sig+at)) == name && word(e+8) == systemValue) return true;
    }
    return false;
}
void shaders() {
    using namespace frame_bytes;
    const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path()/"shaders";
    const auto vs = readFile(root/"fixed_spike_frame_vs.hlsl"),ps = readFile(root/"fixed_spike_frame_ps.hlsl");
    // Edited sources need a rebuild of the header (modules/camera/tools/frame_shaders.py); this fails until then.
    assert(codeHash(vs.data(),vs.size()) == vertexShaderSource && codeHash(ps.data(),ps.size()) == pixelShaderSource);
    for (const auto& [blob,size,version] : {std::tuple{vertexShader.data(),vertexShader.size(),0x00010050u},
                                           std::tuple{pixelShader.data(),pixelShader.size(),0x00000050u}}) {
        assert(checksumMatches(blob,size)); // the container checksum the D3D runtime and DXVK verify
        const Chunks c = chunks(blob,size);
        assert((c.tags == std::vector<std::string>{"ISGN","OSGN","SHEX"})); // the shipped game shaders' form
        assert(word(c.payload[2]) == version && word(c.payload[2]+4)*4 == c.sizes[2]);
    }
    const Chunks v = chunks(vertexShader.data(),vertexShader.size()),p = chunks(pixelShader.data(),pixelShader.size());
    assert(hasElement(v.payload[0],v.sizes[0],"SV_VertexID",6) && hasElement(v.payload[1],v.sizes[1],"SV_Position",1) && word(v.payload[0]) == 1 &&
        word(v.payload[1]) == 1);
    assert(hasElement(p.payload[0],p.sizes[0],"SV_Position",1) && hasElement(p.payload[1],p.sizes[1],"SV_Target",0) && word(p.payload[0]) == 1 &&
        word(p.payload[1]) == 1);
    // The pixel shader declares cb0 with exactly the rows of FrameConstants, and one 2D texture in t0; the vertex shader no resource.
    bool buffer = false,texture = false;
    const std::uint8_t* code = p.payload[2];
    for (std::uint32_t at = 2;at < word(code+4);) {
        const std::uint32_t token = word(code+at*4),opcode = token & 0x7ffu,length = (token >> 24) & 0x7fu;
        if (opcode == op::constantBuffer) buffer = word(code+(at+2)*4) == 0 && word(code+(at+3)*4) == sizeof(FrameConstants)/16u;
        if (opcode == 88u) texture = ((token >> 11) & 0x1fu) == 3u && word(code+(at+2)*4) == 0; // dcl_resource, Texture2D, t0
        assert(length);
        at += length;
    }
    assert(buffer && texture);
    for (std::uint32_t at = 2;at < word(v.payload[2]+4);) {
        const std::uint32_t token = word(v.payload[2]+at*4);
        assert((token & 0x7ffu) != op::constantBuffer && (token & 0x7ffu) != 88u);
        at += (token >> 24) & 0x7fu;
    }
}

void trace() {
    FrameSample s;
    s.seconds = 1234.5; s.frame = 99; s.offMain = 0; s.mismatch = 0; s.state = Feature::installed;
    s.window.calls = 6; s.window.drawn = 5; s.window.skips[static_cast<std::size_t>(FrameSkip::capture)] = 1;
    s.window.haveDrawn = true; s.window.band = BandState::on; s.window.bandFloor = 9396.25f; s.window.planeZ = 9420.25f; s.window.seconds = 31e-6;
    s.colour.format = 10; s.depth = {1920,1080,1,1,44,1,0,0,0x48,0,0};
    char line[sizeof(skycrpg::camera::fixed::TraceRow)]{};
    assert(formatFrame(line,sizeof(line),s));
    assert(std::string(line) == "FixedShaderFrame t=1234.500 frame=99 pass=installed calls=6 drawn=5 skipLatched=0 skipInactive=0 skipStale=0 "
        "skipCapture=1 skipTargets=0 skipMatrix=0 skipShared=0 offMain=0 band=on bandN=96.0 headroom=136.0 bandFrom=9396.25 planeZ=9420.25 falloff=on "
        "radii=900.0,1400.0 vignette=0.250 depth=read target=1920x1080 formats=10,44 mismatch=0 micros=31.0 sample=window");
    // Nothing drawn: no band, no heights, depth not read.
    FrameSample idle;
    idle.state = Feature::pending; idle.window.calls = 6; idle.window.skips[static_cast<std::size_t>(FrameSkip::inactive)] = 6;
    assert(formatFrame(line,sizeof(line),idle));
    const std::string text(line);
    assert(text.find(" pass=pending ") != std::string::npos && text.find(" band=none ") != std::string::npos &&
        text.find(" bandFrom=nan planeZ=nan ") != std::string::npos && text.find(" depth=none ") != std::string::npos &&
        text.find(" skipInactive=6 ") != std::string::npos);
    // A kit cell: the band is reported as kit and has no height.
    s.window.band = BandState::kit;
    assert(formatFrame(line,sizeof(line),s) && std::string(line).find(" band=kit bandN=96.0 headroom=136.0 bandFrom=nan planeZ=9420.25 ") != std::string::npos);
    // The longest row fits the trace row.
    FrameSample wide;
    wide.seconds = 1e9; wide.frame = ~0ull; wide.offMain = ~0ull; wide.mismatch = ~0ull; wide.state = Feature::installed;
    wide.config.band = 512; wide.config.headroom = 512; wide.config.radiusInner = 16384; wide.config.radiusOuter = 16384; wide.config.vignette = 1;
    wide.window.calls = wide.window.drawn = ~0ull;
    for (auto& v : wide.window.skips) v = ~0ull;
    wide.window.haveDrawn = true; wide.window.band = BandState::on; wide.window.bandFloor = -1e9f; wide.window.planeZ = -1e9f; wide.window.seconds = 1e3;
    wide.colour = {~0u,~0u,0,0,~0u,0,0,0,0,0,0}; wide.depth = wide.colour;
    assert(formatFrame(line,sizeof(line),wide));
    std::printf("longest FixedShaderFrame row: %zu of %zu bytes\n",std::strlen(line),sizeof(line));
}
} // namespace

int main() {
    codes();
    settings();
    reconstruction();
    shading();
    gate();
    targets();
    site();
    shaders();
    trace();
    std::puts("fixed_spike_shader_frame_policy_test PASS");
}
