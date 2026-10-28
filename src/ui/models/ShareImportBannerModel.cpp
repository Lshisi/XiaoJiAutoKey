#include "ShareImportBannerModel.h"

ShareImportBannerModel::ShareImportBannerModel(QObject* parent) : QObject(parent) {
    autoHideTimer_.setSingleShot(true);
    autoHideTimer_.setInterval(8000);
    connect(&autoHideTimer_, &QTimer::timeout, this, &ShareImportBannerModel::hideBanner);
}

void ShareImportBannerModel::showForText(const QString& text) {
    shareText_ = text;
    emit visibleChanged();
    autoHideTimer_.start();
}

void ShareImportBannerModel::hideBanner() {
    autoHideTimer_.stop();
    if (shareText_.isEmpty()) return;
    shareText_.clear();
    emit visibleChanged();
}

void ShareImportBannerModel::importShare() {
    if (!visible()) return;
    const QString text = shareText_;
    hideBanner();
    emit importRequested(text);
}

void ShareImportBannerModel::dismiss() {
    if (!visible()) return;
    const QString text = shareText_;
    hideBanner();
    emit dismissed(text);
}
