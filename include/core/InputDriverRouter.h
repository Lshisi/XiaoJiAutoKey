#pragma once

#include "InputDriver.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class InputDriverRouter final : public InputDriver {
public:
    void SetDriver(InputDriver* driver) noexcept;
    [[nodiscard]] InputDriver* CurrentDriver() const noexcept;

    [[nodiscard]] bool IsReady() const noexcept override;
    [[nodiscard]] std::wstring GetLastError() const override;
    bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendMouseMove(int deltaX, int deltaY, InputSource src = InputSource::Injected) override;
    [[nodiscard]] bool SupportsMouseMove() const override;
    void SetSourceActiveKeys(InputSource src, const std::vector<KeyDescriptor>& keys) override;
    void ReleaseSource(InputSource src) override;
    void ReleaseAll() override;
    void ClearAllStates() override;
    bool ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) override;

private:
    struct SourceState {
        std::unordered_map<std::uint64_t, std::uint64_t> activeKeys;
    };

    static bool IsArbitratedSource(InputSource src) noexcept;
    static std::uint64_t EncodeKey(const KeyDescriptor& key) noexcept;
    static KeyDescriptor DecodeKey(std::uint64_t encoded) noexcept;
    static std::uint64_t ArbitrationDiagKey(std::uint64_t encoded, InputSource src) noexcept;
    SourceState& StateFor(InputSource src);
    const SourceState& StateFor(InputSource src) const;
    void RecomputeOwnersLocked();
    bool ReleaseForwardedKeyLocked(std::uint64_t encoded);
    void NoteArbitrationSkipLocked(std::uint64_t encoded, InputSource src, const char* reason);

    mutable std::mutex mutex_;
    InputDriver* driver_ = nullptr;
    SourceState repeaterState_;
    SourceState macroState_;
    std::uint64_t nextActivationOrder_ = 1;
    std::unordered_map<std::uint64_t, InputSource> owners_;
    std::unordered_map<std::uint64_t, InputSource> forwardedDown_;
    // 仲裁跳过诊断：记录每个 (key, source) 首次被静默跳过，恢复正常注入后清除，避免高频刷日志。
    std::unordered_set<std::uint64_t> arbitrationSkipDiag_;
};
