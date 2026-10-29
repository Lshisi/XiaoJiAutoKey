#include "dialogs/AppMessageDialog.h"
#include <Windows.h>
#include <QTimer>

namespace {
void bringDialogToFront(QDialog* dialog) {
    dialog->raise();
    dialog->activateWindow();
    const HWND hwnd = reinterpret_cast<HWND>(dialog->winId());
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);
}
}

AppMessageDialog::AppMessageDialog(Kind kind, const QString& title, const QString& message,
    const QString& primaryButtonText, const QString& secondaryButtonText, QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(title);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowModality(Qt::ApplicationModal);
    QVariantList actions{button("accept", primaryButtonText)};
    if (!secondaryButtonText.isEmpty()) actions << button("cancel", secondaryButtonText, false);
    present("DialogMessage.qml", {{"message", message}, {"warning", kind == Kind::Warning},
        {"actions", actions}}, QSize(400, 176), true);
}
void AppMessageDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "accept") accept();
    else QmlDialog::handleAction(action, argument);
}
void AppMessageDialog::warning(QWidget* parent, const QString& title, const QString& message) {
    MessageBeep(MB_ICONWARNING);
    AppMessageDialog dialog(Kind::Warning, title, message, tr("确定"), {}, parent);
    QTimer::singleShot(0, &dialog, [&dialog] { bringDialogToFront(&dialog); });
    dialog.exec();
}
void AppMessageDialog::information(QWidget* parent, const QString& title, const QString& message) {
    MessageBeep(MB_ICONINFORMATION);
    AppMessageDialog dialog(Kind::Information, title, message, tr("确定"), {}, parent);
    QTimer::singleShot(0, &dialog, [&dialog] { bringDialogToFront(&dialog); });
    dialog.exec();
}
bool AppMessageDialog::informationWithActions(QWidget* parent, const QString& title, const QString& message,
    const QString& primaryButtonText, const QString& secondaryButtonText) {
    MessageBeep(MB_ICONINFORMATION);
    AppMessageDialog dialog(Kind::Information, title, message, primaryButtonText, secondaryButtonText, parent);
    QTimer::singleShot(0, &dialog, [&dialog] { bringDialogToFront(&dialog); });
    return dialog.exec() == QDialog::Accepted;
}
