// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../include/selection_policy.h"
#include "../include/skycrpg/SelectionAPI.h"
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <type_traits>
#include <thread>

using namespace skycrpg::selection;
Context active() {
    Context c;
    c.enabled = c.runtime = c.reads = c.world = c.thirdPerson = true;
    c.loading = c.dead = c.menu = c.dialogue = c.firstPerson = c.special = false;
    return c;
}
void ranking() {
    // No camera direction/angle/type/value/history input exists. Off-axis wins.
    const std::array refs{Candidate{90,{50,0,0}},Candidate{80,{0,60,0}}};
    auto r = rank(refs,{},180);
    assert(r.size() == 2 && r[0].id == 90 && r[0].distance == 2500);
    const std::array close{Candidate{500,{0,50,0}},Candidate{900,{0,49.99999,0}}};
    assert(rank(close,{},180).front().id == 900);
    const std::array ties{Candidate{900,{-30,40,0}},Candidate{7,{30,-40,0}},Candidate{100,{0,0,50}}};
    for (const auto& permutation : {ties,std::array{ties[2],ties[0],ties[1]},std::array{ties[1],ties[2],ties[0]}}) {
        const auto result = rank(permutation,{},180);
        assert(result[0].id == 7 && result[1].id == 100 && result[2].id == 900);
    }
    // Long bench's origin is 150 away, but its near edge is only 10 away.
    Candidate bench{44,{150,0,0}};
    bench.bounds = {{150,0,0},{-140,-10,0},{140,10,30}};
    bench.bounds.available = true;
    const std::array furniture{bench,Candidate{20,{20,0,0}}};
    assert(rank(furniture,{},100).front().id == 44);
    assert(surfaceDistanceSquared(bench,{}) == 100);
    assert(eligible(bench,{},10) && !eligible(bench,{},9.999));
    assert(inBroadPhase({},bench.position,100,2048));
    assert(!inBroadPhase({},Point{3000,0,0},100,2048));
    // Rotate the bench 90 degrees; world distance and ranking are unchanged.
    bench.position = {0,150,0}; bench.bounds.origin = bench.position;
    bench.bounds.axes = {Point{0,1,0},Point{-1,0,0},Point{0,0,1}};
    assert(surfaceDistanceSquared(bench,{}) == 100);
    assert(surfaceDistanceSquared(bench,{0,150,15}) == 0);
    bench.bounds.available = false;
    assert(surfaceDistanceSquared(bench,{}) == 22500); // unavailable -> origin
    bench.bounds.available = true; bench.bounds.low.x = 200;
    assert(!validBounds(bench.bounds) && surfaceDistanceSquared(bench,{}) == 22500);
    auto invalid = refs[0];
    for (int field = 0; field < 8; ++field) {
        invalid = refs[0];
        switch (field) {
        case 0: invalid.player = true; break;
        case 1: invalid.disabled = true; break;
        case 2: invalid.deleted = true; break;
        case 3: invalid.loaded = false; break;
        case 4: invalid.activatable = false; break;
        case 5: invalid.lineOfSight = false; break;
        case 6: invalid.id = 0; break;
        default: invalid.position.x = std::numeric_limits<double>::quiet_NaN(); break;
        }
        assert(!eligible(invalid,{},180));
    }
    assert(eligible({1,{180,0,0}}, {},180));
    assert(!eligible({1,{180.000001,0,0}}, {},180));
    assert(!eligible(refs[0],{},0));
    assert(!eligible(refs[0],{},std::numeric_limits<double>::infinity()));
}
void latches() {
    Settings settings;
    const auto c = active();
    Policy p;
    std::array refs{Candidate{10,{10,0,0}},Candidate{20,{20,0,0}},Candidate{30,{30,0,0}}};
    p.refresh(c,refs,{},180,0,settings);
    assert(p.current() == 10 && !p.latched());
    assert(p.cycle(1,{},1) && p.current() == 20 && p.latched());
    refs[0].position.x = 29; refs[2].position.x = 5;
    p.refresh(c,refs,{},180,2,settings);
    assert(p.current() == 20 && p.order() == std::vector<std::uint32_t>({10,20,30}));
    assert(p.cycle(1,{},2) && p.current() == 30);
    assert(p.cycle(1,{},2) && p.current() == 10);
    assert(p.cycle(-1,{},2) && p.current() == 30);
    assert(p.cycle(-1,{},2) && p.current() == 20);
    refs[0].lineOfSight = false;
    p.refresh(c,refs,{},180,3,settings);
    assert(p.order() == std::vector<std::uint32_t>({20,30}));
    assert(p.cycle(-1,{},3) && p.current() == 30);
    // An invalid selected ref ends the latch and returns to actual nearest.
    refs[2].loaded = false;
    p.refresh(c,refs,{},180,4,settings);
    assert(!p.latched() && p.current() == 20);
    refs[0].lineOfSight = true; refs[2].loaded = true;
    p.refresh(c,refs,{},180,5,settings);
    assert(p.current() == 30);
    assert(p.cycle(1,{},5));
    p.refresh(c,refs,{64,0,0},180,6,settings);
    assert(p.latched()); // exactly threshold is retained
    p.refresh(c,refs,{64.001,0,0},180,6,settings);
    assert(!p.latched() && p.current() == 10);
    assert(p.cycle(1,{64.001,0,0},6));
    p.refresh(c,refs,{64.001,0,0},180,16,settings);
    assert(!p.latched()); // exact timeout
    assert(p.cycle(-1,{64.001,0,0},16));
    p.refresh(c,refs,{64.001,0,0},180,15,settings);
    assert(!p.latched()); // clock rewind invalidates
    assert(!p.push(999,{64.001,0,0},17,true));
    assert(!p.push(20,{64.001,0,0},17,false));
    assert(p.push(20,{64.001,0,0},17,true) && p.current() == 20);
    p.release(); assert(p.current() == 10 && !p.latched());
    // New arrivals cannot reshuffle an existing cycling session.
    assert(p.cycle(1,{64.001,0,0},17));
    const std::array extended{refs[0],refs[1],refs[2],Candidate{40,{64,0,0}}};
    p.refresh(c,extended,{64.001,0,0},180,18,settings);
    assert(p.order().size() == 3);
    p.reset(); assert(p.current() == 0 && !p.cycle(1,{},19));
}
void passthroughTable() {
    const auto valid = active();
    assert(passthrough(valid) == Pass::none);
    struct Row { bool Context::*field; bool value; Pass pass; };
    const std::array rows{
        Row{&Context::enabled,false,Pass::disabled},Row{&Context::runtime,false,Pass::unsupported},
        Row{&Context::reads,false,Pass::readFailure},Row{&Context::world,false,Pass::invalidWorld},
        Row{&Context::loading,true,Pass::loading},Row{&Context::dead,true,Pass::dead},
        Row{&Context::menu,true,Pass::menu},Row{&Context::dialogue,true,Pass::dialogue},
        Row{&Context::firstPerson,true,Pass::firstPerson},Row{&Context::thirdPerson,false,Pass::special},
        Row{&Context::special,true,Pass::special},Row{&Context::conflict,true,Pass::conflict},
        Row{&Context::armed,true,Pass::armed},Row{&Context::combat,true,Pass::combat},
        Row{&Context::telekinesis,true,Pass::telekinesis}};
    const std::array refs{Candidate{1,{1,0,0}},Candidate{2,{2,0,0}}};
    for (const auto& row : rows) {
        auto c = valid; c.*(row.field) = row.value;
        assert(passthrough(c) == row.pass);
        Policy p; p.refresh(valid,refs,{},180,0,{}); p.cycle(1,{},0);
        p.refresh(c,refs,{},180,1,{});
        assert(p.reason() == row.pass && p.current() == 0 && !p.latched() && p.order().empty());
        assert(!p.push(1,{},1,true));
    }
    Policy p;
    p.refresh(valid,refs,{},180,0,{},false);
    assert(p.reason() == Pass::incompleteScan && p.current() == 0);
    p.refresh(valid,{}, {},180,1,{});
    assert(p.reason() == Pass::noCandidate && p.current() == 0);
    p.refresh(valid,refs,{},0,1,{});
    assert(p.reason() == Pass::readFailure);
}
void inputEdges() {
    FreshPress released;
    released.suspend();
    assert(released.update(true,true,true)); // first fresh press after loading/menu
    released.suspend();
    assert(!released.update(true,true,true)); // held through interruption
    released.update(false,false,false);
    released.suspend();
    assert(released.update(true,true,true)); // release in menu, new press in game
    FreshPress key;
    assert(key.update(true,true,true));
    assert(!key.update(true,false,true));
    assert(!key.update(true,true,true)); // repeats cannot step twice
    assert(!key.update(false,false,true));
    assert(key.update(true,true,true));
    key.suspend();
    assert(!key.update(true,true,true));
    assert(!key.update(false,false,false));
    assert(!key.update(true,true,false)); // native mapped key or menu
    assert(!key.update(true,true,true)); // held through mapped/menu exit
    key.update(false,false,true);
    assert(key.update(true,true,true));
    key.update(false,false,true);
    assert(!key.update(true,false,true)); // hold first observed without physical edge
    assert(!key.update(true,true,true));
    std::array<FreshPress,4> independent;
    assert(independent[0].update(true,true,true));
    assert(independent[2].update(true,true,true));
}
void configurationAndBudget() {
    Settings s;
    assert(searchRadius(180,s) == 180 && !s.allowExternal && s.nextGamepad == -1 && s.previousGamepad == -1);
    s.radius = 500; assert(searchRadius(180,s) == 180);
    s.radius = 90; assert(searchRadius(180,s) == 90);
    assert(searchRadius(std::numeric_limits<double>::quiet_NaN(),s) == 0);
    s.nextKey = 64; s.previousKey = 66; s.nextGamepad = 3; s.previousGamepad = 65536;
    s = validate(s);
    assert(s.nextKey == -1 && s.previousKey == -1 && s.nextGamepad == -1 && s.previousGamepad == -1);
    s.nextKey = s.previousKey = 26; s.nextGamepad = s.previousGamepad = 128;
    s = validate(s); assert(s.previousKey == -1 && s.previousGamepad == -1);
    s.rayLimit = 0; s.visitLimit = 999999; s.boundsPadding = -1;
    s = validate(s);
    assert(s.rayLimit == 3 && s.visitLimit == 262144 && s.boundsPadding == 256);
    s.schema = 2; assert(!validate(s).enabled);
    s = {}; s.refreshSeconds = 0; s.inspectLimit = 999999; s.moveThreshold = -1;
    s = validate(s); assert(s.refreshSeconds == .1 && s.inspectLimit == 16384 && s.moveThreshold == 1);
    ScanBudget budget{3};
    assert(budget.inspect() && budget.inspect() && budget.inspect() && budget.complete);
    assert(!budget.inspect() && budget.inspected == 3 && !budget.complete);
    RefreshGate gate;
    assert(gate.due(1,.1)); gate.last = 1;
    assert(!gate.due(1.05,.1) && gate.due(1.11,.1) && gate.due(.9,.1));
    gate.reset(); assert(gate.due(1.05,.1)); // transition refresh is immediate
    ScanBudget nearby{2}, rays{3};
    for (unsigned i = 0; i < 5000; ++i) {
        if (inBroadPhase({},Point{10000,0,0},180,2048)) nearby.inspect();
    }
    assert(nearby.inspected == 0 && nearby.complete);
    assert(rays.inspect() && rays.inspect() && rays.inspect() && !rays.inspect());
    assert(nearby.complete && !rays.complete); // independent physics budget
    static_assert(std::is_standard_layout_v<api::Snapshot>);
    static_assert(std::is_standard_layout_v<api::Interface>);
}
void accessGate() {
    AccessGate gate;
    assert(gate.enter());
    assert(!gate.enter()); // reentrant callback refuses without waiting
    std::thread input([&] { assert(!gate.enter()); });
    input.join();
    gate.leave();
    std::thread job([&] { assert(gate.enter()); gate.leave(); });
    job.join(); // pool threads can acquire; there is no Main-thread requirement
}
void requestsAndLazyValidation() {
    CycleRequests requests;
    assert(!requests.post(0));
    std::thread input([&] { assert(requests.post(1)); assert(requests.post(-1)); assert(requests.post(-1)); });
    input.join();
    auto batch = requests.take();
    assert(requests.take() == 0);
    assert(CycleRequests::pop(batch) == 1 && CycleRequests::pop(batch) == -1 && CycleRequests::pop(batch) == -1);
    assert(batch == 0 && CycleRequests::pop(batch) == 0);
    for (unsigned i = 0; i < 16; ++i) assert(requests.post(i % 2 ? -1 : 1));
    assert(!requests.post(1)); // bounded mailbox does not overwrite old edges
    batch = requests.take();
    for (unsigned i = 0; i < 16; ++i) assert(CycleRequests::pop(batch) == (i % 2 ? -1 : 1));
    assert(batch == 0);
    assert(requests.post(1)); requests.take(); // lifecycle discards pending edges
    assert(requests.take() == 0);

    std::vector<Candidate> refs{Candidate{30,{30,0,0}},Candidate{10,{10,0,0}},Candidate{20,{20,0,0}}};
    std::vector<std::uint32_t> calls;
    auto validate = [&](const Candidate& c) { calls.push_back(c.id); return c.id == 10 ? Validation::invalid : Validation::valid; };
    auto checked = lazyValidate(refs,{},180,false,2,validate);
    assert(calls == std::vector<std::uint32_t>({10,20}));
    assert(checked.exhausted && checked.accepted.size() == 1 && checked.accepted[0].id == 20);
    Policy p;
    assert(p.acceptScan(active(),checked,{},180,0,{},true));
    assert(p.current() == 20);
    calls.clear();
    checked = lazyValidate(refs,{},180,false,8,validate);
    assert(calls == std::vector<std::uint32_t>({10,20,30}));
    assert(p.acceptScan(active(),checked,{},180,1,{},true));
    assert(requests.post(1) && requests.post(-1));
    batch = requests.take();
    while (batch) assert(p.cycle(CycleRequests::pop(batch),{},1));
    assert(p.current() == 20 && p.latched());
    const auto frozen = p.order();
    // A refused first ray is unknown, not an occlusion; neither it nor a
    // contended scan may clear the latch or permit a farther nearest candidate.
    checked = lazyValidate(refs,{},180,false,8,[](const Candidate&) { return Validation::unknown; });
    assert(checked.unknown && checked.evaluated.size() == 1);
    assert(!p.acceptScan(active(),checked,{},180,2,{},true));
    assert(p.latched() && p.current() == 20 && p.order() == frozen);
    assert(!p.acceptScan(active(),{}, {},180,2,{},false));
    assert(p.latched());
    // Definitive invalidation prunes just that target and keeps the neighbour.
    p.invalidate(20);
    assert(!p.latched() && p.current() == 30 && p.order().size() == 1);
    p.invalidate(900); assert(p.current() == 30);
    p.invalidate(30); assert(p.current() == 0);

    // Native seed was inspected but rejected. It is not a new native pick.
    const std::array<std::uint32_t,3> scanned{10,20,30};
    assert(!nativeIsNew(10,scanned,false));
    assert(nativeIsNew(40,scanned,false));
    assert(!nativeIsNew(40,scanned,true)); // crosshair sweep cannot break latch
    assert(!nativeIsNew(0,scanned,false));
    // Frozen member outside the eight nearest remains valid, but cannot skip
    // an unvalidated gap and become nearest after its latch ends.
    p.refresh(active(),refs,{},180,3,{});
    assert(p.push(30,{},3,true));
    LazyResult retained;
    retained.accepted = {refs[0]}; retained.evaluated = {10,20}; retained.exhausted = true;
    assert(p.acceptScan(active(),retained,{},180,4,{},true) && p.current() == 30);
    assert(p.expire({},13,{}) == "timeout" && !p.latched());
    assert(!p.acceptScan(active(),retained,{},180,13,{},true));
    // Expiry is observable even for a single candidate and unchanged target ID.
    p.refresh(active(),std::span<const Candidate>(refs.data(),1),{},180,14,{});
    assert(p.cycle(1,{},14));
    assert(p.expire({},24,{}) == "timeout" && p.current() == 30);
    assert(p.expire({},25,{}).empty());
    assert(p.cycle(1,{},25));
    assert(p.expire({65,0,0},26,{}) == "movement");

    Candidate bench{44,{150,0,0}};
    bench.bounds = {{150,0,0},{-140,-10,-20},{140,10,30}};
    bench.bounds.available = true;
    const auto probes = losPoints(bench,{0,0,40});
    assert(probes[0].x == 10 && probes[0].z == 30); // nearest surface, not distant origin
    assert(probes[1].z == 17.5 && probes[2].z == 5); // includes low.z
    bench.bounds.low = multiply(bench.bounds.low,2); bench.bounds.high = multiply(bench.bounds.high,2);
    const auto scaled = losPoints(bench,{0,0,80});
    assert(scaled[0].x == 0 && scaled[0].z == 60 && scaled[1].z == 35 && scaled[2].z == 10);
    assert(boundsCentre(bench.bounds).z == 10);
    // Repeated external pushes cannot grow or reshuffle a frozen cycle list.
    p.refresh(active(),refs,{},180,30,{});
    assert(p.cycle(1,{},30));
    const auto beforePush = p.order();
    refs[0].position.x = 1;
    p.refresh(active(),refs,{},180,31,{});
    assert(p.push(10,{},31,true) && p.order() == beforePush);
    std::vector<Candidate> crowded;
    for (std::uint32_t i = 1; i <= 20; ++i) crowded.push_back({i,{static_cast<double>(i),0,0}});
    Policy boundedCycle;
    boundedCycle.refresh(active(),crowded,{},180,0,{});
    assert(boundedCycle.cycle(1,{},0) && boundedCycle.order().size() == 8);
    assert(!boundedCycle.push(9,{},0,true));
    assert(boundedCycle.order().size() == 8 && boundedCycle.current() == 2);
}
void nativeSafety() {
    Candidate npc{10,{5,0,0}}, corpse{20,{10,0,0}}, ownedItem{30,{15,0,0}};
    npc.liveActor = true;
    const std::array candidates{npc,corpse,ownedItem};
    assert(rank(candidates,{},180).front().id == 10);
    const auto sneaking = rank(candidates,{},180,true);
    assert(sneaking.size() == 2 && sneaking[0].id == 20 && sneaking[1].id == 30);
    auto c = active(); c.sneaking = true; c.nativeLiveActor = true;
    assert(passthrough(c) == Pass::nativeSneakActor);
    Policy nativeSneak;
    nativeSneak.refresh(c,candidates,{},180,0,{});
    assert(nativeSneak.current() == 0 && !nativeSneak.cycle(1,{},0));
    c.nativeLiveActor = false; assert(passthrough(c) == Pass::none);
    c.sneaking = false; c.nativeLiveActor = true; assert(passthrough(c) == Pass::none);
    assert(!pickChanges(false,true).clearCollider && !pickChanges(false,true).target);
    assert(pickChanges(true,true).clearCollider && pickChanges(true,true).grab);
    assert(pickChanges(true,false).clearCollider && !pickChanges(true,false).grab);
    // An incidental overlay/cell notification has no policy field. Revalidation
    // preserves the latch; only unsafe context or invalid targets cancel it.
    Policy policy;
    policy.refresh(active(),candidates,{},180,0,{});
    assert(policy.cycle(1,{},0));
    policy.refresh(active(),candidates,{},180,.01,{});
    assert(policy.latched() && policy.current() == 20);
}
void preRunRegressions() {
    const std::array<std::uint32_t,3> clutterTypes{0x22,0x23,0x1f};
    std::vector<Candidate> refs;
    for (std::uint32_t i = 0; i < 70; ++i) {
        Candidate c{i+1,{1,0,0}};
        c.activatable = interactiveType(clutterTypes[i%clutterTypes.size()]);
        refs.push_back(c);
    }
    for (std::uint32_t i = 0; i < 8; ++i) refs.push_back({100+i,{10+static_cast<double>(i),0,0}});
    unsigned attempts = 0;
    const auto checked = lazyValidate(refs,{},180,false,8,[&](const Candidate&) {
        ++attempts; return Validation::valid;
    });
    assert(attempts == 8 && checked.accepted.size() == 8 && !checked.exhausted);
    assert(checked.accepted.front().id == 100 && checked.accepted.back().id == 107);
    assert(interactiveType(0x1f,true)); // native carryable torches, not scenery lights
    for (auto type : {0x17u,0x18u,0x19u,0x1cu,0x1du,0x26u,0x27u,0x28u,0x2bu,0x32u}) assert(interactiveType(type));
    // Binary-reviewed canPick type branch (40B2DF..40B2F4), all byte values.
    for (std::uint32_t type = 0; type <= 0xff; ++type) {
        for (const bool carryLight : {false,true}) {
            const bool nativeAllows = type != 0x22 && type != 0x23 && (type != 0x1f || carryLight);
            assert(interactiveType(type,carryLight) == nativeAllows);
        }
    }
    std::array<Candidate,3> loot{Candidate{1,{10,0,0}},Candidate{2,{20,0,0}},Candidate{3,{30,0,0}}};
    loot[0].activatable = interactiveType(0x17); // scroll before book
    loot[1].activatable = interactiveType(0x1b);
    loot[2].activatable = interactiveType(0x32); // stuck arrow remains reachable
    Policy lootPolicy;
    lootPolicy.refresh(active(),loot,{},180,0,{});
    assert(lootPolicy.current() == 1 && lootPolicy.order().size() == 3);
    assert(lootPolicy.cycle(1,{},0) && lootPolicy.current() == 2);
    assert(lootPolicy.cycle(1,{},0) && lootPolicy.current() == 3);
    // Named eligibility is an adapter predicate, with no ranking bonus.
    refs.back().activatable = false;
    assert(rank(refs,{},180).size() == 7);

    auto box = referenceBounds({0,150,0},{0,0,1.5707963267948966},1,{-140,-10,0},{140,10,30});
    assert(validBounds(box));
    assert(std::abs(distanceSquared({},closestPoint(box,{}))-100) < 1e-8);
    box = referenceBounds({10,20,30},{.2,-.3,.7},2,{-10,-20,0},{10,20,60});
    assert(validBounds(box));
    const auto centre = boundsCentre(box);
    assert(distanceSquared(centre,closestPoint(box,centre)) < 1e-8);
    assert(!validBounds(referenceBounds({}, {},0,{-1,-1,-1},{1,1,1})));
    assert(!validBounds(referenceBounds({}, {},1,{},{})));

    std::uint64_t retries = 0;
    unsigned calls = 0;
    assert(tryReadAcquire(1,[&](std::int32_t old, std::int32_t next) {
        assert(next == old+1); return ++calls < 3 ? old+1 : old;
    },retries));
    assert(calls == 3 && retries == 2);
    calls = 0;
    assert(!tryReadAcquire(-1,[&](std::int32_t old, std::int32_t) { ++calls; return old; },retries));
    assert(calls == 0);
    assert(!tryReadAcquire(0,[&](std::int32_t, std::int32_t) { ++calls; return std::int32_t{-1}; },retries));
    assert(calls == 1); // a writer appearing after CAS stops retries immediately
    calls = 0;
    assert(!tryReadAcquire(0,[&](std::int32_t old, std::int32_t) { ++calls; return old+1; },retries));
    assert(calls == 8);
    calls = 0;
    assert(!tryReadAcquire(0x0fffffff,[&](std::int32_t old, std::int32_t) { ++calls; return old; },retries));
    assert(calls == 0);
    RefreshGate gate; gate.last = 1;
    assert(!gate.due(1.01,.1)); gate.reset(); assert(gate.due(1.01,.1));
    CycleRequests edges;
    assert(edges.post(1) && edges.post(-1));
    assert(CycleRequests::count(edges.take()) == 2 && CycleRequests::count(edges.take()) == 0);
}
void rayBudgetExhaustion() {
    const std::array<Candidate,3> refs{Candidate{1,{10,0,0}},Candidate{2,{20,0,0}},Candidate{3,{30,0,0}}};
    ScanBudget rays{1};
    unsigned attempts = 0;
    const auto checked = lazyValidate(refs,{},180,false,8,[&](const Candidate&) {
        ++attempts;
        return rays.inspect() ? Validation::valid : Validation::exhausted;
    });
    assert(attempts == 2 && !rays.complete && rays.inspected == 1);
    assert(checked.exhausted && !checked.unknown);
    assert(checked.evaluated == std::vector<std::uint32_t>({1}));
    assert(checked.accepted.size() == 1 && checked.accepted.front().id == 1);
    Policy policy;
    policy.refresh(active(),refs,{},180,0,{});
    assert(policy.cycle(1,{},0) && policy.current() == 2);
    const auto frozen = policy.order();
    // Adapter passes no ray-incomplete scan to policy; preserve the latch/list.
    assert(!policy.acceptScan(active(),checked,{},180,.01,{},rays.complete));
    assert(policy.latched() && policy.current() == 2 && policy.order() == frozen);
    RefreshGate gate; gate.last = 0;
    assert(!gate.due(.01,.1) && gate.due(.1,.1));

    ScanBudget empty{0};
    const auto noPrefix = lazyValidate(refs,{},180,false,8,[&](const Candidate&) {
        return empty.inspect() ? Validation::valid : Validation::exhausted;
    });
    assert(noPrefix.exhausted && !noPrefix.unknown && noPrefix.accepted.empty() && noPrefix.evaluated.empty());
    assert(!policy.acceptScan(active(),noPrefix,{},180,.1,{},true));
    assert(policy.latched() && policy.current() == 2);
}
void eventDiagnostics() {
    EventLog events;
    assert(events.observe(EventKind::choose,10,EventSource::nearest,"none") == EventEmission::row);
    assert(events.observe(EventKind::choose,10,EventSource::external,"none") == EventEmission::row);
    assert(events.observe(EventKind::cycle,10,EventSource::cycle,"next") == EventEmission::row);
    assert(events.observe(EventKind::cycle,10,EventSource::cycle,"next") == EventEmission::row);
    assert(events.observe(EventKind::pick,10,EventSource::nearest,"none") == EventEmission::row);
    assert(events.observe(EventKind::pick,20,EventSource::nearest,"none") == EventEmission::row);
    for (unsigned i = 0; i < 10000; ++i)
        assert(events.observe(EventKind::pick,20,EventSource::nearest,"none") == EventEmission::none);
    assert(events.sequence() == 6 && events.suppressed() == 10000);
    assert(events.observe(EventKind::pick,10,EventSource::nearest,"none",10) == EventEmission::row);
    assert(events.observe(EventKind::pick,10,EventSource::nearest,"none",20) == EventEmission::row);
    assert(events.observe(EventKind::pick,10,EventSource::nearest,"none",10) == EventEmission::row);
    assert(events.observe(EventKind::pick,20,EventSource::nearest,"none",10) == EventEmission::row);
    assert(events.observe(EventKind::pick,10,EventSource::nearest,"none",10) == EventEmission::row);
    assert(events.observe(EventKind::pick,10,EventSource::native,"menu") == EventEmission::row);
    assert(events.observe(EventKind::latch,10,EventSource::nearest,"timeout") == EventEmission::row);
    assert(events.observe(EventKind::latch,10,EventSource::nearest,"movement") == EventEmission::row);
    assert(events.observe(EventKind::latch,10,EventSource::nearest,"timeout") == EventEmission::none);
    EventLog emptyPick;
    assert(emptyPick.observe(EventKind::pick,0,EventSource::native,"no_candidate") == EventEmission::row);
    assert(emptyPick.observe(EventKind::pick,0,EventSource::native,"no_candidate") == EventEmission::none);
    EventLog budget;
    for (std::uint32_t i = 1; i <= EventLog::limit; ++i) {
        assert(budget.observe(EventKind::pick,i,EventSource::nearest,"none") == EventEmission::row);
        assert(budget.sequence() == i);
    }
    assert(budget.observe(EventKind::choose,7,EventSource::nearest,"none") == EventEmission::dropped);
    assert(budget.sequence() == EventLog::limit+1);
    assert(budget.observe(EventKind::cycle,7,EventSource::cycle,"next") == EventEmission::none);
    assert(budget.suppressed() == 2);

    const std::array names{"none","disabled","unsupported","read_failure","conflict","loading","invalid_world",
        "dead","menu","dialogue","first_person","special","incomplete_scan","no_candidate","armed","combat",
        "telekinesis","native_sneak_actor"};
    for (std::size_t i = 0; i < names.size(); ++i) assert(passName(static_cast<Pass>(i)) == names[i]);
    SelectionEvent event;
    event.kind = EventKind::pick; event.id = 0xFE001ABC; event.nativeID = 0x123;
    event.distance = 12.5; event.bounds = true; event.cycleIndex = 1; event.cycleCount = 3;
    assert(formatEvent(event,EventEmission::row,4,12345) ==
        "selection event=pick seq=4 ms=12345 form_id=FE001ABC distance_units=12.50 distance_source=bounds"
        " source=nearest native_form_id=00000123 reason=none cycle_index=1 cycle_count=3");
    // Formatting must stay machine-readable under a host's non-C locale.
    struct CommaDecimal : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
    const auto previous = std::locale::global(std::locale(std::locale::classic(),new CommaDecimal));
    event.kind = EventKind::cycle; event.source = EventSource::cycle; event.reason = "previous";
    event.bounds = false; event.distance = 0;
    const auto cycle = formatEvent(event,EventEmission::row,5,12346);
    std::locale::global(previous);
    assert(cycle.find("event=cycle") != std::string::npos && cycle.find("distance_units=0.00 distance_source=origin") != std::string::npos);
    assert(cycle.find("source=cycle") != std::string::npos && cycle.find("reason=previous") != std::string::npos);
    event = {}; event.source = EventSource::native; event.reason = passName(Pass::incompleteScan);
    assert(formatEvent(event,EventEmission::row,6,12347) ==
        "selection event=choose seq=6 ms=12347 form_id=00000000 distance_units=unavailable distance_source=origin"
        " source=native native_form_id=00000000 reason=incomplete_scan cycle_index=-1 cycle_count=0");
    for (const auto distance : {-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        event.distance = distance;
        assert(formatEvent(event,EventEmission::row,7,12348).find("distance_units=unavailable") != std::string::npos);
    }
    event.source = EventSource::external;
    assert(formatEvent(event,EventEmission::row,8,12349).find("source=external") != std::string::npos);
    assert(formatEvent(event,EventEmission::none,0,0).empty());
    assert(formatEvent(event,EventEmission::dropped,2001,13000) ==
        "selection event=dropped seq=2001 ms=13000 reason=budget limit=2000");
}
int main() {
    ranking(); latches(); passthroughTable(); inputEdges(); configurationAndBudget(); accessGate(); nativeSafety(); requestsAndLazyValidation(); preRunRegressions(); rayBudgetExhaustion(); eventDiagnostics();
    std::cout << "PASS selection: surface rank/ties/long rotated bench, latch/wrap/invalidation, passthrough, edges, config/budget/API layout, pool-thread/reentrancy gate, atomic FIFO, lazy validation/unknown rays/native seeding/body probes, sneak/combat/telekinesis/collider safety, cheap clutter filtering/reference poses/bounded reader retries, event pairs/repeated cycles/suppression/budget/format\n";
    std::cout << "PASS re-review: all 256 canPick types/carry-light states, scroll/book/projectile cycling, definite ray exhaustion/preserved latch/refresh interval\n";
}
