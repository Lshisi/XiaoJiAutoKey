#include "core/plugins/NativePluginDriver.h"
#include "core/plugins/NativePluginModule.h"
#include <algorithm>
#include <utility>

namespace Plugins {
namespace {
XjKey convert(const KeyDescriptor& key) { return {static_cast<uint32_t>(key.device), key.code, key.modifiers}; }
XjInput event(uint32_t action, const KeyDescriptor& key, InputSource source) {
    XjInput input{};
    input.size = sizeof(input);
    input.action = action;
    input.source = static_cast<uint32_t>(source);
    input.key = convert(key);
    return input;
}
}

NativePluginDriver::NativePluginDriver(std::shared_ptr<NativeModule> module, const XjDriverApi& api, QString providerId)
    : module_(std::move(module)), api_(api), providerId_(std::move(providerId)) {}

std::unique_ptr<NativePluginDriver> NativePluginDriver::create(const Manifest& manifest, const QString& provider,
    const QString& hostDirectory, const QString& dataDirectory, QString* error) {
    const auto found = std::find_if(manifest.drivers.cbegin(), manifest.drivers.cend(),
        [&](const DriverContribution& item) { return item.id == provider; });
    if (found == manifest.drivers.cend()) { if (error) *error = QStringLiteral("插件没有声明此驱动提供方。"); return {}; }
    auto module = NativeModule::acquire(manifest, hostDirectory, dataDirectory, error);
    if (!module) return {};
    XjDriverApi api{};
    api.size = sizeof(api);
    const auto status = module->create(provider, &api);
    if (status != XJ_OK || api.size < sizeof(XjDriverApi) || api.abi_version != XJ_PLUGIN_ABI_VERSION
        || !api.instance || !api.destroy || !api.connect || !api.disconnect || !api.is_ready
        || !api.capabilities || !api.send || !api.release_all || !api.get_text) {
        if (api.instance && api.destroy) { try { api.destroy(api.instance); } catch (...) {} }
        if (error) *error = QStringLiteral("插件未能创建兼容的驱动接口。");
        return {};
    }
    return std::unique_ptr<NativePluginDriver>(new NativePluginDriver(module, api, manifest.providerId(provider)));
}

NativePluginDriver::~NativePluginDriver() {
    enabled_.store(false);
    try { api_.release_all(api_.instance); } catch (...) {}
    try { api_.disconnect(api_.instance); } catch (...) {}
    try { api_.destroy(api_.instance); } catch (...) {}
}
bool NativePluginDriver::connect(const QString& argument) {
    connected_.store(false);
    const auto bytes = argument.toUtf8();
    try {
        const bool ok = enabled_.load() && api_.connect(api_.instance, bytes.constData()) == XJ_OK;
        connected_.store(ok); return ok;
    } catch (...) { return false; }
}
void NativePluginDriver::disconnect() { connected_.store(false); try { api_.disconnect(api_.instance); } catch (...) {} }
bool NativePluginDriver::IsReady() const noexcept {
    try { return enabled_.load() && api_.is_ready(api_.instance) != 0; } catch (...) { return false; }
}
QString NativePluginDriver::text(uint32_t kind, const KeyDescriptor* key) const {
    const XjKey converted = key ? convert(*key) : XjKey{};
    try {
        const auto count = api_.get_text(api_.instance, kind, key ? &converted : nullptr, nullptr, 0);
        if (!count || count > 65536) return {};
        QByteArray bytes(static_cast<qsizetype>(count), '\0');
        const auto actual = api_.get_text(api_.instance, kind, key ? &converted : nullptr, bytes.data(), count);
        if (!actual || actual > count || bytes[static_cast<qsizetype>(actual - 1)] != '\0') return QStringLiteral("插件返回了无效的文本。");
        return QString::fromUtf8(bytes.constData(), static_cast<qsizetype>(actual - 1));
    } catch (...) { return QStringLiteral("读取插件状态失败。"); }
}
std::wstring NativePluginDriver::GetLastError() const { return text(XJ_TEXT_ERROR).toStdWString(); }
std::wstring NativePluginDriver::KeySupportError(const KeyDescriptor& key) const { return text(XJ_TEXT_KEY_SUPPORT, &key).toStdWString(); }
uint64_t NativePluginDriver::capabilities() const { try { return api_.capabilities(api_.instance); } catch (...) { return 0; } }
bool NativePluginDriver::SupportsPhysicalModifierIsolation() const { return (capabilities() & XJ_CAP_MODIFIER_ISOLATION) != 0; }
bool NativePluginDriver::SupportsPauseModifierRelay() const { return (capabilities() & XJ_CAP_PAUSE_MODIFIER_RELAY) != 0; }
bool NativePluginDriver::SupportsPhysicalKeyRelay(const KeyDescriptor& key) const noexcept {
    if (!enabled_.load() || !api_.supports_physical_key) return false;
    const auto converted = convert(key);
    try { return api_.supports_physical_key(api_.instance, &converted) != 0; } catch (...) { return false; }
}
bool NativePluginDriver::SupportsMouseMove() const { return (capabilities() & XJ_CAP_MOVE) != 0; }
bool NativePluginDriver::SupportsMouseWheel() const { return (capabilities() & XJ_CAP_WHEEL) != 0; }
bool NativePluginDriver::send(uint32_t action, const KeyDescriptor& key, InputSource source, int x, int y, int wheel) {
    if (!enabled_.load() && action != XJ_UP) return false;
    auto input = event(action, key, source);
    input.x = x; input.y = y; input.wheel_delta = wheel;
    try { return api_.send(api_.instance, &input) == XJ_OK; } catch (...) { return false; }
}
bool NativePluginDriver::SendTap(const KeyDescriptor& key, InputSource source) { return send(XJ_TAP, key, source); }
bool NativePluginDriver::SendDown(const KeyDescriptor& key, InputSource source) { return send(XJ_DOWN, key, source); }
bool NativePluginDriver::SendHeldDown(const KeyDescriptor& key, InputSource source) { return send(XJ_HELD_DOWN, key, source); }
bool NativePluginDriver::RestorePhysicalDown(const KeyDescriptor& key, InputSource source) { return send(XJ_RESTORE_PHYSICAL, key, source); }
bool NativePluginDriver::SendUp(const KeyDescriptor& key, InputSource source) { return send(XJ_UP, key, source); }
bool NativePluginDriver::SendMouseMove(int x, int y, InputSource source) { return send(XJ_MOVE, {}, source, x, y); }
bool NativePluginDriver::SendMouseWheel(int delta, InputSource source) { return send(XJ_WHEEL, {}, source, 0, 0, delta); }
void NativePluginDriver::ReleaseAll() { try { api_.release_all(api_.instance); } catch (...) {} }
void NativePluginDriver::ClearAllStates() { if (api_.clear_states) { try { api_.clear_states(api_.instance); } catch (...) {} } }
void NativePluginDriver::SetSourceActiveKeys(InputSource source, const std::vector<KeyDescriptor>& keys) {
    if (!api_.set_source_keys || keys.size() > 4096) return;
    std::vector<XjKey> converted;
    converted.reserve(keys.size());
    for (const auto& key : keys) converted.push_back(convert(key));
    try { api_.set_source_keys(api_.instance, static_cast<uint32_t>(source), converted.data(), static_cast<uint32_t>(converted.size())); } catch (...) {}
}
void NativePluginDriver::ReleaseSource(InputSource source) {
    if (!api_.release_source) { ReleaseAll(); return; }
    try { api_.release_source(api_.instance, static_cast<uint32_t>(source)); } catch (...) {}
}
bool NativePluginDriver::consume(const KeyDescriptor& key, bool pressed, uint32_t channel, int wheel) {
    if (!api_.consume) return false;
    auto input = event(pressed ? XJ_DOWN : XJ_UP, key, InputSource::Injected);
    input.wheel_delta = wheel;
    try { return api_.consume(api_.instance, &input, channel) != 0; } catch (...) { return false; }
}
bool NativePluginDriver::ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) { return consume(key, pressed, XJ_CONSUME_HOOK); }
bool NativePluginDriver::ConsumeSelfInjectedRaw(const KeyDescriptor& key, bool pressed) { return consume(key, pressed, XJ_CONSUME_RAW); }
bool NativePluginDriver::ConsumeSelfInjectedWheel(int delta) { return consume({}, false, XJ_CONSUME_WHEEL, delta); }
bool NativePluginDriver::control(uint32_t operation, const QString& argument) {
    if (!api_.control) return true;
    const auto bytes = argument.toUtf8();
    try { const auto result = api_.control(api_.instance, operation, bytes.constData()); return result == XJ_OK || result == XJ_UNSUPPORTED; }
    catch (...) { return false; }
}
}
