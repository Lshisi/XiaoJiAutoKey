#include "dialogs/ConfigConfirmDialog.h"
ConfigConfirmDialog::ConfigConfirmDialog(const QString& title, const QString& message,
    const QString& confirmText, QWidget* parent, const QString& optionText, bool optionChecked) : QmlDialog(parent) {
    setWindowTitle(title);
    present("DialogMessage.qml", {{"message", message}, {"warning", true},
        {"optionText", optionText}, {"optionChecked", optionChecked},
        {"actions", QVariantList{button("accept", confirmText, true, true), button("cancel", tr("取消"), false)}}},
        QSize(380, 176), true);
}
bool ConfigConfirmDialog::confirmWithOption(QWidget* parent, const QString& title, const QString& message,
    const QString& confirmText, const QString& optionText, bool* checked) {
    ConfigConfirmDialog dialog(title, message, confirmText, parent, optionText, !checked || *checked);
    if (dialog.exec() != QDialog::Accepted) return false;
    if (checked) *checked = dialog.value("optionChecked").toBool();
    return true;
}
void ConfigConfirmDialog::editField(const QString& field, const QVariant& value) {
    if (field == QStringLiteral("optionChecked")) setValue(field, value.toBool());
}
bool ConfigConfirmDialog::confirm(QWidget* parent, const QString& title, const QString& message, const QString& confirmText) {
    ConfigConfirmDialog dialog(title, message, confirmText, parent);
    return dialog.exec() == QDialog::Accepted;
}
void ConfigConfirmDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "accept") accept();
    else QmlDialog::handleAction(action, argument);
}
