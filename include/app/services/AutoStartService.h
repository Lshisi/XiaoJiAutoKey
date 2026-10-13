#pragma once

// 开机自启：通过 HKCU\Software\Microsoft\Windows\CurrentVersion\Run 注册表键实现。
// 程序以管理员清单运行，登录时系统会弹出一次 UAC 确认，这是管理员程序开机自启的常规表现。
class AutoStartService {
public:
    [[nodiscard]] static bool isEnabled();
    static bool setEnabled(bool enabled);
};
