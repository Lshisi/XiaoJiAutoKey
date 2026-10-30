#pragma once

#include "AppTypes.h"
#include "services/DdRuntimeService.h"

#include <QObject>
#include <functional>

class DriverPageModel;
class QWidget;

class DriverCoordinator : public QObject {
    Q_OBJECT

public:
    enum class SwitchResult {
        Succeeded,
        Failed,
        Canceled,
    };

    struct Context {
        DriverPageModel* page = nullptr;
        AppConfig* config = nullptr;
        DdRuntimeService* runtimeService = nullptr;
        QWidget* parentWidget = nullptr;
        std::function<SwitchResult(DriverMode)> switchDriverMode;
        std::function<bool(DriverMode)> prepareDriverUninstall;
        std::function<void(DriverMode)> finalizeDriverUninstall;
        std::function<bool()> rebootComputer;
        std::function<void()> refreshStatus;
        std::function<void()> saveConfig;
        std::function<void(bool)> setDialogOpen;
        // 打开驱动自检向导（DriverSelfCheckCoordinator::open）。
        std::function<void()> openSelfCheck;
    };

    explicit DriverCoordinator(QObject* parent = nullptr);

    void bind(Context context);

private:
    Context context_;
};
