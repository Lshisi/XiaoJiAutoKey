#pragma once

#include "AppTypes.h"

#include <QObject>
#include <functional>

class GlobalInputHook;
class MainWindowView;
class QCloseEvent;
class QWidget;

class CloseCoordinator : public QObject {
    Q_OBJECT

public:
    struct Context {
        QWidget* parent = nullptr;
        MainWindowView* view = nullptr;
        AppConfig* config = nullptr;
        GlobalInputHook* inputHook = nullptr;
        std::function<void()> saveConfig;
        std::function<void()> hideWindow;
    };

    explicit CloseCoordinator(QObject* parent = nullptr);

    void bind(Context context);
    void handleClose(QCloseEvent* event);

private:
    Context context_;
};
