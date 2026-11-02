#include "dialogs/ConfigNoteDialog.h"

ConfigNoteDialog::ConfigNoteDialog(const QString& name, const QString& note, QWidget* parent)
    : QmlDialog(parent) {
    setWindowTitle(tr("配置备注"));
    present("DialogConfigNote.qml", {{"name", name}, {"note", note},
        {"actions", QVariantList{button("accept", tr("保存")), button("cancel", tr("取消"), false)}}},
        QSize(420, 320), true);
}

QString ConfigNoteDialog::noteText() const {
    return value("note").toString().trimmed();
}

void ConfigNoteDialog::editField(const QString& field, const QVariant& argument) {
    if (field == "note") setValue(field, argument.toString());
}

void ConfigNoteDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "accept") accept();
    else QmlDialog::handleAction(action, argument);
}
