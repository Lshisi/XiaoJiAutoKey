#pragma once

#include "AppTypes.h"
#include "dialogs/DriverSelfCheckDialog.h"
#include "services/CodeIntegrityInspector.h"
#include "services/DdRuntimeService.h"

#include <QPointer>
#include <QString>

#include <functional>

// 驱动自检：纯诊断 + 指引，不执行任何修复/切换动作。
// 优先检查当前模式的运行文件，再报告安装、设备及加载状态；
// 安全配置和历史拦截记录仅作辅助信息，不据此认定本次故障原因。
class DriverSelfCheckCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parentWidget = nullptr;
        AppConfig* config = nullptr;
        // 当前驱动是否就绪（InputDriverRouter::IsReady）。
        std::function<bool()> driverReady;
        // 驱动运行时快照（ddRuntimeService_.Inspect(driver_)），在主线程调用。
        std::function<DdRuntimeService::RuntimeStatus()> inspectRuntime;
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
        bool uninstalledByUser = false;
        DdRuntimeService::RuntimeStatus status;
        CodeIntegrityInspector::GuardStatus guards;
    };

    void runDiagnosis(DriverSelfCheckDialog* dialog);
    [[nodiscard]] DriverSelfCheckDialog::Conclusion buildConclusion(const Snapshot& snapshot) const;
    void handleAction(DriverSelfCheckDialog* dialog, DriverSelfCheckDialog::Action action);
    [[nodiscard]] bool openSecurityCenter();

    Context context_;
    QPointer<DriverSelfCheckDialog> dialog_;
};
