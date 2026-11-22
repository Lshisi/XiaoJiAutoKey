#include "XiaojiDriver.h"
#include <xiaoji/plugin/NativeInput.hpp>
#include <memory>
#include <string_view>

namespace {
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
        auto driver = std::make_unique<XiaojiDriver>();
        xiaoji::plugin::DriverBridge<XiaojiDriver>::Bind(driver.get(), result);
        result->connect = [](void* value, const char*) -> XjResult {
            try { return static_cast<XiaojiDriver*>(value)->Connect() ? XJ_OK : XJ_ERROR; }
            catch (...) { return XJ_ERROR; }
        };
        result->disconnect = [](void* value) { static_cast<XiaojiDriver*>(value)->Disconnect(); };
        driver.release();
        return XJ_OK;
    } catch (...) { return XJ_ERROR; }
}
void XJ_CALL Shutdown(void*) { xiaoji::plugin::hostApi = nullptr; }
const XjPluginApi api{sizeof(XjPluginApi), XJ_PLUGIN_ABI_VERSION, XIAOJI_PLUGIN_ID, XIAOJI_PLUGIN_VERSION, Initialize, Create, Shutdown};
}
extern "C" XJ_EXPORT const XjPluginApi* XJ_CALL XiaojiPluginGetApi() { return &api; }
