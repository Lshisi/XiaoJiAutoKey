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
    void SetSourceKeys(InputSource src, const std::vector<KeyDescriptor>& keys) override;
    void SetSourceActive(InputSource src, bool active) override;
    void ReleaseSource(InputSource src) override;
    void ReleaseAll() override;
    void ClearAllStates() override;
    bool ConsumeSelfInjected(const KeyDescriptor& key) override;

private:
    struct SourceState {
        bool active = false;
        std::uint64_t activationOrder = 0;
        std::unordered_set<std::uint64_t> keys;
    };

    static bool IsArbitratedSource(InputSource src) noexcept;
    static std::uint64_t EncodeKey(const KeyDescriptor& key) noexcept;
    static KeyDescriptor DecodeKey(std::uint64_t encoded) noexcept;
    SourceState& StateFor(InputSource src);
    const SourceState& StateFor(InputSource src) const;
    void RecomputeOwnersLocked();
    bool ReleaseForwardedKeyLocked(std::uint64_t encoded);

    mutable std::mutex mutex_;
    InputDriver* driver_ = nullptr;
    SourceState repeaterState_;
    SourceState macroState_;
    std::uint64_t nextActivationOrder_ = 1;
    std::unordered_map<std::uint64_t, InputSource> owners_;
    std::unordered_map<std::uint64_t, InputSource> forwardedDown_;
};
