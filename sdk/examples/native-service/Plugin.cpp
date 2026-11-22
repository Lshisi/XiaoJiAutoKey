#include <xiaoji/plugin/api.h>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
struct Service { XjHostApi host{}; unsigned count = 0; bool running = false; };
XjResult XJ_CALL initialize(const XjHostApi* host, const XjPluginPaths*, void** instance) {
    if (!instance || !XJ_HAS_FIELD(host, XjHostApi, publish_event) || !host->reply || !host->publish_event
        || host->abi_version != XJ_PLUGIN_ABI_VERSION) return XJ_INCOMPATIBLE;
    auto* service = new (std::nothrow) Service;
    if (!service) return XJ_ERROR;
    service->host = *host; *instance = service; return XJ_OK;
}
XjResult XJ_CALL start(void* value) { static_cast<Service*>(value)->running = true; return XJ_OK; }
void XJ_CALL stop(void* value) { static_cast<Service*>(value)->running = false; }
XjResult XJ_CALL request(void* value, const char* id, const char* command, const char*, uint32_t) {
    auto& service = *static_cast<Service*>(value);
    if (!service.running || !command || std::strcmp(command, "increment") != 0) return XJ_UNSUPPORTED;
    char json[64]{};
    const auto length = std::snprintf(json, sizeof(json), "{\"count\":%u}", ++service.count);
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(json)) return XJ_ERROR;
    service.host.reply(service.host.context, id, XJ_OK, json, static_cast<uint32_t>(length));
    service.host.publish_event(service.host.context, "counter.changed", json, static_cast<uint32_t>(length));
    return XJ_OK;
}
void XJ_CALL shutdown(void* value) { delete static_cast<Service*>(value); }
const XjPluginApi api{sizeof(XjPluginApi), XJ_PLUGIN_ABI_VERSION, XIAOJI_PLUGIN_ID, XIAOJI_PLUGIN_VERSION,
    initialize, nullptr, shutdown, start, stop, request};
}
extern "C" XJ_EXPORT const XjPluginApi* XJ_CALL XiaojiPluginGetApi() { return &api; }
