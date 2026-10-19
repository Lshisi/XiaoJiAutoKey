#include "DiagLog.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace {

constexpr std::uintmax_t kMaxDiagLogBytes = 256 * 1024;

std::mutex g_diagLogMutex;

std::filesystem::path ResolveDiagLogPath() {
    wchar_t exePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(exePath).parent_path() / L"logs" / L"dd-inject.log";
}

}  // namespace

namespace DiagLog {

void Write(const std::string& line) {
    std::lock_guard lock(g_diagLogMutex);

    const auto path = ResolveDiagLogPath();
    if (path.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);

    bool truncate = false;
    if (std::filesystem::exists(path, error) && !error) {
        const auto size = std::filesystem::file_size(path, error);
        truncate = !error && size > kMaxDiagLogBytes;
    }
    error.clear();

    std::ofstream file(path, truncate ? std::ios::trunc : std::ios::app);
    if (!file) {
        return;
    }

    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    char stamp[32] = {};
    std::snprintf(stamp, sizeof(stamp), "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        now.wYear, now.wMonth, now.wDay,
        now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    file << stamp << ' ' << line << '\n';
}

}  // namespace DiagLog
