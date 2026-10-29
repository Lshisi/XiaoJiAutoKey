#include "dialogs/GameProcessDialog.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <QShowEvent>
#include <set>
namespace {

QStringList RunningProcessNames() {
    QStringList names;
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return names;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (::Process32FirstW(snapshot, &entry)) {
        std::set<QString> seen;
        do {
            const QString name = QString::fromWCharArray(entry.szExeFile).toLower();
            if (!name.isEmpty() && name.size() <= 64 && seen.insert(name).second) {
                names.push_back(name);
            }
        } while (::Process32NextW(snapshot, &entry));
    }
    ::CloseHandle(snapshot);
    names.sort();
    return names;
}

}  // namespace


GameProcessDialog::GameProcessDialog(const QStringList& initialProcesses, QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(tr("游戏进程管理"));
    present("DialogProcesses.qml", {{"processes", initialProcesses}, {"runningProcesses", QStringList{}}, {"processName", QString()},
        {"actions", QVariantList{button("accept", tr("保存")), button("cancel", tr("取消"), false)}}}, QSize(420, 460));
}
QStringList GameProcessDialog::processes() const { return value("processes").toStringList(); }
void GameProcessDialog::addProcess(const QString& input) {
    const QString name = input.trimmed().toLower();
    if (name.isEmpty() || name.size() > 260) return;
    auto names = processes();
    if (names.contains(name)) return;
    names << name;
    setValue("processes", names);
    setValue("processName", QString());
}
void GameProcessDialog::showEvent(QShowEvent* event) {
    setValue("runningProcesses", RunningProcessNames());
    QmlDialog::showEvent(event);
}
void GameProcessDialog::editField(const QString& field, const QVariant& argument) {
    if (field == "processName") setValue(field, argument.toString());
}
void GameProcessDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "add") addProcess(value("processName").toString());
    else if (action == "selectProcess") addProcess(argument.toString());
    else if (action == "refreshProcesses") setValue("runningProcesses", RunningProcessNames());
    else if (action == "remove") {
        auto names = processes();
        const int index = argument.toInt();
        if (index < 0 || index >= names.size()) return;
        names.removeAt(index);
        setValue("processes", names);
    } else if (action == "accept") accept();
    else QmlDialog::handleAction(action, argument);
}
