#include "dialogs/GameProcessDialog.h"

#include "ForegroundWatcher.h"
#include "ui/models/GameProcessData.h"

#include <Windows.h>
#include <dwmapi.h>
#include <QShowEvent>
#include <QSet>
#include <algorithm>

namespace {

struct WindowCollection {
    QVariantList windows;
    QSet<QString> seen;
    quint64 revision;
};

BOOL CALLBACK CollectWindow(HWND window, LPARAM parameter) {
    if (!::IsWindowVisible(window) || ::GetWindowTextLengthW(window) <= 0
        || (::GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) return TRUE;
    DWORD cloaked = 0;
    if (SUCCEEDED(::DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
        && cloaked != 0) return TRUE;
    DWORD processId = 0;
    ::GetWindowThreadProcessId(window, &processId);
    if (processId == 0 || processId == ::GetCurrentProcessId()) return TRUE;

    wchar_t titleBuffer[1024]{};
    const int length = ::GetWindowTextW(window, titleBuffer, 1024);
    const QString title = QString::fromWCharArray(titleBuffer, length).trimmed();
    const QString name = QString::fromStdWString(QueryForegroundProcessName(window));
    if (title.isEmpty() || name.isEmpty()) return TRUE;
    wchar_t className[256]{};
    ::GetClassNameW(window, className, 256);
    if (::lstrcmpW(className, L"Progman") == 0 || ::lstrcmpW(className, L"WorkerW") == 0
        || ::lstrcmpW(className, L"Shell_TrayWnd") == 0) return TRUE;

    auto& collection = *reinterpret_cast<WindowCollection*>(parameter);
    const QString key = name + QChar(u'\n') + title;
    if (collection.seen.contains(key)) return TRUE;
    collection.seen.insert(key);
    collection.windows.append(QVariantMap{{"title", title}, {"processName", name},
        {"icon", QStringLiteral("image://app-icons/windows/%1/%2/%3")
            .arg(static_cast<qulonglong>(reinterpret_cast<quintptr>(window)))
            .arg(processId).arg(collection.revision)}});
    return TRUE;
}

QVariantList RunningWindows(quint64 revision) {
    WindowCollection collection{{}, {}, revision};
    ::EnumWindows(&CollectWindow, reinterpret_cast<LPARAM>(&collection));
    std::stable_sort(collection.windows.begin(), collection.windows.end(), [](const QVariant& left, const QVariant& right) {
        const auto first = left.toMap();
        const auto second = right.toMap();
        const bool firstGame = first.value("processName").toString().startsWith(QStringLiteral("jx3client"));
        const bool secondGame = second.value("processName").toString().startsWith(QStringLiteral("jx3client"));
        if (firstGame != secondGame) return firstGame;
        return QString::localeAwareCompare(first.value("title").toString(), second.value("title").toString()) < 0;
    });
    return collection.windows;
}

}  // namespace

GameProcessDialog::GameProcessDialog(const QStringList& initialProcesses, QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(tr("管理进程"));
    present("DialogProcesses.qml", {{"processData", GameProcessData::create(initialProcesses)},
        {"actions", QVariantList{button("accept", tr("保存")), button("cancel", tr("取消"), false)}}}, QSize(480, 440), true);
}

QStringList GameProcessDialog::processes() const {
    return value("processData").toMap().value("processes").toStringList();
}

void GameProcessDialog::refreshWindows() {
    auto data = value("processData").toMap();
    GameProcessData::refresh(data, RunningWindows(++iconRevision_));
    setValue("processData", data);
}

void GameProcessDialog::showEvent(QShowEvent* event) {
    refreshWindows();
    QmlDialog::showEvent(event);
}

void GameProcessDialog::editField(const QString& field, const QVariant& argument) {
    if (field == "processName") {
        auto data = value("processData").toMap();
        data.insert("processName", argument.toString());
        data.insert("status", QString());
        setValue("processData", data);
    }
}

void GameProcessDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "refreshWindows") {
        refreshWindows();
    } else if (action == "add" || action == "selectProcess" || action == "remove") {
        auto data = value("processData").toMap();
        if (action == "remove") GameProcessData::remove(data, argument.toInt());
        else GameProcessData::add(data, action == "add" ? data.value("processName").toString() : argument.toString());
        setValue("processData", data);
    } else if (action == "accept") {
        accept();
    } else {
        QmlDialog::handleAction(action, argument);
    }
}
