// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/classifier.h"
#include "../src/config.h"
#include "../src/runtime_policy.h"
#include <limits>
#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (false)
using namespace skycrpg::occlusion;
int main()
{
	CHECK(OverheadCandidate({ -1, 1, true }));
	CHECK(!OverheadCandidate({ -.5f, 1, true }));
	CHECK(!OverheadCandidate({ -1, 2, true }));
	CHECK(!OverheadCandidate({ -1, 1, false }));
	CHECK(!OverheadCandidate({ std::numeric_limits<float>::quiet_NaN(), 1, true }));
	IDs refs{}, bases{};
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
	CHECK(ContinueRay({0,0,0}, {100,0,0}, {20,0,0}, next)); CHECK(next.x > 20 && next.x < 20.001f);
	// Beam then enclosure: a continuation reaches the wall; it must not be
	// allowlisted by the earlier beam's identity.
	CHECK(ContinueRay(next, {100,0,0}, {60,0,0}, next)); CHECK(next.x > 60 && next.x < 60.001f);
	CHECK(!ContinueRay({}, {}, {}, next));
	CHECK(!ContinueRay({}, {100,0,0}, {101,0,0}, next));
	CHECK(!ContinueRay({}, {100,0,0}, {-1,0,0}, next));
	CHECK(!ContinueRay({}, {100,0,0}, {100,0,0}, next));
	CHECK(!ContinueRay({}, {100,0,0}, {std::numeric_limits<float>::infinity(),0,0}, next));
	CHECK(ContinueRay({100,20,10}, {}, {50,10,5}, next)); CHECK(next.x < 50 && next.y < 10 && next.z < 5);
	unsigned order{};
	bool correctOrder = true;
	CHECK(!Seed(2, {}, {8,0,0}, {}, 0, 10, [&](Point p) { correctOrder = correctOrder && p.x == 8 - 2*order; ++order; return false; }));
	CHECK(correctOrder);
	CHECK(order == 4);
	Continuation continuation;
	Point origin{};
	for (unsigned i = 0; i < maxRecasts; ++i) {
		CHECK(continuation.advance(origin, {100,0,0}, {float(i+1),0,0}, next) == Continuation::Step::cast);
		origin = next;
	}
	CHECK(continuation.count() == maxRecasts);
	CHECK(continuation.advance(origin, {100,0,0}, {20,0,0}, next) == Continuation::Step::depthLimit);
	Continuation overlap;
	CHECK(overlap.advance({20,0,0}, {100,0,0}, {19,0,0}, next) == Continuation::Step::noProgress);
	CHECK(overlap.count() == 0);

}
