#include "dialogs/CloseConfirmDialog.h"
CloseConfirmDialog::CloseConfirmDialog(QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(tr("关闭确认"));
    present("DialogClose.qml", {{"remember", false},
        {"actions", QVariantList{button("exit", tr("退出程序")), button("tray", tr("最小化到托盘"), false)}}},
        QSize(360, 204), true);
}
CloseConfirmResult CloseConfirmDialog::decision() const { return {exitSelected_, value("remember").toBool()}; }
void CloseConfirmDialog::editField(const QString& field, const QVariant& argument) {
    if (field == "remember") setValue(field, argument.toBool());
}
void CloseConfirmDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "exit" || action == "tray") { exitSelected_ = action == "exit"; accept(); }
    else QmlDialog::handleAction(action, argument);
}
