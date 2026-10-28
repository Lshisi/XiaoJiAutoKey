#include "dialogs/ConfigNameDialog.h"
ConfigNameDialog::ConfigNameDialog(const QString& title, const QString& label,
    const QString& initialName, QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(title);
    present("DialogName.qml", {{"message", label}, {"name", initialName},
        {"actions", QVariantList{button("accept", tr("确定")), button("cancel", tr("取消"), false)}}},
        QSize(440, 260), true);
    editField("name", initialName);
}
QString ConfigNameDialog::nameText() const { return value("name").toString().trimmed(); }
bool ConfigNameDialog::getName(QWidget* parent, const QString& title, const QString& label,
    const QString& initialName, QString& name) {
    ConfigNameDialog dialog(title, label, initialName, parent);
    if (dialog.exec() != QDialog::Accepted) return false;
    name = dialog.nameText();
    return !name.isEmpty();
}
void ConfigNameDialog::editField(const QString& field, const QVariant& argument) {
    if (field != "name") return;
    setValue("name", argument.toString());
    auto actions = value("actions").toList();
    auto confirm = actions[0].toMap();
    confirm.insert("enabled", !nameText().isEmpty());
    actions[0] = confirm;
    setValue("actions", actions);
}
void ConfigNameDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "accept") { if (!nameText().isEmpty()) accept(); }
    else QmlDialog::handleAction(action, argument);
}
