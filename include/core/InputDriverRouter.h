#pragma once

#include "InputDriver.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class InputDriverRouter final : public InputDriver {
public:
    void SetDriver(InputDriver* driver) noexcept;
    void NotePhysicalKey(const KeyDescriptor& key, bool pressed);
    [[nodiscard]] InputDriver* CurrentDriver() const noexcept;

    [[nodiscard]] bool IsReady() const noexcept override;
    [[nodiscard]] std::wstring GetLastError() const override;
    bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendHeldDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendMouseMove(int deltaX, int deltaY, InputSource src = InputSource::Injected) override;
    [[nodiscard]] bool SupportsMouseMove() const override;
    bool SendMouseWheel(int delta, InputSource src = InputSource::Injected) override;
    [[nodiscard]] bool SupportsMouseWheel() const override;
    void SetSourceActiveKeys(InputSource src, const std::vector<KeyDescriptor>& keys) override;
    void ReleaseSource(InputSource src) override;
    void ReleaseAll() override;
    void ClearAllStates() override;
    bool ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) override;
    [[nodiscard]] bool IsHeldDown(const KeyDescriptor& key) const noexcept;
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> ForwardedDownSince(
        const KeyDescriptor& key, InputSource src) const;

private:
    using HeldKeySet = std::unordered_set<std::uint64_t>;

    struct SourceState {
        std::unordered_map<std::uint64_t, std::uint64_t> activeKeys;
        std::unordered_set<std::uint64_t> heldKeys;
    };

    struct ForwardedPress {
        InputSource source;
        std::chrono::steady_clock::time_point since;
    };

    static bool IsArbitratedSource(InputSource src) noexcept;
    static std::uint64_t EncodeKey(const KeyDescriptor& key) noexcept;
    static KeyDescriptor DecodeKey(std::uint64_t encoded) noexcept;
    static std::uint64_t ArbitrationDiagKey(std::uint64_t encoded, InputSource src) noexcept;
    [[nodiscard]] static std::vector<KeyDescriptor> KeyComponents(const KeyDescriptor& key);
    SourceState& StateFor(InputSource src);
    const SourceState& StateFor(InputSource src) const;
    void RecomputeOwnersLocked();
    bool SendDownLocked(const KeyDescriptor& key, InputSource src);
    bool SendComboDownLocked(const KeyDescriptor& key, InputSource src);
    void RestoreHeldKeysLocked(InputSource src);
    void PublishHeldKeysLocked();
    bool ReleaseForwardedKeyLocked(std::uint64_t encoded);
    void NoteArbitrationSkipLocked(std::uint64_t encoded, InputSource src, const char* reason);
    [[nodiscard]] bool IsPhysicalModifierActiveLocked(std::uint8_t modifiers) const;

    mutable std::mutex mutex_;
    InputDriver* driver_ = nullptr;
    // The hook reads these without acquiring the mutex used during driver I/O.
    std::atomic<InputDriver*> selfInjectedDriver_{nullptr};
    std::atomic<std::shared_ptr<const HeldKeySet>> heldDownKeys_{nullptr};
    SourceState repeaterState_;
    SourceState macroState_;
    std::uint64_t nextActivationOrder_ = 1;
    std::unordered_map<std::uint64_t, InputSource> owners_;
    std::atomic<std::uint8_t> physicalModifiers_{0};
    std::unordered_map<std::uint64_t, ForwardedPress> forwardedDown_;
    // 仲裁跳过诊断：记录每个 (key, source) 首次被静默跳过，恢复正常注入后清除，避免高频刷日志。
    std::unordered_set<std::uint64_t> arbitrationSkipDiag_;
};
