#pragma once

#include "AppTypes.h"
#include "services/DdRuntimeService.h"

#include <QObject>
#include <functional>

class DriverPage;
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
        DriverPage* page = nullptr;
        AppConfig* config = nullptr;
        DdRuntimeService* runtimeService = nullptr;
        QWidget* parentWidget = nullptr;
        std::function<SwitchResult(DriverMode)> switchDriverMode;
        std::function<void(DriverMode)> prepareDriverUninstall;
        std::function<void(DriverMode)> finalizeDriverUninstall;
        std::function<void()> updatePicoFirmware;
        std::function<bool()> rebootComputer;
        std::function<void()> refreshStatus;
        std::function<void()> saveConfig;
        std::function<void(bool)> setDialogOpen;
    };

    explicit DriverCoordinator(QObject* parent = nullptr);

    void bind(Context context);

private:
    Context context_;
};
