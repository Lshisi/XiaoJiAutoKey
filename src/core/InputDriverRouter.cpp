#include "InputDriverRouter.h"

#include <chrono>
#include <thread>

void InputDriverRouter::SetDriver(InputDriver* driver) noexcept {
    std::lock_guard lock(mutex_);
    driver_ = driver;
    forwardedDown_.clear();
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
    if (!driver_) return false;
    if (!IsArbitratedSource(src)) return driver_->SendDown(key, src);

    const auto encoded = EncodeKey(key);
    const auto& state = StateFor(src);
    const auto owner = owners_.find(encoded);
    if (state.activeKeys.count(encoded) == 0
        || (owner != owners_.end() && owner->second != src)) {
        return true;
    }

    const auto forwarded = forwardedDown_.find(encoded);
    if (forwarded != forwardedDown_.end()) {
        if (forwarded->second == src) return true;
        if (!ReleaseForwardedKeyLocked(encoded)) return false;
    }

    if (!driver_->SendDown(key, src)) return false;
    forwardedDown_[encoded] = src;
    return true;
}

bool InputDriverRouter::SendUp(const KeyDescriptor& key, InputSource src) {
    std::lock_guard lock(mutex_);
    if (!driver_) return false;
    if (!IsArbitratedSource(src)) return driver_->SendUp(key, src);

    const auto encoded = EncodeKey(key);
    const auto forwarded = forwardedDown_.find(encoded);
    if (forwarded == forwardedDown_.end() || forwarded->second != src) {
        return true;
    }
    return ReleaseForwardedKeyLocked(encoded);
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
        if (key.IsValid()) desiredKeys.insert(EncodeKey(key));
    }

    for (auto it = state.activeKeys.begin(); it != state.activeKeys.end();) {
        if (desiredKeys.count(it->first) == 0) {
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
    if (driver_) driver_->ReleaseAll();
    forwardedDown_.clear();
    owners_.clear();
    repeaterState_.activeKeys.clear();
    macroState_.activeKeys.clear();
}

void InputDriverRouter::ClearAllStates() {
    std::lock_guard lock(mutex_);
    if (driver_) driver_->ClearAllStates();
    forwardedDown_.clear();
}

bool InputDriverRouter::ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) {
    std::lock_guard lock(mutex_);
    return driver_ && driver_->ConsumeSelfInjected(key, pressed);
}

bool InputDriverRouter::IsArbitratedSource(InputSource src) noexcept {
    return src == InputSource::Repeater || src == InputSource::Macro;
}

std::uint64_t InputDriverRouter::EncodeKey(const KeyDescriptor& key) noexcept {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse ? 1 : 0) << 32) | key.code;
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
}

bool InputDriverRouter::ReleaseForwardedKeyLocked(std::uint64_t encoded) {
    const auto forwarded = forwardedDown_.find(encoded);
    if (forwarded == forwardedDown_.end()) return true;
    if (!driver_ || !driver_->SendUp(DecodeKey(encoded), forwarded->second)) return false;
    forwardedDown_.erase(forwarded);
    return true;
}
