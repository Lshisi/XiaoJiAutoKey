#pragma once

#include "InputDriverRouter.h"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

// Hook callbacks only queue events. No driver I/O or engine locks are taken
// while Windows is waiting for the low-level hook to return.
class PauseModifierGate final {
public:
    PauseModifierGate(InputDriverRouter& driver, std::function<void()> wake);
    void Arm(const std::vector<KeyDescriptor>& pauseKeys, HWND foreground);
    bool InterceptKey(const KeyDescriptor& key, bool pressed, bool allowStart);
    bool InterceptWheel(int delta);
    [[nodiscard]] bool IsPausing() const noexcept;
    [[nodiscard]] bool HasPendingInput() const noexcept;
    [[nodiscard]] bool UsesHookModifierState(const KeyDescriptor& key) const;
    // Called by the repeater worker after its release/close transaction.
    bool Dispatch(bool closing);
    void Cancel();

private:
    struct Input {
        KeyDescriptor key;
        bool pressed = false;
        int wheel = 0;
    };
    struct OwnedKey {
        KeyDescriptor key;
        bool down = false;
        bool pause = false;
        unsigned pending = 0;
    };
    static std::uint64_t Encode(const KeyDescriptor& key);
    void UpdatePauseStateLocked();

    InputDriverRouter& driver_;
    std::function<void()> wake_;
    // This lock protects queue bookkeeping only, never a driver call or wait.
    mutable std::mutex mutex_;
    std::vector<KeyDescriptor> pauseKeys_;
    HWND armedWindow_ = nullptr;
    HWND transactionWindow_ = nullptr;
    std::chrono::steady_clock::time_point started_{};
    std::deque<Input> inputs_;
    std::unordered_map<std::uint64_t, OwnedKey> owned_;
    std::unordered_set<std::uint64_t> physicalDown_;
    // Once a modifier is intercepted, its physical edges come from the hook
    // for the rest of this process, including strokes subsequently passed through.
    std::unordered_set<std::uint64_t> hookModifiers_;
    bool barrier_ = false;
    bool waitingForClose_ = false;
    std::atomic<bool> pausing_{false};
    std::atomic<bool> pending_{false};
};
