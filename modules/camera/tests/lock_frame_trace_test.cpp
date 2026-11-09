// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "lock_frame_trace.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace skycrpg::camera;
int main() {
    LockTraceWindow window;
    std::vector<unsigned> emitted;
    auto emit = [&](const LockTraceRow& row) { emitted.push_back(static_cast<unsigned>(std::stoul(row.data()))); };
    auto sample = [&](unsigned frame, bool locked) {
        LockTraceRow row{};
        std::snprintf(row.data(),row.size(),"%u",frame);
        window.sample(row,locked,emit);
    };
    for (unsigned frame = 0; frame < 10000; ++frame) sample(frame,false);
    assert(emitted.empty() && window.budget.lines == 0);
    sample(10000,true);
    assert(emitted.size() == 17 && emitted.front() == 9984 && emitted.back() == 10000);
    for (unsigned frame = 10001; frame <= 10020; ++frame) sample(frame,false);
    assert(emitted.size() == 33 && emitted.back() == 10016);
    sample(10021,true);
    assert(emitted.size() == 38 && emitted.back() == 10021);
    for (unsigned frame = 10022; frame < 14000; ++frame) sample(frame,true);
    assert(emitted.size() == LockFrameTraceBudget::limit);
    for (unsigned i = 1; i < emitted.size(); ++i) assert(emitted[i] == emitted[i-1]+1);
    const auto path = std::filesystem::temp_directory_path()/
        ("skycrpg-lock-trace-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".log");
    {
        LockFrameWriter writer;
        writer.start(path);
        for (unsigned frame = 0; frame < LockFrameTraceBudget::limit+10; ++frame) {
            LockTraceRow row{};
            std::snprintf(row.data(),row.size(),"%u",frame);
            writer.append(row);
        }
        writer.stop();
    }
    std::ifstream file(path);
    std::string line;
    unsigned count = 0;
    while (std::getline(file,line)) assert(std::stoul(line) == count++);
    assert(count == LockFrameTraceBudget::limit);
    file.close();
    std::filesystem::remove(path);
}
