#pragma once

#include "InputDriver.h"
#include "XiaojiKeyMap.h"
#include "input_profile.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace xiaoji { class Client; }

class XiaojiDriver final : public InputDriver {
public:
    std::wstring DiagnosticText() const override {
        return L"输出范围：NKRO 键盘、媒体/浏览器键、鼠标五键、相对移动及滚轮。";
    }
    XiaojiDriver();
    ~XiaojiDriver() override;
    bool Connect();
    void Disconnect();
    [[nodiscard]] bool IsReady() const noexcept override { return ready_.load(); }
    [[nodiscard]] std::wstring GetLastError() const override;
    [[nodiscard]] std::wstring KeySupportError(const KeyDescriptor& key) const override;
    [[nodiscard]] bool SupportsPhysicalModifierIsolation() const override { return false; }
    [[nodiscard]] bool SupportsPauseModifierRelay() const override { return true; }
    [[nodiscard]] bool SupportsPhysicalKeyRelay(const KeyDescriptor& key) const noexcept override;
    bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool RestorePhysicalDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendMouseMove(int deltaX, int deltaY, InputSource src = InputSource::Injected) override;
    bool SendMouseWheel(int delta, InputSource src = InputSource::Injected) override;
    [[nodiscard]] bool SupportsMouseMove() const override { return true; }
    [[nodiscard]] bool SupportsMouseWheel() const override { return true; }
    void ReleaseAll() override;
    void ClearAllStates() override;
    bool ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) override;
    bool ConsumeSelfInjectedRaw(const KeyDescriptor& key, bool pressed) override;
    bool ConsumeSelfInjectedWheel(int delta) override;

private:
    struct PendingEvent {
        int down = 0;
        int up = 0;
        int rawDown = 0;
        int rawUp = 0;
        ULONGLONG downAt = 0;
        ULONGLONG upAt = 0;
        bool held = false;
    };
    struct HeldKey {
        KeyDescriptor key;
        XiaojiKeys::Output output;
    };
    bool SendState(const KeyDescriptor& key, bool pressed);
    bool SendMouseReport(int x, int y, int wheel);
    void ReleaseAllLocked();
    void Fail(const std::exception& error);
    void SetError(std::wstring error);
    void MarkInjected(const KeyDescriptor& key, bool pressed);
    bool ConsumeInjected(const KeyDescriptor& key, bool pressed, bool raw);

    std::atomic<bool> ready_{false};
    std::mutex ioMutex_;
    mutable std::mutex stateMutex_;
    std::unique_ptr<xiaoji::Client> client_;
    std::vector<HeldKey> heldKeys_;
    xiaoji::KeyboardState keyboard_{};
    xiaoji::ConsumerState consumer_{};
    std::uint8_t mouseButtons_ = 0;
    bool keyboardUncertain_ = false;
    bool mouseUncertain_ = false;
    bool consumerUncertain_ = false;
    std::wstring lastError_;
    std::unordered_map<std::uint64_t, PendingEvent> pending_;
    int pendingWheel_ = 0;
    ULONGLONG wheelAt_ = 0;
};
