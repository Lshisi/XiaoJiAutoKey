#pragma once

#include "AppTypes.h"
#include "ui/models/PageModel.h"

#include <QElapsedTimer>
#include <functional>

class KeyCaptureDialogBase;
class MainWindowView;
class QWidget;

class OnboardingCoordinator final : public PageModel {
    Q_OBJECT

public:
    struct Context {
        QWidget* window = nullptr;
        AppConfig* config = nullptr;
        MainWindowView* view = nullptr;
        std::function<bool()> running;
        std::function<bool()> capturing;
        std::function<bool()> driverReady;
        std::function<void()> save;
        std::function<void()> selfCheck;
    };

    explicit OnboardingCoordinator(QObject* parent = nullptr);
    void bind(Context context);
    void initialize();
    void scheduleRefresh();
    bool active() const { return active_; }
    bool blocksStart() const;
    void prepareKeyEditor(KeyCaptureDialogBase& dialog);

    Q_INVOKABLE void start();
    Q_INVOKABLE void begin();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void skip();
    Q_INVOKABLE void act();
    Q_INVOKABLE void secondaryAction();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refresh();
    void navigate();
    void persist();
    void finish(bool skipped);
    bool busy() const;
    ProfileConfig* profile() const;
    MacroConfig* macro() const;
    bool modeTipSeen(RepeatMode mode) const;
    void markModeTipSeen(RepeatMode mode);
    bool actionTipSeen(MacroAction action) const;
    void markActionTipSeen(MacroAction action);

    Context context_;
    bool initialized_ = false;
    bool active_ = false;
    bool welcome_ = false;
    bool macroTour_ = false;
    bool refreshPending_ = false;
    int step_ = 0;
    QElapsedTimer completionGuard_;
};
