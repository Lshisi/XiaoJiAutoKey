#include "agent/AgentRuntime.h"

#include "JsonLite.h"
#include "config/ActiveConfigValidator.h"
#include "config/ConfigNormalizer.h"
#include "services/AgentService.h"

#include <QApplication>
#include <QMetaObject>

#include <Windows.h>
#include <sddl.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <future>
#include <vector>

namespace {
constexpr DWORD kMaxPayload = 1024 * 1024;
constexpr wchar_t kRequiredFirmware[] = L"XiaoJi USB V1.1.4";

const ProfileConfig* ActiveProfile(const AppConfig& config) {
    const auto it = std::find_if(config.profiles.begin(), config.profiles.end(),
        [&config](const ProfileConfig& profile) { return profile.id == config.activeProfileId; });
    return it == config.profiles.end() ? nullptr : &*it;
}

const MacroConfig* ActiveMacro(const AppConfig& config) {
    const auto it = std::find_if(config.macros.begin(), config.macros.end(),
        [&config](const MacroConfig& macro) { return macro.id == config.activeMacroId; });
    return it == config.macros.end() ? nullptr : &*it;
}

jsonlite::Object Reply(bool ok, const std::string& error = {}) {
    jsonlite::Object result;
    result.emplace("ok", ok);
    if (!error.empty()) result.emplace("error", error);
    return result;
}

void WriteAgentLog(const char* event) {
    std::error_code ec;
    std::filesystem::create_directories(AgentService::DataDirectory(), ec);
    if (ec) return;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char timestamp[32] = {};
    std::snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02u %02u:%02u:%02u",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    std::ofstream log(AgentService::DataDirectory() / L"agent.log",
        std::ios::binary | std::ios::app);
    if (log) log << timestamp << " " << event << '\n';
}

std::wstring CurrentUserSid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> buffer(size);
    if (!GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) {
        CloseHandle(token);
        return {};
    }
    LPWSTR sidText = nullptr;
    const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
    if (!ConvertSidToStringSidW(user->User.Sid, &sidText)) {
        CloseHandle(token);
        return {};
    }
    std::wstring result(sidText);
    LocalFree(sidText);
    CloseHandle(token);
    return result;
}

bool ReadExact(HANDLE pipe, void* data, DWORD size) {
    auto* cursor = static_cast<BYTE*>(data);
    DWORD total = 0;
    while (total < size) {
        DWORD read = 0;
        if (!ReadFile(pipe, cursor + total, size - total, &read, nullptr) || read == 0) return false;
        total += read;
    }
    return true;
}

bool WriteExact(HANDLE pipe, const void* data, DWORD size) {
    const auto* cursor = static_cast<const BYTE*>(data);
    DWORD total = 0;
    while (total < size) {
        DWORD written = 0;
        if (!WriteFile(pipe, cursor + total, size - total, &written, nullptr) || written == 0) return false;
        total += written;
    }
    return true;
}
}

AgentRuntime::AgentRuntime(QWidget* parent)
    : QWidget(parent),
      store_(std::filesystem::path{}),
      repeater_(inputDriver_),
      macro_(inputDriver_),
      hook_(this),
      rawInput_(this),
      sound_(AgentService::InstallDirectory()),
      overlay_(nullptr) {
    setAttribute(Qt::WA_DontShowOnScreen, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    resize(1, 1);

    const auto handleRawInput = [this](const KeyDescriptor& key, bool pressed) {
        const auto* profile = ActiveProfile(config_);
        if (hook_.isRunning() && profile && profile->enabled && profile->controlHotkey &&
            *profile->controlHotkey == key) {
            return;
        }
        handleInput(key, pressed);
    };
    connect(&rawInput_, &RawInputManager::keyboardChanged, this, handleRawInput);
    connect(&rawInput_, &RawInputManager::mouseChanged, this, handleRawInput);
    connect(&hook_, &GlobalInputHook::controlHotkeyPressed, this, [this]() { toggleRepeat(); });
    connect(&hook_, &GlobalInputHook::physicalKeyChanged, this,
        [this](const KeyDescriptor& key, bool pressed) { handleInput(key, pressed); });
    connect(&hook_, &GlobalInputHook::macroTriggerChanged, this,
        [this](const KeyDescriptor& key, bool pressed) { handleMacroInput(key, pressed); });

    connect(&overlay_, &StatusOverlayWindow::positionChanged, this, [this](const QPoint& position) {
        config_.statusOverlayPositionSaved = true;
        config_.statusOverlayX = position.x();
        config_.statusOverlayY = position.y();
        std::wstring error;
        store_.Save(config_, error);
    });
    connect(&overlay_, &StatusOverlayWindow::lockChanged, this, [this](bool locked) {
        config_.statusOverlayLocked = locked;
        std::wstring error;
        store_.Save(config_, error);
    });

    runtimeTimer_.setInterval(750);
    connect(&runtimeTimer_, &QTimer::timeout, this, [this]() { ensureRuntimeState(); });
    stateTimer_.setInterval(250);
    connect(&stateTimer_, &QTimer::timeout, this, [this]() {
        checkFrontendProcess();
        updateOverlay();
    });
}

AgentRuntime::~AgentRuntime() {
    pipeExit_ = true;
    wakePipeServer();
    if (pipeThread_.joinable()) pipeThread_.join();
    deactivateRuntime(true, false);
}

bool AgentRuntime::initialize() {
    createWinId();
    refreshConfigPath();
    desiredRepeatRunning_ = false;
    WriteAgentLog("agent started");
    pipeThread_ = std::thread(&AgentRuntime::pipeLoop, this);
    runtimeTimer_.start();
    stateTimer_.start();
    QTimer::singleShot(0, this, [this]() { ensureRuntimeState(); });
    return true;
}

bool AgentRuntime::nativeEvent(const QByteArray& eventType, void* message, long* result) {
    auto* msg = static_cast<MSG*>(message);
    if (rawInput_.handleNativeMessage(msg, result)) return true;
    if (msg && msg->message == WM_DEVICECHANGE) {
        QTimer::singleShot(200, this, [this]() {
            if (runtimeActive_ && !usbDriver_.Heartbeat()) {
                WriteAgentLog("USB disconnected");
                desiredRepeatRunning_ = false;
                deactivateRuntime(true, false);
            }
            ensureRuntimeState();
        });
    }
    return QWidget::nativeEvent(eventType, message, result);
}

void AgentRuntime::ensureRuntimeState() {
    if (shutdownRequested_) return;
    if (pausedForUi_) {
        deactivateRuntime(true);
        return;
    }
    if (!refreshConfigPath()) {
        desiredRepeatRunning_ = false;
        deactivateRuntime(true, false);
        return;
    }
    if (config_.driverMode != DriverMode::Usb || !config_.backgroundAutoRun) {
        desiredRepeatRunning_ = false;
        deactivateRuntime(true, false);
        return;
    }
    if (runtimeActive_) {
        if (!usbDriver_.Heartbeat()) {
            WriteAgentLog("USB heartbeat failed");
            desiredRepeatRunning_ = false;
            deactivateRuntime(true, false);
        }
        return;
    }
    if (connectUsb()) activateRuntime();
}

bool AgentRuntime::refreshConfigPath() {
    const auto configPath = AgentService::FrontendConfigPath();
    std::error_code ec;
    if (configPath.empty() || !std::filesystem::exists(configPath, ec) || ec) {
        configPathReady_ = false;
        return false;
    }
    if (configPathReady_ && store_.GetPath() == configPath) return true;
    if (!ConfigStore::IsValidConfigFile(configPath)) {
        configPathReady_ = false;
        return false;
    }
    if (!configPathReady_ || store_.GetPath() != configPath) {
        store_.SetPath(configPath);
        config_ = store_.Load();
        ConfigNormalizer::normalize(config_);
        configPathReady_ = true;
        WriteAgentLog("frontend configuration path refreshed");
    }
    return true;
}

bool AgentRuntime::connectUsb() {
    if (!usbDriver_.AutoConnect()) {
        firmwareUpdateRequired_ = false;
        return false;
    }
    if (usbDriver_.FirmwareVersion() != kRequiredFirmware || !usbDriver_.BeginLease()) {
        if (!firmwareUpdateRequired_) WriteAgentLog("USB firmware update required");
        firmwareUpdateRequired_ = true;
        usbDriver_.Disconnect();
        return false;
    }
    firmwareUpdateRequired_ = false;
    WriteAgentLog("USB connected");
    return true;
}

void AgentRuntime::activateRuntime() {
    if (runtimeActive_ || !usbDriver_.IsReady()) return;
    inputDriver_.SetDriver(&usbDriver_);
    applyConfig();
    repeater_.StartThread();
    macro_.StartThread();
    rawInput_.registerWindow(reinterpret_cast<HWND>(winId()));
    hook_.setSelfInjectedFilter([this](const KeyDescriptor& key, bool pressed) {
        return inputDriver_.ConsumeSelfInjected(key, pressed);
    });
    refreshHookState();
    runtimeActive_ = true;
    if (!hook_.start()) {
        WriteAgentLog("input hook failed to start");
        deactivateRuntime(true, false);
        return;
    }
    repeater_.SetToolRunning(desiredRepeatRunning_);
    hook_.setRepeatRunning(desiredRepeatRunning_);
    updateOverlay();
}

void AgentRuntime::deactivateRuntime(bool disconnectUsb, bool preserveRepeatState) {
    overlay_.setOverlayEnabled(false);
    if (runtimeActive_) {
        desiredRepeatRunning_ = preserveRepeatState ? repeater_.IsToolRunning() : false;
        hook_.stop();
        rawInput_.unregisterWindow();
        macro_.StopAll();
        repeater_.SetToolRunning(false);
        repeater_.StopThread();
        macro_.StopThread();
        inputDriver_.ReleaseAll();
        inputDriver_.ClearAllStates();
        inputDriver_.SetDriver(nullptr);
        runtimeActive_ = false;
        controlHotkeyDown_ = false;
    } else if (!preserveRepeatState) {
        desiredRepeatRunning_ = false;
    }
    if (disconnectUsb) usbDriver_.Disconnect();
}

void AgentRuntime::applyConfig() {
    const auto* profile = ActiveProfile(config_);
    const auto* activeMacro = ActiveMacro(config_);
    if (profile && profile->enabled) {
        repeater_.UpdateBindings(profile->id, profile->bindings);
        repeater_.SetMixedMode(profile->mixedMode);
        repeater_.SetWasdProtection(profile->wasdProtection);
        repeater_.SetSequenceSwitchKey(profile->sequenceSwitchKey);
        repeater_.SetPauseKeys(profile->pauseKeys);
    } else {
        repeater_.UpdateBindings(0, {});
        repeater_.SetSequenceSwitchKey(std::nullopt);
        repeater_.SetPauseKeys({});
    }
    macro_.StopAll();
    macro_.UpdateMacros(activeMacro && activeMacro->enabled ? std::vector<MacroConfig>{*activeMacro}
                                                            : std::vector<MacroConfig>{});
    refreshHookState();
    sound_.warmUpToggleSounds(config_.customSoundOn, config_.customSoundOff,
        config_.macroSoundOn, config_.macroSoundOff);
}

void AgentRuntime::reloadConfig(bool repeatRunning) {
    desiredRepeatRunning_ = repeatRunning;
    if (!refreshConfigPath()) {
        deactivateRuntime(true, false);
        return;
    }
    config_ = store_.Load();
    ConfigNormalizer::normalize(config_);
    if (runtimeActive_) {
        repeater_.SetToolRunning(false);
        inputDriver_.ReleaseAll();
        applyConfig();
        repeater_.SetToolRunning(desiredRepeatRunning_);
        hook_.setRepeatRunning(desiredRepeatRunning_);
    }
    ensureRuntimeState();
}

void AgentRuntime::refreshHookState() {
    const auto* profile = ActiveProfile(config_);
    hook_.setControlHotkey(profile && profile->enabled ? profile->controlHotkey : std::nullopt);
    hook_.setCaptureActive(false);
    hook_.setRepeatRunning(repeater_.IsToolRunning());
    hook_.setRawKeyboardActive(rawInput_.isActive());
    hook_.setRawMouseActive(rawInput_.isMouseActive());
    repeater_.SetSelfInjectedKeyboardVisible(false);
    repeater_.SetSelfInjectedMouseVisible(false);
}

void AgentRuntime::updateOverlay() {
    if (!runtimeActive_ || !usbDriver_.IsReady()) {
        overlay_.setOverlayEnabled(false);
        return;
    }
    const auto* profile = ActiveProfile(config_);
    const auto* activeMacro = ActiveMacro(config_);
    overlay_.updateStatus(config_, profile, activeMacro, repeater_.IsToolRunning(),
        activeMacro ? macro_.IsRunning(activeMacro->id) : false);
}

void AgentRuntime::playToggleSound(bool running, bool macroMode) {
    if (macroMode ? !config_.macroBeepEnabled : !config_.beepEnabled) return;
    const auto path = sound_.resolveToggleSoundPath(running, macroMode,
        macroMode ? config_.macroSoundOn : config_.customSoundOn,
        macroMode ? config_.macroSoundOff : config_.customSoundOff);
    sound_.playFile(path, macroMode ? config_.macroSoundVolume : config_.soundVolume,
        macroMode ? L"autokey_macro_toggle_sound" : L"autokey_toggle_sound");
}

void AgentRuntime::handleInput(const KeyDescriptor& key, bool pressed) {
    if (!runtimeActive_) return;
    const auto* profile = ActiveProfile(config_);
    if (profile && profile->enabled && profile->controlHotkey && *profile->controlHotkey == key) {
        if (pressed && !controlHotkeyDown_) {
            controlHotkeyDown_ = true;
            toggleRepeat();
        } else if (!pressed) {
            controlHotkeyDown_ = false;
        }
        handleMacroInput(key, pressed);
        return;
    }
    if (repeater_.IsToolRunning()) {
        if (pressed) repeater_.OnPhysicalDown(key);
        else repeater_.OnPhysicalUp(key);
    }
    handleMacroInput(key, pressed);
}

void AgentRuntime::handleMacroInput(const KeyDescriptor& key, bool pressed) {
    if (!runtimeActive_) return;
    const auto* activeMacro = ActiveMacro(config_);
    const bool wasRunning = activeMacro && macro_.IsRunning(activeMacro->id);
    if (pressed) macro_.OnTriggerDown(key);
    else macro_.OnTriggerUp(key);
    const bool isRunning = activeMacro && macro_.IsRunning(activeMacro->id);
    if (wasRunning != isRunning) playToggleSound(isRunning, true);
    updateOverlay();
}

void AgentRuntime::toggleRepeat() {
    const auto* profile = ActiveProfile(config_);
    const auto* activeMacro = ActiveMacro(config_);
    if (!runtimeActive_ || !profile || !profile->enabled) return;
    if (!repeater_.IsToolRunning()) {
        const auto validation = ActiveConfigValidator::validateActiveHotkeys(profile, activeMacro);
        if (!validation.ok) return;
    }
    desiredRepeatRunning_ = !repeater_.IsToolRunning();
    repeater_.SetToolRunning(desiredRepeatRunning_);
    hook_.setRepeatRunning(desiredRepeatRunning_);
    playToggleSound(desiredRepeatRunning_, false);
    updateOverlay();
}

void AgentRuntime::checkFrontendProcess() {
    if (!pausedForUi_ || frontendPid_ == 0) return;
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, frontendPid_);
    if (!process || WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
        if (process) CloseHandle(process);
        frontendPid_ = 0;
        pausedForUi_ = false;
        WriteAgentLog("frontend exited unexpectedly; agent resumed");
        ensureRuntimeState();
        return;
    }
    CloseHandle(process);
}

void AgentRuntime::pipeLoop() {
    const std::wstring sid = CurrentUserSid();
    const std::wstring descriptor = L"D:P(A;;GA;;;SY)(A;;GA;;;" + sid + L")";
    PSECURITY_DESCRIPTOR securityDescriptor = nullptr;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(
        descriptor.c_str(), SDDL_REVISION_1, &securityDescriptor, nullptr);
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = securityDescriptor;

    while (!pipeExit_) {
        HANDLE pipe = CreateNamedPipeW(AgentService::PipeName().c_str(),
            PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, kMaxPayload + sizeof(DWORD), kMaxPayload + sizeof(DWORD), 0,
            securityDescriptor ? &attributes : nullptr);
        if (pipe == INVALID_HANDLE_VALUE) break;
        const bool connected = ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
        if (connected && !pipeExit_) {
            DWORD length = 0;
            if (ReadExact(pipe, &length, sizeof(length)) && length > 0 && length <= kMaxPayload) {
                std::string request(length, '\0');
                if (ReadExact(pipe, request.data(), length)) {
                    const std::string response = handleRequest(request);
                    const DWORD responseLength = static_cast<DWORD>(response.size());
                    WriteExact(pipe, &responseLength, sizeof(responseLength));
                    WriteExact(pipe, response.data(), responseLength);
                    FlushFileBuffers(pipe);
                }
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
    if (securityDescriptor) LocalFree(securityDescriptor);
}

std::string AgentRuntime::handleRequest(const std::string& requestText) {
    jsonlite::Value value;
    std::string parseError;
    if (!jsonlite::Parse(requestText, value, parseError) || !value.AsObject()) {
        return jsonlite::Stringify(Reply(false, "invalid request"), 0);
    }
    const auto& request = *value.AsObject();
    const auto commandIt = request.find("command");
    if (commandIt == request.end() || !commandIt->second.AsString()) {
        return jsonlite::Stringify(Reply(false, "missing command"), 0);
    }
    const std::string command = *commandIt->second.AsString();
    jsonlite::Object response;
    QMetaObject::invokeMethod(this, [&]() {
        if (command == "STATUS") {
            response = Reply(true);
            response.emplace("paused", pausedForUi_);
            response.emplace("usbConnected", runtimeActive_ && usbDriver_.IsReady());
            response.emplace("firmwareUpdateRequired", firmwareUpdateRequired_);
        } else if (command == "PAUSE_FOR_UI") {
            const auto pid = request.find("pid");
            if (pid == request.end() || !pid->second.AsNumber() || *pid->second.AsNumber() <= 0) {
                response = Reply(false, "invalid pid");
                return;
            }
            frontendPid_ = static_cast<DWORD>(*pid->second.AsNumber());
            pausedForUi_ = true;
            deactivateRuntime(true);
            WriteAgentLog("paused for frontend");
            response = Reply(true);
            response.emplace("repeatRunning", desiredRepeatRunning_);
        } else if (command == "RESUME") {
            const auto repeatRunning = request.find("repeatRunning");
            const bool shouldRepeat = repeatRunning != request.end() && repeatRunning->second.AsBool()
                ? *repeatRunning->second.AsBool() : false;
            frontendPid_ = 0;
            pausedForUi_ = false;
            WriteAgentLog("resumed after frontend exit");
            reloadConfig(shouldRepeat);
            response = Reply(true);
        } else if (command == "SHUTDOWN") {
            response = Reply(true);
            shutdownRequested_ = true;
            deactivateRuntime(true, false);
            WriteAgentLog("agent stopped");
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        } else {
            response = Reply(false, "unsupported command");
        }
    }, Qt::BlockingQueuedConnection);
    return jsonlite::Stringify(response, 0);
}

void AgentRuntime::wakePipeServer() {
    HANDLE pipe = CreateFileW(AgentService::PipeName().c_str(), GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe != INVALID_HANDLE_VALUE) CloseHandle(pipe);
}
