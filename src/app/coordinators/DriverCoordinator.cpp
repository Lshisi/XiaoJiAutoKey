#include "coordinators/DriverCoordinator.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/ConfigConfirmDialog.h"
#include "dialogs/QmlDialog.h"
#include "ui/models/DriverPageModel.h"

#include <QThread>
#include <QTimer>

#include <memory>
#include <exception>
#include <QPointer>
#include <utility>

namespace {
struct DriverUninstallResult {
    bool ok = false;
    std::wstring message;
};

QString driverName(DriverRemovalTarget mode) {
    if (mode == DriverRemovalTarget::RetiredHid) return QStringLiteral("HID");
    return mode == DriverRemovalTarget::Xiaoji ? QObject::tr("小叽") : QStringLiteral("DD");
}

class DriverUninstallProgressDialog final : public QmlDialog {
public:
    explicit DriverUninstallProgressDialog(DriverRemovalTarget mode, QWidget* parent) : QmlDialog(parent) {
        const QString name = driverName(mode);
        setWindowTitle(tr("%1 驱动卸载").arg(name));
        setWindowModality(Qt::ApplicationModal);
        present("DialogReport.qml", {{"heading", tr("正在后台卸载 %1").arg(name)},
            {"body", mode == DriverRemovalTarget::Xiaoji ? tr("请勿关闭程序，正在移除小叽驱动设备及其所属驱动包。") : tr("请勿关闭程序，卸载完成后将尝试重启电脑。")}, {"bodyRich", false},
            {"status", QString()}, {"severity", 0}, {"progressVisible", true},
            {"progressIndeterminate", true}, {"progressValue", 0}, {"progressText", QString()},
            {"closeAllowed", false}}, QSize(420, 168), true);
    }
    void markFinished(bool ok) {
        setValue("progressVisible", false);
        setValue("status", ok ? tr("卸载已完成，正在处理后续状态…") : tr("卸载未完成，正在保留重试入口…"));
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

    connect(context_.page, &DriverPageModel::diagnosticsInfoRequested, this, [this]() {
        if (context_.openDiagnostics) context_.openDiagnostics();
    });

    connect(context_.page, &DriverPageModel::uninstallDriverRequested, this, [this](DriverRemovalTarget target) {
        if (!context_.runtimeService || !context_.xiaojiRuntimeService) return;
        const QString name = driverName(target);
        const bool retired = target == DriverRemovalTarget::RetiredHid;
        const bool xiaoji = target == DriverRemovalTarget::Xiaoji;
        const QString message = retired
            ? tr("将卸载旧 HID 驱动并清理其安装残留。旧 HID 已停止提供安装和输入功能，卸载后无法重新启用。"
                 "完成后将重启电脑，请先保存其他工作。是否继续？")
            : xiaoji ? tr("将移除小叽驱动的键盘、鼠标及其所属驱动包，保留本机签名证书和签名缓存。需要再次使用时可在此主动安装。是否继续？")
                : tr("将卸载 DD 驱动并停用其自动安装，完成后重启电脑。请先保存其他工作。是否继续？");
        context_.page->setBusy(true);
        if (context_.setDialogOpen) context_.setDialogOpen(true);
        const bool confirmed = ConfigConfirmDialog::confirm(context_.parentWidget,
            tr("卸载 %1 驱动").arg(name), message, tr("卸载"));
        if (!confirmed || (context_.prepareDriverUninstall && !context_.prepareDriverUninstall(target))) {
            context_.page->setBusy(false);
            if (context_.setDialogOpen) context_.setDialogOpen(false);
            return;
        }
        auto* progress = new DriverUninstallProgressDialog(target, context_.parentWidget);
        const auto result = std::make_shared<DriverUninstallResult>();
        auto* thread = QThread::create([runtime = context_.runtimeService,
            xiaojiRuntime = context_.xiaojiRuntimeService, result, target] {
            try {
                if (target == DriverRemovalTarget::Xiaoji)
                    result->ok = xiaojiRuntime->Uninstall(&result->message);
                else if (target == DriverRemovalTarget::RetiredHid)
                    result->ok = runtime->UninstallEmbeddedHidDriver(&result->message);
                else result->ok = runtime->UninstallEmbeddedInfDriver(&result->message);
            } catch (const std::exception& error) {
                result->message = ToWide(error.what());
            }
        });
        thread->setParent(this);
        connect(thread, &QThread::finished, this, [this, progress, thread, result, target, name, xiaoji] {
            thread->deleteLater();
            if (context_.finalizeDriverUninstall) context_.finalizeDriverUninstall(target, result->ok);
            if (context_.refreshStatus) context_.refreshStatus();
            progress->markFinished(result->ok);
            progress->accept();
            progress->deleteLater();
            if (!result->ok) {
                AppMessageDialog::warning(context_.parentWidget, tr("%1 卸载未完成").arg(name),
                    tr("请检查卸载结果；残留仍存在时会继续保留卸载按钮，可在重启后重试。\n\n%1")
                        .arg(QString::fromStdWString(result->message)));
            } else if (!xiaoji && (!context_.rebootComputer || !context_.rebootComputer())) {
                AppMessageDialog::warning(context_.parentWidget, tr("请重启电脑"), tr("请手动重启电脑以完成驱动卸载。"));
            }
            if (context_.page) context_.page->setBusy(false);
            if (context_.setDialogOpen) context_.setDialogOpen(false);
        }, Qt::QueuedConnection);
        thread->start();
        progress->exec();
    });
}
