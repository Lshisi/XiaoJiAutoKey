#pragma once
#include "InputDriver.h"
#include <mutex>
#include <vector>

namespace checks {
// An output adapter, not a second implementation of engine/routing rules.
struct RecordingDriver final : InputDriver {
    bool IsReady() const noexcept override { return true; }
    std::wstring GetLastError() const override { return {}; }
    bool SendTap(const KeyDescriptor& key, InputSource src) override;
    bool SendDown(const KeyDescriptor& key, InputSource src) override;
    bool SendUp(const KeyDescriptor& key, InputSource src) override;
    bool SendMouseWheel(int delta, InputSource src) override;
    bool SupportsMouseWheel() const override { return true; }
    void ReleaseAll() override;
    std::mutex mutex;
    std::vector<KeyDescriptor> held;
};
}
