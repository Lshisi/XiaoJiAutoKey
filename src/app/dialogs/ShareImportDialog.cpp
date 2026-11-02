#include "dialogs/ShareImportDialog.h"
#include "dialogs/ShareNotePreview.h"
#include <QApplication>
#include <QClipboard>
#include <QStringList>
namespace {

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
    QString summary = QString::fromWCharArray(L"\u5c06\u5bfc\u5165 ") + parts.join(QString::fromWCharArray(L"\uff0c"));
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
    if (!payload.profiles.empty() && !payload.faction.empty()) {
        summary += QString::fromWCharArray(
            L"\n\u5bfc\u5165\u540e\u5c06\u540c\u6b65\u5207\u6362\u95e8\u6d3e\u4e3b\u9898\u4e3a %1\u3002")
            .arg(QString::fromStdString(payload.faction));
    }
    return summary;
}

}  // namespace



ShareImportDialog::ShareImportDialog(QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(tr("导入配置"));
    present("DialogShare.qml", {{"exporting", false}, {"format", 0}, {"code", QString()}, {"status", QString()}, {"notePreview", QString()},
        {"canImport", false}, {"actions", QVariantList{button("import", tr("导入")), button("paste", tr("粘贴"), false), button("cancel", tr("取消"), false)}}}, QSize(480, 340));
    previewImport();
}
void ShareImportDialog::setText(const QString& text) { setValue("code", text); previewImport(); }
void ShareImportDialog::previewImport() {
    hasImport_ = false;
    setValue("notePreview", QString());
    const auto text = value("code").toString().trimmed().toStdString();
    auto payload = ShareCodec::DecodeShareCode(text);
    if (!payload) payload = ShareCodec::DecodeTextRecipe(text);
    if (payload) {
        importPayload_ = std::move(*payload);
        hasImport_ = true;
        setValue("status", PayloadSummary(importPayload_));
        setValue("notePreview", ShareNotePreview(importPayload_));
    } else setValue("status", text.empty() ? QString() : tr("解析失败，请检查内容是否完整"));
    setValue("canImport", hasImport_);
    auto actions = value("actions").toList();
    auto primary = actions[0].toMap();
    primary.insert("enabled", hasImport_);
    actions[0] = primary;
    setValue("actions", actions);
}
void ShareImportDialog::editField(const QString& field, const QVariant& argument) {
    if (field == "code") setText(argument.toString());
}
void ShareImportDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "paste") setText(QApplication::clipboard()->text());
    else if (action == "import") { if (hasImport_) accept(); }
    else QmlDialog::handleAction(action, argument);
}
