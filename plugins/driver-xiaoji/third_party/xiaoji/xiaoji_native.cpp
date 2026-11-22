// SPDX-License-Identifier: AGPL-3.0-only
// Derived from ZyphrZero/XiaojiDriver, commit 678246972f2d720631253628383d4e07c10c8b13.
// Changes (2026-11-21): full-state input profiles, owner lifetime, and owned-device repair/removal.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include <shlobj.h>
#include "xiaoji_native.h"
#include "pnp_management.h"

#include <algorithm>
#include <chrono>
#include <charconv>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "newdev.lib")
#pragma comment(lib, "advapi32.lib")

namespace xiaoji {

constexpr GUID kHidClassGuid = {
    0x745a17a0, 0x74d3, 0x11d0, {0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda}};

constexpr wchar_t kHardwareId[] = L"ROOT\\XIAOJI";
constexpr wchar_t kKeyboardInstance[] = L"ROOT\\XIAOJI\\KBD0001";
constexpr wchar_t kMouseInstance[] = L"ROOT\\XIAOJI\\MOU0001";
constexpr wchar_t kSecuritySddl[] =
    L"O:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;LS)(A;;GR;;;WD)";

#pragma pack(push, 1)
struct SharedInput {
    volatile ULONG seqNo;
    ULONG dataSize;
    BYTE data[256];
    BYTE gipData[14];
    ULONG extendedReportSize;
    BYTE extendedReportData[80];
};

struct InputAckFrame {
    ULONG size;
    BYTE data[80];
};

struct InputAck {
    volatile LONG deliveredSeqNo;
    volatile LONG capabilities;
    volatile LONG releaseSeq;
    volatile LONG releaseCount;
    InputAckFrame release[8];
};
#pragma pack(pop)

static_assert(sizeof(SharedInput) == 362, "shared input layout must match HIDMaestro/xiaoji driver");
static_assert(offsetof(SharedInput, data) == 8, "shared input data offset");
static_assert(offsetof(SharedInput, extendedReportSize) == 278, "shared input extended size offset");
static_assert(offsetof(SharedInput, extendedReportData) == 282, "shared input extended data offset");
static_assert(sizeof(InputAck) == 688, "input acknowledgement layout");
static_assert(offsetof(InputAck, release) == 16, "input acknowledgement release offset");

constexpr LONG kAckDelivered = 0x00000001;

std::string Narrow(const std::wstring& value) {
    if (value.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), needed, nullptr, nullptr);
    out.pop_back();
    return out;
}

std::string Win32Message(DWORD error) {
    LPWSTR buffer = nullptr;
    DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring text = length && buffer ? std::wstring(buffer, length) : L"unknown error";
    if (buffer) LocalFree(buffer);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
        text.pop_back();
    return Narrow(text) + " (" + std::to_string(error) + ")";
}

[[noreturn]] void Fail(const std::string& what) {
    throw std::runtime_error(what + ": " + Win32Message(GetLastError()));
}

class SecurityDescriptor {
public:
    explicit SecurityDescriptor(const wchar_t* sddl = kSecuritySddl) {
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                sddl, SDDL_REVISION_1, &descriptor_, nullptr)) {
            Fail("ConvertStringSecurityDescriptorToSecurityDescriptorW");
        }
        attributes_.nLength = sizeof(attributes_);
        attributes_.lpSecurityDescriptor = descriptor_;
        attributes_.bInheritHandle = FALSE;
    }

    ~SecurityDescriptor() {
        if (descriptor_) LocalFree(descriptor_);
    }

    SecurityDescriptor(const SecurityDescriptor&) = delete;
    SecurityDescriptor& operator=(const SecurityDescriptor&) = delete;

    PSECURITY_ATTRIBUTES Get() { return &attributes_; }

private:
    PSECURITY_DESCRIPTOR descriptor_ = nullptr;
    SECURITY_ATTRIBUTES attributes_{};
};

void CheckObjectOwner(HANDLE handle) {
    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    DWORD error = GetSecurityInfo(handle, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION, &owner,
        nullptr, nullptr, nullptr, &descriptor);
    bool trusted = error == ERROR_SUCCESS && owner &&
        (IsWellKnownSid(owner, WinBuiltinAdministratorsSid) || IsWellKnownSid(owner, WinLocalSystemSid));
    if (descriptor) LocalFree(descriptor);
    if (!trusted) throw std::runtime_error("Refusing IPC object not owned by Administrators or SYSTEM.");
}

class OperationLock {
public:
    OperationLock() {
        SecurityDescriptor security;
        handle_ = CreateMutexW(security.Get(), FALSE, L"Global\\XiaojiHostOperation");
        if (!handle_) Fail("CreateMutexW");
        try {
            CheckObjectOwner(handle_);
            DWORD wait = WaitForSingleObject(handle_, 0);
            if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
                throw std::runtime_error("Another host operation is active, or the device lock is unavailable.");
        } catch (...) { CloseHandle(handle_); throw; }
    }
    ~OperationLock() { ReleaseMutex(handle_); CloseHandle(handle_); }
    OperationLock(const OperationLock&) = delete;
    OperationLock& operator=(const OperationLock&) = delete;
private:
    HANDLE handle_{};
};

// Hold the upstream operation mutex for the whole input session. The owning
// thread only holds the lease; engine threads serialize reports in the adapter.
class SessionLease {
public:
    SessionLease() {
        stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stop_) Fail("Create input lease stop event");
        std::promise<void> acquired;
        auto ready = acquired.get_future();
        try {
            thread_ = std::thread([this, promise = std::move(acquired)]() mutable {
                try {
                    OperationLock lock;
                    promise.set_value();
                    WaitForSingleObject(stop_, INFINITE);
                } catch (...) { promise.set_exception(std::current_exception()); }
            });
            ready.get();
        } catch (...) {
            if (thread_.joinable()) thread_.join();
            CloseHandle(stop_);
            throw;
        }
    }
    ~SessionLease() {
        SetEvent(stop_);
        if (thread_.joinable()) thread_.join();
        CloseHandle(stop_);
    }
    SessionLease(const SessionLease&) = delete;
    SessionLease& operator=(const SessionLease&) = delete;
private:
    HANDLE stop_ = nullptr;
    std::thread thread_;
};

// The driver observes this pipe closing even if the application is killed.
class OwnerPipe {
public:
    explicit OwnerPipe(int index) {
        const auto parameters = std::wstring(L"SYSTEM\\CurrentControlSet\\Enum\\ROOT\\XIAOJI\\")
            + (index == 0 ? L"KBD0001" : L"MOU0001") + L"\\Device Parameters";
        const auto deadline = GetTickCount64() + 2000;
        for (;;) {
            DWORD connected = 0, bytes = sizeof(connected);
            if (RegGetValueW(HKEY_LOCAL_MACHINE, parameters.c_str(), L"OwnerConnected",
                    RRF_RT_REG_DWORD, nullptr, &connected, &bytes) != ERROR_SUCCESS || connected == 0) break;
            if (GetTickCount64() >= deadline)
                throw std::runtime_error("Previous xiaoji input session has not released its owner watch; retry after closing consumers.");
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        SecurityDescriptor security(L"O:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;LS)");
        const auto name = L"\\\\.\\pipe\\XiaojiOwner" + std::to_wstring(index);
        do {
            handle_ = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED
                | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT
                | PIPE_REJECT_REMOTE_CLIENTS, 1, 1, 1, 0, security.Get());
            if (handle_ != INVALID_HANDLE_VALUE) break;
            const auto error = GetLastError();
            if ((error != ERROR_ACCESS_DENIED && error != ERROR_PIPE_BUSY) || GetTickCount64() >= deadline) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        } while (true);
        if (handle_ == INVALID_HANDLE_VALUE) Fail("Create xiaoji input owner pipe");
        connected_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!connected_.hEvent) { CloseHandle(handle_); Fail("Create owner connection event"); }
        if (!ConnectNamedPipe(handle_, &connected_)) {
            const auto error = GetLastError();
            if (error != ERROR_IO_PENDING && error != ERROR_PIPE_CONNECTED) {
                CloseHandle(connected_.hEvent);
                CloseHandle(handle_);
                SetLastError(error);
                Fail("Connect input owner pipe");
            }
            pending_ = error == ERROR_IO_PENDING;
        }
    }
    bool Connected() const {
        if (!pending_) return true;
        DWORD bytes = 0;
        if (GetOverlappedResult(handle_, &connected_, &bytes, FALSE)) return true;
        if (GetLastError() == ERROR_IO_INCOMPLETE) return false;
        Fail("Wait for xiaoji owner connection");
    }
    ~OwnerPipe() {
        if (pending_) {
            CancelIoEx(handle_, &connected_);
            DWORD transferred = 0;
            GetOverlappedResult(handle_, &connected_, &transferred, TRUE);
        }
        DisconnectNamedPipe(handle_);
        CloseHandle(handle_);
        CloseHandle(connected_.hEvent);
    }
    OwnerPipe(const OwnerPipe&) = delete;
    OwnerPipe& operator=(const OwnerPipe&) = delete;
private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    mutable OVERLAPPED connected_{};
    bool pending_ = false;
};

class Section {
public:
    Section(const std::wstring& name, size_t size) {
        SecurityDescriptor security;
        handle_ = CreateFileMappingW(
            INVALID_HANDLE_VALUE, security.Get(), PAGE_READWRITE, 0,
            static_cast<DWORD>(size), name.c_str());
        if (!handle_) Fail("CreateFileMappingW(" + Narrow(name) + ")");
        view_ = MapViewOfFile(handle_, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size);
        if (!view_) {
            DWORD error = GetLastError(); CloseHandle(handle_);
            throw std::runtime_error("MapViewOfFile: " + Win32Message(error));
        }
        try { CheckObjectOwner(handle_); }
        catch (...) { UnmapViewOfFile(view_); CloseHandle(handle_); throw; }
    }

    ~Section() {
        if (view_) UnmapViewOfFile(view_);
        if (handle_) CloseHandle(handle_);
    }

    Section(const Section&) = delete;
    Section& operator=(const Section&) = delete;

    void* Data() const { return view_; }

private:
    HANDLE handle_ = nullptr;
    void* view_ = nullptr;
};

class Event {
public:
    explicit Event(const std::wstring& name) {
        SecurityDescriptor security;
        handle_ = CreateEventW(security.Get(), FALSE, FALSE, name.c_str());
        if (!handle_) Fail("CreateEventW(" + Narrow(name) + ")");
        try { CheckObjectOwner(handle_); } catch (...) { CloseHandle(handle_); throw; }
    }

    ~Event() {
        if (handle_) CloseHandle(handle_);
    }

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    HANDLE Get() const { return handle_; }

private:
    HANDLE handle_ = nullptr;
};

void SetBinaryValue(HKEY key, const wchar_t* name, const void* data, DWORD size) {
    LSTATUS error = RegSetValueExW(key, name, 0, REG_BINARY, static_cast<const BYTE*>(data), size);
    if (error != ERROR_SUCCESS) throw std::runtime_error("Write " + Narrow(name) + ": " + Win32Message(error));
}

void SetDwordValue(HKEY key, const wchar_t* name, DWORD value) {
    LSTATUS error = RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    if (error != ERROR_SUCCESS) throw std::runtime_error("RegSetValueExW(" + Narrow(name) + "): " + Win32Message(error));
}

void SetStringValue(HKEY key, const wchar_t* name, const std::wstring& value) {
    DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    LSTATUS error = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), bytes);
    if (error != ERROR_SUCCESS) throw std::runtime_error("Write " + Narrow(name) + ": " + Win32Message(error));
}

std::wstring ControllerKey(int index) {
    return L"SOFTWARE\\Xiaoji\\Controller" + std::to_wstring(index);
}

bool MatchesProfile(HKEY key, int index, bool legacy);

void WriteControllerConfig(
    int index,
    const BYTE* reportDescriptor,
    DWORD reportDescriptorSize,
    DWORD inputReportByteLength,
    WORD vendorId,
    WORD productId,
    const std::wstring& productString) {
    HKEY key = nullptr;
    const std::wstring path = ControllerKey(index);
    DWORD disposition = 0;
    LSTATUS error = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, nullptr, 0,
            KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key, &disposition);
    if (error != ERROR_SUCCESS) throw std::runtime_error("Open " + Narrow(path) + ": " + Win32Message(error));

    try {
        const wchar_t* instance = index == 0 ? kKeyboardInstance : kMouseInstance;
        if (disposition == REG_OPENED_EXISTING_KEY) {
            wchar_t owner[128]{};
            DWORD bytes = sizeof(owner);
            const auto ownerResult = RegGetValueW(key, nullptr, L"OwnerInstance", RRF_RT_REG_SZ, nullptr, owner, &bytes);
            const bool recognizedLegacy = ownerResult == ERROR_FILE_NOT_FOUND
                && (MatchesProfile(key, index, true) || MatchesProfile(key, index, false));
            if (!recognizedLegacy && (ownerResult != ERROR_SUCCESS || _wcsicmp(owner, instance) != 0))
                throw std::runtime_error("Controller configuration is not owned by this instance.");
        }
        SetStringValue(key, L"OwnerInstance", instance);
        SetBinaryValue(key, L"ReportDescriptor", reportDescriptor, reportDescriptorSize);
        SetDwordValue(key, L"VendorId", vendorId);
        SetDwordValue(key, L"ProductId", productId);
        SetDwordValue(key, L"VersionNumber", 0x0100);
        SetDwordValue(key, L"InputReportByteLength", inputReportByteLength);
        SetDwordValue(key, L"FunctionMode", 0);
        SetStringValue(key, L"ProductString", productString);
    } catch (...) {
        RegCloseKey(key);
        throw;
    }
    RegCloseKey(key);
}


void WriteDeviceControllerIndex(const std::wstring& instanceId, int controllerIndex) {
    const std::wstring parameters = L"SYSTEM\\CurrentControlSet\\Enum\\" + instanceId + L"\\Device Parameters";
    HKEY key = nullptr;
    auto rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, parameters.c_str(), 0, nullptr, 0,
            KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
    if (rc != ERROR_SUCCESS) throw std::runtime_error("Open device parameters: " + Win32Message(rc));
    try {
        SetDwordValue(key, L"ControllerIndex", static_cast<DWORD>(controllerIndex));
    } catch (...) {
        RegCloseKey(key);
        throw;
    }
    RegCloseKey(key);
}

void CreateRootDevice(const std::wstring& instanceId, const std::wstring& description, int controllerIndex) {
    HDEVINFO devices = SetupDiCreateDeviceInfoList(&kHidClassGuid, nullptr);
    if (devices == INVALID_HANDLE_VALUE)
        Fail("SetupDiCreateDeviceInfoList");

    SP_DEVINFO_DATA info{};
    info.cbSize = sizeof(info);
    if (!SetupDiCreateDeviceInfoW(
            devices, instanceId.c_str(), &kHidClassGuid, description.c_str(),
            nullptr, 0, &info)) {
        DWORD error = GetLastError();
        SetupDiDestroyDeviceInfoList(devices);
        SetLastError(error);
        Fail("SetupDiCreateDeviceInfoW(" + Narrow(instanceId) + ")");
    }

    const wchar_t hardwareIds[] = L"ROOT\\XIAOJI\0\0";
    if (!SetupDiSetDeviceRegistryPropertyW(
            devices, &info, SPDRP_HARDWAREID,
            reinterpret_cast<const BYTE*>(hardwareIds), sizeof(hardwareIds))) {
        DWORD error = GetLastError();
        SetupDiDestroyDeviceInfoList(devices);
        SetLastError(error);
        Fail("SetupDiSetDeviceRegistryPropertyW");
    }

    // HIDMaestro reads this from the devnode's Device Parameters key while
    // EvtDeviceAdd runs. Write it before registration so a driver already
    // present in the store cannot start against the previous index.
    try { WriteDeviceControllerIndex(instanceId, controllerIndex); }
    catch (...) { SetupDiDestroyDeviceInfoList(devices); throw; }

    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, devices, &info)) {
        DWORD error = GetLastError();
        SetupDiDestroyDeviceInfoList(devices);
        SetLastError(error);
        Fail("DIF_REGISTERDEVICE(" + Narrow(instanceId) + ")");
    }
    SetupDiDestroyDeviceInfoList(devices);
}

bool LocateDevice(const std::wstring& instance, DEVINST& node, ULONG flags) {
    const auto result = CM_Locate_DevNodeW(&node, const_cast<wchar_t*>(instance.c_str()), flags);
    if (result == CR_NO_SUCH_DEVNODE) return false;
    if (result != CR_SUCCESS)
        throw std::runtime_error("Locate " + Narrow(instance) + ": CONFIGRET=" + std::to_string(result));
    return true;
}

void CollectChildren(DEVINST parent, std::vector<std::wstring>& children) {
    DEVINST child = 0;
    auto result = CM_Get_Child(&child, parent, 0);
    while (result == CR_SUCCESS) {
        wchar_t instance[MAX_DEVICE_ID_LEN]{};
        const auto identified = CM_Get_Device_IDW(child, instance, ARRAYSIZE(instance), 0);
        if (identified != CR_SUCCESS || children.size() >= 128)
            throw std::runtime_error("Cannot identify the owned device subtree before removal.");
        children.emplace_back(instance);
        CollectChildren(child, children);
        DEVINST sibling = 0;
        result = CM_Get_Sibling(&sibling, child, 0);
        child = sibling;
    }
    if (result != CR_NO_SUCH_DEVNODE)
        throw std::runtime_error("Read owned device subtree: CONFIGRET=" + std::to_string(result));
}

bool DeviceDown(const std::wstring& instance) {
    DEVINST node = 0;
    if (!LocateDevice(instance, node, CM_LOCATE_DEVNODE_NORMAL)) return true;
    ULONG flags = 0, problem = 0;
    const auto result = CM_Get_DevNode_Status(&flags, &problem, node, 0);
    if (result == CR_NO_SUCH_DEVNODE) return true;
    if (result != CR_SUCCESS) return false;
    return (flags & DN_WILL_BE_REMOVED) != 0
        || ((flags & DN_HAS_PROBLEM) != 0 && (problem == CM_PROB_FAILED_POST_START
            || problem == CM_PROB_WILL_BE_REMOVED || problem == CM_PROB_DEVICE_NOT_THERE));
}

// Derived from HIDMaestro SharedMemoryIO.RequestFailure. Create the two
// removal events before the input event, including when no sender is running.
bool RequestDriverRemoval(int index) {
    const auto suffix = std::to_wstring(index);
    Event request(L"Global\\XiaojiRemoveEvent" + suffix);
    Event answer(L"Global\\XiaojiRemovedEvent" + suffix);
    Event input(L"Global\\XiaojiInputEvent" + suffix);
    if (!ResetEvent(answer.Get()) || !SetEvent(request.Get())) Fail("Request xiaoji device removal");
    const auto withdraw = [](void* event) { ResetEvent(event); };
    std::unique_ptr<void, decltype(withdraw)> pending(request.Get(), withdraw);
    for (int attempt = 0; attempt < 2; ++attempt) {
        const auto wait = WaitForSingleObject(answer.Get(), 1500);
        if (wait == WAIT_OBJECT_0) return true;
        if (wait != WAIT_TIMEOUT) Fail("Wait for xiaoji removal acknowledgement");
        if (attempt == 0) {
            // An old input event may have been opened before the removal
            // events existed. The upstream worker absorbs this signal and
            // reopens its events; it does not terminate a healthy worker.
            const auto stopName = L"Global\\XiaojiStopEvent" + suffix;
            HANDLE raw = OpenEventW(EVENT_MODIFY_STATE | READ_CONTROL, FALSE, stopName.c_str());
            if (raw) {
                std::unique_ptr<void, decltype(&CloseHandle)> stop(raw, CloseHandle);
                CheckObjectOwner(raw);
                if (!SetEvent(raw)) Fail("Refresh xiaoji removal channel");
            } else if (GetLastError() != ERROR_FILE_NOT_FOUND) Fail("Open xiaoji removal channel");
        }
    }
    // Do not leave a late request armed for a new life on this index.
    if (!ResetEvent(request.Get())) Fail("Withdraw xiaoji removal request");
    return WaitForSingleObject(answer.Get(), 0) == WAIT_OBJECT_0;
}

bool RemoveRootDevice(const std::wstring& instanceId, int index) {
    DEVINST node = 0;
    std::vector<std::wstring> children;
    if (LocateDevice(instanceId, node, CM_LOCATE_DEVNODE_NORMAL)) CollectChildren(node, children);
    const auto down = [&] {
        return DeviceDown(instanceId) && std::all_of(children.begin(), children.end(), [](const auto& child) {
            DEVINST current = 0;
            return !LocateDevice(child, current, CM_LOCATE_DEVNODE_NORMAL);
        });
    };
    if (!down() && LocateDevice(instanceId, node, CM_LOCATE_DEVNODE_NORMAL)) {
        PNP_VETO_TYPE veto = PNP_VetoTypeUnknown;
        wchar_t holder[MAX_PATH]{};
        // A query leaves every record intact when refused. Deleting a live
        // keyboard's record first can leave its child without a driver.
        const auto query = CM_Query_And_Remove_SubTreeW(node, &veto, holder, ARRAYSIZE(holder),
            CM_REMOVE_UI_NOT_OK | CM_REMOVE_NO_RESTART);
        if (query != CR_SUCCESS && !down() && !RequestDriverRemoval(index) && !down()) {
            throw std::runtime_error("Device removal refused: " + Narrow(instanceId)
                + "; CONFIGRET=" + std::to_string(query) + "; veto=" + std::to_string(veto)
                + "; holder=" + Narrow(holder) + "; driver did not acknowledge removal. No device records deleted.");
        }
    }
    for (int attempt = 0; !down(); ++attempt) {
        if (attempt == 100)
            throw std::runtime_error("Device or HID children did not leave the tree: " + Narrow(instanceId)
                + ". Restart Windows before retrying. No device records deleted.");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Only the captured descendants of the identity-checked root are candidates.
    // Remove children before their keyboard parent (ERROR_NOT_DISABLEABLE).
    std::reverse(children.begin(), children.end());
    children.push_back(instanceId);
    bool rebootRequired = false;
    HDEVINFO raw = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (raw == INVALID_HANDLE_VALUE) Fail("Open xiaoji removal device list");
    std::unique_ptr<void, decltype(&SetupDiDestroyDeviceInfoList)> devices(raw, SetupDiDestroyDeviceInfoList);
    for (const auto& instance : children) {
        if (!LocateDevice(instance, node, CM_LOCATE_DEVNODE_PHANTOM)) continue;
        SP_DEVINFO_DATA info{sizeof(SP_DEVINFO_DATA)};
        if (!SetupDiOpenDeviceInfoW(raw, instance.c_str(), nullptr, 0, &info)) Fail("Open owned device record " + Narrow(instance));
        BOOL reboot = FALSE;
        if (!DiUninstallDevice(nullptr, raw, &info, 0, &reboot)) Fail("Uninstall owned device " + Narrow(instance));
        rebootRequired |= reboot != FALSE;
    }
    if (rebootRequired)
        throw std::runtime_error("Windows requires a restart to finish device removal. The uninstall entry is retained; retry after restarting.");
    for (const auto& instance : children) {
        for (int attempt = 0; LocateDevice(instance, node, CM_LOCATE_DEVNODE_PHANTOM); ++attempt) {
            if (attempt == 100)
                throw std::runtime_error("Device record remains after uninstall: " + Narrow(instance) + ". Restart Windows and retry.");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    return true;
}

std::wstring StageDriverPackage(const std::wstring& infPath) {
    wchar_t published[MAX_PATH]{};
    auto source = std::filesystem::absolute(infPath);
    if (_wcsicmp(source.filename().c_str(), L"xiaoji.inf") != 0) throw std::runtime_error("Expected xiaoji.inf");
    if (!SetupCopyOEMInfW(source.c_str(), nullptr, SPOST_PATH, 0, published, MAX_PATH, nullptr, nullptr)) Fail("SetupCopyOEMInfW");
    return std::filesystem::path(CharLowerW(published)).filename().wstring();
}

constexpr ULONG NextSequence(ULONG published, ULONG delivered) {
    ULONG last = published & ~1UL;
    if (last == 0 || (delivered != 0 && static_cast<LONG>(delivered - last) > 0)) last = delivered;
    ULONG next = last + 2;
    return next == 0 ? 2 : next;
}

class XiaojiDevice {
public:
    explicit XiaojiDevice(int index, const std::wstring& prefix = L"Global\\Xiaoji")
        : owner_(index), input_(prefix + L"Input" + std::to_wstring(index), sizeof(SharedInput)),
          ack_(prefix + L"InputAck" + std::to_wstring(index), sizeof(InputAck)),
          ackEvent_(prefix + L"InputAckEvent" + std::to_wstring(index)),
          inputEvent_(prefix + L"InputEvent" + std::to_wstring(index)),
          inputView_(static_cast<SharedInput*>(input_.Data())),
          ackView_(static_cast<InputAck*>(ack_.Data())) {
        // Clear the previous owner's ended marker, retaining driver-owned bits.
        InterlockedAnd(&ackView_->capabilities, 0x7fffffffL);
        inputView_->seqNo = 1;
        MemoryBarrier();
        inputView_->dataSize = 0;
        inputView_->extendedReportSize = 0;
        MemoryBarrier();
        inputView_->seqNo = 0;
    }

    bool WaitForDriver(int timeoutMs, const std::function<bool(DWORD)>& stop = {}) const {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (owner_.Connected()
                && (ackView_->capabilities & (kAckDelivered | 0x02)) == (kAckDelivered | 0x02))
                return true;
            if (stop) { if (stop(25)) return false; }
            else std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        throw std::runtime_error(
            "xiaoji driver did not open the input acknowledgement section; "
            "check device installation and signatures");
    }

    void ArmRelease(const std::vector<std::vector<BYTE>>& reports) {
        if (reports.size() > std::size(ackView_->release))
            throw std::runtime_error("Too many release reports for the pinned protocol.");
        for (const auto& report : reports) {
            if (report.size() > sizeof(ackView_->release[0].data))
                throw std::runtime_error("Release report exceeds the pinned protocol capacity.");
        }
        const LONG sequence = (ackView_->releaseSeq & ~1L) + 2;
        InterlockedExchange(&ackView_->releaseSeq, sequence - 1);
        ackView_->releaseCount = static_cast<LONG>(reports.size());
        for (std::size_t i = 0; i < reports.size(); ++i) {
            ackView_->release[i].size = static_cast<ULONG>(reports[i].size());
            std::memcpy(ackView_->release[i].data, reports[i].data(), reports[i].size());
        }
        InterlockedExchange(&ackView_->releaseSeq, sequence);
    }

    void WriteReport(const BYTE* report, ULONG size, bool reportId = false) {
        WriteFrame(report, size, reportId);
    }

private:
    void WriteFrame(const BYTE* data, ULONG size, bool reportId) {
        if (size > (reportId ? sizeof(SharedInput::extendedReportData) : sizeof(SharedInput::data)))
            throw std::runtime_error("report exceeds xiaoji shared input capacity");

        const ULONG sequence = NextSequence(inputView_->seqNo, static_cast<ULONG>(ackView_->deliveredSeqNo));
        inputView_->seqNo = sequence - 1;
        MemoryBarrier();
        inputView_->dataSize = reportId ? 0 : size;
        inputView_->extendedReportSize = reportId ? size : 0;
        std::memcpy(reportId ? inputView_->extendedReportData : inputView_->data, data, size);
        MemoryBarrier();
        inputView_->seqNo = sequence;
        if (!SetEvent(inputEvent_.Get()))
            Fail("SetEvent");

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (static_cast<ULONG>(ackView_->deliveredSeqNo) != sequence) {
            if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("HID report acknowledgement timed out; delivery is uncertain.");
            if (WaitForSingleObject(ackEvent_.Get(), 10) == WAIT_FAILED) Fail("Wait for HID acknowledgement");
        }
    }

    OwnerPipe owner_;
    Section input_;
    Section ack_;
    Event ackEvent_;
    Event inputEvent_;
    SharedInput* inputView_;
    InputAck* ackView_;
};

bool MatchesProfile(HKEY key, int index, bool legacy = false) {
    const BYTE* descriptor = index == 0
        ? (legacy ? kLegacyKeyboardDescriptor : kKeyboardDescriptor)
        : (legacy ? kLegacyMouseDescriptor : kMouseDescriptor);
    const DWORD expectedSize = static_cast<DWORD>(index == 0
        ? (legacy ? sizeof(kLegacyKeyboardDescriptor) : sizeof(kKeyboardDescriptor))
        : (legacy ? sizeof(kLegacyMouseDescriptor) : sizeof(kMouseDescriptor)));
    BYTE actual[4096]{};
    DWORD size = sizeof(actual);
    if (RegGetValueW(key, nullptr, L"ReportDescriptor", RRF_RT_REG_BINARY, nullptr, actual, &size) != ERROR_SUCCESS
        || size != expectedSize || std::memcmp(actual, descriptor, expectedSize) != 0) return false;
    const auto matches = [key](const wchar_t* name, DWORD expected) {
        DWORD value = 0, bytes = sizeof(value);
        return RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &value, &bytes) == ERROR_SUCCESS
            && value == expected;
    };
    return matches(L"VendorId", 0x1209) && matches(L"ProductId", static_cast<DWORD>(index + 1))
        && matches(L"InputReportByteLength", index == 0 ? (legacy ? 8u : static_cast<DWORD>(kKeyboardWireSize)) : 4u)
        && matches(L"FunctionMode", 0);
}

bool CurrentProfile(int index) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, ControllerKey(index).c_str(), 0,
            KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) return false;
    const bool matches = MatchesProfile(key, index);
    RegCloseKey(key);
    return matches;
}

bool OwnedConfig(int index, bool allowLegacy = false) {
    wchar_t owner[128]{};
    DWORD size = sizeof(owner);
    auto path = ControllerKey(index);
    HKEY key{};
    auto rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key);
    if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) return false;
    if (rc != ERROR_SUCCESS) throw std::runtime_error("Read controller configuration: " + Win32Message(rc));
    rc = RegGetValueW(key, nullptr, L"OwnerInstance", RRF_RT_REG_SZ, nullptr, owner, &size);
    if (rc == ERROR_FILE_NOT_FOUND && allowLegacy) {
        const bool match = MatchesProfile(key, index, true) || MatchesProfile(key, index);
        RegCloseKey(key);
        if (!match) throw std::runtime_error("Legacy slot does not match the original xiaoji controller configuration.");
        return true;
    }
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || _wcsicmp(owner, index == 0 ? kKeyboardInstance : kMouseInstance) != 0)
        throw std::runtime_error("Controller slot has an unknown owner; not changing it.");
    return true;
}

std::wstring BoundOem(int index, bool requireStarted, const std::wstring& allowedInfs, bool includeAbsent) {
    const std::wstring instance = index == 0 ? kKeyboardInstance : kMouseInstance;
    DEVINST node = 0;
    auto rc = CM_Locate_DevNodeW(&node, const_cast<wchar_t*>(instance.c_str()), includeAbsent ? CM_LOCATE_DEVNODE_PHANTOM : CM_LOCATE_DEVNODE_NORMAL);
    if (rc == CR_NO_SUCH_DEVNODE) return {};
    if (rc != CR_SUCCESS) throw std::runtime_error("CM_Locate_DevNode failed: " + std::to_string(rc));
    wchar_t hardware[256]{};
    ULONG size = sizeof(hardware), type = 0;
    if (CM_Get_DevNode_Registry_PropertyW(node, CM_DRP_HARDWAREID, &type, hardware, &size, 0) != CR_SUCCESS ||
        type != REG_MULTI_SZ || size < sizeof(kHardwareId) + sizeof(wchar_t) || hardware[ARRAYSIZE(kHardwareId)-1] != 0 ||
        _wcsicmp(hardware, kHardwareId) != 0
        || !OwnedConfig(index, !allowedInfs.empty() || (!requireStarted && includeAbsent)))
        throw std::runtime_error("Device identity mismatch: " + Narrow(instance));
    DWORD boundIndex = MAXDWORD, bytes = sizeof(boundIndex);
    std::wstring parameters = L"SYSTEM\\CurrentControlSet\\Enum\\" + instance + L"\\Device Parameters";
    if (RegGetValueW(HKEY_LOCAL_MACHINE, parameters.c_str(), L"ControllerIndex", RRF_RT_REG_DWORD, nullptr, &boundIndex, &bytes) != ERROR_SUCCESS ||
        boundIndex != static_cast<DWORD>(index)) throw std::runtime_error("Controller index mismatch.");
    const DEVPROPKEY driverInf = {{0xa8b865dd,0x2e3d,0x4094,{0xad,0x97,0xe5,0x93,0xa7,0x0c,0x75,0xd6}},5};
    wchar_t bound[MAX_PATH]{};
    size = sizeof(bound);
    DEVPROPTYPE propType = 0;
    rc = CM_Get_DevNode_PropertyW(node, &driverInf, &propType, reinterpret_cast<BYTE*>(bound), &size, 0);
    if (rc == CR_NO_SUCH_VALUE && !requireStarted && OwnedConfig(index)) return L"<unbound>";
    if (rc != CR_SUCCESS || propType != DEVPROP_TYPE_STRING || size < 4 || size > sizeof(bound) || bound[size/2-1] != 0)
        throw std::runtime_error("Cannot identify the bound driver package.");
    std::wstring oem = CharLowerW(bound);
    wchar_t storeInf[MAX_PATH]{};
    if (!SetupGetInfDriverStoreLocationW(oem.c_str(), nullptr, nullptr, storeInf, MAX_PATH, nullptr) ||
        _wcsicmp(std::filesystem::path(storeInf).filename().c_str(), L"xiaoji.inf") != 0)
        throw std::runtime_error("The fixed instance is bound to an unknown driver package.");
    if (!allowedInfs.empty() && allowedInfs.find(L"," + oem + L",") == std::wstring::npos)
        throw std::runtime_error("Bound OEM driver differs from the selected package: " + Narrow(oem));
    if (requireStarted) {
        ULONG flags = 0, problem = 0;
        if (CM_Get_DevNode_Status(&flags, &problem, node, 0) != CR_SUCCESS || !(flags & DN_STARTED) || problem != 0)
            throw std::runtime_error("Device not started: " + Narrow(instance) + ", problem=" + std::to_string(problem));
    }
    return oem;
}

void BindSelected(const std::wstring& instance, const std::wstring& oem) {
    wchar_t storeInf[MAX_PATH]{};
    if (!SetupGetInfDriverStoreLocationW(oem.c_str(), nullptr, nullptr, storeInf, MAX_PATH, nullptr)) Fail("Resolve selected OEM INF");
    HDEVINFO devices = SetupDiCreateDeviceInfoList(&kHidClassGuid, nullptr);
    if (devices == INVALID_HANDLE_VALUE) Fail("Open HID device list");
    SP_DEVINFO_DATA data{sizeof(SP_DEVINFO_DATA)};
    SP_DEVINSTALL_PARAMS_W params{sizeof(SP_DEVINSTALL_PARAMS_W)};
    SP_DRVINFO_DATA_W driver{sizeof(SP_DRVINFO_DATA_W)};
    BOOL reboot = FALSE;
    try {
        if (!SetupDiOpenDeviceInfoW(devices, instance.c_str(), nullptr, 0, &data) ||
            !SetupDiGetDeviceInstallParamsW(devices, &data, &params)) Fail("Open persistent xiaoji device");
        params.Flags |= DI_ENUMSINGLEINF;
        wcscpy_s(params.DriverPath, storeInf);
        if (!SetupDiSetDeviceInstallParamsW(devices, &data, &params) ||
            !SetupDiBuildDriverInfoList(devices, &data, SPDIT_COMPATDRIVER) ||
            !SetupDiEnumDriverInfoW(devices, &data, SPDIT_COMPATDRIVER, 0, &driver) ||
            !DiInstallDevice(nullptr, devices, &data, &driver, 0, &reboot)) Fail("Bind selected driver to fixed device");
        if (reboot) throw std::runtime_error("Restart required to finish binding the driver.");
    } catch (...) { SetupDiDestroyDeviceInfoList(devices); throw; }
    SetupDiDestroyDeviceInfoList(devices);
}

void EnsureDevices(const std::wstring& allowedInfs, const std::wstring& priorInfs, bool forceRecreate) {
    const auto target = allowedInfs.substr(1, allowedInfs.find(L',', 1) - 1);
    for (int index : {0, 1}) {
        const std::wstring product = index == 0 ? L"xiaoji Keyboard" : L"xiaoji Mouse";
        const std::wstring instance = index == 0 ? kKeyboardInstance : kMouseInstance;
        auto current = BoundOem(index, false, allowedInfs + priorInfs, true);
        bool started = false;
        if (!current.empty()) {
            DEVINST node = 0;
            ULONG flags = 0, problem = 0;
            started = CM_Locate_DevNodeW(&node, const_cast<wchar_t*>(instance.c_str()), 0) == CR_SUCCESS
                && CM_Get_DevNode_Status(&flags, &problem, node, 0) == CR_SUCCESS
                && (flags & DN_STARTED) != 0 && problem == 0;
            // Configuration is read at device start. Recreate only our two fixed,
            // ownership-checked instances when migrating or repairing them.
            if (forceRecreate || !CurrentProfile(index) || !started) {
                if (!RemoveRootDevice(instance, index))
                    throw std::runtime_error("Cannot migrate xiaoji configuration; close other consumers and retry.");
                current.clear();
            }
        }
        if (current.empty()) {
            if (index == 0) WriteControllerConfig(index, kKeyboardDescriptor, sizeof(kKeyboardDescriptor),
                static_cast<DWORD>(kKeyboardWireSize), 0x1209, 1, product);
            else WriteControllerConfig(index, kMouseDescriptor, sizeof(kMouseDescriptor), 4, 0x1209, 2, product);
            CreateRootDevice(instance, product, index);
            BindSelected(instance, target);
        } else if (allowedInfs.find(L"," + current + L",") == std::wstring::npos) {
            BindSelected(instance, target);
        }
        if (BoundOem(index, true, allowedInfs).empty() || !CurrentProfile(index))
            throw std::runtime_error("Xiaoji keyboard/mouse configuration is not ready after installation.");
        HKEY key = nullptr;
        const auto rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, ControllerKey(index).c_str(), 0,
            KEY_SET_VALUE | KEY_WOW64_64KEY, &key);
        if (rc != ERROR_SUCCESS) throw std::runtime_error("Open controller: " + Win32Message(rc));
        try {
            SetStringValue(key, L"OwnerInstance", instance);
            SetDwordValue(key, L"InputProfileRevision", kInputProfileRevision);
        } catch (...) { RegCloseKey(key); throw; }
        RegCloseKey(key);
    }
}

std::wstring SavedRemovalPackage(int index) {
    if (!OwnedConfig(index, true)) return {};
    wchar_t name[MAX_PATH]{};
    DWORD bytes = sizeof(name);
    const auto result = RegGetValueW(HKEY_LOCAL_MACHINE, ControllerKey(index).c_str(), L"UninstallPackage",
        RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, name, &bytes);
    if (result == ERROR_FILE_NOT_FOUND) return {};
    if (result != ERROR_SUCCESS) throw std::runtime_error("Read uninstall package: " + Win32Message(result));
    std::wstring value = CharLowerW(name);
    if (!value.starts_with(L"oem") || !value.ends_with(L".inf") || value.size() <= 7
        || !std::all_of(value.begin() + 3, value.end() - 4, [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; }))
        throw std::runtime_error("Uninstall record contains an invalid OEM package name.");
    return value;
}

void RemoveXiaojiDevices(const std::wstring& allowedInfs) {
    bool present[2];
    for (int index : {0, 1}) {
        const auto package = BoundOem(index, false, allowedInfs, true);
        present[index] = !package.empty();
        if (!package.empty() && package != L"<unbound>") {
            HKEY key = nullptr;
            const auto result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, ControllerKey(index).c_str(), 0,
                KEY_SET_VALUE | KEY_WOW64_64KEY, &key);
            if (result != ERROR_SUCCESS) throw std::runtime_error("Save uninstall package: " + Win32Message(result));
            try { SetStringValue(key, L"UninstallPackage", package); }
            catch (...) { RegCloseKey(key); throw; }
            RegCloseKey(key);
        }
    }
    for (int index : {0, 1}) {
        if (present[index]) RemoveRootDevice(index == 0 ? kKeyboardInstance : kMouseInstance, index);
    }
    // Configuration and the saved package survive until package deletion also
    // succeeds, so a partial uninstall remains visible and can be retried.
}

void RemoveXiaojiConfiguration() {
    for (int index : {0, 1}) {
        DEVINST node = 0;
        if (LocateDevice(index == 0 ? kKeyboardInstance : kMouseInstance, node, CM_LOCATE_DEVNODE_PHANTOM))
            throw std::runtime_error("A fixed device still exists; controller configuration was retained.");
        if (OwnedConfig(index, true)) {
            auto rc = RegDeleteKeyExW(HKEY_LOCAL_MACHINE, ControllerKey(index).c_str(), KEY_WOW64_64KEY, 0);
            if (rc != ERROR_SUCCESS) throw std::runtime_error("Remove controller configuration: " + Win32Message(rc));
        }
    }
}

void WithOperationLock(const std::function<void()>& operation) {
    OperationLock lock;
    operation();
}

std::uint32_t StartedDevices() {
    std::uint32_t mask = 0;
    for (int index : {0, 1}) {
        if (!BoundOem(index, true, {}).empty()) mask |= 1u << index;
    }
    return mask;
}


Devices InspectDevices() {
    Devices result;
    for (int index : {0, 1}) {
        DEVINST node = 0;
        const auto* instance = index == 0 ? kKeyboardInstance : kMouseInstance;
        const bool present = CM_Locate_DevNodeW(&node, const_cast<wchar_t*>(instance),
            CM_LOCATE_DEVNODE_PHANTOM) == CR_SUCCESS;
        (index == 0 ? result.keyboardPresent : result.mousePresent) = present;
        try {
            result.configurationPresent |= OwnedConfig(index, true);
            const auto savedPackage = SavedRemovalPackage(index);
            wchar_t savedInf[MAX_PATH]{};
            result.boundPackagePresent |= !savedPackage.empty()
                && SetupGetInfDriverStoreLocationW(savedPackage.c_str(), nullptr, nullptr,
                    savedInf, ARRAYSIZE(savedInf), nullptr)
                && GetFileAttributesW(savedInf) != INVALID_FILE_ATTRIBUTES;
            const auto bound = BoundOem(index, false, {}, true);
            result.boundPackagePresent |= !bound.empty() && bound != L"<unbound>";
            const bool ready = !BoundOem(index, true, {}).empty();
            (index == 0 ? result.keyboardReady : result.mouseReady) = ready;
        } catch (const std::exception& error) {
            if (!result.error.empty()) result.error += L"\n";
            result.error += (index == 0 ? L"Keyboard: " : L"Mouse: ");
            const std::string text = error.what();
            const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                static_cast<int>(text.size()), nullptr, 0);
            std::wstring wide(static_cast<std::size_t>(count), L'\0');
            if (count > 0) MultiByteToWideChar(CP_UTF8, 0, text.data(),
                static_cast<int>(text.size()), wide.data(), count);
            result.error += wide;
        }
    }
    result.profileReady = result.keyboardReady && result.mouseReady && CurrentProfile(0) && CurrentProfile(1);
    return result;
}

void CheckPackageVersion(const std::wstring& infPath) {
    HINF inf = SetupOpenInfFileW(infPath.c_str(), L"HIDClass", INF_STYLE_WIN4, nullptr);
    if (inf == INVALID_HANDLE_VALUE) Fail("Open xiaoji package version");
    INFCONTEXT line{};
    wchar_t version[64]{};
    const bool compatible = SetupFindFirstLineW(inf, L"Version", L"DriverVer", &line)
        && SetupGetStringFieldW(&line, 2, version, ARRAYSIZE(version), nullptr)
        && _wcsicmp(version, L"1.0.0.1") == 0;
    SetupCloseInfFile(inf);
    if (!compatible)
        throw std::runtime_error("Unsupported XiaojiDriver version; this adapter requires driver 1.0.0.1 from product 0.1.0.");
}

namespace {
void CheckDriverVersion(const std::wstring& publishedInf) {
    wchar_t storeInf[MAX_PATH]{};
    if (!SetupGetInfDriverStoreLocationW(publishedInf.c_str(), nullptr, nullptr,
            storeInf, MAX_PATH, nullptr)) Fail("Resolve xiaoji package version");
    CheckPackageVersion(storeInf);
}

void CheckReportConfig(int index) {
    if (!CurrentProfile(index))
        throw std::runtime_error("Xiaoji input configuration needs upgrading. Use Install/Repair to enable NKRO, consumer keys and five mouse buttons.");
}

}

struct Client::State {
    SessionLease lease;
    std::mutex ioMutex;
    std::unique_ptr<XiaojiDevice> keyboard;
    std::unique_ptr<XiaojiDevice> mouse;
};

Client::Client() {
    SYSTEM_INFO system{};
    GetNativeSystemInfo(&system);
    if (sizeof(void*) != 8 || system.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_AMD64)
        throw std::runtime_error("XiaojiDriver supports Windows x64 only.");
    if (!IsUserAnAdmin()) throw std::runtime_error("XiaojiDriver input requires administrator privileges.");
    state_ = std::make_unique<State>();
    for (int index : {0, 1}) {
        const auto published = BoundOem(index, true, {});
        if (published.empty()) throw std::runtime_error("XiaojiDriver keyboard and mouse must both be installed and started.");
        CheckDriverVersion(published);
        CheckReportConfig(index);
    }
    state_->keyboard = std::make_unique<XiaojiDevice>(0);
    state_->mouse = std::make_unique<XiaojiDevice>(1);
    std::vector<BYTE> keyboardRelease(kKeyboardWireSize, 0), consumerRelease(kConsumerWireSize, 0);
    keyboardRelease[0] = kKeyboardReportId;
    consumerRelease[0] = kConsumerReportId;
    state_->keyboard->ArmRelease({keyboardRelease, consumerRelease});
    state_->mouse->ArmRelease({std::vector<BYTE>(4, 0)});
    state_->keyboard->WaitForDriver(10000);
    state_->mouse->WaitForDriver(10000);
}

Client::~Client() = default;

void Client::SendKeyboard(const KeyboardState& state) {
    std::lock_guard lock(state_->ioMutex);
    std::array<BYTE, kKeyboardWireSize> report{};
    report[0] = kKeyboardReportId;
    std::copy(state.begin(), state.end(), report.begin() + 1);
    state_->keyboard->WriteReport(report.data(), static_cast<ULONG>(report.size()), true);
}

void Client::SendConsumer(const ConsumerState& state) {
    std::lock_guard lock(state_->ioMutex);
    std::array<BYTE, kConsumerWireSize> report{};
    report[0] = kConsumerReportId;
    for (std::size_t i = 0; i < state.size(); ++i) {
        report[1 + i * 2] = static_cast<BYTE>(state[i] & 0xFF);
        report[2 + i * 2] = static_cast<BYTE>(state[i] >> 8);
    }
    state_->keyboard->WriteReport(report.data(), static_cast<ULONG>(report.size()), true);
}

void Client::SendMouse(const MouseState& report) {
    std::lock_guard lock(state_->ioMutex);
    state_->mouse->WriteReport(report.data(), static_cast<ULONG>(report.size()));
}

} // namespace xiaoji
