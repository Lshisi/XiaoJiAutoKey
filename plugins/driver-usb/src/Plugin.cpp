#include "Ch552UsbDriver.h"
#include <xiaoji/plugin/NativeInput.hpp>
#include <memory>
#include <string_view>

namespace {
constexpr wchar_t kLeaseFirmware[] = L"XiaoJi USB V1.1.4";
XjResult XJ_CALL Initialize(const XjHostApi* host, const XjPluginPaths* paths, void** result) {
    if (!host || host->size < sizeof(XjHostApi) || host->abi_version != XJ_PLUGIN_ABI_VERSION
        || !paths || paths->size < sizeof(XjPluginPaths) || !result) return XJ_INVALID_ARGUMENT;
    xiaoji::plugin::hostApi = host;
    *result = nullptr;
    return XJ_OK;
}
XjResult XJ_CALL Create(void*, const char* provider, XjDriverApi* result) {
    if (!provider || !result || result->size < sizeof(XjDriverApi)) return XJ_INVALID_ARGUMENT;
    if (std::string_view(provider) != "default") return XJ_UNSUPPORTED;
    try {
        auto driver = std::make_unique<Ch552UsbDriver>();
        xiaoji::plugin::DriverBridge<Ch552UsbDriver>::Bind(driver.get(), result);
        result->capabilities = [](void*) -> uint64_t { return XJ_CAP_KEYBOARD | XJ_CAP_MOUSE | XJ_CAP_MODIFIER_ISOLATION | XJ_CAP_HARDWARE_OUTPUT; };
        result->connect = [](void* value, const char* port) -> XjResult {
            try {
                auto& target = *static_cast<Ch552UsbDriver*>(value);
                if (!(port && *port ? target.Connect(ToWide(port)) : target.AutoConnect())) return XJ_ERROR;
                if (target.FirmwareVersion() == kLeaseFirmware && !target.BeginLease()) { target.Disconnect(); return XJ_ERROR; }
                return XJ_OK;
            } catch (...) { return XJ_ERROR; }
        };
        result->disconnect = [](void* value) { static_cast<Ch552UsbDriver*>(value)->Disconnect(); };
        result->get_text = [](void* value, uint32_t kind, const XjKey* key, char* buffer, uint32_t capacity) -> uint32_t {
            auto& target = *static_cast<Ch552UsbDriver*>(value);
            std::wstring text;
            if (kind == XJ_TEXT_PORT) text = target.PortName();
            else if (kind == XJ_TEXT_FIRMWARE) text = target.FirmwareVersion();
            else if (kind == XJ_TEXT_ERROR) text = target.GetLastError();
            else if (kind == XJ_TEXT_DIAGNOSTIC) text = L"通过 CH552 串口设备输出键盘和鼠标按键，无需由插件安装 Windows 系统驱动。";
            else if (kind == XJ_TEXT_KEY_SUPPORT && key) text = target.KeySupportError(xiaoji::plugin::FromAbi(*key));
            return xiaoji::plugin::CopyText(ToUtf8(text), buffer, capacity);
        };
        result->control = [](void* value, uint32_t operation, const char*) -> XjResult {
            try {
                auto& target = *static_cast<Ch552UsbDriver*>(value);
                if (operation == XJ_CONTROL_HEARTBEAT) return target.FirmwareVersion() != kLeaseFirmware || target.Heartbeat() ? XJ_OK : XJ_ERROR;
                if (operation == XJ_CONTROL_SUSPEND) { target.ReleaseAll(); target.Disconnect(); return XJ_OK; }
                return XJ_UNSUPPORTED;
            } catch (...) { return XJ_ERROR; }
        };
        driver.release();
        return XJ_OK;
    } catch (...) { return XJ_ERROR; }
}
void XJ_CALL Shutdown(void*) { xiaoji::plugin::hostApi = nullptr; }
const XjPluginApi api{sizeof(XjPluginApi), XJ_PLUGIN_ABI_VERSION, XIAOJI_PLUGIN_ID, XIAOJI_PLUGIN_VERSION, Initialize, Create, Shutdown};
}
extern "C" XJ_EXPORT const XjPluginApi* XJ_CALL XiaojiPluginGetApi() { return &api; }
