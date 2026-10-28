#include "coordinators/DriverCoordinator.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/QmlDialog.h"
#include "ui/models/DriverPageModel.h"

#include <QThread>
#include <QTimer>

#include <memory>
#include <QPointer>
#include <utility>

namespace {
struct DriverUninstallResult {
    bool ok = false;
    std::wstring message;
};

QString driverName(DriverMode mode) {
    return mode == DriverMode::Hid ? QStringLiteral("HID") : QStringLiteral("DD");
}

class DriverUninstallProgressDialog final : public QmlDialog {
public:
    explicit DriverUninstallProgressDialog(DriverMode mode, QWidget* parent) : QmlDialog(parent) {
        const QString name = driverName(mode);
        setWindowTitle(tr("%1 驱动卸载").arg(name));
        setWindowModality(Qt::ApplicationModal);
        present("DialogReport.qml", {{"heading", tr("正在后台卸载 %1").arg(name)},
            {"body", tr("请勿关闭程序，卸载完成后将尝试重启电脑。")}, {"bodyRich", false},
            {"status", QString()}, {"severity", 0}, {"progressVisible", true},
            {"progressIndeterminate", true}, {"progressValue", 0}, {"progressText", QString()},
            {"closeAllowed", false}}, QSize(480, 300), true);
    }
    void markFinished() {
        setValue("progressVisible", false);
        setValue("status", tr("卸载已完成，正在处理后续状态…"));
    }
};
}

DriverCoordinator::DriverCoordinator(QObject* parent) : QObject(parent) {}

void DriverCoordinator::bind(Context context) {
    context_ = std::move(context);
    if (!context_.page || !context_.config) return;

    connect(context_.page, &DriverPageModel::driverModeChanged, this, [this](DriverMode mode) {
        if (!context_.config || !context_.switchDriverMode) return;
        const auto result = context_.switchDriverMode(mode);
        if (result == SwitchResult::Failed) {
            AppMessageDialog::warning(
                context_.parentWidget,
                QString::fromWCharArray(L"\u9a71\u52a8\u5207\u6362\u5931\u8d25"),
                QString::fromWCharArray(L"\u65e0\u6cd5\u5207\u6362\u5230\u6240\u9009\u9a71\u52a8\uff0c\u5df2\u4fdd\u7559\u5f53\u524d\u914d\u7f6e\u3002"));
        }
        if (context_.refreshStatus) context_.refreshStatus();
    });

    connect(context_.page, &DriverPageModel::refreshRequested, this, [this]() {
        if (context_.refreshStatus) context_.refreshStatus();
    });

    connect(context_.page, &DriverPageModel::selfCheckRequested, this, [this]() {
        if (context_.openSelfCheck) context_.openSelfCheck();
    });

    connect(context_.page, &DriverPageModel::uninstallDriverRequested, this, [this](DriverMode mode) {
        if (mode == DriverMode::Usb) {
            return;
        }
        if (!context_.runtimeService) return;
        const QString name = driverName(mode);
        const bool confirmed = ConfigConfirmDialog::confirm(
            context_.parentWidget,
            QString::fromWCharArray(L"\u5378\u8f7d ") + name + QString::fromWCharArray(L" \u9a71\u52a8"),
            QString::fromWCharArray(L"\u5378\u8f7d\u540e ") + name + QString::fromWCharArray(L" \u5c06\u4e0d\u53ef\u7528\uff0c\u5e76\u4f1a\u5728\u5378\u8f7d\u5b8c\u6210\u540e\u91cd\u542f\u7535\u8111\u3002\u5378\u8f7d\u8fc7\u7a0b\u4e2d\u8bf7\u52ff\u5173\u95ed\u7a0b\u5e8f\u3002\u662f\u5426\u7ee7\u7eed\uff1f"),
            QString::fromWCharArray(L"\u5378\u8f7d"));
        if (!confirmed) return;

        if (context_.prepareDriverUninstall) context_.prepareDriverUninstall(mode);
        if (context_.page) context_.page->setBusy(true);
        if (context_.setDialogOpen) context_.setDialogOpen(true);

        auto* progressDialog = new DriverUninstallProgressDialog(mode, context_.parentWidget);
        const QPointer<DriverUninstallProgressDialog> progressDialogPtr(progressDialog);
        const auto result = std::make_shared<DriverUninstallResult>();
        auto* thread = QThread::create([runtimeService = context_.runtimeService, result, mode]() {
            result->ok = mode == DriverMode::Hid
                ? runtimeService->UninstallEmbeddedHidDriver(&result->message)
                : runtimeService->UninstallEmbeddedInfDriver(&result->message);
        });
        thread->setParent(this);

        connect(thread, &QThread::finished, this, [this, progressDialog, thread, result, mode, name]() {
            thread->deleteLater();

            if (context_.finalizeDriverUninstall) context_.finalizeDriverUninstall(mode);
            if (context_.page) context_.page->setBusy(false);
            if (context_.refreshStatus) context_.refreshStatus();

            if (progressDialog) {
                progressDialog->markFinished();
                progressDialog->accept();
                progressDialog->deleteLater();
            }
            if (context_.setDialogOpen) context_.setDialogOpen(false);

            if (result->ok) {
                const bool rebootStarted = context_.rebootComputer && context_.rebootComputer();
                if (!rebootStarted) {
                    AppMessageDialog::warning(
                        context_.parentWidget,
                        QString::fromWCharArray(L"\u91cd\u542f\u5931\u8d25"),
                        QString::fromWCharArray(L"\u65e0\u6cd5\u81ea\u52a8\u53d1\u8d77\u7cfb\u7edf\u91cd\u542f\uff0c\u8bf7\u624b\u52a8\u91cd\u542f\u7535\u8111\u3002"));
                }
            } else {
                AppMessageDialog::warning(
                    context_.parentWidget,
                    name + QString::fromWCharArray(L" \u5378\u8f7d\u9700\u8981\u624b\u52a8\u5904\u7406"),
                    name + QString::fromWCharArray(L" \u9a71\u52a8\u5df2\u8fdb\u5165\u5378\u8f7d\u6d41\u7a0b\uff0c\u4f46 Windows \u4ecd\u663e\u793a\u6709\u8bbe\u5907\u6216\u670d\u52a1\u5904\u4e8e\u5f85\u91ca\u653e\u72b6\u6001\u3002\n\n\u8bf7\u624b\u52a8\u91cd\u542f\u7535\u8111\u540e\u518d\u7ee7\u7eed\u4f7f\u7528\u3002\n\n\u5378\u8f7d\u7ed3\u679c\uff1a\n") +
                        QString::fromStdWString(result->message));
            }
        }, Qt::QueuedConnection);

        thread->start();
        progressDialog->exec();
        if (progressDialogPtr && context_.setDialogOpen) context_.setDialogOpen(false);
    });
}
