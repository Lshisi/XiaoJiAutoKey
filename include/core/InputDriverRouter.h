#pragma once

#include "InputDriver.h"

class InputDriverRouter final : public InputDriver {
public:
    void SetDriver(InputDriver* driver) noexcept;
    [[nodiscard]] InputDriver* CurrentDriver() const noexcept;

    [[nodiscard]] bool IsReady() const noexcept override;
    [[nodiscard]] std::wstring GetLastError() const override;
    bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) override;
    void ReleaseAll() override;
    void ClearAllStates() override;
    bool ConsumeSelfInjected(const KeyDescriptor& key) override;

private:
    InputDriver* driver_ = nullptr;
};
