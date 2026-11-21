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
constexpr std::uint8_t kPhysicalWinLeft = 0x04;
constexpr std::uint8_t kPhysicalWinRight = 0x08;
constexpr std::array<std::uint32_t, 8> kModifierKeys = {
    VK_LCONTROL, VK_RCONTROL, VK_LSHIFT, VK_RSHIFT, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN
};
constexpr auto kIsolatedTapWidth = std::chrono::milliseconds(10);
constexpr auto kModifierReleaseGap = std::chrono::milliseconds(kInputPulseWidthMs);
constexpr std::uint64_t kIsolatedModifierEventTtlMs = 250;

std::uint8_t PhysicalModifierKeyBit(const KeyDescriptor& key) {
    if (key.device != InputDevice::Keyboard) return 0;
    switch (key.code) {
    case VK_LCONTROL: return kPhysicalCtrlLeft;
    case VK_RCONTROL: return kPhysicalCtrlRight;
    case VK_LSHIFT: return kPhysicalShiftLeft;
    case VK_RSHIFT: return kPhysicalShiftRight;
    case VK_LMENU: return kPhysicalAltLeft;
    case VK_RMENU: return kPhysicalAltRight;
    case VK_LWIN: return kPhysicalWinLeft;
    case VK_RWIN: return kPhysicalWinRight;
    default: return 0;
    }
}

std::size_t ModifierKeyIndex(const KeyDescriptor& key) {
    if (key.device != InputDevice::Keyboard) return kModifierKeys.size();
    const auto found = std::find(kModifierKeys.begin(), kModifierKeys.end(), key.code);
    return static_cast<std::size_t>(found - kModifierKeys.begin());
}

void UpdatePhysicalModifier(std::atomic<std::uint8_t>& state, std::uint8_t bit, bool pressed) {
    if (pressed) state.fetch_or(bit);
    else state.fetch_and(static_cast<std::uint8_t>(~bit));
}

bool ConsumePendingEvent(std::atomic<unsigned>& count) {
    auto pending = count.load();
    while (pending != 0) {
        if (count.compare_exchange_weak(pending, pending - 1)) return true;
    }
    return false;
}

}

void InputDriverRouter::SetDriver(InputDriver* driver) noexcept {
    std::lock_guard lock(mutex_);
    heldDownKeys_.store(nullptr);
    driver_ = driver;
    pauseModifierRelaySupported_.store(driver && driver->SupportsPauseModifierRelay());
    relayedKeys_.clear();
    selfInjectedDriver_.store(driver);
    forwardedDown_.clear();
    repeaterState_.heldKeys.clear();
    macroState_.heldKeys.clear();
    arbitrationSkipDiag_.clear();
    for (auto& events : isolatedModifierEvents_) {
        events.down.store(0);
        events.up.store(0);
    }
}

void InputDriverRouter::NotePhysicalKey(const KeyDescriptor& key, bool pressed) {
    const auto bit = PhysicalModifierKeyBit(key);
    if (bit == 0) return;
    const auto previous = pressed ? physicalModifiers_.fetch_or(bit)
        : physicalModifiers_.fetch_and(static_cast<std::uint8_t>(~bit));
    if (((previous & bit) != 0) != pressed) physicalModifierRevision_.fetch_add(1);
}

void InputDriverRouter::NoteHookPhysicalKey(const KeyDescriptor& key, bool pressed) noexcept {
    const auto bit = PhysicalModifierKeyBit(key);
    if (bit == 0) return;
    UpdatePhysicalModifier(hookPhysicalModifiers_, bit, pressed);
    if (!pressed) physicalModifiers_.fetch_and(static_cast<std::uint8_t>(~bit));
    hookPhysicalModifiersKnown_.store(true);
    physicalModifierRevision_.fetch_add(1);
}

InputDriver* InputDriverRouter::CurrentDriver() const noexcept {
    std::lock_guard lock(mutex_);
    return driver_;
}

bool InputDriverRouter::CanRelayPauseModifier(const KeyDescriptor& key) const noexcept {
    const auto bit = PhysicalModifierKeyBit(key);
    // Do not take over an auto-repeat of a key Windows has already seen, or
    // promise isolation while another real modifier is already down.
    auto* driver = selfInjectedDriver_.load();
    return bit != 0 && pauseModifierRelaySupported_.load() && driver && driver->IsReady()
        && (hookPhysicalModifiers_.load() & bit) == 0
        && ((physicalModifiers_.load() | hookPhysicalModifiers_.load())
            & static_cast<std::uint8_t>(~(bit | interceptedModifiers_.load()))) == 0;
}

void InputDriverRouter::SetPhysicalModifierIntercepted(const KeyDescriptor& key, bool intercepted) noexcept {
    const auto bit = PhysicalModifierKeyBit(key);
    if (bit == 0) return;
    UpdatePhysicalModifier(interceptedModifiers_, bit, intercepted);
    physicalModifierRevision_.fetch_add(1);
}

bool InputDriverRouter::RelayPhysicalKey(const KeyDescriptor& key, bool pressed) {
    std::lock_guard lock(mutex_);
    if (!driver_ || !driver_->IsReady()) return false;
    const auto encoded = EncodeKey(key);
    if (pressed) {
        if (relayedKeys_.count(encoded)) return true;
        if (!forwardedDown_.count(encoded) && !driver_->SendDown(key, InputSource::Injected)) return false;
        relayedKeys_.insert(encoded);
    } else {
        if (!relayedKeys_.count(encoded)) return true;
        if (!forwardedDown_.count(encoded) && !driver_->SendUp(key, InputSource::Injected)) return false;
        relayedKeys_.erase(encoded);
    }
    return true;
}

void InputDriverRouter::ReleaseRelayedKeys() {
    std::lock_guard lock(mutex_);
    std::vector<std::uint64_t> keys(relayedKeys_.begin(), relayedKeys_.end());
    std::stable_sort(keys.begin(), keys.end(), [](auto left, auto right) {
        return PhysicalModifierKeyBit(DecodeKey(left)) < PhysicalModifierKeyBit(DecodeKey(right));
    });
    for (const auto encoded : keys) {
        if (!forwardedDown_.count(encoded) && driver_) driver_->SendUp(DecodeKey(encoded), InputSource::Injected);
    }
    relayedKeys_.clear();
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

std::wstring InputDriverRouter::KeySupportError(const KeyDescriptor& key) const {
    std::lock_guard lock(mutex_);
    return driver_ ? driver_->KeySupportError(key) : L"当前驱动未就绪。";
}

bool InputDriverRouter::ConsumeSelfInjectedWheel(int delta) {
    auto* driver = selfInjectedDriver_.load();
    return driver && driver->ConsumeSelfInjectedWheel(delta);
}

bool InputDriverRouter::ConsumeSelfInjectedRaw(const KeyDescriptor& key, bool pressed) {
    auto* driver = selfInjectedDriver_.load();
    return driver && driver->ConsumeSelfInjectedRaw(key, pressed);
}

InputDriverRouter::IsolatedTapResult InputDriverRouter::SendIsolatedTap(
    const KeyDescriptor& key, InputSource src, const std::function<bool()>& canSend,
    const std::function<void()>& beforeTap) {
    // Keep macro output out of the entire release/tap/restore transaction.
    std::lock_guard lock(mutex_);
    const auto components = KeyComponents(key);
    if (!driver_ || !IsArbitratedSource(src) || components.empty()) return IsolatedTapResult::Failed;
    if (!canSend()) return IsolatedTapResult::Deferred;
    for (const auto& component : components) {
        const auto encoded = EncodeKey(component);
        const auto owner = owners_.find(encoded);
        if (StateFor(src).activeKeys.count(encoded) == 0
            || (owner != owners_.end() && owner->second != src)) {
            NoteArbitrationSkipLocked(encoded, src, "isolated tap owner unavailable");
            return IsolatedTapResult::Deferred;
        }
    }
    if (forwardedDown_.count(EncodeKey(components.back())) != 0
        || relayedKeys_.count(EncodeKey(components.back())) != 0) return IsolatedTapResult::Deferred;

    const auto physicalModifiers = [this]() {
        const auto visible = static_cast<std::uint8_t>(~interceptedModifiers_.load());
        auto raw = static_cast<std::uint8_t>(physicalModifiers_.load() & visible);
        auto hook = static_cast<std::uint8_t>(hookPhysicalModifiers_.load() & visible);
        for (const auto encoded : relayedKeys_) {
            const auto bit = PhysicalModifierKeyBit(DecodeKey(encoded));
            raw |= bit;
            hook |= bit;
        }
        if (!driver_->SupportsPhysicalModifierIsolation()) return static_cast<std::uint8_t>(raw | hook);
        return hookPhysicalModifiersKnown_.load() ? hook : raw;
    };
    if (!driver_->SupportsPhysicalModifierIsolation() && physicalModifiers() != 0)
        return IsolatedTapResult::Deferred;
    std::array<bool, kModifierKeys.size()> releasedModifiers{};
    std::array<std::optional<ForwardedPress>, kModifierKeys.size()> suspendedModifiers;
    std::vector<std::uint64_t> tapKeys;
    bool ok = true;
    bool settled = false;
    bool hadModifiers = false;
    bool tapped = false;
    auto result = IsolatedTapResult::Deferred;

    // A newly pressed modifier during the release gap must also be released.
    // Stop before the tap if physical input keeps changing.
    for (int attempt = 0; attempt < 3 && ok; ++attempt) {
        const auto revision = physicalModifierRevision_.load();
        const auto physical = physicalModifiers();
        if (!driver_->SupportsPhysicalModifierIsolation() && physical != 0) break;
        for (std::size_t index = 0; index < kModifierKeys.size(); ++index) {
            const KeyDescriptor modifier{InputDevice::Keyboard, kModifierKeys[index]};
            const auto encoded = EncodeKey(modifier);
            const auto forwarded = forwardedDown_.find(encoded);
            if ((physical & PhysicalModifierKeyBit(modifier)) == 0 && forwarded == forwardedDown_.end()) continue;
            const auto modifierSource = forwarded != forwardedDown_.end() ? forwarded->second.source : src;
            if (!SendIsolatedModifierLocked(modifier, false, modifierSource)) {
                ok = false;
                break;
            }
            releasedModifiers[index] = true;
            hadModifiers = true;
            if (forwarded != forwardedDown_.end()) {
                suspendedModifiers[index] = forwarded->second;
                forwardedDown_.erase(forwarded);
            }
        }
        PublishHeldKeysLocked();
        if (!ok) break;
        if (hadModifiers) std::this_thread::sleep_for(kModifierReleaseGap);
        if (revision == physicalModifierRevision_.load()) {
            settled = true;
            break;
        }
    }

    if (ok && settled && canSend()
        && (driver_->SupportsPhysicalModifierIsolation() || physicalModifiers() == 0)) {
        for (const auto& component : components) {
            if (!canSend()) break;
            if (!driver_->SupportsPhysicalModifierIsolation() && physicalModifiers() != 0) break;
            if (tapKeys.size() + 1 == components.size()) beforeTap();
            const auto encoded = EncodeKey(component);
            const bool modifier = PhysicalModifierKeyBit(component) != 0;
            const bool sent = modifier
                ? SendIsolatedModifierLocked(component, true, src)
                : SendDownLocked(component, src);
            if (sent && modifier) {
                forwardedDown_.insert_or_assign(encoded, ForwardedPress{src, std::chrono::steady_clock::now()});
            }
            if (!sent || forwardedDown_.count(encoded) == 0) {
                ok = false;
                break;
            }
            tapKeys.push_back(encoded);
        }
        tapped = tapKeys.size() == components.size();
        if (tapped) {
            std::this_thread::sleep_for(kIsolatedTapWidth);
            result = IsolatedTapResult::Completed;
        }
    }

    // Always release the primary before restoring any user's modifier.
    for (auto it = tapKeys.rbegin(); it != tapKeys.rend(); ++it) {
        const auto component = DecodeKey(*it);
        if (PhysicalModifierKeyBit(component) == 0) {
            ok = ReleaseForwardedKeyLocked(*it) && ok;
        } else if (SendIsolatedModifierLocked(component, false, src)) {
            forwardedDown_.erase(*it);
        } else {
            ok = false;
        }
    }
    if (tapped && hadModifiers) std::this_thread::sleep_for(kModifierReleaseGap);
    for (std::size_t index = 0; index < kModifierKeys.size(); ++index) {
        if (!releasedModifiers[index]) continue;
        const KeyDescriptor modifier{InputDevice::Keyboard, kModifierKeys[index]};
        const auto encoded = EncodeKey(modifier);
        if (const auto& previous = suspendedModifiers[index]) {
            if (SendIsolatedModifierLocked(modifier, true, previous->source)) {
                forwardedDown_.insert_or_assign(encoded,
                    ForwardedPress{previous->source, std::chrono::steady_clock::now()});
            } else {
                ok = false;
            }
        } else if ((physicalModifiers() & PhysicalModifierKeyBit(modifier)) != 0) {
            ok = SendIsolatedModifierLocked(modifier, true, src, true) && ok;
            // A release can arrive on the hook while the driver call is in flight.
            if ((physicalModifiers() & PhysicalModifierKeyBit(modifier)) == 0) {
                ok = SendIsolatedModifierLocked(modifier, false, src) && ok;
            }
        }
    }
    PublishHeldKeysLocked();
    return ok ? result : IsolatedTapResult::Failed;
}

bool InputDriverRouter::SendIsolatedModifierLocked(const KeyDescriptor& key, bool pressed,
    InputSource src, bool physicalRestore) {
    const auto index = ModifierKeyIndex(key);
    if (!driver_ || index == kModifierKeys.size()) return false;
    auto& events = isolatedModifierEvents_[index];
    auto& count = pressed ? events.down : events.up;
    auto& lastTick = pressed ? events.lastDownTick : events.lastUpTick;
    const auto now = ::GetTickCount64();
    if (now - lastTick.load() > kIsolatedModifierEventTtlMs) count.store(0);
    lastTick.store(now);
    count.fetch_add(1);
    const bool sent = pressed
        ? (physicalRestore ? driver_->RestorePhysicalDown(key, src) : driver_->SendDown(key, src))
        : driver_->SendUp(key, src);
    if (!sent) ConsumePendingEvent(count);
    return sent;
}

bool InputDriverRouter::ConsumeIsolatedModifier(const KeyDescriptor& key, bool pressed) noexcept {
    const auto index = ModifierKeyIndex(key);
    if (index == kModifierKeys.size()) return false;
    auto& events = isolatedModifierEvents_[index];
    auto& count = pressed ? events.down : events.up;
    const auto lastTick = (pressed ? events.lastDownTick : events.lastUpTick).load();
    if (::GetTickCount64() - lastTick > kIsolatedModifierEventTtlMs) {
        count.store(0);
        return false;
    }
    return ConsumePendingEvent(count);
}

bool InputDriverRouter::SendDown(const KeyDescriptor& key, InputSource src) {
    std::lock_guard lock(mutex_);
    const bool sent = SendComboDownLocked(key, src);
    PublishHeldKeysLocked();
    return sent;
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
                if (state.heldKeys.insert(encoded).second) heldKeys.push_back(encoded);
            }
        }
    }
    const bool sent = SendComboDownLocked(key, src);
    if (!sent && IsArbitratedSource(src)) {
        auto& state = StateFor(src);
        for (const auto encoded : heldKeys) state.heldKeys.erase(encoded);
    }
    PublishHeldKeysLocked();
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
        if (forwarded->second.source == src) {
            arbitrationSkipDiag_.erase(diagKey);
            return true;
        }
        if (!ReleaseForwardedKeyLocked(encoded)) return false;
    }

    if (!driver_->SendDown(key, src)) return false;
    forwardedDown_.insert_or_assign(encoded, ForwardedPress{src, std::chrono::steady_clock::now()});
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
        const auto existingDown = forwardedDown_.find(encoded);
        const bool alreadyDown = existingDown != forwardedDown_.end() && existingDown->second.source == src;
        if (!SendDownLocked(component, src)) {
            for (auto it = sent.rbegin(); it != sent.rend(); ++it) {
                const auto forwarded = forwardedDown_.find(*it);
                if (forwarded != forwardedDown_.end() && forwarded->second.source == src) {
                    ReleaseForwardedKeyLocked(*it);
                }
            }
            return false;
        }
        if (!alreadyDown) sent.push_back(encoded);
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
        if (forwarded == forwardedDown_.end() || forwarded->second.source != src) continue;
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
    relayedKeys_.clear();
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
    relayedKeys_.clear();
    repeaterState_.heldKeys.clear();
    macroState_.heldKeys.clear();
}

bool InputDriverRouter::ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) {
    // Hook callbacks must not wait on the router while driver I/O is in progress.
    const bool isolatedModifier = ConsumeIsolatedModifier(key, pressed);
    auto* driver = selfInjectedDriver_.load();
    const bool injected = driver && driver->ConsumeSelfInjected(key, pressed);
    return isolatedModifier || injected;
}

bool InputDriverRouter::IsHeldDown(const KeyDescriptor& key) const noexcept {
    // Physical modifiers must reach Raw Input to keep chord snapshots accurate.
    // The engine restores a held request after a real physical release.
    if (IsKeyboardModifierKey(key)) return false;
    const auto heldKeys = heldDownKeys_.load();
    if (!heldKeys) return false;
    const auto components = KeyComponents(key);
    return std::all_of(components.begin(), components.end(),
        [&heldKeys](const KeyDescriptor& component) {
            return heldKeys->count(EncodeKey(component)) > 0;
        });
}

std::optional<std::chrono::steady_clock::time_point> InputDriverRouter::ForwardedDownSince(
    const KeyDescriptor& key, InputSource src) const {
    std::lock_guard lock(mutex_);
    const auto forwarded = forwardedDown_.find(EncodeKey(key));
    if (forwarded == forwardedDown_.end() || forwarded->second.source != src) return std::nullopt;
    return forwarded->second.since;
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

    std::vector<std::uint64_t> releases;
    for (const auto& [encoded, press] : forwardedDown_) {
        const auto owner = updatedOwners.find(encoded);
        const bool sourceStillOwnsKey = StateFor(press.source).activeKeys.count(encoded) > 0;
        const bool ownershipChanged = owner != updatedOwners.end() && owner->second != press.source;
        if (!sourceStillOwnsKey || ownershipChanged) {
            releases.push_back(encoded);
        }
    }
    std::stable_sort(releases.begin(), releases.end(), [](const auto left, const auto right) {
        return IsKeyboardModifierKey(DecodeKey(left)) < IsKeyboardModifierKey(DecodeKey(right));
    });
    for (const auto encoded : releases) {
        ReleaseForwardedKeyLocked(encoded);
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
    for (const auto& [encoded, press] : forwardedDown_) {
        if (StateFor(press.source).heldKeys.count(encoded) > 0) {
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
    if (!driver_) return false;
    if (!relayedKeys_.count(encoded) && !driver_->SendUp(DecodeKey(encoded), forwarded->second.source)) return false;
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
    auto physical = static_cast<std::uint8_t>(physicalModifiers_.load(std::memory_order_relaxed)
        & static_cast<std::uint8_t>(~interceptedModifiers_.load()));
    for (const auto encoded : relayedKeys_) physical |= PhysicalModifierKeyBit(DecodeKey(encoded));
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
