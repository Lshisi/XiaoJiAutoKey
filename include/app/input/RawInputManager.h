#pragma once

#include "AppTypes.h"

#include <QObject>
#include <QStringList>
#include <Windows.h>

#include <optional>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <string>

class RawInputManager : public QObject {
    Q_OBJECT

public:
    explicit RawInputManager(QObject* parent = nullptr);
    ~RawInputManager() override;

    bool registerWindow(HWND hwnd);
    void unregisterWindow();

    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] bool isMouseActive() const noexcept;
    bool handleNativeMessage(MSG* message, qintptr* result);
    void setSelfInjectedFilter(std::function<bool(const KeyDescriptor&, bool)> filter);
    void setHookModifierSource(std::function<bool(const KeyDescriptor&)> source);
    void noteHookPhysicalKey(const KeyDescriptor& key, bool pressed);
    void setPluginDeviceTokens(const QStringList& tokens);

signals:
    void keyboardChanged(KeyDescriptor key, bool pressed);
    void mouseChanged(KeyDescriptor key, bool pressed);

private:
    struct DeviceInfo {
        bool virtualInjector = false;
        std::wstring name;
    };

    std::optional<DeviceInfo> queryDeviceInfo(HANDLE device);
    bool shouldAcceptDevice(HANDLE device);
    KeyDescriptor rawKeyboardKeyDescriptor(const RAWKEYBOARD& keyboard) const;
    void emitRawMouseChanges(const RAWMOUSE& mouse, bool unknownDevice);
    void updateModifierState(const KeyDescriptor& key, bool pressed);

    HWND hwnd_ = nullptr;
    bool keyboardActive_ = false;
    bool mouseActive_ = false;
    std::uint8_t keyboardModifiers_ = 0;
    std::unordered_set<std::uint32_t> modifierKeysDown_;
    std::unordered_map<HANDLE, DeviceInfo> deviceCache_;
    std::function<bool(const KeyDescriptor&, bool)> selfInjectedFilter_;
    std::function<bool(const KeyDescriptor&)> hookModifierSource_;
    std::vector<std::wstring> pluginDeviceTokens_;
};
