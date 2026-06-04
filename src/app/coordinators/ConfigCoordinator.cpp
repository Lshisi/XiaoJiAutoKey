#include "coordinators/ConfigCoordinator.h"

#include "config/ConfigNormalizer.h"

#include <QMessageBox>

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

void ConfigCoordinator::save() {
    if (!context_.config) return;

    std::wstring error;
    if (!store_.Save(*context_.config, error)) {
        QMessageBox::warning(context_.parent, QString::fromWCharArray(L"\u4fdd\u5b58\u914d\u7f6e\u5931\u8d25"), QString::fromStdWString(error));
    }
}

const std::filesystem::path& ConfigCoordinator::configPath() const noexcept {
    return store_.GetPath();
}
