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
    const auto owner = owners_.find(encoded);
    if (!StateFor(src).active || (owner != owners_.end() && owner->second != src)) {
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

void InputDriverRouter::SetSourceKeys(InputSource src, const std::vector<KeyDescriptor>& keys) {
    if (!IsArbitratedSource(src)) return;

    std::lock_guard lock(mutex_);
    auto& state = StateFor(src);
    state.keys.clear();
    for (const auto& key : keys) {
        if (key.IsValid()) state.keys.insert(EncodeKey(key));
    }
    RecomputeOwnersLocked();
}

void InputDriverRouter::SetSourceActive(InputSource src, bool active) {
    if (!IsArbitratedSource(src)) return;

    std::lock_guard lock(mutex_);
    auto& state = StateFor(src);
    if (state.active == active) {
        RecomputeOwnersLocked();
        return;
    }
    state.active = active;
    state.activationOrder = active ? nextActivationOrder_++ : 0;
    RecomputeOwnersLocked();
}

void InputDriverRouter::ReleaseSource(InputSource src) {
    if (!IsArbitratedSource(src)) return;
    SetSourceActive(src, false);
}

void InputDriverRouter::ReleaseAll() {
    std::lock_guard lock(mutex_);
    if (driver_) driver_->ReleaseAll();
    forwardedDown_.clear();
    owners_.clear();
    repeaterState_.active = false;
    repeaterState_.activationOrder = 0;
    macroState_.active = false;
    macroState_.activationOrder = 0;
}

void InputDriverRouter::ClearAllStates() {
    std::lock_guard lock(mutex_);
    if (driver_) driver_->ClearAllStates();
    forwardedDown_.clear();
}

bool InputDriverRouter::ConsumeSelfInjected(const KeyDescriptor& key) {
    std::lock_guard lock(mutex_);
    return driver_ && driver_->ConsumeSelfInjected(key);
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
    if (repeaterState_.active && macroState_.active) {
        const InputSource preferred = repeaterState_.activationOrder < macroState_.activationOrder
            ? InputSource::Repeater
            : InputSource::Macro;
        for (const auto encoded : repeaterState_.keys) {
            if (macroState_.keys.count(encoded) > 0) updatedOwners.emplace(encoded, preferred);
        }
    }

    for (auto it = forwardedDown_.begin(); it != forwardedDown_.end();) {
        const auto encoded = it->first;
        const auto source = it->second;
        const auto owner = updatedOwners.find(encoded);
        const bool sourceStillActive = StateFor(source).active;
        const bool sourceStillOwnsKey = StateFor(source).keys.count(encoded) > 0;
        const bool ownershipChanged = owner != updatedOwners.end() && owner->second != source;
        if (!sourceStillActive || !sourceStillOwnsKey || ownershipChanged) {
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
