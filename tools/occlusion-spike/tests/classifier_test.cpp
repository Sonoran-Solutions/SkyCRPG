// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/classifier.h"
#include "../src/config.h"
#include "../src/runtime_policy.h"
#include <limits>
#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (false)
using namespace skycrpg::occlusion;
// One-dimensional swept hull fixture. The closest-hit adapter deliberately
// misses surfaces when starting inside them; the all-contact prefix must still
// reject enclosure overlaps instead of trusting that miss.
struct Solid { float lo, hi; unsigned id; bool allowed; };
struct MarchResult { unsigned id, casts, guards; float end; bool rollback; };
MarchResult March(const std::vector<Solid>& solids, float goal = 400)
{
	unsigned casts{}, guards{};
	auto cast = [&](float from) {
		++casts;
		const Solid* nearest = nullptr;
		float distance = goal;
		for (const auto& solid : solids) {
			if (solid.hi < from || solid.lo > goal) continue;
			if (solid.lo < from && !solid.allowed) continue; // adversarial embedded enclosure
			const float hit = std::max(from, solid.lo);
			if (hit < distance) { distance = hit; nearest = &solid; }
		}
		return std::pair{ nearest, distance };
	};
	float from{};
	auto [solid, end] = cast(from);
	const unsigned first = solid ? solid->id : 0;
	const float firstEnd = end;
	Continuation continuation;
	bool rollback = false;
	while (solid && solid->allowed) {
		Point next;
		if (continuation.advance({from,0,0}, {goal,0,0}, {end,0,0}, 15, next) != Continuation::Step::cast) { rollback = true; break; }
		GapGuard guard; ++guards;
		for (const auto& item : solids) if (item.hi >= from && item.lo <= next.x) guard.contact(true, item.allowed);
		if (!guard.safe()) { rollback = true; break; }
		from = next.x;
		const auto hit = cast(from); solid = hit.first; end = hit.second;
	}
	if (casts > 1) ++casts; // original-input context reset
	return { rollback ? first : solid ? solid->id : 0, casts, guards, rollback ? firstEnd : end, rollback };
}
int main()
{
	CHECK(OverheadCandidate({ -1, 1, true }));
	CHECK(!OverheadCandidate({ -.5f, 1, true }));
	CHECK(!OverheadCandidate({ -1, 2, true }));
	CHECK(!OverheadCandidate({ -1, 1, false }));
	CHECK(!OverheadCandidate({ std::numeric_limits<float>::quiet_NaN(), 1, true }));
	IDs refs{}, bases{};
	CHECK(ParseIDs("none", refs)); CHECK(refs[0] == 0);
	CHECK(ParseIDs(" 0x12345678, 00000001 ", refs));
	CHECK(ParseIDs("ABCD", bases));
	CHECK(Allowlisted(1, 2, refs, bases)); CHECK(Allowlisted(2, 0xabcd, refs, bases));
	CHECK(!Allowlisted(0, 0, refs, bases)); CHECK(!Allowlisted(2, 2, refs, bases));
	for (auto s : {"0", "0x", "1,", ",1", "-1", "1garbage", "100000000", "1,,2"}) CHECK(!ParseIDs(s, bases));
	CHECK(ParseMode("observe") == Mode::observe); CHECK(ParseMode("observe,filter") == Mode::invalid);
	CHECK(ParseMode("room_seed_3") == Mode::roomSeed3); CHECK(ParseMode("near_clip") == Mode::nearClip);
	CHECK(CanDiscard(Mode::filter, true, true, true, true, true, true));
	for (unsigned bits = 0; bits < 63; ++bits)
		CHECK(!CanDiscard(Mode::filter, bits&1, bits&2, bits&4, bits&8, bits&16, bits&32));
	CHECK(!CanDiscard(Mode::observe, true, true, true, true, true, true));
	CHECK(!ValidAlpha(.001f)); CHECK(!ValidAlpha(.099f)); CHECK(ValidAlpha(.1f)); CHECK(!ValidAlpha(1)); CHECK(!ValidAlpha(0)); CHECK(ValidAlpha(.35f));
	unsigned calls{};
	auto contains = [&](Point p) { ++calls; return p.x == 8; };
	CHECK(Seed(1, {0,0,0}, {8,0,0}, {}, 0, 10, contains)); CHECK(calls == 1);
	calls = 0; CHECK(Seed(2, {0,0,0}, {8,0,0}, {}, 0, 10, contains)); CHECK(calls == 1);
	CHECK(!Seed(0, {}, {}, {}, 0, 10, contains));
	CHECK(Seed(3, {}, {}, {9,0,0}, 1, 10, contains));
	CHECK(!Seed(3, {}, {}, {11,0,0}, 1, 10, contains));
	CHECK(!Seed(3, {}, {}, {}, -1, 10, contains));
	CHECK(MainThread(42, 42)); CHECK(!MainThread(43, 42)); CHECK(!MainThread(0, 0));
	Requests requests;
	requests.post(armRequest); requests.post(menuRequest);
	CHECK(!MayArm(requests.take())); CHECK(!requests.pending());
	requests.post(stopRequest); requests.post(armRequest); CHECK(!MayArm(requests.take()));
	requests.post(armRequest); CHECK(MayArm(requests.take()));
	Point next;
	CHECK(ContinueRay({0,0,0}, {100,0,0}, {20,0,0}, 15, next)); CHECK(next.x == 50);
	CHECK(ContinueRay({0,0,0}, {200,0,0}, {20,0,0}, 30, next)); CHECK(next.x == 80);
	CHECK(ContinueRay({20,0,0}, {100,0,0}, {19.953f,0,0}, 15, next)); CHECK(next.x == 50);
	CHECK(!ContinueRay({}, {}, {}, 15, next));
	CHECK(!ContinueRay({}, {100,0,0}, {101,0,0}, 15, next));
	CHECK(!ContinueRay({}, {100,0,0}, {70,0,0}, 15, next));
	CHECK(!ContinueRay({}, {100,0,0}, {100,0,0}, 15, next));
	CHECK(!ContinueRay({}, {100,0,0}, {}, 0, next));
	CHECK(!ContinueRay({}, {100,0,0}, {std::numeric_limits<float>::infinity(),0,0}, 15, next));
	CHECK(ContinueRay({100,20,10}, {}, {50,10,5}, 15, next)); CHECK(next.x < 50 && next.y < 10 && next.z < 5);
	for (auto mode : {Mode::filterMaterialAlpha, Mode::filterAlphaProperty}) {
		CHECK(FilterMode(mode) && FadeMode(mode) && CasterMode(mode));
		CHECK(CanDiscard(mode, true, true, true, true, true, true));
		CHECK(!CanDiscard(mode, false, true, true, true, true, true));
		CHECK(ParseIDs("E7A7B", refs)); bases = {};
		CHECK(CombinedTarget(mode, 0xe7a7b, refs, bases));
		CHECK(!CombinedTarget(mode, 0x123, refs, bases));
		bases[0] = 0x602a6; CHECK(!CombinedTarget(mode, 0xe7a7b, refs, bases));
		bases = {}; refs[1] = 1; CHECK(!CombinedTarget(mode, 0xe7a7b, refs, bases));
	}
	CHECK(ParseMode("filter_material_alpha") == Mode::filterMaterialAlpha);
	CHECK(ParseMode("filter_alpha_property") == Mode::filterAlphaProperty);
	CHECK(AlphaPropertyMode(Mode::filterAlphaProperty));
	CHECK(!AlphaPropertyMode(Mode::filterMaterialAlpha));
	unsigned order{};
	bool correctOrder = true;
	CHECK(!Seed(2, {}, {8,0,0}, {}, 0, 10, [&](Point p) { correctOrder = correctOrder && p.x == 8 - 2*order; ++order; return false; }));
	CHECK(correctOrder);
	CHECK(order == 4);
	Continuation continuation;
	Point origin{};
	for (unsigned i = 0; i < maxRecasts; ++i) {
		CHECK(continuation.advance(origin, {1000,0,0}, origin, 15, next) == Continuation::Step::cast);
		CHECK(next.x - origin.x >= 30);
		origin = next;
	}
	CHECK(continuation.count() == maxRecasts);
	CHECK(continuation.advance(origin, {1000,0,0}, origin, 15, next) == Continuation::Step::depthLimit);
	CHECK(NativeIgnoredContact(1, true, false));
	CHECK(!NativeIgnoredContact(17, true, false));
	CHECK(NativeIgnoredContact(8, false, true));
	CHECK(!NativeIgnoredContact(8, false, false));
	CHECK(!NativeIgnoredContact(1, false, true));
	GapGuard guard;
	for (unsigned i = 0; i < maxGuardContacts; ++i) guard.contact(true, true);
	CHECK(guard.safe()); guard.contact(true, true); CHECK(!guard.safe());
	GapGuard unknown; unknown.contact(false, true); CHECK(!unknown.safe());
	GapGuard enclosure; enclosure.contact(true, true); enclosure.contact(true, false); CHECK(!enclosure.safe());
	GapGuard lost; lost.invalid(); CHECK(!lost.safe());
	const auto clear = March({{20,45,1,true}});
	CHECK(clear.id == 0 && clear.end == 400 && !clear.rollback && clear.casts == 3);
	const auto wall = March({{20,45,1,true},{100,130,2,false},{200,230,3,false}});
	CHECK(wall.id == 2 && wall.end == 100 && !wall.rollback); // first enclosure returned
	const auto multiple = March({{20,45,1,true},{80,105,2,true},{150,200,3,false}});
	CHECK(multiple.id == 3 && !multiple.rollback && multiple.guards == 2);
	// A <30-unit gap, touching hulls, and an overlapping/embedded wall are not clear.
	for (const auto& solids : {std::vector<Solid>{{20,45,1,true},{46,49,2,false}},
		std::vector<Solid>{{20,45,1,true},{45,75,2,false}},
		std::vector<Solid>{{20,45,1,true},{25,80,2,false}},
		std::vector<Solid>{{20,45,1,true},{-10,90,2,false}}}) {
		const auto result = March(solids); CHECK(result.rollback && result.id == 1 && result.end == 20);
	}
	const auto thick = March({{20,350,1,true}});
	CHECK(thick.rollback && thick.id == 1 && thick.casts <= maxRecasts + 2 && thick.guards == maxRecasts);
	const auto nearGoal = March({{380,395,1,true}});
	CHECK(nearGoal.rollback && nearGoal.id == 1); // never synthesize a miss at the bound
	const auto empty = March({}); CHECK(empty.id == 0 && empty.casts == 1 && empty.end == 400);

}
