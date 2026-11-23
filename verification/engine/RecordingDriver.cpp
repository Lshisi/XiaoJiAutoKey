#include "RecordingDriver.h"
#include "EngineChecks.h"
#include <algorithm>
#include <thread>

namespace checks {
bool RecordingDriver::SendDown(const KeyDescriptor& key, InputSource) {
    std::lock_guard lock(mutex);
    for (auto component : ExpandKeyComponents(key)) {
        if (std::find(held.begin(), held.end(), component) == held.end()) {
            held.push_back(component);
            Capture(component, true);
        }
    }
    return true;
}
bool RecordingDriver::SendUp(const KeyDescriptor& key, InputSource) {
    std::lock_guard lock(mutex);
    auto components = ExpandKeyComponents(key);
    for (auto it = components.rbegin(); it != components.rend(); ++it) {
        auto found = std::find(held.begin(), held.end(), *it);
        if (found != held.end()) {
            Capture(*it, false);
            held.erase(found);
        }
    }
    return true;
}
bool RecordingDriver::SendTap(const KeyDescriptor& key, InputSource src) {
    SendDown(key, src);
    std::this_thread::sleep_for(std::chrono::milliseconds(kInputPulseWidthMs));
    return SendUp(key, src);
}
bool RecordingDriver::SendMouseWheel(int delta, InputSource) {
    Capture({}, false, delta);
    return true;
}
void RecordingDriver::ReleaseAll() {
    std::lock_guard lock(mutex);
    for (auto it = held.rbegin(); it != held.rend(); ++it) Capture(*it, false);
    held.clear();
}
}
