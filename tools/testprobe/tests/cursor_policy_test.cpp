// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/cursor_policy.h"
#include <cassert>
#include <limits>
using namespace skycrpg::testprobe;
int main()
{
 assert(CursorNumber(true, 1.0));
 assert(!CursorNumber(false, 1.0));
 assert(!CursorNumber(true, std::numeric_limits<double>::infinity()));
 assert(CursorHitKnown(true, true, true));
 assert(!CursorHitKnown(false, true, true));
 assert(!CursorHitKnown(true, false, true));
 assert(!CursorHitKnown(true, true, false));
}
