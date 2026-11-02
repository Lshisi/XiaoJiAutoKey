#include "coordinators/ConfigCoordinator.h"

#include "config/ConfigNormalizer.h"
#include "dialogs/AppMessageDialog.h"

ConfigCoordinator::ConfigCoordinator(std::filesystem::path configPath, QObject* parent)
    : QObject(parent),
      store_(std::move(configPath)) {}

void ConfigCoordinator::bind(Context context) {
    context_ = std::move(context);
}

void ConfigCoordinator::load() {
    if (!context_.config) return;

    *context_.config = store_.Load();
    ConfigNormalizer::normalize(*context_.config);
    if (context_.refreshHookState) context_.refreshHookState();
}

bool ConfigCoordinator::save(bool showError, std::uint32_t lockTimeoutMs) {
    if (!context_.config) return false;
    if (context_.beforeSave) context_.beforeSave();

    std::wstring error;
    if (!store_.Save(*context_.config, error, lockTimeoutMs)) {
        if (showError) {
            AppMessageDialog::warning(context_.parent, QString::fromWCharArray(L"\u4fdd\u5b58\u914d\u7f6e\u5931\u8d25"), QString::fromStdWString(error));
        }
        return false;
    } else if (context_.saved) {
        context_.saved();
    }
    return true;
}

const std::filesystem::path& ConfigCoordinator::configPath() const noexcept {
    return store_.GetPath();
}
