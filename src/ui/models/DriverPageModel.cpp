#include "DriverPageModel.h"

#include <QStringList>

namespace {
QString compactDriverDetails(const QString& details) {
    QString dd32;
    QString dd64;
    QString infKeyboardService;
    QString infMouseService;
    QString infKeyboardDevice;
    QString infMouseDevice;
    QString hidDll;
    QString hidService;
    QString hidSys;
    QString hidRootDevice;
    QString hidChildDevice;
    QString ch552Device;
    QString usbConnection;
    QString usbFirmware;
    QString kernelIsolation;
    const QStringList lines = details.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QString::fromWCharArray(L"\u5185\u6838\u9694\u79bb:"))) kernelIsolation = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QStringLiteral("DD32.dll:"))) dd32 = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QStringLiteral("DD64.dll:"))) dd64 = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"INF \u952e\u76d8\u670d\u52a1:"))) infKeyboardService = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"INF \u9f20\u6807\u670d\u52a1:"))) infMouseService = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"INF \u952e\u76d8\u8bbe\u5907:"))) infKeyboardDevice = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"INF \u9f20\u6807\u8bbe\u5907:"))) infMouseDevice = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QStringLiteral("HID DLL:"))) hidDll = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"HID \u670d\u52a1:"))) hidService = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QStringLiteral("HID SYS:"))) hidSys = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"HID Root \u8bbe\u5907:"))) hidRootDevice = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"HID Child \u8bbe\u5907:"))) hidChildDevice = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"USB \u8bbe\u5907:"))) ch552Device = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"USB \u901a\u4fe1:"))) usbConnection = trimmed.section(QLatin1Char(':'), 1).trimmed();
        else if (trimmed.startsWith(QString::fromWCharArray(L"USB \u56fa\u4ef6:"))) usbFirmware = trimmed.section(QLatin1Char(':'), 1).trimmed();
    }

    QStringList sections;
    // 内核隔离是驱动失效的最常见根因，置顶展示。
    if (!kernelIsolation.isEmpty()) {
        sections.push_back(QString::fromWCharArray(L"\u5185\u6838\u9694\u79bb"));
        sections.push_back(kernelIsolation);
    }
    const auto pairOk = [](const QString& a, const QString& b, const QString& okText) {
        return !a.isEmpty() && !b.isEmpty() && a == okText && b == okText;
    };
    const QString found = QString::fromWCharArray(L"\u5df2\u627e\u5230");
    const QString installed = QString::fromWCharArray(L"\u5df2\u5b89\u88c5");
    const QString notInstalled = QString::fromWCharArray(L"\u672a\u5b89\u88c5");
    const QString pendingRestart = QString::fromWCharArray(L"\u7b49\u5f85\u91cd\u542f");
    const QString ok = QStringLiteral("OK");

    QStringList dd;
    if (!dd32.isEmpty() || !dd64.isEmpty()) {
        dd.push_back(QString::fromWCharArray(L"\u8fd0\u884c\u6587\u4ef6: ") +
            (pairOk(dd32, dd64, found) ? QString::fromWCharArray(L"\u5df2\u5c31\u7eea") : QString::fromWCharArray(L"\u4e0d\u5b8c\u6574")));
    }
    if (!infKeyboardService.isEmpty() || !infMouseService.isEmpty()) {
        dd.push_back(QString::fromWCharArray(L"INF \u670d\u52a1: ") +
            (pairOk(infKeyboardService, infMouseService, installed) ? QString::fromWCharArray(L"\u5df2\u5b89\u88c5") : QString::fromWCharArray(L"\u4e0d\u5b8c\u6574")));
    }
    if (!infKeyboardDevice.isEmpty() || !infMouseDevice.isEmpty()) {
        dd.push_back(QString::fromWCharArray(L"INF \u8bbe\u5907: ") +
            (pairOk(infKeyboardDevice, infMouseDevice, ok) ? QStringLiteral("OK") : QString::fromWCharArray(L"\u5f02\u5e38")));
    }

    QStringList hid;
    if (!hidDll.isEmpty()) {
        hid.push_back(QStringLiteral("DLL: ") + hidDll);
    }
    if (!hidService.isEmpty() || !hidSys.isEmpty()) {
        const bool hidServiceSysInstalled = hidService == installed && hidSys == installed;
        const bool hidServiceSysMissing = hidService == notInstalled && hidSys == notInstalled;
        const bool hidServicePendingRestart = hidService == pendingRestart && hidSys == notInstalled;
        hid.push_back(QString::fromWCharArray(L"\u670d\u52a1/SYS: ") +
            (hidServiceSysInstalled ? QString::fromWCharArray(L"\u5df2\u5b89\u88c5") :
                (hidServicePendingRestart ? QString::fromWCharArray(L"\u7b49\u5f85\u91cd\u542f") :
                    (hidServiceSysMissing ? QString::fromWCharArray(L"\u672a\u5b89\u88c5") : QString::fromWCharArray(L"\u4e0d\u5b8c\u6574")))));
    }
    if (!hidRootDevice.isEmpty() || !hidChildDevice.isEmpty()) {
        const bool hidDevicesOk = hidRootDevice == ok && hidChildDevice == ok;
        const bool hidDevicesMissing = hidRootDevice == notInstalled && hidChildDevice == notInstalled;
        const bool hidDevicesPendingRestart = hidRootDevice == pendingRestart && hidChildDevice == pendingRestart;
        hid.push_back(QString::fromWCharArray(L"\u8bbe\u5907: ") +
            (hidDevicesOk ? QStringLiteral("OK") :
                (hidDevicesPendingRestart ? QString::fromWCharArray(L"\u7b49\u5f85\u91cd\u542f") :
                    (hidDevicesMissing ? QString::fromWCharArray(L"\u672a\u5b89\u88c5") : QString::fromWCharArray(L"\u5f02\u5e38")))));
    }

    if (!dd.isEmpty()) {
        if (!sections.isEmpty()) sections.push_back(QString());
        sections.push_back(QStringLiteral("DD"));
        sections.append(dd);
    }
    if (!hid.isEmpty()) {
        if (!sections.isEmpty()) sections.push_back(QString());
        sections.push_back(QStringLiteral("HID"));
        sections.append(hid);
    }
    QStringList usb;
    if (!ch552Device.isEmpty()) usb.push_back(QString::fromWCharArray(L"\u8bbe\u5907: ") + ch552Device);
    if (!usbConnection.isEmpty()) usb.push_back(QString::fromWCharArray(L"\u901a\u4fe1: ") + usbConnection);
    if (!usbFirmware.isEmpty()) usb.push_back(QString::fromWCharArray(L"\u56fa\u4ef6: ") + usbFirmware);
    if (!usb.isEmpty()) {
        if (!sections.isEmpty()) sections.push_back(QString());
        sections.push_back(QStringLiteral("USB"));
        sections.append(usb);
    }
    return sections.isEmpty()
        ? QString::fromWCharArray(L"\u6682\u65e0\u8be6\u7ec6\u72b6\u6001")
        : sections.join(QLatin1Char('\n'));
}
}

DriverPageModel::DriverPageModel(QObject* parent) : PageModel(parent) {
    state_ = {{"mode", 0}, {"summary", tr("正在读取驱动状态")}, {"details", QString()},
        {"ddInstalled", false}, {"hidInstalled", false}, {"busy", false}};
}

void DriverPageModel::setDriverActions(bool ddInstalled, bool hidInstalled) {
    state_.insert("ddInstalled", ddInstalled);
    setValue("hidInstalled", hidInstalled);
}

void DriverPageModel::setDetailText(const QString& value) {
    setValue("details", compactDriverDetails(value));
}

void DriverPageModel::selectMode(int value) {
    if (value < 0 || value > 2) return;
    emit driverModeChanged(static_cast<DriverMode>(value));
}

void DriverPageModel::uninstallMode(int value) {
    if (value < 0 || value > 1) return;
    emit uninstallDriverRequested(static_cast<DriverMode>(value));
}
