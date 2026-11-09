// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
#include "target_lock_policy.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

namespace skycrpg::camera {
using LockTraceRow = std::array<char,640>;
// Retain pre-lock context without consuming the session budget in menus/idle.
struct LockTraceWindow {
    static constexpr unsigned margin = 16;
    std::array<LockTraceRow,margin> before{};
    unsigned count = 0, next = 0, after = 0;
    LockFrameTraceBudget budget;
    template<class Emit> void sample(const LockTraceRow& row, bool locked, Emit emit) {
        const auto write = [&](const LockTraceRow& value) {
            if (budget.take(true,true)) emit(value);
        };
        if (locked) {
            for (unsigned i = 0; i < count; ++i) write(before[(next+margin-count+i)%margin]);
            count = next = 0;
            after = margin;
            write(row);
        } else if (after) {
            --after; write(row);
        } else {
            before[next] = row;
            next = (next+1)%margin;
            if (count < margin) ++count;
        }
    }
};
// Single producer, immutable published rows. Only the worker touches the file;
// no allocation, file I/O, mutex or wait on the measured input-dispatch path.
class LockFrameWriter {
public:
    ~LockFrameWriter() { stop(); }
    void start(const std::filesystem::path& path) {
        if (worker.joinable()) return;
        rows = std::make_unique<LockTraceRow[]>(LockFrameTraceBudget::limit);
        worker = std::thread([this,path] {
            std::ofstream file(path,std::ios::trunc);
            unsigned written = 0;
            for (;;) {
                const bool done = stopping.load(std::memory_order_acquire);
                const auto available = published.load(std::memory_order_acquire);
                for (; written < available; ++written) file << rows[written].data() << '\n';
                file.flush();
                if (done || written == LockFrameTraceBudget::limit) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
    void append(const LockTraceRow& row) {
        const auto index = published.load(std::memory_order_relaxed);
        if (!rows || index >= LockFrameTraceBudget::limit) return;
        rows[index] = row;
        published.store(index+1,std::memory_order_release);
    }
    void stop() {
        stopping.store(true,std::memory_order_release);
        if (worker.joinable()) worker.join();
    }
private:
    std::unique_ptr<LockTraceRow[]> rows;
    std::atomic<unsigned> published{0};
    std::atomic<bool> stopping{false};
    std::thread worker;
};
} // namespace skycrpg::camera
