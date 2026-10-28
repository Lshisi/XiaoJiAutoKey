#include "dialogs/ShareExportDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QStringList>
namespace {

// 概述导出内容，方便确认导出的是哪份方案 / 哪条宏。
QString PayloadSummary(const SharePayload& payload) {
    QStringList parts;
    if (!payload.profiles.empty()) {
        parts.push_back(QString::fromWCharArray(L"%1 \u4e2a\u65b9\u6848")
            .arg(payload.profiles.size()));
    }
    if (!payload.macros.empty()) {
        parts.push_back(QString::fromWCharArray(L"%1 \u4e2a\u5b8f")
            .arg(payload.macros.size()));
    }
    if (parts.empty()) {
        return {};
    }
    QString summary = parts.join(QString::fromWCharArray(L"\uff0c"));
    QStringList names;
    for (const auto& profile : payload.profiles) {
        names.push_back(QString::fromStdWString(profile.name));
        if (names.size() >= 4) break;
    }
    for (const auto& macro : payload.macros) {
        names.push_back(QString::fromStdWString(macro.name));
        if (names.size() >= 4) break;
    }
    if (!names.empty()) {
        summary += QString::fromWCharArray(L"\uff1a") + names.join(QString::fromWCharArray(L"\u3001"));
        if (payload.profiles.size() + payload.macros.size() > names.size()) {
            summary += QString::fromWCharArray(L" \u7b49");
        }
    }
    return summary;
}

}  // namespace



ShareExportDialog::ShareExportDialog(const QString& title, const SharePayload& payload, QWidget* parent,
    const std::function<void(const QString&)>& onCopied) : QmlDialog(parent), payload_(payload), onCopied_(onCopied) {
    setWindowTitle(title);
    summary_ = PayloadSummary(payload);
    present("DialogShare.qml", {{"exporting", true}, {"format", 0}, {"code", QString()}, {"status", summary_},
        {"actions", QVariantList{button("copy", tr("复制")), button("cancel", tr("关闭"), false)}}}, QSize(620, 480));
    refreshExport();
}
void ShareExportDialog::refreshExport() {
    const QString content = value("format").toInt() == 1
        ? QString::fromStdString(ShareCodec::EncodeTextRecipe(payload_)) : QString::fromStdString(ShareCodec::EncodeShareCode(payload_));
    setValue("code", content);
    setValue("status", summary_ + tr(" · 长度：%1 字符").arg(content.size()));
}
void ShareExportDialog::editField(const QString& field, const QVariant& argument) {
    if (field == "format" && argument.toInt() >= 0 && argument.toInt() <= 1) { setValue(field, argument.toInt()); refreshExport(); }
}
void ShareExportDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "copy") {
        const QString content = value("code").toString();
        QApplication::clipboard()->setText(content);
        setValue("status", tr("已复制到剪贴板"));
        if (onCopied_) onCopied_(content.trimmed());
    } else QmlDialog::handleAction(action, argument);
}
