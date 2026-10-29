#include "dialogs/ConfigConfirmDialog.h"
ConfigConfirmDialog::ConfigConfirmDialog(const QString& title, const QString& message,
    const QString& confirmText, QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(title);
    present("DialogMessage.qml", {{"message", message}, {"warning", true},
        {"actions", QVariantList{button("accept", confirmText, true, true), button("cancel", tr("取消"), false)}}},
        QSize(380, 176), true);
}
bool ConfigConfirmDialog::confirm(QWidget* parent, const QString& title, const QString& message, const QString& confirmText) {
    ConfigConfirmDialog dialog(title, message, confirmText, parent);
    return dialog.exec() == QDialog::Accepted;
}
void ConfigConfirmDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "accept") accept();
    else QmlDialog::handleAction(action, argument);
}
