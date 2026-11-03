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
    virtual bool SendHeldDown(const KeyDescriptor& key, InputSource src = InputSource::Injected) {
        return SendDown(key, src);
    }
    virtual bool SendUp(const KeyDescriptor& key, InputSource src = InputSource::Injected) = 0;
    // 相对移动光标（像素）；驱动不支持时返回 false。
    virtual bool SendMouseMove(int deltaX, int deltaY, InputSource src = InputSource::Injected) {
        (void)deltaX; (void)deltaY; (void)src;
        return false;
    }
    [[nodiscard]] virtual bool SupportsMouseMove() const { return false; }
    virtual bool SendMouseWheel(int delta, InputSource src = InputSource::Injected) { (void)delta; (void)src; return false; }
    [[nodiscard]] virtual bool SupportsMouseWheel() const { return false; }
    virtual void SetSourceActiveKeys(InputSource, const std::vector<KeyDescriptor>&) {}
    virtual void ReleaseSource(InputSource) { ReleaseAll(); }
    virtual void ReleaseAll() = 0;
    virtual void ClearAllStates() {}
    virtual bool ConsumeSelfInjected(const KeyDescriptor&, bool) { return false; }
};
