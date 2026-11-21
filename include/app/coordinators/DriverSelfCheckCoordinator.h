#pragma once

#include "AppTypes.h"
#include "diagnostics/DriverDiagnosticReport.h"
#include "dialogs/DriverDiagnosticDialog.h"
#include "dialogs/DriverSelfCheckDialog.h"
#include "services/CodeIntegrityInspector.h"
#include "services/DdRuntimeService.h"

#include <QPointer>
#include <QString>

#include <functional>
#include <memory>

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
        std::function<std::wstring()> driverLastError;
        // 后台系统读取；回调只持有服务副本，不访问主窗口或运行中的驱动。
        std::function<DdRuntimeService::RuntimeStatus(bool, const std::wstring&)> inspectRuntime;
        // 按键与循环的运行状态快照，在主线程调用。
        std::function<DriverRuntimeDiagnosticState()> inspectRuntimeState;
        std::function<void(bool)> setDialogOpen;
    };

    explicit DriverSelfCheckCoordinator(QObject* parent = nullptr);

    void bind(Context context);

public slots:
    // 打开自检弹窗并开始检测；已打开时仅置前。
    void open();
    // 生成并打开诊断信息报告；已打开时仅置前。
    void openDiagnostic();

signals:
    void dialogsClosed();

private:
    struct Snapshot {
        DriverMode mode = DriverMode::Xiaoji;
        bool driverReady = false;
        bool uninstalledByUser = false;
        bool guardsInspected = false;
        DdRuntimeService::RuntimeStatus status;
        CodeIntegrityInspector::GuardStatus guards;
        DriverRuntimeDiagnosticState runtimeState;
        QString error;
    };

    [[nodiscard]] std::shared_ptr<Snapshot> collectSnapshot() const;
    void updateDialogOpenState();
    void inspectSnapshot(std::shared_ptr<Snapshot> snapshot, bool fullReport,
        std::function<void(const Snapshot&)> completed);
    void runDiagnosis(DriverSelfCheckDialog* dialog);
    void runDiagnosticReport(DriverDiagnosticDialog* dialog);
    [[nodiscard]] DriverSelfCheckDialog::Conclusion buildConclusion(const Snapshot& snapshot) const;
    void handleAction(DriverSelfCheckDialog* dialog, DriverSelfCheckDialog::Action action);
    [[nodiscard]] bool openSecurityCenter();

    Context context_;
    QPointer<DriverSelfCheckDialog> dialog_;
    QPointer<DriverDiagnosticDialog> diagnosticDialog_;
    bool diagnosticOpenPending_ = false;
};
