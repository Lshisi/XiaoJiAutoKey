// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "input_profile.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace xiaoji {
struct Devices {
    bool keyboardPresent = false;
    bool mousePresent = false;
    bool keyboardReady = false;
    bool mouseReady = false;
    bool profileReady = false;
    bool boundPackagePresent = false;
    bool configurationPresent = false;
    std::wstring error;
};

// Read-only: never installs a device or publishes an input report.
Devices InspectDevices();

// Full-state reports extend the v0.1.0 SDK, which exposes taps only.
// Keep this adapter pinned to input_profile.h and the driver/payload.json release.
class Client {
public:
    Client();
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    void SendKeyboard(const KeyboardState& report);
    void SendConsumer(const ConsumerState& report);
    void SendMouse(const MouseState& report);

private:
    struct State;
    std::unique_ptr<State> state_;
};
}
