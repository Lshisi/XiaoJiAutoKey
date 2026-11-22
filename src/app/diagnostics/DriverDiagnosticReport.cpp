#include "diagnostics/DriverDiagnosticReport.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <QDateTime>
#include <QRegularExpression>
#include <QSysInfo>

#include <algorithm>

namespace {
QString boolText(bool value) {
    return value ? QString::fromWCharArray(L"是") : QString::fromWCharArray(L"否");
}

QString modeText(DriverMode mode) {
    if (mode == DriverMode::Usb) return QStringLiteral("USB");
    return mode == DriverMode::Xiaoji ? QObject::tr("小叽") : QStringLiteral("DD");
}

QString nativeArchitecture() {
    SYSTEM_INFO info{};
    ::GetNativeSystemInfo(&info);
    switch (info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64: return QStringLiteral("x64");
    case PROCESSOR_ARCHITECTURE_ARM64: return QStringLiteral("ARM64");
    case PROCESSOR_ARCHITECTURE_INTEL: return QStringLiteral("x86");
    default: return QString::fromWCharArray(L"未知");
    }
}

QString redactPaths(QString text) {
    if (text.isEmpty()) return text;
    static const QRegularExpression pathPattern(
        QStringLiteral(R"((?:[A-Za-z]:\\|\\\\)[^;\r\n]+)"));
    text.replace(pathPattern, QString::fromWCharArray(L"［路径已省略］"));
    text = text.left(1200);
    return text;
}

void appendRuntimeLine(QStringList* lines, const QString& name, bool value) {
    lines->push_back(name + QStringLiteral(": ")
        + (value ? QString::fromWCharArray(L"已找到／正常")
            : QString::fromWCharArray(L"未找到／异常")));
}

QString noteText(const std::wstring& note) {
    if (note.find(L"Selected input driver is not ready.") != std::wstring::npos) {
        return QString::fromWCharArray(L"当前选中驱动未就绪，输入模拟会失败。");
    }
    if (note.find(L"No DD runtime") != std::wstring::npos) {
        return QString::fromWCharArray(L"未找到匹配当前进程架构的 DD 运行库。");
    }
    if (note.find(L"No embedded driver package") != std::wstring::npos) {
        return QString::fromWCharArray(L"未找到内置驱动安装包。");
    }
    if (note.find(L"DD INF device instances") != std::wstring::npos) {
        return QString::fromWCharArray(L"DD 键盘/鼠标设备未完全启动或绑定服务。");
    }
    return redactPaths(QString::fromStdWString(note));
}
}

DriverDiagnosticReport BuildDriverDiagnosticReport(const DriverDiagnosticInput& input) {
    DriverDiagnosticReport report;
    report.input = input;
    report.generatedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    report.applicationVersion = QString::fromLatin1(APP_VERSION);
    report.operatingSystem = QSysInfo::prettyProductName();
    report.processArchitecture =
#if defined(_WIN64)
        QStringLiteral("x64");
#else
        QStringLiteral("x86");
#endif
    report.nativeArchitecture = nativeArchitecture();
    return report;
}

QString FormatDriverDiagnosticReport(const DriverDiagnosticReport& report) {
    const auto& input = report.input;
    const auto& runtime = input.runtime;
    QStringList lines;
    lines << QString::fromWCharArray(L"［小叽按键诊断信息］")
        << QString::fromWCharArray(L"生成时间: ") + report.generatedAt
        << QString()
        << QString::fromWCharArray(L"［基础环境］")
        << QStringLiteral("软件版本: V") + report.applicationVersion
        << QString::fromWCharArray(L"操作系统: ") + report.operatingSystem
        << QString::fromWCharArray(L"系统架构: ") + report.nativeArchitecture
        << QString::fromWCharArray(L"进程架构: ") + report.processArchitecture
        << QString()
        << QString::fromWCharArray(L"［驱动状态］")
        << QString::fromWCharArray(L"当前模式: ") + (input.providerName.isEmpty() ? (input.providerId.isEmpty() ? modeText(input.mode) : input.providerId) : input.providerName)
        << QString::fromWCharArray(L"驱动就绪: ") + boolText(input.driverReady)
        << QString::fromWCharArray(L"主动卸载后停用自动安装: ") + boolText(input.uninstalledByUser);
    if (!input.providerId.isEmpty()) lines << QObject::tr("插件提供方: ") + input.providerId;

    QStringList runtimeLines;
    const bool xiaoji = input.providerId == QStringLiteral("xiaoji.driver.xiaoji/default")
        || (input.providerId.isEmpty() && input.mode == DriverMode::Xiaoji);
    const bool dd = input.providerId == QStringLiteral("xiaoji.driver.dd/default")
        || (input.providerId.isEmpty() && input.mode == DriverMode::Legacy);
    if (xiaoji) {
        appendRuntimeLine(&runtimeLines, QObject::tr("小叽键盘设备"), runtime.xiaojiKeyboardReady);
        appendRuntimeLine(&runtimeLines, QObject::tr("小叽鼠标设备"), runtime.xiaojiMouseReady);
        appendRuntimeLine(&runtimeLines, QObject::tr("小叽管理工具"), runtime.xiaojiManagerExists);
        appendRuntimeLine(&runtimeLines, QObject::tr("小叽本机签名组件"), runtime.xiaojiInstallerExists);
        appendRuntimeLine(&runtimeLines, QObject::tr("小叽扩展输入配置"), runtime.xiaojiProfileReady);
        appendRuntimeLine(&runtimeLines, QObject::tr("小叽设备配置记录"), runtime.xiaojiConfigurationPresent);
        appendRuntimeLine(&runtimeLines, QObject::tr("小叽绑定包或待清理包记录"), runtime.xiaojiBoundPackagePresent);
    }
    if (dd) {
        appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"驱动目录"), runtime.driverRootExists);
        appendRuntimeLine(&runtimeLines, QStringLiteral("DD32.dll"), runtime.legacy32Exists);
        appendRuntimeLine(&runtimeLines, QStringLiteral("DD64.dll"), runtime.legacy64Exists);
    }
    const bool retiredHidPresent = runtime.hidDevicesPresent || runtime.hidServiceInstalled
        || runtime.hidServicePendingDelete || runtime.hidSysInstalled || runtime.hidDriverStorePresent;
    if (retiredHidPresent) {
        runtimeLines << QObject::tr("旧 HID: 检测到安装残留，仅提供卸载入口");
        runtimeLines << QObject::tr("旧 HID 服务存在: ") + boolText(runtime.hidServiceInstalled)
            << QObject::tr("旧 HID 服务待删除: ") + boolText(runtime.hidServicePendingDelete)
            << QObject::tr("旧 HID 设备存在: ") + boolText(runtime.hidDevicesPresent)
            << QObject::tr("旧 HID SYS 存在: ") + boolText(runtime.hidSysInstalled)
            << QObject::tr("旧 HID 驱动包存在: ") + boolText(runtime.hidDriverStorePresent);
    }
    if (dd) {
        appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 键盘服务"), runtime.infKeyboardServiceInstalled);
        appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 鼠标服务"), runtime.infMouseServiceInstalled);
        appendRuntimeLine(&runtimeLines, QStringLiteral("dd.key.96688.sys"), runtime.infKeyboardSysInstalled);
        appendRuntimeLine(&runtimeLines, QStringLiteral("dd.mou.96688.sys"), runtime.infMouseSysInstalled);
        appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 插件安装包"), runtime.embeddedInfVariant0Exists
            || runtime.embeddedInfVariant1Exists);
        appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 键盘设备"), runtime.infKeyboardDeviceStarted
            && runtime.infKeyboardDeviceBound);
        appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 鼠标设备"), runtime.infMouseDeviceStarted
            && runtime.infMouseDeviceBound);
    }
    lines << QObject::tr("［系统运行库与设备］");
    lines << runtimeLines;
    if (!input.pluginDetails.isEmpty()) lines << redactPaths(input.pluginDetails);
    if (dd && !runtime.notes.empty()) {
        lines << QString::fromWCharArray(L"检查提示:");
        for (const auto& note : runtime.notes) {
            lines << QStringLiteral("- ") + noteText(note);
        }
    }

    lines << QString() << QString::fromWCharArray(L"［最近错误］");
    if (runtime.lastError.empty()) {
        lines << QString::fromWCharArray(L"无");
    } else {
        lines << redactPaths(QString::fromStdWString(runtime.lastError));
    }

    lines << QString() << QString::fromWCharArray(L"［安全环境］");
    if (!input.guardsInspected) {
        lines << QString::fromWCharArray(L"未检测（当前场景无需读取安全事件日志）");
    } else {
        const auto& guards = input.guards;
        lines << QString::fromWCharArray(L"内存完整性运行中: ") + boolText(guards.memoryIntegrityRunning)
            << QString::fromWCharArray(L"内存完整性已配置: ") + boolText(guards.memoryIntegrityConfigured)
            << QString::fromWCharArray(L"易受攻击驱动阻止列表: ") + boolText(guards.vulnerableBlocklistEnabled);
        QString smartApp;
        if (guards.smartAppControlState == 1) smartApp = QString::fromWCharArray(L"开启");
        else if (guards.smartAppControlState == 2) smartApp = QString::fromWCharArray(L"评估模式");
        else smartApp = QString::fromWCharArray(L"关闭");
        lines << QString::fromWCharArray(L"智能应用控制: ") + smartApp;
        if (guards.recentBlocks.empty()) {
            lines << (guards.recentBlocksIncomplete
                ? QString::fromWCharArray(L"近 7 天相关拦截记录: 暂未读到（日志未完整扫描）")
                : QString::fromWCharArray(L"近 7 天相关拦截记录: 无"));
        } else {
            lines << QString::fromWCharArray(L"近 7 天相关拦截记录:");
            for (const auto& block : guards.recentBlocks) {
                lines << QStringLiteral("- %1 · %2 · EventID %3")
                    .arg(QString::fromStdWString(block.timeText),
                        QString::fromStdWString(block.fileName))
                    .arg(block.eventId);
            }
        }
        if (guards.recentBlocksIncomplete) {
            lines << QString::fromWCharArray(L"事件日志读取受限或已达扫描上限，仅展示已读取的记录；可在 Windows 事件查看器中核对。");
        }
    }

    const auto& state = input.state;
    lines << QString() << QString::fromWCharArray(L"［运行配置］")
        << QString::fromWCharArray(L"按键配置启用: ") + boolText(state.keyProfileEnabled)
        << QString::fromWCharArray(L"循环配置启用: ") + boolText(state.macroEnabled)
        << QString::fromWCharArray(L"按键引擎运行中: ") + boolText(state.keyEngineRunning)
        << QString::fromWCharArray(L"循环引擎运行中: ") + boolText(state.macroEngineRunning)
        << QString::fromWCharArray(L"仅游戏前台生效: ") + boolText(state.gameGateEnabled);
    if (state.gameGateEnabled) {
        lines << QString::fromWCharArray(L"当前因前台限制暂停: ") + boolText(state.gameGatePaused);
    }
    lines << QString()
        << QString::fromWCharArray(L"说明: 本报告不包含个人路径、剪贴板内容和完整配置；仅由用户主动点击复制。");
    return lines.join(QLatin1Char('\n'));
}
