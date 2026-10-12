#include "services/AgentService.h"

#include "ConfigStore.h"
#include "JsonLite.h"

#include <Windows.h>
#include <sddl.h>
#include <shellapi.h>

#include <vector>

namespace {
constexpr DWORD kMaxPayload = 1024 * 1024;

std::wstring EnvironmentPath(const wchar_t* name) {
    DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) return {};
    std::wstring value(size, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), size);
    value.resize(written);
    return value;
}

std::wstring CurrentUserSid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return L"unknown";
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> buffer(size);
    if (!GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) {
        CloseHandle(token);
        return L"unknown";
    }
    LPWSTR sidText = nullptr;
    const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
    if (!ConvertSidToStringSidW(user->User.Sid, &sidText)) {
        CloseHandle(token);
        return L"unknown";
    }
    std::wstring result(sidText);
    LocalFree(sidText);
    CloseHandle(token);
    return result;
}

std::wstring TaskName() {
    return L"XiaoJiAutoKeyAgent-" + CurrentUserSid();
}

bool RunAndWait(const std::wstring& executable, const std::wstring& arguments, DWORD* exitCode,
    const wchar_t* verb = nullptr, DWORD waitMs = 15000) {
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = verb;
    info.lpFile = executable.c_str();
    info.lpParameters = arguments.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || !info.hProcess) return false;
    if (WaitForSingleObject(info.hProcess, waitMs) != WAIT_OBJECT_0) {
        CloseHandle(info.hProcess);
        return false;
    }
    DWORD code = 1;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    if (exitCode) *exitCode = code;
    return code == 0;
}

bool SendRequest(const jsonlite::Object& request, jsonlite::Object* response,
    std::wstring* error, DWORD waitMs = 1500) {
    const std::string payload = jsonlite::Stringify(request, 0);
    if (payload.size() > kMaxPayload) {
        if (error) *error = L"Agent IPC payload is too large";
        return false;
    }
    if (!WaitNamedPipeW(AgentService::PipeName().c_str(), waitMs)) {
        if (error) *error = L"Agent is not responding";
        return false;
    }
    HANDLE pipe = CreateFileW(AgentService::PipeName().c_str(), GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        if (error) *error = L"Unable to open Agent IPC";
        return false;
    }
    const DWORD length = static_cast<DWORD>(payload.size());
    DWORD transferred = 0;
    bool ok = WriteFile(pipe, &length, sizeof(length), &transferred, nullptr) && transferred == sizeof(length)
        && WriteFile(pipe, payload.data(), length, &transferred, nullptr) && transferred == length;
    DWORD replyLength = 0;
    ok = ok && ReadFile(pipe, &replyLength, sizeof(replyLength), &transferred, nullptr)
        && transferred == sizeof(replyLength) && replyLength <= kMaxPayload;
    std::string reply(replyLength, '\0');
    ok = ok && ReadFile(pipe, reply.data(), replyLength, &transferred, nullptr) && transferred == replyLength;
    CloseHandle(pipe);
    if (!ok) {
        if (error) *error = L"Agent IPC transfer failed";
        return false;
    }
    jsonlite::Value value;
    std::string parseError;
    if (!jsonlite::Parse(reply, value, parseError) || !value.AsObject()) {
        if (error) *error = L"Agent returned an invalid response";
        return false;
    }
    *response = *value.AsObject();
    const auto success = response->find("ok");
    if (success == response->end() || !success->second.AsBool() || !*success->second.AsBool()) {
        const auto detail = response->find("error");
        if (error) *error = detail != response->end() && detail->second.AsString()
            ? ToWide(*detail->second.AsString()) : L"Agent rejected the request";
        return false;
    }
    return true;
}

jsonlite::Object Command(const char* command) {
    jsonlite::Object request;
    request.emplace("command", std::string(command));
    return request;
}

bool TaskRegistered() {
    DWORD code = 1;
    return RunAndWait(L"schtasks.exe",
        L"/Query /TN \"" + TaskName() + L"\"", &code);
}
}

std::filesystem::path AgentService::DataDirectory() {
    return std::filesystem::path(EnvironmentPath(L"LOCALAPPDATA")) / L"XiaoJiAutoKey";
}

std::filesystem::path AgentService::LegacyConfigPath() {
    return DataDirectory() / L"agent-config.json";
}

std::filesystem::path AgentService::InstallDirectory() {
    return std::filesystem::path(EnvironmentPath(L"ProgramFiles")) / L"XiaoJiAutoKey" / L"Agent";
}

std::filesystem::path AgentService::ExecutablePath() {
    return InstallDirectory() / L"XiaoJiAgent.exe";
}

std::wstring AgentService::PipeName() {
    std::wstring sid = CurrentUserSid();
    for (auto& ch : sid) if (ch == L'\\') ch = L'_';
    return L"\\\\.\\pipe\\XiaoJiAutoKey.Agent." + sid;
}

bool AgentService::RecoverFrontendConfig(const std::filesystem::path& frontendConfig) {
    auto frontendTemp = frontendConfig;
    frontendTemp += L".tmp";
    auto frontendBackup = frontendConfig;
    frontendBackup += L".bak";
    // ConfigStore already recovers these files. Never overwrite a newer
    // frontend configuration with stale Agent runtime state.
    for (const auto& path : {frontendConfig, frontendTemp, frontendBackup}) {
        if (ConfigStore::IsValidConfigFile(path)) return true;
    }

    const auto legacyConfig = LegacyConfigPath();
    auto legacyTemp = legacyConfig;
    legacyTemp += L".tmp";
    auto legacyBackup = legacyConfig;
    legacyBackup += L".bak";
    if (!ConfigStore::IsValidConfigFile(legacyConfig) &&
        !ConfigStore::IsValidConfigFile(legacyTemp) &&
        !ConfigStore::IsValidConfigFile(legacyBackup)) {
        return true;
    }

    std::error_code ec;
    if (std::filesystem::exists(frontendConfig, ec)) {
        auto corruptPath = frontendConfig;
        corruptPath += L".corrupt-" + std::to_wstring(GetTickCount64());
        if (!std::filesystem::copy_file(frontendConfig, corruptPath, ec)) return false;
    } else if (ec) {
        return false;
    }

    ConfigStore legacyStore(legacyConfig);
    ConfigStore frontendStore(frontendConfig);
    std::wstring error;
    return frontendStore.Save(legacyStore.Load(), error);
}

bool AgentService::Uninstall(std::wstring* error) {
    if (std::filesystem::exists(ExecutablePath()) || WaitNamedPipeW(PipeName().c_str(), 0)) {
        Shutdown(nullptr);
    }
    for (int attempt = 0; attempt < 30 && WaitNamedPipeW(PipeName().c_str(), 0); ++attempt) {
        Sleep(100);
    }
    bool removed = true;
    if (TaskRegistered()) {
        DWORD exitCode = 0;
        removed = RunAndWait(
            L"schtasks.exe", L"/Delete /F /TN \"" + TaskName() + L"\"", &exitCode);
    }
    if (!removed) {
        if (error) *error = L"Unable to remove the Agent logon task";
        return false;
    }

    std::error_code ec;
    for (int attempt = 0; attempt < 30; ++attempt) {
        ec.clear();
        std::filesystem::remove_all(InstallDirectory(), ec);
        if (!ec) break;
        Sleep(100);
    }
    if (ec) {
        if (error) *error = L"Unable to remove the installed Agent files";
        return false;
    }
    // Keep legacy configurations and logs available for recovery after an upgrade.
    return true;
}

bool AgentService::Shutdown(std::wstring* error) {
    jsonlite::Object response;
    return SendRequest(Command("SHUTDOWN"), &response, error);
}
