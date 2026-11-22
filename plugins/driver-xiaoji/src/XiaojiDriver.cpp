#include "XiaojiDriver.h"

#include "xiaoji_native.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <thread>

namespace {
constexpr ULONGLONG kEventTtlMs = 250;

std::uint64_t Encode(const KeyDescriptor& key) {
    return (static_cast<std::uint64_t>(key.device == InputDevice::Mouse) << 32) | key.code;
}

}

XiaojiDriver::XiaojiDriver() = default;
XiaojiDriver::~XiaojiDriver() { Disconnect(); }

bool XiaojiDriver::Connect() {
    Disconnect();
    std::lock_guard lock(ioMutex_);
    try {
        client_ = std::make_unique<xiaoji::Client>();
        // A previous failed release may still be pending on this adapter.
        ReleaseAllLocked();
        if (keyboardUncertain_ || consumerUncertain_ || mouseUncertain_ || !heldKeys_.empty()) {
            client_.reset();
            return false;
        }
        SetError({});
        ready_ = true;
        return true;
    } catch (const std::exception& error) {
        SetError(L"小叽驱动连接失败：" + ToWide(error.what()));
        return false;
    }
}

void XiaojiDriver::Disconnect() {
    std::lock_guard lock(ioMutex_);
    ready_ = false;
    ReleaseAllLocked();
    client_.reset();
}

std::wstring XiaojiDriver::GetLastError() const {
    std::lock_guard lock(stateMutex_);
    return lastError_;
}

void XiaojiDriver::SetError(std::wstring error) {
    std::lock_guard lock(stateMutex_);
    lastError_ = std::move(error);
}

std::wstring XiaojiDriver::KeySupportError(const KeyDescriptor& key) const {
    if (!key.IsValid()) return L"按键无效，无法通过小叽驱动输出。";
    for (const auto& part : ExpandKeyComponents(NormalizeKeyDescriptor(key))) {
        const auto mapped = XiaojiKeys::Resolve(part);
        if (!mapped.IsValid() || mapped.keyboardUsage >= xiaoji::kKeyboardUsageCount)
            return L"小叽驱动暂时无法将“" + GetKeyDisplayName(part)
                + L"”映射为输入报告。该键可能是输入法合成事件或未映射的特殊键，请修改配置。";
    }
    return {};
}

bool XiaojiDriver::SupportsPhysicalKeyRelay(const KeyDescriptor& key) const noexcept {
    const auto mapped = XiaojiKeys::Resolve(key);
    return mapped.IsValid() && mapped.keyboardUsage < xiaoji::kKeyboardUsageCount;
}

bool XiaojiDriver::SendTap(const KeyDescriptor& key, InputSource src) {
    if (!SendDown(key, src)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(kInputPulseWidthMs));
    return SendUp(key, src);
}

bool XiaojiDriver::SendDown(const KeyDescriptor& key, InputSource) { return SendState(key, true); }
bool XiaojiDriver::SendUp(const KeyDescriptor& key, InputSource) { return SendState(key, false); }

bool XiaojiDriver::RestorePhysicalDown(const KeyDescriptor&, InputSource) {
    // A state-report device cannot restore another keyboard's physical state.
    // PauseModifierGate can withhold a new pause modifier until the isolated
    // close finishes. Already-forwarded physical modifiers still cannot be reset here.
    return false;
}

bool XiaojiDriver::SendState(const KeyDescriptor& rawKey, bool pressed) {
    std::lock_guard lock(ioMutex_);
    if (!ready_ || !client_) return false;
    auto key = NormalizeKeyDescriptor(rawKey);
    key.modifiers = 0; // The router expands combinations before dispatch.
    const auto found = std::find_if(heldKeys_.begin(), heldKeys_.end(),
        [&key](const auto& held) { return held.key == key; });
    if ((found != heldKeys_.end()) == pressed) return true;
    const auto output = pressed ? XiaojiKeys::Resolve(key) : found->output;
    if (!output.IsValid() || output.keyboardUsage >= xiaoji::kKeyboardUsageCount) {
        SetError(KeySupportError(key));
        return false;
    }
    auto desired = heldKeys_;
    if (pressed) desired.push_back({key, output});
    else std::erase_if(desired, [&key](const auto& held) { return held.key == key; });
    xiaoji::KeyboardState keyboard{};
    xiaoji::ConsumerState consumer{};
    std::uint8_t buttons = 0;
    std::size_t consumerCount = 0;
    for (const auto& held : desired) {
        const auto& mapped = held.output;
        buttons |= mapped.mouseButton;
        keyboard[0] |= mapped.modifier;
        if (mapped.keyboardUsage != 0)
            keyboard[1 + mapped.keyboardUsage / 8] |= static_cast<std::uint8_t>(1u << (mapped.keyboardUsage % 8));
        if (mapped.consumerUsage != 0
            && std::find(consumer.begin(), consumer.begin() + consumerCount, mapped.consumerUsage) == consumer.begin() + consumerCount) {
            if (consumerCount == consumer.size()) {
                SetError(L"媒体键报告容量已满，本次输出已停止。");
                ready_ = false;
                ReleaseAllLocked();
                return false;
            }
            consumer[consumerCount++] = mapped.consumerUsage;
        }
    }
    heldKeys_ = std::move(desired);
    try {
        if (key.device == InputDevice::Mouse) {
            if (mouseButtons_ == buttons) return true;
            MarkInjected(key, pressed);
            mouseButtons_ = buttons;
            mouseUncertain_ = true;
            client_->SendMouse({mouseButtons_, 0, 0, 0});
            mouseUncertain_ = false;
        } else if (output.consumerUsage != 0) {
            if (consumer_ == consumer) return true;
            MarkInjected(key, pressed);
            consumer_ = consumer;
            consumerUncertain_ = true;
            client_->SendConsumer(consumer_);
            consumerUncertain_ = false;
        } else {
            if (keyboard_ == keyboard) return true;
            MarkInjected(key, pressed);
            keyboard_ = keyboard;
            keyboardUncertain_ = true;
            client_->SendKeyboard(keyboard_);
            keyboardUncertain_ = false;
        }
        return true;
    } catch (const std::exception& error) {
        Fail(error);
        return false;
    }
}

bool XiaojiDriver::SendMouseReport(int x, int y, int wheel) {
    try {
        if (wheel != 0) {
            std::lock_guard lock(stateMutex_);
            if (GetTickCount64() - wheelAt_ > kEventTtlMs) pendingWheel_ = 0;
            pendingWheel_ += wheel * WHEEL_DELTA;
            wheelAt_ = GetTickCount64();
        }
        mouseUncertain_ = true;
        client_->SendMouse({mouseButtons_, static_cast<std::uint8_t>(static_cast<std::int8_t>(x)),
            static_cast<std::uint8_t>(static_cast<std::int8_t>(y)),
            static_cast<std::uint8_t>(static_cast<std::int8_t>(wheel))});
        mouseUncertain_ = false;
        return true;
    } catch (const std::exception& error) {
        Fail(error);
        return false;
    }
}

bool XiaojiDriver::SendMouseMove(int deltaX, int deltaY, InputSource) {
    std::lock_guard lock(ioMutex_);
    if (!ready_ || !client_) return false;
    while (deltaX || deltaY) {
        const int x = std::clamp(deltaX, -127, 127);
        const int y = std::clamp(deltaY, -127, 127);
        if (!SendMouseReport(x, y, 0)) return false;
        deltaX -= x;
        deltaY -= y;
    }
    return true;
}

bool XiaojiDriver::SendMouseWheel(int delta, InputSource) {
    std::lock_guard lock(ioMutex_);
    if (!ready_ || !client_) return false;
    if (delta == 0 || delta % WHEEL_DELTA != 0) {
        SetError(L"小叽驱动滚轮输出必须是 120 的非零整数倍。");
        return false;
    }
    for (int remaining = delta / WHEEL_DELTA; remaining != 0;) {
        const int step = std::clamp(remaining, -127, 127);
        if (!SendMouseReport(0, 0, step)) return false;
        remaining -= step;
    }
    return true;
}

void XiaojiDriver::Fail(const std::exception& error) {
    ready_ = false;
    SetError(L"小叽驱动输出失败，已停止发送，未重试按下或点击：" + ToWide(error.what()));
    ReleaseAllLocked();
}

void XiaojiDriver::ReleaseAllLocked() {
    if (!client_) return;
    for (const auto& key : heldKeys_) MarkInjected(key.key, false);
    bool keyboardReleased = !keyboardUncertain_ && keyboard_ == xiaoji::KeyboardState{};
    bool mouseReleased = !mouseUncertain_ && mouseButtons_ == 0;
    bool consumerReleased = !consumerUncertain_ && consumer_ == xiaoji::ConsumerState{};
    try {
        if (!keyboardReleased) {
            keyboardUncertain_ = true;
            client_->SendKeyboard({});
            keyboard_ = {};
            keyboardUncertain_ = false;
            keyboardReleased = true;
        }
    } catch (const std::exception& error) {
        ready_ = false;
        SetError(L"小叽驱动键盘释放未确认：" + ToWide(error.what()));
    }
    try {
        if (!consumerReleased) {
            consumerUncertain_ = true;
            client_->SendConsumer({});
            consumer_ = {};
            consumerUncertain_ = false;
            consumerReleased = true;
        }
    } catch (const std::exception& error) {
        ready_ = false;
        SetError(L"小叽驱动媒体键释放未确认：" + ToWide(error.what()));
    }
    try {
        if (!mouseReleased) {
            mouseUncertain_ = true;
            client_->SendMouse({});
            mouseButtons_ = 0;
            mouseUncertain_ = false;
            mouseReleased = true;
        }
    } catch (const std::exception& error) {
        ready_ = false;
        SetError(L"小叽驱动鼠标释放未确认：" + ToWide(error.what()));
    }
    if (keyboardReleased && consumerReleased && mouseReleased) heldKeys_.clear();
}

void XiaojiDriver::ReleaseAll() { std::lock_guard lock(ioMutex_); ReleaseAllLocked(); }

void XiaojiDriver::ClearAllStates() {
    std::lock_guard lock(stateMutex_);
    // Keep pending feedback until its TTL expires, including cleanup releases.
    // Do not forget a held report whose release Windows has not acknowledged.
    std::erase_if(pending_, [](const auto& entry) {
        return !entry.second.held
            && GetTickCount64() - std::max(entry.second.downAt, entry.second.upAt) > kEventTtlMs;
    });
}

void XiaojiDriver::MarkInjected(const KeyDescriptor& key, bool pressed) {
    std::lock_guard lock(stateMutex_);
    auto& event = pending_[Encode(key)];
    event.held = pressed;
    int& count = pressed ? event.down : event.up;
    int& rawCount = pressed ? event.rawDown : event.rawUp;
    auto& tick = pressed ? event.downAt : event.upAt;
    if (GetTickCount64() - tick > kEventTtlMs) count = rawCount = 0;
    count = std::min(count + 1, 32);
    rawCount = std::min(rawCount + 1, 32);
    tick = GetTickCount64();
}

bool XiaojiDriver::ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) {
    return ConsumeInjected(key, pressed, false);
}

bool XiaojiDriver::ConsumeSelfInjectedRaw(const KeyDescriptor& key, bool pressed) {
    return ConsumeInjected(key, pressed, true);
}

bool XiaojiDriver::ConsumeInjected(const KeyDescriptor& key, bool pressed, bool raw) {
    std::lock_guard lock(stateMutex_);
    const auto found = pending_.find(Encode(NormalizeKeyDescriptor(key)));
    if (found == pending_.end()) return false;
    auto& event = found->second;
    int& count = raw ? (pressed ? event.rawDown : event.rawUp) : (pressed ? event.down : event.up);
    if (pressed && event.held) {
        if (count > 0) --count;
        return true;
    }
    const auto tick = pressed ? event.downAt : event.upAt;
    if (GetTickCount64() - tick > kEventTtlMs) count = 0;
    if (count == 0) return false;
    --count;
    return true;
}

bool XiaojiDriver::ConsumeSelfInjectedWheel(int delta) {
    std::lock_guard lock(stateMutex_);
    if (GetTickCount64() - wheelAt_ > kEventTtlMs) pendingWheel_ = 0;
    if (delta == 0 || (delta > 0) != (pendingWheel_ > 0) || std::abs(delta) > std::abs(pendingWheel_)) return false;
    pendingWheel_ -= delta;
    return true;
}
