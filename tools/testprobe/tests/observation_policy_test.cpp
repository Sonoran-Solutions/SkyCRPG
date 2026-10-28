// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/observation_policy.h"
#include <cassert>
using namespace skycrpg::testprobe;
int main()
{
	assert(kObservationProtocol == 2);
	const auto raw = ParseStateRequest(std::string("raw\n\0tag", 8));
	assert(raw.valid && raw.tag.size() == 8 && raw.count == 0);
	const auto one = ParseStateRequest("@state before 0x000139BA,0x00012eb7");
	assert(one.valid && one.tag == "before" && one.count == 2 && one.forms[1] == 0x12EB7);
	for (const auto* bad : {"@state x", "@state  0x00000001", "@state x ", "@state x 0x00000000",
		"@state x 0x00000001,", "@state x 0x00000001,0x00000001", "@state x 0x12345ZZZ", "@state x 0000000001",
		"@state x 0x00000001\n", "@state x 0x000000001", "@state x/y 0x00000001"}) {
		assert(!ParseStateRequest(bad).valid);
	}
	std::string sixteen = "@state limit ";
	for (unsigned i = 1; i <= 16; ++i) {
		if (i > 1) sixteen += ',';
		constexpr char hex[] = "0123456789ABCDEF";
		sixteen += "0x000000";
		sixteen += hex[i >> 4]; sixteen += hex[i & 15];
	}
	assert(ParseStateRequest(sixteen).valid && ParseStateRequest(sixteen).count == 16);
	assert(!ParseStateRequest(sixteen + ",0x00000011").valid);
	for (unsigned type = 0; type <= 10; ++type)
		assert(TwoHandedWeapon(type) == (type == 5 || type == 6 || type == 7 || type == 9));
}
