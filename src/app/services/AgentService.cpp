#include "services/AgentService.h"

#include "ConfigStore.h"
#include "JsonLite.h"

#include <Windows.h>
#include <sddl.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <vector>

namespace {
constexpr DWORD kMaxPayload = 1024 * 1024;
constexpr wchar_t kConfigRegistryValue[] = L"ConfigPath";

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

const std::wstring& ConfigRegistryKey() {
    static const std::wstring key = []() {
        const auto sid = CurrentUserSid();
        return sid.empty() || sid == L"unknown"
            ? std::wstring{} : L"Software\\XiaoJiAutoKey\\Users\\" + sid;
    }();
    return key;
}

bool CopyTree(const std::filesystem::path& source, const std::filesystem::path& target) {
    std::error_code ec;
    if (!std::filesystem::exists(source)) return true;
    std::filesystem::create_directories(target, ec);
    if (ec) return false;
    for (std::filesystem::recursive_directory_iterator it(source, ec), end; it != end && !ec; it.increment(ec)) {
        const auto relative = std::filesystem::relative(it->path(), source, ec);
        if (ec) return false;
        const auto destination = target / relative;
        if (it->is_directory()) {
            std::filesystem::create_directories(destination, ec);
        } else if (it->is_regular_file()) {
            std::filesystem::create_directories(destination.parent_path(), ec);
            if (ec) return false;
            if (!std::filesystem::equivalent(it->path(), destination, ec)) {
                ec.clear();
                std::filesystem::copy_file(it->path(), destination,
                    std::filesystem::copy_options::overwrite_existing, ec);
            } else {
                ec.clear();
            }
        }
    }
    return !ec;
}

bool FilesEqual(const std::filesystem::path& left, const std::filesystem::path& right) {
    std::error_code ec;
    if (!std::filesystem::exists(left) || !std::filesystem::exists(right) ||
        std::filesystem::file_size(left, ec) != std::filesystem::file_size(right, ec) || ec) {
        return false;
    }
    std::ifstream a(left, std::ios::binary);
    std::ifstream b(right, std::ios::binary);
    std::array<char, 65536> aBuffer{};
    std::array<char, 65536> bBuffer{};
    while (a && b) {
        a.read(aBuffer.data(), aBuffer.size());
        b.read(bBuffer.data(), bBuffer.size());
        if (a.gcount() != b.gcount() ||
            !std::equal(aBuffer.begin(), aBuffer.begin() + a.gcount(), bBuffer.begin())) return false;
    }
    return true;
}

bool TreesEqual(const std::filesystem::path& source, const std::filesystem::path& target) {
    std::error_code ec;
    if (!std::filesystem::exists(source)) return true;
    if (!std::filesystem::exists(target)) return false;
    for (std::filesystem::recursive_directory_iterator it(source, ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        const auto relative = std::filesystem::relative(it->path(), source, ec);
        if (ec || !FilesEqual(it->path(), target / relative)) return false;
    }
    return !ec;
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

std::filesystem::path AgentService::FrontendConfigPath() {
    const auto& registryKey = ConfigRegistryKey();
    if (registryKey.empty()) return {};
    DWORD size = 0;
    constexpr DWORD flags = RRF_RT_REG_SZ;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, registryKey.c_str(), kConfigRegistryValue,
            flags, nullptr, nullptr, &size) != ERROR_SUCCESS || size < sizeof(wchar_t)) {
        return {};
    }
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_LOCAL_MACHINE, registryKey.c_str(), kConfigRegistryValue,
            flags, nullptr, value.data(), &size) != ERROR_SUCCESS) {
        return {};
    }
    if (!value.empty() && value.back() == L'\0') value.pop_back();
    return value.empty() ? std::filesystem::path{} : std::filesystem::path(value);
}

bool AgentService::PublishFrontendConfigPath(
    const std::filesystem::path& configPath, std::wstring* error) {
    const auto& registryKey = ConfigRegistryKey();
    if (registryKey.empty() || configPath.empty() || !configPath.is_absolute()) {
        if (error) *error = L"Frontend configuration path is invalid";
        return false;
    }
    HKEY key = nullptr;
    const auto openResult = RegCreateKeyExW(HKEY_LOCAL_MACHINE, registryKey.c_str(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (openResult != ERROR_SUCCESS) {
        if (error) *error = L"Unable to register the frontend configuration path";
        return false;
    }
    const std::wstring value = configPath.wstring();
    const auto setResult = RegSetValueExW(key, kConfigRegistryValue, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    if (setResult != ERROR_SUCCESS) {
        if (error) *error = L"Unable to register the frontend configuration path";
        return false;
    }
    return true;
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
    const auto legacyConfig = LegacyConfigPath();
    auto legacyBackup = legacyConfig;
    legacyBackup += L".bak";
    const auto removeLegacyFiles = [legacyConfig, legacyBackup]() {
        std::error_code ec;
        std::filesystem::remove(legacyConfig, ec);
        std::filesystem::remove(legacyBackup, ec);
        auto legacyTemp = legacyConfig;
        legacyTemp += L".tmp";
        std::filesystem::remove(legacyTemp, ec);
    };
    const auto migrateLegacyRuntimeState = [&]() {
        const auto source = ConfigStore::IsValidConfigFile(legacyConfig)
            ? legacyConfig : legacyBackup;
        if (ConfigStore::IsValidConfigFile(source)) {
            ConfigStore frontendStore(frontendConfig);
            ConfigStore legacyStore(source);
            AppConfig frontend = frontendStore.Load();
            const AppConfig legacy = legacyStore.Load();
            frontend.statusOverlayLocked = legacy.statusOverlayLocked;
            frontend.statusOverlayPositionSaved = legacy.statusOverlayPositionSaved;
            frontend.statusOverlayX = legacy.statusOverlayX;
            frontend.statusOverlayY = legacy.statusOverlayY;
            std::wstring error;
            if (!frontendStore.Save(frontend, error)) return false;
        }
        removeLegacyFiles();
        return true;
    };
    auto frontendTemp = frontendConfig;
    frontendTemp += L".tmp";
    auto frontendBackup = frontendConfig;
    frontendBackup += L".bak";
    ConfigStore frontendStore(frontendConfig);
    const bool currentValid = ConfigStore::IsValidConfigFile(frontendConfig);
    const bool tempValid = ConfigStore::IsValidConfigFile(frontendTemp);
    std::error_code tempTimeError;
    std::error_code currentTimeError;
    const auto tempTime = std::filesystem::last_write_time(frontendTemp, tempTimeError);
    const auto currentTime = std::filesystem::last_write_time(frontendConfig, currentTimeError);
    const bool tempIsNewer = !tempTimeError && (currentTimeError || tempTime >= currentTime);
    if (tempValid && (!currentValid || tempIsNewer)) {
        std::wstring error;
        if (frontendStore.RestoreFrom(frontendTemp, error)) return migrateLegacyRuntimeState();
    }
    if (currentValid) return migrateLegacyRuntimeState();

    for (const auto& candidate : {frontendBackup, legacyConfig, legacyBackup}) {
        if (!ConfigStore::IsValidConfigFile(candidate)) continue;
        std::wstring error;
        if (frontendStore.RestoreFrom(candidate, error)) return migrateLegacyRuntimeState();
    }

    const auto preserveCorruptFile = [](const std::filesystem::path& path) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) return;
        auto corruptPath = path;
        corruptPath += L".corrupt";
        ::MoveFileExW(path.c_str(), corruptPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    };
    for (const auto& path : {frontendConfig, frontendTemp, frontendBackup}) {
        if (!ConfigStore::IsValidConfigFile(path)) preserveCorruptFile(path);
    }
    removeLegacyFiles();
    return false;
}

bool AgentService::EnsureInstalled(const std::filesystem::path& sourceExe, std::wstring* error) {
    const bool filesCurrent = FilesEqual(sourceExe, ExecutablePath()) &&
        TreesEqual(sourceExe.parent_path() / L"audio", InstallDirectory() / L"audio");
    if (!filesCurrent || !TaskRegistered()) {
        if (WaitNamedPipeW(PipeName().c_str(), 0)) Shutdown(nullptr);
        DWORD exitCode = 1;
        if (!RunAndWait(sourceExe.wstring(), L"--install-agent", &exitCode, L"runas", 120000)) {
            if (error) *error = L"Agent installation was cancelled or failed";
            return false;
        }
    }
    return Start(error);
}

int AgentService::InstallElevated(const std::filesystem::path& sourceExe) {
    std::error_code ec;
    std::filesystem::create_directories(InstallDirectory(), ec);
    if (ec) return 3;

    if (!FilesEqual(sourceExe, ExecutablePath())) {
        auto stagedExe = ExecutablePath();
        stagedExe += L".new";
        std::filesystem::copy_file(sourceExe, stagedExe,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) return 4;
        bool replaced = false;
        for (int attempt = 0; attempt < 40 && !replaced; ++attempt) {
            replaced = MoveFileExW(stagedExe.c_str(), ExecutablePath().c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
            if (!replaced) Sleep(100);
        }
        if (!replaced) {
            std::filesystem::remove(stagedExe, ec);
            return 4;
        }
    }
    if (!CopyTree(sourceExe.parent_path() / L"audio", InstallDirectory() / L"audio")) return 5;

    const std::wstring taskCommand = L"/Create /F /SC ONLOGON /RL HIGHEST /IT /TN \"" +
        TaskName() + L"\" /TR \"\\\"" + ExecutablePath().wstring() + L"\\\" --agent\"";
    DWORD exitCode = 1;
    return RunAndWait(L"schtasks.exe", taskCommand, &exitCode) ? 0 : 6;
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
    auto backupPath = LegacyConfigPath();
    backupPath += L".bak";
    auto tempPath = LegacyConfigPath();
    tempPath += L".tmp";
    const std::array<std::filesystem::path, 4> dataFiles = {
        LegacyConfigPath(), backupPath, tempPath, DataDirectory() / L"agent.log"};
    for (const auto& path : dataFiles) {
        std::filesystem::remove(path, ec);
        if (ec) {
            if (error) *error = L"Unable to remove the Agent data files";
            return false;
        }
    }
    std::filesystem::remove(DataDirectory(), ec);
    if (ec && error) {
        *error = L"Unable to remove the Agent data directory";
        return false;
    }
    return true;
}

bool AgentService::Start(std::wstring* error) {
    if (QueryStatus().running) return true;
    DWORD exitCode = 1;
    if (!RunAndWait(L"schtasks.exe", L"/Run /TN \"" + TaskName() + L"\"", &exitCode)) {
        if (error) *error = L"Unable to start the Agent task";
        return false;
    }
    for (int i = 0; i < 20; ++i) {
        Sleep(100);
        if (QueryStatus().running) return true;
    }
    if (error) *error = L"Agent did not start in time";
    return false;
}

bool AgentService::IsRunning() {
    jsonlite::Object response;
    return SendRequest(Command("STATUS"), &response, nullptr, 100);
}

AgentStatus AgentService::QueryStatus() {
    AgentStatus status;
    status.installed = std::filesystem::exists(ExecutablePath());
    status.taskRegistered = TaskRegistered();
    jsonlite::Object response;
    status.running = SendRequest(Command("STATUS"), &response, nullptr, 100);
    if (status.running) {
        const auto readBool = [&response](const char* key) {
            const auto it = response.find(key);
            return it != response.end() && it->second.AsBool() && *it->second.AsBool();
        };
        status.pausedForUi = readBool("paused");
        status.usbConnected = readBool("usbConnected");
        status.firmwareUpdateRequired = readBool("firmwareUpdateRequired");
    }
    return status;
}

bool AgentService::PauseForUi(std::uint32_t processId, bool* repeatRunning, std::wstring* error) {
    auto request = Command("PAUSE_FOR_UI");
    request.emplace("pid", static_cast<double>(processId));
    jsonlite::Object response;
    if (!SendRequest(request, &response, error)) return false;
    if (repeatRunning) {
        const auto it = response.find("repeatRunning");
        *repeatRunning = it != response.end() && it->second.AsBool() && *it->second.AsBool();
    }
    return true;
}

bool AgentService::Resume(bool repeatRunning, std::wstring* error) {
    auto request = Command("RESUME");
    request.emplace("repeatRunning", repeatRunning);
    jsonlite::Object response;
    return SendRequest(request, &response, error);
}

bool AgentService::Shutdown(std::wstring* error) {
    jsonlite::Object response;
    return SendRequest(Command("SHUTDOWN"), &response, error);
}
