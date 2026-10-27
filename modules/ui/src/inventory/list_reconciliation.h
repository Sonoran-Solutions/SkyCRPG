// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once

namespace skycrpg::ui::inventory {
// Keep the request pending through Update's synchronous selection callbacks.
// Only the queued UI task completes it, including a cancelled menu session.
class ListReconciliation {
public:
    // Pending coalesces queued work; only this scope forbids live list reads.
    class UpdateScope {
    public:
        explicit UpdateScope(ListReconciliation& state) : _state(state), _previous(state._updating) {
            _state._updating = true;
        }
        ~UpdateScope() { _state._updating = _previous; }
        UpdateScope(const UpdateScope&) = delete;
        UpdateScope& operator=(const UpdateScope&) = delete;
    private:
        ListReconciliation& _state;
        bool _previous;
    };
    bool Request() {
        if (_pending) return false;
        _pending = true;
        return true;
    }
    bool Pending() const { return _pending; }
    bool Updating() const { return _updating; }
    void Complete() { _pending = false; }
private:
    bool _pending{};
    bool _updating{};
};
}
