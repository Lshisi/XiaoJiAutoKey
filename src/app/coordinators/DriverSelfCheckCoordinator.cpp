#include "coordinators/DriverSelfCheckCoordinator.h"

#include "dialogs/AppMessageDialog.h"

#include <QThread>
#include <QTimer>

#include <Windows.h>
#include <shellapi.h>

#include <exception>
#include <memory>
#include <utility>

namespace {

using Action = DriverSelfCheckDialog::Action;
using Conclusion = DriverSelfCheckDialog::Conclusion;

QVector<QPair<Action, QString>> closeActions() {
    return {{Action::OpenDiagnostics, QString::fromWCharArray(L"\u8bca\u65ad\u4fe1\u606f")},
        {Action::None, QString::fromWCharArray(L"\u5173\u95ed")}};
}

QString modeText(DriverMode mode) {
    if (mode == DriverMode::Usb) return QStringLiteral("USB");
    return mode == DriverMode::Hid ? QStringLiteral("HID") : QStringLiteral("DD");
}

QString guardItemsHtml(const CodeIntegrityInspector::GuardStatus& guards) {
    QStringList items;
    if (guards.memoryIntegrityRunning) {
        items << QString::fromWCharArray(L"\u5185\u5b58\u5b8c\u6574\u6027\uff08\u5185\u6838\u9694\u79bb\uff09\uff1a\u8fd0\u884c\u4e2d");
    } else if (guards.memoryIntegrityConfigured) {
        items << QString::fromWCharArray(L"\u5185\u5b58\u5b8c\u6574\u6027\uff08\u5185\u6838\u9694\u79bb\uff09\uff1a\u5df2\u5f00\u542f\uff08\u7b49\u5f85\u91cd\u542f\u751f\u6548\uff09");
    }
    if (guards.vulnerableBlocklistEnabled) {
        items << QString::fromWCharArray(L"\u6613\u53d7\u653b\u51fb\u7684\u9a71\u52a8\u7a0b\u5e8f\u963b\u6b62\u5217\u8868\uff1a\u5f00\u542f");
    }
    if (guards.smartAppControlState == 1) {
        items << QString::fromWCharArray(L"\u667a\u80fd\u5e94\u7528\u63a7\u5236\uff1a\u5f00\u542f");
    } else if (guards.smartAppControlState == 2) {
        items << QString::fromWCharArray(L"\u667a\u80fd\u5e94\u7528\u63a7\u5236\uff1a\u8bc4\u4f30\u6a21\u5f0f");
    }
    if (items.isEmpty()) return {};
    return QStringLiteral("<ul><li>") + items.join(QStringLiteral("</li><li>"))
        + QStringLiteral("</li></ul>");
}

}  // namespace

DriverSelfCheckCoordinator::DriverSelfCheckCoordinator(QObject* parent) : QObject(parent) {}

void DriverSelfCheckCoordinator::bind(Context context) {
    context_ = std::move(context);
}

void DriverSelfCheckCoordinator::updateDialogOpenState() {
    const bool open = dialog_ || diagnosticDialog_ || diagnosticOpenPending_;
    if (context_.setDialogOpen) context_.setDialogOpen(open);
    if (!open) emit dialogsClosed();
}

void DriverSelfCheckCoordinator::open() {
    if (!context_.parentWidget || !context_.config) return;
    if (dialog_) {
        dialog_->showNormal();
        dialog_->raise();
        dialog_->activateWindow();
        return;
    }
    if (context_.setDialogOpen) context_.setDialogOpen(true);

    auto* dialog = new DriverSelfCheckDialog(context_.parentWidget);
    dialog_ = dialog;
    connect(dialog, &QDialog::finished, this, [this, dialog]() {
        if (dialog_ == dialog) dialog_.clear();
        updateDialogOpenState();
        dialog->deleteLater();
    });
    connect(dialog, &DriverSelfCheckDialog::actionRequested, this, [this, dialog](int action) {
        if (dialog_ == dialog) handleAction(dialog, static_cast<Action>(action));
    }, Qt::QueuedConnection);

    runDiagnosis(dialog);
    dialog->open();
}

void DriverSelfCheckCoordinator::openDiagnostic() {
    if (!context_.parentWidget || !context_.config) return;
    if (diagnosticDialog_) {
        diagnosticDialog_->showNormal();
        diagnosticDialog_->raise();
        diagnosticDialog_->activateWindow();
        return;
    }
    if (context_.setDialogOpen) context_.setDialogOpen(true);

    auto* dialog = new DriverDiagnosticDialog(context_.parentWidget);
    diagnosticDialog_ = dialog;
    connect(dialog, &QDialog::finished, this, [this, dialog]() {
        if (diagnosticDialog_ == dialog) diagnosticDialog_.clear();
        updateDialogOpenState();
        dialog->deleteLater();
    });

    runDiagnosticReport(dialog);
    dialog->open();
}

std::shared_ptr<DriverSelfCheckCoordinator::Snapshot> DriverSelfCheckCoordinator::collectSnapshot() const {
    // Only capture live application state here; device and filesystem scans run in the worker.
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->mode = context_.config->driverMode;
    snapshot->uninstalledByUser = context_.config->IsDriverUninstalledByUser(snapshot->mode);
    snapshot->driverReady = context_.driverReady ? context_.driverReady() : false;
    snapshot->status.lastError = context_.driverLastError ? context_.driverLastError() : std::wstring{};
    snapshot->runtimeState = context_.inspectRuntimeState ? context_.inspectRuntimeState()
                                                          : DriverRuntimeDiagnosticState{};
    return snapshot;
}

void DriverSelfCheckCoordinator::inspectSnapshot(std::shared_ptr<Snapshot> snapshot, bool fullReport,
    std::function<void(const Snapshot&)> completed) {
    auto* thread = QThread::create([snapshot, fullReport, inspectRuntime = context_.inspectRuntime]() {
        try {
            if (inspectRuntime)
                snapshot->status = inspectRuntime(snapshot->driverReady, snapshot->status.lastError);
            const bool hid = snapshot->mode == DriverMode::Hid;
            const bool runtimeExists = hid ? snapshot->status.hidRuntimeExists : snapshot->status.legacyRuntimeExists;
            if (fullReport || (snapshot->mode != DriverMode::Usb && runtimeExists && !snapshot->driverReady
                && !snapshot->uninstalledByUser && !(hid && snapshot->status.hidServicePendingDelete))) {
                snapshot->guards = CodeIntegrityInspector::Inspect(true);
                snapshot->guardsInspected = true;
            }
        } catch (const std::exception&) {
            snapshot->error = tr("无法完成系统状态读取，请稍后重新检查。");
        }
    });
    // The worker owns only copied data and can finish safely after the window closes.
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [snapshot, completed = std::move(completed)]() {
        completed(*snapshot);
    }, Qt::QueuedConnection);
    thread->start();
}

void DriverSelfCheckCoordinator::runDiagnosis(DriverSelfCheckDialog* dialog) {
    dialog->beginCheck();
    const QPointer<DriverSelfCheckDialog> dialogPtr(dialog);
    inspectSnapshot(collectSnapshot(), false, [this, dialogPtr](const Snapshot& snapshot) {
        if (!dialogPtr || dialog_ != dialogPtr) return;
        if (!snapshot.error.isEmpty()) {
            Conclusion conclusion;
            conclusion.severity = DriverSelfCheckDialog::Severity::Warning;
            conclusion.heading = tr("驱动自检未完成");
            conclusion.bodyHtml = snapshot.error.toHtmlEscaped();
            conclusion.actions = closeActions();
            dialogPtr->setConclusion(conclusion);
            return;
        }
        dialogPtr->setConclusion(buildConclusion(snapshot));
    });
}

void DriverSelfCheckCoordinator::runDiagnosticReport(DriverDiagnosticDialog* dialog) {
    dialog->beginReport();
    const QPointer<DriverDiagnosticDialog> dialogPtr(dialog);
    inspectSnapshot(collectSnapshot(), true, [this, dialogPtr](const Snapshot& snapshot) {
        if (!dialogPtr || diagnosticDialog_ != dialogPtr) return;
        if (!snapshot.error.isEmpty()) {
            dialogPtr->setReport(snapshot.error);
            return;
        }
        DriverDiagnosticInput input;
        input.mode = snapshot.mode;
        input.driverReady = snapshot.driverReady;
        input.uninstalledByUser = snapshot.uninstalledByUser;
        input.guardsInspected = snapshot.guardsInspected;
        input.runtime = snapshot.status;
        input.guards = snapshot.guards;
        input.state = snapshot.runtimeState;
        dialogPtr->setReport(FormatDriverDiagnosticReport(BuildDriverDiagnosticReport(input)));
    });
}

Conclusion DriverSelfCheckCoordinator::buildConclusion(const Snapshot& snapshot) const {
    Conclusion conclusion;
    conclusion.actions = closeActions();
    const auto& status = snapshot.status;
    const QString name = modeText(snapshot.mode);

    if (snapshot.mode == DriverMode::Usb) {
        conclusion.severity = snapshot.driverReady
            ? DriverSelfCheckDialog::Severity::Ok : DriverSelfCheckDialog::Severity::Danger;
        conclusion.heading = snapshot.driverReady ? tr("USB 设备已就绪") : tr("USB 设备连接或通信未就绪");
        QStringList body;
        body << (snapshot.driverReady ? tr("当前 USB 设备已连接，通信已就绪。")
            : tr("请确认 CH552 设备已连接，并检查 USB 线缆、端口及设备固件后重新自检。"));
        if (!snapshot.driverReady && !status.lastError.empty()) {
            body << tr("USB 错误信息：") + QString::fromStdWString(status.lastError).toHtmlEscaped();
        }
        conclusion.bodyHtml = body.join(QStringLiteral("<br><br>"));
        return conclusion;
    }

    const bool hid = snapshot.mode == DriverMode::Hid;
    const bool runtimeExists = hid ? status.hidRuntimeExists : status.legacyRuntimeExists;
    const QString runtimePath = QString::fromStdWString(hid ? status.hidRuntimePath : status.legacyRuntimePath);
    // Missing files take precedence over security settings and historical events.
    if (!runtimeExists) {
        conclusion.severity = snapshot.driverReady
            ? DriverSelfCheckDialog::Severity::Warning : DriverSelfCheckDialog::Severity::Danger;
        conclusion.heading = status.driverRootExists ? tr("%1 运行文件缺失").arg(name) : tr("驱动目录缺失");
        QStringList body;
        if (!status.driverRootExists) {
            body << tr("未找到 exe 同级的 driver 目录：<br>%1")
                .arg(QString::fromStdWString(status.driverRootPath).toHtmlEscaped());
        }
        body << tr("当前 %1 模式所需的运行文件未找到：<br>%2")
            .arg(name, runtimePath.toHtmlEscaped());
        body << tr("请从完整发布包中恢复 driver 目录，或重新下载并完整解压发布包，"
                   "确保 driver 目录与 XiaoJiAutoKey.exe 位于同一目录，再重新启动程序。");
        body << tr("重启电脑或重新安装系统驱动无法补回这些运行文件，无需为此修改 Windows 安全设置。");
        if (snapshot.driverReady) {
            body << tr("当前已加载的驱动仍处于就绪状态，但运行文件缺失会影响下次启动。");
        }
        if (snapshot.uninstalledByUser) {
            body << tr("当前模式的自动安装已因主动卸载而停用。恢复文件后，"
                       "还需在驱动管理页点击“安装 %1”才能再次使用。").arg(name);
        }
        conclusion.bodyHtml = body.join(QStringLiteral("<br><br>"));
        return conclusion;
    }

    if (snapshot.driverReady) {
        conclusion.severity = DriverSelfCheckDialog::Severity::Ok;
        conclusion.heading = tr("驱动已就绪");
        conclusion.bodyHtml = tr("当前模式：%1，运行文件已找到，驱动已加载并处于就绪状态。").arg(name);
        return conclusion;
    }

    if (snapshot.uninstalledByUser) {
        conclusion.severity = DriverSelfCheckDialog::Severity::Warning;
        conclusion.heading = tr("当前 %1 已停用").arg(name);
        conclusion.bodyHtml = tr("你已主动发起 %1 驱动卸载，程序保留了当前模式，并停用了自动安装。"
                                 "<br><br>需要再次使用时，请在驱动管理页点击“安装 %1”；"
                                 "也可以自行选择其他驱动模式。").arg(name);
        if (hid && status.hidServicePendingDelete)
            conclusion.bodyHtml += tr("<br><br>Windows 仍在等待删除 HID 驱动服务，请先重启电脑完成卸载。");
        return conclusion;
    }

    conclusion.severity = DriverSelfCheckDialog::Severity::Danger;
    if (hid && status.hidServicePendingDelete) {
        conclusion.heading = tr("HID 驱动卸载尚未完成");
        conclusion.bodyHtml = tr("Windows 已将 HID 驱动服务标记为待删除。"
                                 "请先重启电脑，让系统完成卸载，再到驱动管理页点击“安装/修复 HID”。");
        return conclusion;
    }

    QStringList missingComponents;
    if (hid) {
        if (!status.hidServiceInstalled) missingComponents << tr("HID 驱动服务");
        if (!status.hidSysInstalled) missingComponents << QStringLiteral("ddhid63340.sys");
    } else {
        if (!status.infKeyboardServiceInstalled) missingComponents << tr("DD 键盘驱动服务");
        if (!status.infMouseServiceInstalled) missingComponents << tr("DD 鼠标驱动服务");
        if (!status.infKeyboardSysInstalled) missingComponents << QStringLiteral("dd.key.96688.sys");
        if (!status.infMouseSysInstalled) missingComponents << QStringLiteral("dd.mou.96688.sys");
    }
    const bool devicesReady = hid ? (status.hidRootDeviceOk && status.hidChildDeviceOk)
        : (status.infKeyboardDeviceStarted && status.infMouseDeviceStarted
            && status.infKeyboardDeviceBound && status.infMouseDeviceBound);
    QStringList body;
    if (!missingComponents.isEmpty()) {
        conclusion.heading = tr("%1 驱动安装不完整").arg(name);
        body << tr("运行文件已找到，但以下系统安装组件未找到：<br>%1")
            .arg(missingComponents.join(QStringLiteral("<br>")));
        const bool embeddedDriverExists = hid ? status.hidEmbeddedDriverExists
            : (status.embeddedInfVariant0Exists || status.embeddedInfVariant1Exists);
        if (!embeddedDriverExists) {
            body << tr("当前程序的内置驱动安装资源也不可用，请重新下载完整发布包后再安装。");
        } else {
            body << tr("请到驱动管理页点击“安装/修复 %1”，由你主动发起安装。").arg(name);
        }
    } else if (!devicesReady) {
        conclusion.heading = tr("%1 驱动设备未就绪").arg(name);
        body << tr("运行文件、驱动服务和 SYS 文件已找到，但以下设备未就绪：");
        if (hid) {
            if (!status.hidRootDeviceOk) body << tr("HID 根设备：未就绪");
            if (!status.hidChildDeviceOk) body << tr("HID 子设备：未就绪");
        } else {
            if (!status.infKeyboardDeviceStarted || !status.infKeyboardDeviceBound)
                body << tr("DD 键盘设备：未启动或未绑定驱动服务");
            if (!status.infMouseDeviceStarted || !status.infMouseDeviceBound)
                body << tr("DD 鼠标设备：未启动或未绑定驱动服务");
        }
        body << tr("请先重新启动程序；若刚安装或卸载过驱动，可重启电脑后重新自检。");
    } else {
        conclusion.heading = tr("%1 驱动加载失败").arg(name);
        body << tr("运行文件、系统安装组件和设备状态均已检查到，但当前驱动仍未就绪，"
                   "暂时无法确定本次加载失败的原因。请重新启动程序后再次自检。");
    }
    if (!status.lastError.empty()) {
        body << tr("当前 %1 模式的错误信息：<br>").arg(name)
            + QString::fromStdWString(status.lastError).toHtmlEscaped();
    }

    QStringList records;
    for (const auto& block : snapshot.guards.recentBlocks) {
        const QString fileName = QString::fromStdWString(block.fileName);
        const bool matchesMode = hid
            ? fileName.compare(QStringLiteral("ddhid63340.sys"), Qt::CaseInsensitive) == 0
            : (fileName.compare(QStringLiteral("dd.key.96688.sys"), Qt::CaseInsensitive) == 0
                || fileName.compare(QStringLiteral("dd.mou.96688.sys"), Qt::CaseInsensitive) == 0);
        if (!matchesMode) continue;
        records << QStringLiteral("- %1 · %2 (%3)")
            .arg(QString::fromStdWString(block.timeText).toHtmlEscaped(), fileName.toHtmlEscaped())
            .arg(block.eventId);
    }
    if (!records.isEmpty()) {
        body << tr("近 7 天与当前 %1 模式相关的 Windows 拦截记录：<br>%2")
            .arg(name, records.join(QStringLiteral("<br>")));
        body << tr("这些是历史记录，不能据此确认本次故障由安全策略引起。"
                   "可打开 Windows 安全中心，核对被阻止的驱动及发生时间，再按系统提示处理。");
        const QString items = guardItemsHtml(snapshot.guards);
        if (!items.isEmpty()) body << tr("当前系统安全配置（仅供核对）：") + items;
        conclusion.actions.prepend({Action::OpenSecurityCenter, tr("打开 Windows 安全中心")});
    }
    body << tr("若仍不可用，请保留 logs 目录中的驱动日志并到开源仓库反馈。");
    conclusion.bodyHtml = body.join(QStringLiteral("<br><br>"));
    return conclusion;
}

void DriverSelfCheckCoordinator::handleAction(DriverSelfCheckDialog* dialog, Action action) {
    if (action == Action::OpenDiagnostics) {
        diagnosticOpenPending_ = true;
        dialog->close();
        // Finish closing the self-check before presenting the next window.
        QTimer::singleShot(0, this, [this]() {
            diagnosticOpenPending_ = false;
            openDiagnostic();
        });
        return;
    }
    if (action == Action::None) {
        dialog->close();
        return;
    }
    if (!openSecurityCenter()) {
        AppMessageDialog::warning(dialog,
            QString::fromWCharArray(L"\u65e0\u6cd5\u6253\u5f00\u8bbe\u7f6e"),
            QString::fromWCharArray(
                L"\u8bf7\u624b\u52a8\u6253\u5f00\uff1a\u5f00\u59cb\u83dc\u5355\u641c\u7d22\u300cWindows \u5b89\u5168\u4e2d\u5fc3\u300d"
                L"\u2192 \u8bbe\u5907\u5b89\u5168\u6027 \u2192 \u5185\u6838\u9694\u79bb\u8be6\u7ec6\u4fe1\u606f\u3002"));
    }
}

bool DriverSelfCheckCoordinator::openSecurityCenter() {
    // 新系统支持直达内核隔离页；旧系统逐级回退到安全中心首页 / 系统设置。
    for (const wchar_t* uri : {L"windowsdefender://coreisolation", L"windowsdefender://",
             L"ms-settings:windowsdefender"}) {
        const HINSTANCE result =
            ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) > 32) {
            return true;
        }
    }
    return false;
}
