#pragma once

#include "InputDriver.h"
#include "core/plugins/PluginManifest.h"
#include <xiaoji/plugin/api.h>
#include <QByteArray>
#include <QString>
#include <atomic>
#include <memory>

namespace Plugins {
class NativeModule;

class NativePluginDriver final : public InputDriver {
public:
    static std::unique_ptr<NativePluginDriver> create(const Manifest& manifest, const QString& provider,
        const QString& hostDirectory, const QString& dataDirectory, QString* error);
    ~NativePluginDriver() override;
    bool connect(const QString& argument = {});
    void disconnect();
    bool connected() const noexcept { return connected_.load(); }
    void setEnabled(bool enabled) noexcept { enabled_.store(enabled); }
    QString text(uint32_t kind, const KeyDescriptor* key = nullptr) const;
    bool control(uint32_t operation, const QString& argument = {});
    QString providerId() const { return providerId_; }
    bool hardwareOutput() const { return (capabilities() & XJ_CAP_HARDWARE_OUTPUT) != 0; }

    bool IsReady() const noexcept override;
    std::wstring GetLastError() const override;
    std::wstring KeySupportError(const KeyDescriptor& key) const override;
    bool SupportsPhysicalModifierIsolation() const override;
    bool SupportsPauseModifierRelay() const override;
    bool SupportsPhysicalKeyRelay(const KeyDescriptor& key) const noexcept override;
    bool SendTap(const KeyDescriptor& key, InputSource source = InputSource::Injected) override;
    bool SendDown(const KeyDescriptor& key, InputSource source = InputSource::Injected) override;
    bool SendHeldDown(const KeyDescriptor& key, InputSource source = InputSource::Injected) override;
    bool RestorePhysicalDown(const KeyDescriptor& key, InputSource source = InputSource::Injected) override;
    bool SendUp(const KeyDescriptor& key, InputSource source = InputSource::Injected) override;
    bool SendMouseMove(int x, int y, InputSource source = InputSource::Injected) override;
    bool SupportsMouseMove() const override;
    bool SendMouseWheel(int delta, InputSource source = InputSource::Injected) override;
    bool SupportsMouseWheel() const override;
    void SetSourceActiveKeys(InputSource source, const std::vector<KeyDescriptor>& keys) override;
    void ReleaseSource(InputSource source) override;
    void ReleaseAll() override;
    void ClearAllStates() override;
    bool ConsumeSelfInjected(const KeyDescriptor& key, bool pressed) override;
    bool ConsumeSelfInjectedRaw(const KeyDescriptor& key, bool pressed) override;
    bool ConsumeSelfInjectedWheel(int delta) override;

private:
    NativePluginDriver(std::shared_ptr<NativeModule> module, const XjDriverApi& api, QString providerId);
    bool send(uint32_t action, const KeyDescriptor& key, InputSource source, int x = 0, int y = 0, int wheel = 0);
    bool consume(const KeyDescriptor& key, bool pressed, uint32_t channel, int wheel = 0);
    uint64_t capabilities() const;
    std::shared_ptr<NativeModule> module_;
    XjDriverApi api_{};
    QString providerId_;
    std::atomic<bool> enabled_{true};
    std::atomic<bool> connected_{false};
};
}
