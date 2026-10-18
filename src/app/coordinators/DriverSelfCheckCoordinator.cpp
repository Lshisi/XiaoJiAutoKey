#include "coordinators/DriverSelfCheckCoordinator.h"

#include "dialogs/AppMessageDialog.h"

#include <QThread>

#include <Windows.h>
#include <shellapi.h>

#include <memory>

namespace {

using Action = DriverSelfCheckDialog::Action;
using Conclusion = DriverSelfCheckDialog::Conclusion;

QVector<QPair<Action, QString>> closeActions() {
    return {{Action::None, QString::fromWCharArray(L"\u5173\u95ed")}};
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

const QString kSecuritySteps = QString::fromWCharArray(
    L"\u5173\u95ed\u6b65\u9aa4\uff1a<br>"
    L"1. \u6253\u5f00\u300cWindows \u5b89\u5168\u4e2d\u5fc3\u300d\u2192\u300c\u8bbe\u5907\u5b89\u5168\u6027\u300d\u2192\u300c\u5185\u6838\u9694\u79bb\u8be6\u7ec6\u4fe1\u606f\u300d<br>"
    L"2. \u5173\u95ed\u300c\u5185\u5b58\u5b8c\u6574\u6027\u300d\u548c\u300c\u6613\u53d7\u653b\u51fb\u7684\u9a71\u52a8\u7a0b\u5e8f\u963b\u6b62\u5217\u8868\u300d<br>"
    L"3. \u91cd\u542f\u7535\u8111<br>"
    L"4. \u91cd\u542f\u540e\u6253\u5f00\u5c0f\u9e21\u8fde\u53d1\uff0c\u7a0b\u5e8f\u4f1a\u81ea\u52a8\u91cd\u65b0\u5b89\u88c5\u5e76\u6062\u590d\u9a71\u52a8");

}  // namespace

DriverSelfCheckCoordinator::DriverSelfCheckCoordinator(QObject* parent) : QObject(parent) {}

void DriverSelfCheckCoordinator::bind(Context context) {
    context_ = std::move(context);
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

    dialog_ = new DriverSelfCheckDialog(context_.parentWidget);
    connect(dialog_, &QDialog::finished, this, [this]() {
        if (context_.setDialogOpen) context_.setDialogOpen(false);
        if (dialog_) dialog_->deleteLater();
    });
    connect(dialog_, &QObject::destroyed, this, [this]() { dialog_ = nullptr; });
    connect(dialog_, &DriverSelfCheckDialog::actionRequested, this, [this](int action) {
        if (dialog_) handleAction(dialog_, static_cast<Action>(action));
    });

    runDiagnosis(dialog_);
    dialog_->exec();
}

void DriverSelfCheckCoordinator::runDiagnosis(DriverSelfCheckDialog* dialog) {
    dialog->beginCheck();

    // Inspect 与驱动页刷新同源同量级，留在主线程；事件日志等纯系统读取放后台。
    const auto snapshot = std::make_shared<Snapshot>();
    snapshot->mode = context_.config->driverMode;
    snapshot->driverReady = context_.driverReady ? context_.driverReady() : false;
    snapshot->status = context_.inspectRuntime ? context_.inspectRuntime()
                                               : DdRuntimeService::RuntimeStatus{};

    const QPointer<DriverSelfCheckDialog> dialogPtr(dialog);
    auto* thread = QThread::create([snapshot]() {
        snapshot->guards = CodeIntegrityInspector::Inspect(true);
    });
    thread->setParent(this);
    connect(thread, &QThread::finished, this, [this, dialogPtr, thread, snapshot]() {
        thread->deleteLater();
        if (!dialogPtr) return;
        dialogPtr->setConclusion(buildConclusion(*snapshot));
    }, Qt::QueuedConnection);
    thread->start();
}

Conclusion DriverSelfCheckCoordinator::buildConclusion(const Snapshot& snapshot) const {
    Conclusion conclusion;
    const bool blockedByGuards =
        snapshot.guards.memoryIntegrityRunning || !snapshot.guards.recentBlocks.empty();

    if (!snapshot.driverReady && blockedByGuards) {
        conclusion.severity = DriverSelfCheckDialog::Severity::Danger;
        conclusion.heading = QString::fromWCharArray(
            L"Windows \u5185\u6838\u9694\u79bb\u6b63\u5728\u62e6\u622a\u6309\u952e\u9a71\u52a8");
        QStringList body;
        body << QString::fromWCharArray(
            L"\u68c0\u6d4b\u5230\u4ee5\u4e0b\u5b89\u5168\u529f\u80fd\u4f1a\u62e6\u622a DD / HID \u9a71\u52a8\u52a0\u8f7d\uff0c"
            L"\u8fd9\u5c31\u662f\u6309\u952e\u8fde\u53d1\u65e0\u6548\u7684\u539f\u56e0\uff1a");
        const QString items = guardItemsHtml(snapshot.guards);
        if (!items.isEmpty()) body << items;
        if (!snapshot.guards.recentBlocks.empty()) {
            body << QString::fromWCharArray(L"Windows \u4e8b\u4ef6\u65e5\u5fd7\u4e2d\u7684\u62e6\u622a\u8bb0\u5f55\uff1a");
            QStringList records;
            for (const auto& block : snapshot.guards.recentBlocks) {
                records << QStringLiteral("- ")
                    + QString::fromStdWString(block.timeText) + QStringLiteral(" \u00b7 ")
                    + QString::fromStdWString(block.fileName)
                    + QStringLiteral(" (") + QString::number(block.eventId)
                    + QStringLiteral(")");
            }
            body << records.join(QStringLiteral("<br>"));
        }
        body << QString::fromWCharArray(
            L"<b>\u6b64\u65f6\u5378\u8f7d/\u91cd\u88c5 DD\u3001\u5207\u6362 HID \u5747\u65e0\u6548</b>"
            L"\u2014\u2014\u5b83\u4eec\u540c\u5c5e\u4f1a\u88ab\u62e6\u622a\u7684\u9a71\u52a8\uff0c"
            L"\u8bf7\u5148\u6309\u4ee5\u4e0b\u6b65\u9aa4\u5173\u95ed\u62e6\u622a\uff1a");
        body << kSecuritySteps;
        conclusion.bodyHtml = body.join(QStringLiteral("<br><br>"));
        conclusion.actions = {
            {Action::OpenSecurityCenter, QString::fromWCharArray(L"\u6253\u5f00 Windows \u5b89\u5168\u4e2d\u5fc3")},
            {Action::None, QString::fromWCharArray(L"\u5173\u95ed")},
        };
        return conclusion;
    }

    if (snapshot.guards.memoryIntegrityRunning && snapshot.driverReady) {
        conclusion.severity = DriverSelfCheckDialog::Severity::Warning;
        conclusion.heading = QString::fromWCharArray(
            L"\u68c0\u6d4b\u5230\u5185\u5b58\u5b8c\u6574\u6027\uff08\u5185\u6838\u9694\u79bb\uff09\u5f00\u542f");
        conclusion.bodyHtml = QString::fromWCharArray(
            L"\u5f53\u524d\u9a71\u52a8\u4ecd\u53ef\u8fd0\u884c\uff0c\u4f46\u7cfb\u7edf\u66f4\u65b0\u540e\u53ef\u80fd\u88ab\u62e6\u622a\u800c\u5931\u6548\uff0c"
            L"\u5efa\u8bae\u63d0\u524d\u5173\u95ed\uff1a<br><br>") + kSecuritySteps;
        conclusion.actions = {
            {Action::OpenSecurityCenter, QString::fromWCharArray(L"\u6253\u5f00 Windows \u5b89\u5168\u4e2d\u5fc3")},
            {Action::None, QString::fromWCharArray(L"\u5173\u95ed")},
        };
        return conclusion;
    }

    if (!snapshot.driverReady) {
        conclusion.severity = DriverSelfCheckDialog::Severity::Danger;
        conclusion.heading = QString::fromWCharArray(L"\u6309\u952e\u9a71\u52a8\u672a\u80fd\u6b63\u5e38\u5de5\u4f5c");
        QStringList body;
        if (snapshot.mode == DriverMode::Usb) {
            body << QString::fromWCharArray(
                L"\u5f53\u524d\u4f7f\u7528 USB \u6a21\u5f0f\uff0c\u4f46\u672a\u68c0\u6d4b\u5230\u53ef\u7528\u8bbe\u5907\u3002"
                L"\u8bf7\u786e\u8ba4 CH552 \u8bbe\u5907\u5df2\u63d2\u5165\uff0c\u53ef\u5c1d\u8bd5\u66f4\u6362 USB \u53e3\u540e\u91cd\u65b0\u81ea\u68c0\u3002");
        } else {
            body << QString::fromWCharArray(L"\u5f53\u524d\u4f7f\u7528 ")
                + modeText(snapshot.mode)
                + QString::fromWCharArray(
                    L" \u9a71\u52a8\uff0c\u4f46\u672a\u80fd\u52a0\u8f7d\u3002"
                    L"\u8bf7\u5148\u91cd\u542f\u7535\u8111\uff0c\u91cd\u542f\u540e\u6253\u5f00\u5c0f\u9e21\u8fde\u53d1\u4f1a\u81ea\u52a8\u8865\u88c5\u9a71\u52a8\u3002");
            if (!snapshot.status.lastError.empty()) {
                body << QString::fromWCharArray(L"\u9519\u8bef\u4fe1\u606f\uff1a")
                    + QString::fromStdWString(snapshot.status.lastError).toHtmlEscaped();
            }
            if (snapshot.guards.vulnerableBlocklistEnabled) {
                body << QString::fromWCharArray(
                    L"\u63d0\u793a\uff1a\u6613\u53d7\u653b\u51fb\u7684\u9a71\u52a8\u7a0b\u5e8f\u963b\u6b62\u5217\u8868\u5904\u4e8e\u5f00\u542f\u72b6\u6001\uff0c"
                    L"\u82e5\u91cd\u542f\u540e\u4ecd\u4e0d\u53ef\u7528\uff0c\u53ef\u6309\u4e0b\u65b9\u6b65\u9aa4\u5173\u95ed\u540e\u518d\u91cd\u542f\uff1a<br>")
                    + kSecuritySteps;
                conclusion.actions = {
                    {Action::OpenSecurityCenter, QString::fromWCharArray(L"\u6253\u5f00 Windows \u5b89\u5168\u4e2d\u5fc3")},
                    {Action::None, QString::fromWCharArray(L"\u5173\u95ed")},
                };
            } else {
                body << QString::fromWCharArray(
                    L"\u82e5\u91cd\u542f\u540e\u4ecd\u4e0d\u53ef\u7528\uff0c\u53ef\u5728\u9a71\u52a8\u7ba1\u7406\u9875\u5207\u6362\u9a71\u52a8\u6a21\u5f0f\uff0c"
                    L"\u6216\u524d\u5f80\u5f00\u6e90\u4ed3\u5e93\u53cd\u9988\u3002");
                conclusion.actions = closeActions();
            }
        }
        conclusion.bodyHtml = body.join(QStringLiteral("<br><br>"));
        return conclusion;
    }

    conclusion.severity = DriverSelfCheckDialog::Severity::Ok;
    conclusion.heading = QString::fromWCharArray(L"\u9a71\u52a8\u5de5\u4f5c\u6b63\u5e38");
    conclusion.bodyHtml = QString::fromWCharArray(L"\u5f53\u524d\u6a21\u5f0f\uff1a") + modeText(snapshot.mode)
        + QString::fromWCharArray(L"\uff0c\u9a71\u52a8\u52a0\u8f7d\u6b63\u5e38\uff0c\u672a\u53d1\u73b0\u5f02\u5e38\u3002");
    conclusion.actions = closeActions();
    return conclusion;
}

void DriverSelfCheckCoordinator::handleAction(DriverSelfCheckDialog* dialog, Action action) {
    if (action != Action::OpenSecurityCenter) {
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
