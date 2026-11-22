#include "DdDriver.h"
#include <xiaoji/plugin/NativeInput.hpp>
#include <filesystem>
#include <memory>
#include <string_view>

namespace {
struct Context { std::wstring directory; };
struct Driver final : DdDriver {
    std::wstring directory;
    std::wstring DiagnosticText() const override {
        return L"通过插件内 DD 运行库提供键盘、鼠标移动与滚轮输出。";
    }
};

XjResult XJ_CALL Initialize(const XjHostApi* host, const XjPluginPaths* paths, void** result) {
    if (!host || host->size < sizeof(XjHostApi) || host->abi_version != XJ_PLUGIN_ABI_VERSION
        || !paths || paths->size < sizeof(XjPluginPaths) || !paths->plugin_directory || !result) return XJ_INVALID_ARGUMENT;
    try {
        xiaoji::plugin::hostApi = host;
        *result = new Context{ToWide(paths->plugin_directory)};
        return XJ_OK;
    } catch (...) { return XJ_ERROR; }
}

XjResult XJ_CALL Create(void* context, const char* provider, XjDriverApi* result) {
    if (!context || !provider || !result || result->size < sizeof(XjDriverApi)) return XJ_INVALID_ARGUMENT;
    if (std::string_view(provider) != "default") return XJ_UNSUPPORTED;
    try {
        auto driver = std::make_unique<Driver>();
        driver->directory = static_cast<Context*>(context)->directory;
        xiaoji::plugin::DriverBridge<Driver>::Bind(driver.get(), result);
        result->connect = [](void* value, const char*) -> XjResult {
            try { auto& target = *static_cast<Driver*>(value); return target.LoadLegacyFromDirectory(target.directory) ? XJ_OK : XJ_ERROR; }
            catch (...) { return XJ_ERROR; }
        };
        result->disconnect = [](void* value) { static_cast<Driver*>(value)->Unload(); };
        driver.release();
        return XJ_OK;
    } catch (...) { return XJ_ERROR; }
}

void XJ_CALL Shutdown(void* context) { delete static_cast<Context*>(context); xiaoji::plugin::hostApi = nullptr; }
const XjPluginApi api{sizeof(XjPluginApi), XJ_PLUGIN_ABI_VERSION, XIAOJI_PLUGIN_ID, XIAOJI_PLUGIN_VERSION, Initialize, Create, Shutdown};
}
extern "C" XJ_EXPORT const XjPluginApi* XJ_CALL XiaojiPluginGetApi() { return &api; }
