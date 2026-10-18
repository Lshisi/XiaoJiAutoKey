#pragma once

#include "AppTypes.h"
#include "coordinators/DriverCoordinator.h"
#include "dialogs/DriverSelfCheckDialog.h"
#include "services/CodeIntegrityInspector.h"
#include "services/DdRuntimeService.h"

#include <QPointer>
#include <QString>
#include <QVector>

#include <functional>

class DriverSelfCheckCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parentWidget = nullptr;
        DdRuntimeService* runtimeService = nullptr;
        AppConfig* config = nullptr;
        // 当前驱动是否就绪（InputDriverRouter::IsReady）。
        std::function<bool()> driverReady;
        // 驱动运行时快照（ddRuntimeService_.Inspect(driver_)），在主线程调用。
        std::function<DdRuntimeService::RuntimeStatus()> inspectRuntime;
        // 暂存加载旧 DD 是否可用（作为 RepairEmbeddedInfDriver 的就绪探测）。
        std::function<bool()> legacyDriverLoadable;
        // 修复/安装完成后按当前配置重新加载驱动。
        std::function<bool()> reloadConfiguredDriver;
        std::function<DriverCoordinator::SwitchResult(DriverMode)> switchDriverMode;
        std::function<bool()> rebootComputer;
        std::function<void()> refreshStatus;
        std::function<void()> saveConfig;
        std::function<void(bool)> setDialogOpen;
    };

    explicit DriverSelfCheckCoordinator(QObject* parent = nullptr);

    void bind(Context context);

public slots:
    // 打开自检弹窗并开始检测；已打开时仅置前。
    void open();

private:
    struct Snapshot {
        DriverMode mode = DriverMode::Legacy;
        bool driverReady = false;
        DdRuntimeService::RuntimeStatus status;
        CodeIntegrityInspector::GuardStatus guards;
    };

    void runDiagnosis(DriverSelfCheckDialog* dialog);
    [[nodiscard]] DriverSelfCheckDialog::Conclusion buildConclusion(const Snapshot& snapshot) const;
    void handleAction(DriverSelfCheckDialog* dialog, DriverSelfCheckDialog::Action action);
    void executeRepairDd(DriverSelfCheckDialog* dialog);
    void executeSwitchToHid(DriverSelfCheckDialog* dialog);
    void executeReinstallHid(DriverSelfCheckDialog* dialog);
    void logRepair(const QString& action, bool ok, const QString& detail);
    bool openSecurityCenter();

    Context context_;
    QPointer<DriverSelfCheckDialog> dialog_;
};
