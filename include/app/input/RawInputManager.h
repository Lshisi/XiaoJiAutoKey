#pragma once

#include "AppTypes.h"

#include <QObject>
#include <Windows.h>

#include <optional>
#include <unordered_map>
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
    void emitRawMouseChanges(const RAWMOUSE& mouse);

    HWND hwnd_ = nullptr;
    bool keyboardActive_ = false;
    bool mouseActive_ = false;
    std::unordered_map<HANDLE, DeviceInfo> deviceCache_;
};
