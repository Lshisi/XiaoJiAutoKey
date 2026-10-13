#include "services/AutoStartService.h"

#include <windows.h>

#include <string>

namespace {

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"XiaoJiAutoKey";

std::wstring currentExePath() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length >= path.size()) {
        // 路径超过 MAX_PATH 时用系统上限重试一次。
        path.resize(32768, L'\0');
        length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    }
    if (length == 0 || length >= path.size()) return std::wstring();
    path.resize(length);
    return path;
}

}  // namespace

bool AutoStartService::isEnabled() {
    wchar_t buffer[1024] = {};
    DWORD bytes = sizeof(buffer);
    const LSTATUS status = ::RegGetValueW(HKEY_CURRENT_USER, kRunKeyPath, kValueName,
        RRF_RT_REG_SZ, nullptr, buffer, &bytes);
    return status == ERROR_SUCCESS;
}

bool AutoStartService::setEnabled(bool enabled) {
    if (!enabled) {
        const LSTATUS status = ::RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKeyPath, kValueName);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
    const std::wstring exePath = currentExePath();
    if (exePath.empty()) return false;
    // 带引号的完整命令行，避免含空格目录被系统截断。
    const std::wstring command = L"\"" + exePath + L"\"";
    HKEY key = nullptr;
    LSTATUS status = ::RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) return false;
    const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    status = ::RegSetValueExW(key, kValueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()), bytes);
    ::RegCloseKey(key);
    return status == ERROR_SUCCESS;
}
