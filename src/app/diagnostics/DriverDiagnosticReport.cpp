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
    return mode == DriverMode::Hid ? QStringLiteral("HID") : QStringLiteral("DD");
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
    if (note.find(L"DD is not ready.") != std::wstring::npos) {
        return QString::fromWCharArray(L"当前选中驱动未就绪，输入模拟会失败。");
    }
    if (note.find(L"No HID runtime or DD runtime") != std::wstring::npos) {
        return QString::fromWCharArray(L"未找到匹配当前进程架构的 HID/DD 运行库。");
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
        << QString::fromWCharArray(L"当前模式: ") + modeText(input.mode)
        << QString::fromWCharArray(L"驱动就绪: ") + boolText(input.driverReady)
        << QString::fromWCharArray(L"主动卸载后停用自动安装: ") + boolText(input.uninstalledByUser);

    QStringList runtimeLines;
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"驱动目录"), runtime.driverRootExists);
    appendRuntimeLine(&runtimeLines, QStringLiteral("DD32.dll"), runtime.legacy32Exists);
    appendRuntimeLine(&runtimeLines, QStringLiteral("DD64.dll"), runtime.legacy64Exists);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"HID 运行库"), runtime.hidRuntimeExists);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"HID 内置安装包"), runtime.hidEmbeddedDriverExists);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"HID 服务"), runtime.hidServiceInstalled);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"HID 服务等待重启删除"), runtime.hidServicePendingDelete);
    appendRuntimeLine(&runtimeLines, QStringLiteral("ddhid63340.sys"), runtime.hidSysInstalled);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"HID DriverStore"), runtime.hidDriverStorePresent);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"HID 根设备"), runtime.hidRootDeviceOk);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"HID 子设备"), runtime.hidChildDeviceOk);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 键盘服务"), runtime.infKeyboardServiceInstalled);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 鼠标服务"), runtime.infMouseServiceInstalled);
    appendRuntimeLine(&runtimeLines, QStringLiteral("dd.key.96688.sys"), runtime.infKeyboardSysInstalled);
    appendRuntimeLine(&runtimeLines, QStringLiteral("dd.mou.96688.sys"), runtime.infMouseSysInstalled);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 内置安装包"), runtime.embeddedInfVariant0Exists
        || runtime.embeddedInfVariant1Exists);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 键盘设备"), runtime.infKeyboardDeviceStarted
        && runtime.infKeyboardDeviceBound);
    appendRuntimeLine(&runtimeLines, QString::fromWCharArray(L"DD 鼠标设备"), runtime.infMouseDeviceStarted
        && runtime.infMouseDeviceBound);
    lines << QString::fromWCharArray(L"［运行库与设备］");
    lines << runtimeLines;
    if (!runtime.notes.empty()) {
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
