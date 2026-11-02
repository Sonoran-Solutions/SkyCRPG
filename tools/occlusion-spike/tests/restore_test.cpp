// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/restore.h"
#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (false)
using skycrpg::occlusion::RestoreValue;
int main()
{
	float alpha = .35f;
	CHECK(RestoreValue(alpha, .8f, .35f)); CHECK(alpha == .8f);
	CHECK(RestoreValue(alpha, .8f, .35f));
	alpha = .5f; CHECK(!RestoreValue(alpha, .8f, .35f)); CHECK(alpha == .5f);
	int original{}, injected{}, foreign{};
	int* property = &injected;
	CHECK(RestoreValue(property, &original, &injected)); CHECK(property == &original);
	property = &injected;
	CHECK(RestoreValue(property, static_cast<int*>(nullptr), &injected)); CHECK(!property);
	property = &foreign;
	CHECK(!RestoreValue(property, &original, &injected)); CHECK(property == &foreign);
}
