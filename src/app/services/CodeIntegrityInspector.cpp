#include "services/CodeIntegrityInspector.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winevt.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <optional>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wevtapi.lib")

namespace {

// NtQuerySystemInformation(SystemCodeIntegrityInformation) 是微软文档记载的
// "判断 HVCI（内存完整性）是否生效"方法，反映本次开机实际状态而非注册表配置。
constexpr ULONG kSystemCodeIntegrityInformation = 103;
constexpr ULONG kCodeIntegrityOptionVsmEnabled = 0x8;

struct SystemCodeIntegrityInformation {
    ULONG Length;
    ULONG CodeIntegrityOptions;
};

using NtQuerySystemInformationFn = LONG(WINAPI*)(ULONG, PVOID, ULONG, PULONG);

struct EventHandleCloser {
    void operator()(void* handle) const { EvtClose(handle); }
};
using EventHandle = std::unique_ptr<void, EventHandleCloser>;

std::optional<DWORD> readRegistryDword(HKEY root, const wchar_t* subKey, const wchar_t* value) {
    DWORD data = 0;
    DWORD size = sizeof(data);
    const LSTATUS status = RegGetValueW(root, subKey, value, RRF_RT_REG_DWORD,
        nullptr, &data, &size);
    if (status != ERROR_SUCCESS) return std::nullopt;
    return data;
}

std::wstring toLower(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return text;
}

std::wstring extractTagText(const std::wstring& xml, const wchar_t* tag) {
    const std::wstring openTag = std::wstring(L"<") + tag;
    size_t pos = xml.find(openTag);
    if (pos == std::wstring::npos) return {};
    pos = xml.find(L'>', pos);
    if (pos == std::wstring::npos) return {};
    const size_t end = xml.find(std::wstring(L"</") + tag + L'>', pos + 1);
    if (end == std::wstring::npos) return {};
    return xml.substr(pos + 1, end - pos - 1);
}

// XML 时间形如 2026-10-18T03:21:45.123456700Z，转本地时间展示文本。
std::wstring extractLocalTimeText(const std::wstring& xml) {
    const size_t pos = xml.find(L"SystemTime='");
    if (pos == std::wstring::npos) return {};
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (swscanf_s(xml.c_str() + pos + 12, L"%4d-%2d-%2dT%2d:%2d:%2d",
            &year, &month, &day, &hour, &minute, &second) < 6) {
        return {};
    }
    SYSTEMTIME utc{};
    utc.wYear = static_cast<WORD>(year);
    utc.wMonth = static_cast<WORD>(month);
    utc.wDay = static_cast<WORD>(day);
    utc.wHour = static_cast<WORD>(hour);
    utc.wMinute = static_cast<WORD>(minute);
    utc.wSecond = static_cast<WORD>(second);
    SYSTEMTIME local{};
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) return {};
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u",
        local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute);
    return buffer;
}

// 近 7 天代码完整性拦截事件（3033/3077），匹配我方驱动文件名，最多返回 5 条。
std::vector<CodeIntegrityInspector::BlockedDriverEvent> queryRecentBlocks(
    const std::vector<std::wstring>& driverFiles, bool* incomplete) {
    constexpr DWORD maxScannedEvents = 512;
    constexpr ULONGLONG maxScanTimeMs = 1500;
    const ULONGLONG startedAt = GetTickCount64();
    *incomplete = true;
    const wchar_t* channel = L"Microsoft-Windows-CodeIntegrity/Operational";
    const wchar_t* query =
        L"*[System[(EventID=3033 or EventID=3077) and "
        L"TimeCreated[timediff(@SystemTime) <= 604800000]]]";

    EventHandle queryHandle(EvtQuery(nullptr, channel, query,
        EvtQueryChannelPath | EvtQueryReverseDirection));
    if (!queryHandle) return {};

    std::vector<CodeIntegrityInspector::BlockedDriverEvent> result;
    DWORD scanned = 0;
    bool renderFailed = false;
    while (result.size() < 5 && scanned < maxScannedEvents
        && GetTickCount64() - startedAt < maxScanTimeMs) {
        std::array<EVT_HANDLE, 8> events{};
        DWORD returned = 0;
        if (!EvtNext(queryHandle.get(), static_cast<DWORD>(events.size()), events.data(), 50, 0, &returned)) {
            *incomplete = GetLastError() != ERROR_NO_MORE_ITEMS || renderFailed;
            return result;
        }
        if (returned == 0) {
            *incomplete = renderFailed;
            return result;
        }
        std::array<EventHandle, 8> handles;
        for (DWORD i = 0; i < returned; ++i) handles[i].reset(events[i]);
        for (DWORD i = 0; i < returned && result.size() < 5 && scanned < maxScannedEvents
            && GetTickCount64() - startedAt < maxScanTimeMs; ++i) {
            ++scanned;
            DWORD needed = 0;
            DWORD propertyCount = 0;
            EvtRender(nullptr, handles[i].get(), EvtRenderEventXml, 0, nullptr, &needed, &propertyCount);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && needed > 0) {
                std::wstring xml(needed / sizeof(wchar_t), L'\0');
                DWORD used = 0;
                if (EvtRender(nullptr, handles[i].get(), EvtRenderEventXml, needed, xml.data(), &used, &propertyCount) && used > 0) {
                    xml.resize(used / sizeof(wchar_t));
                    const std::wstring lowerXml = toLower(xml);
                    for (const auto& file : driverFiles) {
                        if (lowerXml.find(toLower(file)) == std::wstring::npos) continue;
                        CodeIntegrityInspector::BlockedDriverEvent event;
                        event.fileName = file;
                        event.timeText = extractLocalTimeText(xml);
                        const std::wstring idText = extractTagText(xml, L"EventID");
                        if (!idText.empty()) event.eventId = _wtoi(idText.c_str());
                        result.push_back(std::move(event));
                        break;
                    }
                } else {
                    renderFailed = true;
                }
            } else {
                renderFailed = true;
            }
        }
    }
    if (result.size() >= 5) *incomplete = renderFailed;
    return result;
}

}  // namespace

const std::vector<std::wstring>& CodeIntegrityInspector::bundledKernelDrivers() {
    static const std::vector<std::wstring> files = {
        L"dd.key.96688.sys",
        L"dd.mou.96688.sys",
        L"xiaoji.dll",
    };
    return files;
}

CodeIntegrityInspector::GuardStatus CodeIntegrityInspector::Inspect(bool includeEventLog) {
    GuardStatus status;

    if (auto* ntQuery = reinterpret_cast<NtQuerySystemInformationFn>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation"))) {
        SystemCodeIntegrityInformation info{};
        info.Length = sizeof(info);
        ULONG returned = 0;
        if (ntQuery(kSystemCodeIntegrityInformation, &info, sizeof(info), &returned) == 0) {
            status.memoryIntegrityRunning =
                (info.CodeIntegrityOptions & kCodeIntegrityOptionVsmEnabled) != 0;
        }
    }

    status.memoryIntegrityConfigured =
        readRegistryDword(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
            L"Enabled").value_or(0) != 0;

    // 阻止列表注册表值不存在时按微软默认视为开启（Win10 1903+/Win11 默认启用）。
    const auto blocklist = readRegistryDword(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\CI\\Config", L"VulnerableDriverBlocklistEnable");
    status.vulnerableBlocklistEnabled = blocklist.value_or(1) != 0;

    status.smartAppControlState = static_cast<int>(readRegistryDword(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\CI\\Policy", L"VerifiedAndReputablePolicyState")
        .value_or(0));

    if (includeEventLog) {
        status.recentBlocks = queryRecentBlocks(bundledKernelDrivers(), &status.recentBlocksIncomplete);
    }
    return status;
}
