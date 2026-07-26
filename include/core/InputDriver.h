#pragma once

#include "AppTypes.h"

#include <string>
#include <vector>

class InputDriver {
public:
    virtual ~InputDriver() = default;

    [[nodiscard]] virtual bool IsReady() const noexcept = 0;
    [[nodiscard]] virtual std::wstring GetLastError() const = 0;

    virtual bool SendTap(const KeyDescriptor& key, InputSource src = InputSource::Injected) = 0;
    virtual bool SendDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) = 0;
    virtual bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) = 0;
    virtual void SetSourceActiveKeys(InputSource, const std::vector<KeyDescriptor>&) {}
    virtual void ReleaseSource(InputSource) { ReleaseAll(); }
    virtual void ReleaseAll() = 0;
    virtual void ClearAllStates() {}
    virtual bool ConsumeSelfInjected(const KeyDescriptor&) { return false; }
};
