// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/runtime_policy.h"
#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (false)
using namespace skycrpg::testprobe;
int main()
{
	skycrpg::occlusion::FrameRing<100> ring;
	for (unsigned i = 1; i <= 100; ++i) ring.push(i);
	auto s = ring.summary();
	CHECK(s.count == 100 && s.p95 == 95 && s.p99 == 99 && s.median == 50.5 && s.maximum == 100);
	ring.push(101); CHECK(ring.total() == 101 && ring.summary().count == 100 && ring.sum() == 5151);
	ring.clear(); CHECK(ring.sum() == 0);
	skycrpg::occlusion::FrameRing<65536> large;
	static_assert(sizeof(large) < 128);
	for (unsigned i = 0; i < 70000; ++i) large.push(i);
	CHECK(large.summary().count == 65536 && large.total() == 70000);
	large.clear(); CHECK(large.summary().count == 0);
	FrameChain chain; CHECK(chain.start()); CHECK(!chain.start());
	CHECK(chain.uiRun(1) == FrameChain::Tick::sample); chain.stop();
	CHECK(!chain.mainRun()); CHECK(chain.start());
	CHECK(chain.uiRun(2) == FrameChain::Tick::sample);
}
