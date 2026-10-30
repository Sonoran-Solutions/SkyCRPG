// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/cursor_policy.h"
#include <cassert>
#include <limits>
using namespace skycrpg::testprobe;
int main()
{
 auto box = SelectedCell(true, true, true, 8, 9, 2, 3);
 assert(box && box->x_min == 2 && box->x_max == 8 && box->y_min == 3 && box->y_max == 9);
 assert(!SelectedCell(false, true, true, 0, 0, 1, 1));
 assert(!SelectedCell(true, false, true, 0, 0, 1, 1));
 assert(!SelectedCell(true, true, false, 0, 0, 1, 1));
 assert(!SelectedCell(true, true, true, 0, 0, std::numeric_limits<double>::infinity(), 1));
 assert(CursorNumber(true, 1.0));
 assert(!CursorNumber(false, 1.0));
 assert(!CursorNumber(true, std::numeric_limits<double>::infinity()));
 assert(CursorHitKnown(true, true, true));
 assert(!CursorHitKnown(false, true, true));
 assert(!CursorHitKnown(true, false, true));
 assert(!CursorHitKnown(true, true, false));
}
