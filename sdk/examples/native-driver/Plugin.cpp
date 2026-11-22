#include <xiaoji/plugin/api.h>
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <string>

namespace {
struct Keyboard {
    std::atomic<bool> ready{false};
    std::mutex mutex;
    std::array<bool, 256> held{};
    std::string error;
};

uint32_t text(const std::string& value, char* output, uint32_t capacity) {
    const auto required = static_cast<uint32_t>(value.size() + 1);
    if (output && capacity) {
        const auto count = value.size() < capacity ? value.size() : capacity - 1;
        std::memcpy(output, value.data(), count);
        output[count] = '\0';
    }
    return required;
}

bool sendKey(uint32_t code, bool pressed) {
    INPUT input{}; input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(code);
    input.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
    switch (code) {
    case VK_RCONTROL: case VK_RMENU: case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
    case VK_PRIOR: case VK_NEXT: case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
    case VK_DIVIDE: case VK_NUMLOCK: case VK_SNAPSHOT: input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY; break;
    default: break;
    }
    return SendInput(1, &input, sizeof(input)) == 1;
}

void release(Keyboard& driver) {
    std::lock_guard lock(driver.mutex);
    for (uint32_t code = 1; code < driver.held.size(); ++code)
        if (driver.held[code] && sendKey(code, false)) driver.held[code] = false;
}

XjResult XJ_CALL initialize(const XjHostApi* host, const XjPluginPaths*, void** instance) {
    if (!host || host->abi_version != XJ_PLUGIN_ABI_VERSION || !instance) return XJ_INCOMPATIBLE;
    *instance = nullptr;
    return XJ_OK;
}

XjResult XJ_CALL create(void*, const char* provider, XjDriverApi* output) {
    if (!provider || std::strcmp(provider, "keyboard") || !output || output->size < sizeof(XjDriverApi)) return XJ_INVALID_ARGUMENT;
    auto* driver = new (std::nothrow) Keyboard;
    if (!driver) return XJ_ERROR;
    *output = {};
    output->size = sizeof(*output); output->abi_version = XJ_PLUGIN_ABI_VERSION; output->instance = driver;
    output->destroy = [](void* value) { auto* instance = static_cast<Keyboard*>(value); release(*instance); delete instance; };
    output->connect = [](void* value, const char*) -> XjResult { static_cast<Keyboard*>(value)->ready = true; return XJ_OK; };
    output->disconnect = [](void* value) { auto& instance = *static_cast<Keyboard*>(value); instance.ready = false; release(instance); };
    output->is_ready = [](void* value) -> uint32_t { return static_cast<Keyboard*>(value)->ready.load(); };
    output->capabilities = [](void*) -> uint64_t { return XJ_CAP_KEYBOARD; };
    output->release_all = [](void* value) { release(*static_cast<Keyboard*>(value)); };
    output->get_text = [](void* value, uint32_t kind, const XjKey* key, char* buffer, uint32_t capacity) -> uint32_t {
        auto& instance = *static_cast<Keyboard*>(value);
        std::lock_guard lock(instance.mutex);
        if (kind == XJ_TEXT_KEY_SUPPORT && key && (key->device != XJ_KEYBOARD || !key->code || key->code > 255 || key->modifiers))
            return text("This example supports ordinary keyboard keys without modifier combinations only.", buffer, capacity);
        return text(kind == XJ_TEXT_ERROR ? instance.error : std::string(), buffer, capacity);
    };
    output->send = [](void* value, const XjInput* event) -> XjResult {
        if (!event || event->size < sizeof(XjInput) || event->key.device != XJ_KEYBOARD || !event->key.code || event->key.code > 255 || event->key.modifiers) return XJ_UNSUPPORTED;
        auto& instance = *static_cast<Keyboard*>(value);
        if (!instance.ready.load() && event->action != XJ_UP) return XJ_ERROR;
        std::lock_guard lock(instance.mutex);
        const auto code = event->key.code;
        bool ok = false;
        if (event->action == XJ_UP) { ok = sendKey(code, false); if (ok) instance.held[code] = false; }
        else if (event->action == XJ_TAP) {
            ok = sendKey(code, true);
            if (ok) { instance.held[code] = true; Sleep(5); ok = sendKey(code, false); if (ok) instance.held[code] = false; }
        } else if (event->action == XJ_RESTORE_PHYSICAL) {
            ok = sendKey(code, true); if (ok) instance.held[code] = false;
        } else if (event->action == XJ_DOWN || event->action == XJ_HELD_DOWN) {
            ok = sendKey(code, true); if (ok) instance.held[code] = true;
        } else return XJ_UNSUPPORTED;
        instance.error = ok ? std::string() : "Windows SendInput failed.";
        return ok ? XJ_OK : XJ_ERROR;
    };
    return XJ_OK;
}
void XJ_CALL shutdown(void*) {}
const XjPluginApi api{sizeof(XjPluginApi), XJ_PLUGIN_ABI_VERSION, XIAOJI_PLUGIN_ID, XIAOJI_PLUGIN_VERSION, initialize, create, shutdown};
}
extern "C" XJ_EXPORT const XjPluginApi* XJ_CALL XiaojiPluginGetApi() { return &api; }
