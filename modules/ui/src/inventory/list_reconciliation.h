// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once

namespace skycrpg::ui::inventory {
// Keep the request pending through Update's synchronous selection callbacks.
// Only the queued UI task completes it, including a cancelled menu session.
class ListReconciliation {
public:
    bool Request() {
        if (_pending) return false;
        _pending = true;
        return true;
    }
    bool Pending() const { return _pending; }
    void Complete() { _pending = false; }
private:
    bool _pending{};
};
}
