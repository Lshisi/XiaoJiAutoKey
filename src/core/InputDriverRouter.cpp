#include "InputDriverRouter.h"

#include "DiagLog.h"

#include <Windows.h>

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <thread>
#include <utility>

namespace {

constexpr std::uint8_t kPhysicalCtrlLeft = 0x10;
constexpr std::uint8_t kPhysicalCtrlRight = 0x20;
constexpr std::uint8_t kPhysicalShiftLeft = 0x40;
constexpr std::uint8_t kPhysicalShiftRight = 0x80;
constexpr std::uint8_t kPhysicalAltLeft = 0x01;
constexpr std::uint8_t kPhysicalAltRight = 0x02;

std::uint8_t PhysicalModifierKeyBit(const KeyDescriptor& key) {
    if (key.device != InputDevice::Keyboard) return 0;
    switch (key.code) {
    case VK_LCONTROL: return kPhysicalCtrlLeft;
    case VK_RCONTROL: return kPhysicalCtrlRight;
    case VK_LSHIFT: return kPhysicalShiftLeft;
    case VK_RSHIFT: return kPhysicalShiftRight;
    case VK_LMENU: return kPhysicalAltLeft;
    case VK_RMENU: return kPhysicalAltRight;
    default: return 0;
    }
}

}

void InputDriverRouter::SetDriver(InputDriver* driver) noexcept {
    std::lock_guard lock(mutex_);
    heldDownKeys_.store(nullptr);
    driver_ = driver;
    selfInjectedDriver_.store(driver);
    forwardedDown_.clear();
    repeaterState_.heldKeys.clear();
    macroState_.heldKeys.clear();
    arbitrationSkipDiag_.clear();
}

void InputDriverRouter::NotePhysicalKey(const KeyDescriptor& key, bool pressed) {
    const auto bit = PhysicalModifierKeyBit(key);
    if (bit == 0) return;
    auto modifiers = physicalModifiers_.load(std::memory_order_relaxed);
    while (true) {
        const auto next = pressed
            ? static_cast<std::uint8_t>(modifiers | bit)
            : static_cast<std::uint8_t>(modifiers & ~bit);
        if (physicalModifiers_.compare_exchange_weak(modifiers, next,
            std::memory_order_relaxed, std::memory_order_relaxed)) {
            return;
        }
    }
}

InputDriver* InputDriverRouter::CurrentDriver() const noexcept {
    std::lock_guard lock(mutex_);
    return driver_;
}

bool InputDriverRouter::IsReady() const noexcept {
    std::lock_guard lock(mutex_);
    return driver_ && driver_->IsReady();
}

std::wstring InputDriverRouter::GetLastError() const {
    std::lock_guard lock(mutex_);
    return driver_ ? driver_->GetLastError() : L"No input driver selected";
}

bool InputDriverRouter::SendTap(const KeyDescriptor& key, InputSource src) {
    if (!SendDown(key, src)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return SendUp(key, src);
}

bool InputDriverRouter::SendDown(const KeyDescriptor& key, InputSource src) {
    std::lock_guard lock(mutex_);
    return SendComboDownLocked(key, src);
}

bool InputDriverRouter::SendHeldDown(const KeyDescriptor& key, InputSource src) {
    std::lock_guard lock(mutex_);
    std::vector<std::uint64_t> heldKeys;
    if (IsArbitratedSource(src)) {
        auto& state = StateFor(src);
        const auto components = KeyComponents(key);
        const bool allActive = std::all_of(components.begin(), components.end(),
            [&state](const KeyDescriptor& component) {
                return state.activeKeys.count(EncodeKey(component)) > 0;
            });
        if (allActive) {
            for (const auto& component : components) {
                const auto encoded = EncodeKey(component);
                heldKeys.push_back(encoded);
                state.heldKeys.insert(encoded);
            }
        }
    }
    const bool sent = SendComboDownLocked(key, src);
    PublishHeldKeysLocked();
    if (!sent && IsArbitratedSource(src)) {
        auto& state = StateFor(src);
        for (const auto encoded : heldKeys) state.heldKeys.erase(encoded);
    }
    return sent;
}

bool InputDriverRouter::SendDownLocked(const KeyDescriptor& key, InputSource src) {
    if (!driver_) return false;
    if (!IsArbitratedSource(src)) return driver_->SendDown(key, src);

    const auto encoded = EncodeKey(key);
    const auto& state = StateFor(src);
    const auto owner = owners_.find(encoded);
    if (state.activeKeys.count(encoded) == 0
        || (owner != owners_.end() && owner->second != src)) {
        NoteArbitrationSkipLocked(encoded, src,
            state.activeKeys.count(encoded) == 0
                ? "key not in source active set"
                : "owner lost to other source");
        return true;
    }

    const auto diagKey = ArbitrationDiagKey(encoded, src);

    const auto forwarded = forwardedDown_.find(encoded);
    if (forwarded != forwardedDown_.end()) {
        if (forwarded->second == src) {
            arbitrationSkipDiag_.erase(diagKey);
            return true;
        }
        if (!ReleaseForwardedKeyLocked(encoded)) return false;
    }

    if (!driver_->SendDown(key, src)) return false;
    forwardedDown_[encoded] = src;
    arbitrationSkipDiag_.erase(diagKey);
    return true;
}

bool InputDriverRouter::SendComboDownLocked(const KeyDescriptor& key, InputSource src) {
    if (!driver_) return false;
    const auto components = KeyComponents(key);
    if (components.empty()) return false;

    if (!IsArbitratedSource(src)) {
        std::vector<KeyDescriptor> sent;
        sent.reserve(components.size());
        for (const auto& component : components) {
            const auto modifierMask = ModifierMaskFromKey(component);
            if (modifierMask != 0 && IsPhysicalModifierActiveLocked(modifierMask)) continue;
            if (!driver_->SendDown(component, src)) {
                for (auto it = sent.rbegin(); it != sent.rend(); ++it) {
                    driver_->SendUp(*it, src);
                }
                return false;
            }
            sent.push_back(component);
        }
        return true;
    }

    std::vector<std::uint64_t> sent;
    sent.reserve(components.size());
    for (const auto& component : components) {
        const auto modifierMask = ModifierMaskFromKey(component);
        if (modifierMask != 0 && IsPhysicalModifierActiveLocked(modifierMask)) continue;
        const auto encoded = EncodeKey(component);
        if (!SendDownLocked(component, src)) {
            for (auto it = sent.rbegin(); it != sent.rend(); ++it) {
                const auto forwarded = forwardedDown_.find(*it);
                if (forwarded != forwardedDown_.end() && forwarded->second == src) {
                    ReleaseForwardedKeyLocked(*it);
                }
            }
            return false;
        }
        sent.push_back(encoded);
    }
    return true;
}

bool InputDriverRouter::SendUp(const KeyDescriptor& key, InputSource src) {
    std::lock_guard lock(mutex_);
    const auto components = KeyComponents(key);
    if (IsArbitratedSource(src)) {
        auto& state = StateFor(src);
        bool changed = false;
        for (const auto& component : components) {
            changed |= state.heldKeys.erase(EncodeKey(component)) > 0;
        }
        if (changed) PublishHeldKeysLocked();
    }
    if (!driver_) return false;
    if (!IsArbitratedSource(src)) {
        bool ok = true;
        for (auto it = components.rbegin(); it != components.rend(); ++it) {
            const auto modifierMask = ModifierMaskFromKey(*it);
            if (modifierMask != 0 && IsPhysicalModifierActiveLocked(modifierMask)) continue;
            ok = driver_->SendUp(*it, src) && ok;
        }
        return ok;
    }

    bool ok = true;
    for (auto it = components.rbegin(); it != components.rend(); ++it) {
        const auto encoded = EncodeKey(*it);
        const auto forwarded = forwardedDown_.find(encoded);
        if (forwarded == forwardedDown_.end() || forwarded->second != src) continue;
        ok = ReleaseForwardedKeyLocked(encoded) && ok;
    }
    return ok;
}

bool InputDriverRouter::SendMouseMove(int deltaX, int deltaY, InputSource src) {
    (void)src;  // 光标移动是瞬态操作，不参与按键所有权仲裁。
    std::lock_guard lock(mutex_);
    return driver_ && driver_->SendMouseMove(deltaX, deltaY, src);
}

bool InputDriverRouter::SupportsMouseMove() const {
    std::lock_guard lock(mutex_);
    return driver_ && driver_->SupportsMouseMove();
}

void InputDriverRouter::SetSourceActiveKeys(InputSource src, const std::vector<KeyDescriptor>& keys) {
    if (!IsArbitratedSource(src)) return;

    std::lock_guard lock(mutex_);
    auto& state = StateFor(src);

    std::unordered_set<std::uint64_t> desiredKeys;
    for (const auto& key : keys) {
        for (const auto& component : KeyComponents(key)) {
            if (component.IsValid()) desiredKeys.insert(EncodeKey(component));
        }
    }

    for (auto it = state.activeKeys.begin(); it != state.activeKeys.end();) {
        if (desiredKeys.count(it->first) == 0) {
            state.heldKeys.erase(it->first);
            it = state.activeKeys.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto encoded : desiredKeys) {
        if (state.activeKeys.count(encoded) == 0) {
            state.activeKeys.emplace(encoded, nextActivationOrder_++);
        }
    }

    RecomputeOwnersLocked();
}

void InputDriverRouter::ReleaseSource(InputSource src) {
    if (!IsArbitratedSource(src)) return;
    SetSourceActiveKeys(src, {});
}

void InputDriverRouter::ReleaseAll() {
    std::lock_guard lock(mutex_);
    heldDownKeys_.store(nullptr);
    if (driver_) driver_->ReleaseAll();
    forwardedDown_.clear();
    owners_.clear();
    repeaterState_.activeKeys.clear();
    macroState_.activeKeys.clear();
    repeaterState_.heldKeys.clear();
    macroState_.heldKeys.clear();
}

void InputDriverRouter::ClearAllStates() {
    std::lock_guard lock(mutex_);
    heldDownKeys_.store(nullptr);
    if (driver_) driver_->ClearAllStates();
    forwardedDown_.clear();
    repeaterState_.heldKeys.clear();
    macroState_.heldKeys.clear();
}

bool InputDriverRouter::ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) {
    // Hook callbacks must not wait on the router while driver I/O is in progress.
    auto* driver = selfInjectedDriver_.load();
    return driver && driver->ConsumeSelfInjected(key, pressed);
}

bool InputDriverRouter::IsHeldDown(const KeyDescriptor& key) const noexcept {
    const auto heldKeys = heldDownKeys_.load();
    if (!heldKeys) return false;
    const auto components = KeyComponents(key);
    return std::all_of(components.begin(), components.end(),
        [&heldKeys](const KeyDescriptor& component) {
            return heldKeys->count(EncodeKey(component)) > 0;
        });
}

bool InputDriverRouter::IsArbitratedSource(InputSource src) noexcept {
    return src == InputSource::Repeater || src == InputSource::Macro;
}

std::uint64_t InputDriverRouter::EncodeKey(const KeyDescriptor& key) noexcept {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
}

std::vector<KeyDescriptor> InputDriverRouter::KeyComponents(const KeyDescriptor& key) {
    return ExpandKeyComponents(key);
}

std::uint64_t InputDriverRouter::ArbitrationDiagKey(std::uint64_t encoded, InputSource src) noexcept {
    return (encoded << 1) | (src == InputSource::Repeater ? 0u : 1u);
}

KeyDescriptor InputDriverRouter::DecodeKey(std::uint64_t encoded) noexcept {
    return {
        (encoded >> 32) != 0 ? InputDevice::Mouse : InputDevice::Keyboard,
        static_cast<std::uint32_t>(encoded & 0xFFFFFFFFull)
    };
}

InputDriverRouter::SourceState& InputDriverRouter::StateFor(InputSource src) {
    return src == InputSource::Repeater ? repeaterState_ : macroState_;
}

const InputDriverRouter::SourceState& InputDriverRouter::StateFor(InputSource src) const {
    return src == InputSource::Repeater ? repeaterState_ : macroState_;
}

void InputDriverRouter::RecomputeOwnersLocked() {
    heldDownKeys_.store(nullptr);
    std::unordered_map<std::uint64_t, InputSource> updatedOwners;
    for (const auto& [encoded, repeaterOrder] : repeaterState_.activeKeys) {
        const auto macro = macroState_.activeKeys.find(encoded);
        if (macro == macroState_.activeKeys.end()) continue;

        updatedOwners.emplace(
            encoded,
            repeaterOrder < macro->second ? InputSource::Repeater : InputSource::Macro);
    }

    for (auto it = forwardedDown_.begin(); it != forwardedDown_.end();) {
        const auto encoded = it->first;
        const auto source = it->second;
        const auto owner = updatedOwners.find(encoded);
        const bool sourceStillOwnsKey = StateFor(source).activeKeys.count(encoded) > 0;
        const bool ownershipChanged = owner != updatedOwners.end() && owner->second != source;
        if (!sourceStillOwnsKey || ownershipChanged) {
            const auto current = it++;
            if (driver_ && driver_->SendUp(DecodeKey(encoded), source)) {
                forwardedDown_.erase(current);
            }
            continue;
        }
        ++it;
    }

    owners_ = std::move(updatedOwners);
    RestoreHeldKeysLocked(InputSource::Repeater);
    RestoreHeldKeysLocked(InputSource::Macro);
    PublishHeldKeysLocked();
}

void InputDriverRouter::RestoreHeldKeysLocked(InputSource src) {
    // A held key has no repeat pulse to retry after the earlier owner stops.
    std::vector<std::uint64_t> held(StateFor(src).heldKeys.begin(), StateFor(src).heldKeys.end());
    std::stable_sort(held.begin(), held.end(), [](const auto left, const auto right) {
        return IsKeyboardModifierKey(DecodeKey(left)) > IsKeyboardModifierKey(DecodeKey(right));
    });
    for (const auto encoded : held) {
        const auto key = DecodeKey(encoded);
        const auto modifierMask = ModifierMaskFromKey(key);
        if (modifierMask != 0 && IsPhysicalModifierActiveLocked(modifierMask)) continue;
        SendDownLocked(key, src);
    }
}

void InputDriverRouter::PublishHeldKeysLocked() {
    HeldKeySet heldKeys;
    for (const auto& [encoded, source] : forwardedDown_) {
        if (StateFor(source).heldKeys.count(encoded) > 0) {
            heldKeys.insert(encoded);
        }
    }

    const auto current = heldDownKeys_.load();
    if (current && *current == heldKeys) return;
    if (heldKeys.empty()) {
        heldDownKeys_.store(nullptr);
    } else {
        heldDownKeys_.store(std::make_shared<const HeldKeySet>(std::move(heldKeys)));
    }
}

bool InputDriverRouter::ReleaseForwardedKeyLocked(std::uint64_t encoded) {
    const auto forwarded = forwardedDown_.find(encoded);
    if (forwarded == forwardedDown_.end()) return true;
    if (!driver_ || !driver_->SendUp(DecodeKey(encoded), forwarded->second)) return false;
    forwardedDown_.erase(forwarded);
    return true;
}

void InputDriverRouter::NoteArbitrationSkipLocked(std::uint64_t encoded, InputSource src, const char* reason) {
    const auto diagKey = ArbitrationDiagKey(encoded, src);
    if (arbitrationSkipDiag_.count(diagKey) > 0) {
        return;
    }
    arbitrationSkipDiag_.insert(diagKey);

    const KeyDescriptor key = DecodeKey(encoded);
    char line[160] = {};
    std::snprintf(line, sizeof(line), "%s SendDown skipped (%s): %s:%u -> not injected",
        src == InputSource::Repeater ? "repeater" : "macro",
        reason,
        key.device == InputDevice::Mouse ? "mouse" : "keyboard",
        key.code);
    DiagLog::Write(line);
}

bool InputDriverRouter::IsPhysicalModifierActiveLocked(std::uint8_t modifiers) const {
    if (modifiers == 0) return false;
    const auto physical = physicalModifiers_.load(std::memory_order_relaxed);
    if ((modifiers & kKeyModifierCtrl) != 0
        && (physical & (kPhysicalCtrlLeft | kPhysicalCtrlRight)) != 0) {
        return true;
    }
    if ((modifiers & kKeyModifierShift) != 0
        && (physical & (kPhysicalShiftLeft | kPhysicalShiftRight)) != 0) {
        return true;
    }
    if ((modifiers & kKeyModifierAlt) != 0
        && (physical & (kPhysicalAltLeft | kPhysicalAltRight)) != 0) {
        return true;
    }
    return false;
}

bool InputDriverRouter::SendMouseWheel(int delta, InputSource src) {
    std::lock_guard lock(mutex_);
    return driver_ && driver_->SendMouseWheel(delta, src);
}
bool InputDriverRouter::SupportsMouseWheel() const {
    std::lock_guard lock(mutex_);
    return driver_ && driver_->SupportsMouseWheel();
}
