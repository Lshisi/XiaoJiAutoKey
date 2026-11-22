#include "PauseModifierGate.h"

#include "DiagLog.h"

#include <algorithm>
#include <thread>
#include <utility>

namespace {
constexpr auto kCloseBudget = std::chrono::milliseconds(250);
constexpr unsigned kDispatchBatch = 64;
}

PauseModifierGate::PauseModifierGate(InputDriverRouter& driver, std::function<void()> wake)
    : driver_(driver), wake_(std::move(wake)) {}

std::uint64_t PauseModifierGate::Encode(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse) << 32) | key.code;
}

void PauseModifierGate::Arm(const std::vector<KeyDescriptor>& pauseKeys, HWND foreground) {
    std::lock_guard lock(mutex_);
    pauseKeys_ = pauseKeys;
    armedWindow_ = foreground;
}

void PauseModifierGate::UpdatePauseStateLocked() {
    pausing_.store(barrier_ || std::any_of(owned_.begin(), owned_.end(), [](const auto& entry) {
        // Do not resume F until the virtual Alt UP has actually been delivered.
        return entry.second.pause && (entry.second.down || entry.second.pending != 0);
    }));
    pending_.store(!inputs_.empty());
}

bool PauseModifierGate::InterceptKey(const KeyDescriptor& physicalKey, bool pressed, bool allowStart) {
    KeyDescriptor key = physicalKey;
    key.modifiers = 0;
    const auto encoded = Encode(key);
    {
        std::lock_guard lock(mutex_);
        const bool wasDown = physicalDown_.count(encoded) != 0;
        if (pressed) physicalDown_.insert(encoded);
        else physicalDown_.erase(encoded);
        auto found = owned_.find(encoded);
        if (found == owned_.end()) {
            // Releases of keys pressed before this transaction must pass through.
            if (!pressed || wasDown) return false;
            const bool pause = IsKeyboardModifierKey(key)
                && std::any_of(pauseKeys_.begin(), pauseKeys_.end(), [&](const auto& item) {
                    return item.modifiers == 0 && SamePrimaryKey(item, key);
                });
            const bool begin = allowStart && pause && armedWindow_
                && armedWindow_ == GetForegroundWindow() && driver_.CanRelayPauseModifier(key);
            if (!barrier_ && !begin) return false;
            // This hook query is provided by the selected driver without I/O or engine locks.
            if (!driver_.CanRelayPhysicalKey(key)) return false;
            if (!barrier_ && begin) {
                barrier_ = waitingForClose_ = true;
                transactionWindow_ = armedWindow_;
                started_ = std::chrono::steady_clock::now();
            }
            found = owned_.emplace(encoded, OwnedKey{key, false, pause, 0}).first;
            if (IsKeyboardModifierKey(key)) hookModifiers_.insert(encoded);
            driver_.SetPhysicalModifierIntercepted(key, true);
        }
        auto& owned = found->second;
        // The driver holds a full-state key; physical auto-repeat is not a new stroke.
        if (owned.down == pressed) return true;
        owned.down = pressed;
        ++owned.pending;
        inputs_.push_back({key, pressed});
        UpdatePauseStateLocked();
    }
    wake_();
    return true;
}

bool PauseModifierGate::InterceptWheel(int delta) {
    if (delta == 0) return false;
    {
        std::lock_guard lock(mutex_);
        if (!barrier_) return false;
        inputs_.push_back({{}, false, delta});
        UpdatePauseStateLocked();
    }
    wake_();
    return true;
}

bool PauseModifierGate::IsPausing() const noexcept { return pausing_.load(); }
bool PauseModifierGate::HasPendingInput() const noexcept { return pending_.load(); }

bool PauseModifierGate::UsesHookModifierState(const KeyDescriptor& key) const {
    if (!IsKeyboardModifierKey(key)) return false;
    std::lock_guard lock(mutex_);
    return hookModifiers_.count(Encode(key)) != 0;
}

bool PauseModifierGate::Dispatch(bool closing) {
    bool timedOut = false;
    {
        std::lock_guard lock(mutex_);
        if (waitingForClose_) {
            const bool inWindow = transactionWindow_ == GetForegroundWindow();
            if (closing && inWindow && std::chrono::steady_clock::now() - started_ < kCloseBudget) return true;
            timedOut = closing && inWindow;
            waitingForClose_ = false;
        }
    }
    if (timedOut) DiagLog::Write("martial assist pause modifier handoff: close deferred beyond 250ms; restoring user input");
    for (unsigned count = 0; count < kDispatchBatch; ++count) {
        Input input;
        {
            std::lock_guard lock(mutex_);
            if (inputs_.empty()) {
                barrier_ = false;
                UpdatePauseStateLocked();
                return true;
            }
            input = inputs_.front();
            inputs_.pop_front();
        }
        const bool ok = input.wheel != 0
            ? driver_.SendMouseWheel(input.wheel, InputSource::Injected)
            : driver_.RelayPhysicalKey(input.key, input.pressed);
        if (!ok) {
            DiagLog::Write("martial assist pause modifier handoff failed; releasing relayed input");
            Cancel();
            return false;
        }
        if (IsKeyboardModifierKey(input.key)) {
            DiagLog::Write("pause modifier relayed code=" + std::to_string(input.key.code)
                + " down=" + std::to_string(input.pressed));
        }
        bool shortPress = false;
        {
            std::lock_guard lock(mutex_);
            const auto found = owned_.find(Encode(input.key));
            if (found != owned_.end()) {
                auto& owned = found->second;
                --owned.pending;
                shortPress = input.pressed && !owned.down;
                if (!owned.down && owned.pending == 0) {
                    driver_.SetPhysicalModifierIntercepted(owned.key, false);
                    owned_.erase(found);
                }
            }
            UpdatePauseStateLocked();
        }
        if (shortPress) std::this_thread::sleep_for(std::chrono::milliseconds(kInputPulseWidthMs));
    }
    wake_();
    return true;
}

void PauseModifierGate::Cancel() {
    {
        std::lock_guard lock(mutex_);
        armedWindow_ = nullptr;
        inputs_.clear();
        for (const auto& entry : owned_) driver_.SetPhysicalModifierIntercepted(entry.second.key, false);
        owned_.clear();
        barrier_ = waitingForClose_ = false;
        UpdatePauseStateLocked();
    }
    driver_.ReleaseRelayedKeys();
}
