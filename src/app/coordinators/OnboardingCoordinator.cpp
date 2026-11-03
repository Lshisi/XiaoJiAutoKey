#include "coordinators/OnboardingCoordinator.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/KeyCaptureDialogBase.h"
#include "ui/models/AboutPageModel.h"
#include "ui/models/DriverPageModel.h"
#include "ui/models/KeyPageModel.h"
#include "ui/models/MacroPageModel.h"
#include "ui/models/SettingsPageModel.h"
#include "view/MainWindowView.h"

#include <QApplication>
#include <QEvent>
#include <QTimer>
#include <QWidget>
#include <algorithm>

namespace {
constexpr int kIndependentTip = 1;
constexpr int kSequenceTip = 2;
constexpr int kMacroTip = 4;
}

OnboardingCoordinator::OnboardingCoordinator(QObject* parent) : PageModel(parent) {
    state_ = {{"active", false}, {"visible", false}, {"startBlocked", false}, {"welcome", false}, {"title", QString()},
        {"body", QString()}, {"hint", QString()}, {"target", QString()}, {"actionText", QString()},
        {"secondaryText", QString()}, {"nextText", QString()}, {"progress", QString()},
        {"canNext", true}, {"canPrevious", false}};
}

void OnboardingCoordinator::bind(Context context) {
    context_ = std::move(context);
    qApp->installEventFilter(this);
    connect(context_.view, &MainWindowView::currentPageChanged, this, &OnboardingCoordinator::scheduleRefresh);
    connect(context_.view->keyPage(), &PageModel::stateChanged, this, &OnboardingCoordinator::scheduleRefresh);
    connect(context_.view->macroPage(), &PageModel::stateChanged, this, &OnboardingCoordinator::scheduleRefresh);
    connect(context_.view->driverPage(), &PageModel::stateChanged, this, &OnboardingCoordinator::scheduleRefresh);
    connect(context_.view->settingsPage(), &PageModel::stateChanged, this, &OnboardingCoordinator::scheduleRefresh);
    connect(context_.view->aboutPage(), &AboutPageModel::onboardingRequested, this, &OnboardingCoordinator::start);
}

void OnboardingCoordinator::initialize() {
    if (initialized_) return;
    initialized_ = true;
    const auto status = context_.config->onboardingStatus;
    if (status == OnboardingStatus::Pending || status == OnboardingStatus::InProgress) {
        active_ = true;
        welcome_ = status == OnboardingStatus::Pending;
        step_ = std::clamp(context_.config->onboardingStep, 0, 5);
        context_.config->onboardingStatus = OnboardingStatus::InProgress;
        persist();
        navigate();
    }
    refresh();
}

bool OnboardingCoordinator::busy() const {
    return QApplication::activeModalWidget() || (context_.capturing && context_.capturing())
        || (context_.running && context_.running());
}

bool OnboardingCoordinator::blocksStart() const {
    return active_ || (completionGuard_.isValid() && completionGuard_.elapsed() < 250);
}

ProfileConfig* OnboardingCoordinator::profile() const {
    for (auto& value : context_.config->profiles)
        if (value.id == context_.config->activeProfileId) return &value;
    return context_.config->profiles.empty() ? nullptr : &context_.config->profiles.front();
}

MacroConfig* OnboardingCoordinator::macro() const {
    for (auto& value : context_.config->macros)
        if (value.id == context_.config->activeMacroId) return &value;
    return context_.config->macros.empty() ? nullptr : &context_.config->macros.front();
}

void OnboardingCoordinator::start() {
    if (!initialized_ || !context_.window->isVisible()) return;
    if (busy()) {
        AppMessageDialog::information(context_.window, tr("新手引导"), tr("请先停止连发并结束当前编辑，再查看新手引导。"));
        return;
    }
    active_ = true;
    welcome_ = true;
    macroTour_ = false;
    step_ = 0;
    context_.config->onboardingStatus = OnboardingStatus::InProgress;
    context_.config->onboardingStep = 0;
    context_.config->onboardingMacroStep = 0;
    context_.config->onboardingSeenTips = 0;
    persist();
    refresh();
}

void OnboardingCoordinator::begin() {
    if (!active_ || !welcome_ || busy()) return;
    welcome_ = false;
    if (context_.driverReady && context_.driverReady()) step_ = 1;
    context_.config->onboardingStep = step_;
    persist();
    navigate();
    refresh();
}

void OnboardingCoordinator::next() {
    if (!active_ || welcome_ || busy()) return;
    refresh();
    if (!state_.value("canNext").toBool()) return;
    const int lastStep = macroTour_ ? 2 : 5;
    if (step_ == lastStep) {
        finish(false);
        return;
    }
    ++step_;
    if (macroTour_) context_.config->onboardingMacroStep = step_;
    else context_.config->onboardingStep = step_;
    persist();
    navigate();
    refresh();
}

void OnboardingCoordinator::previous() {
    if (!active_ || welcome_ || busy() || step_ == 0) return;
    --step_;
    if (macroTour_) context_.config->onboardingMacroStep = step_;
    else context_.config->onboardingStep = step_;
    persist();
    navigate();
    refresh();
}

void OnboardingCoordinator::skip() {
    if (active_ && !busy()) finish(true);
}

void OnboardingCoordinator::finish(bool skipped) {
    if (macroTour_) {
        context_.config->onboardingSeenTips |= kMacroTip;
        context_.config->onboardingMacroStep = 0;
    } else context_.config->onboardingStatus = skipped ? OnboardingStatus::Skipped : OnboardingStatus::Completed;
    active_ = false;
    welcome_ = false;
    macroTour_ = false;
    // Ignore queued key events from the key that completed or dismissed the guide.
    completionGuard_.start();
    QTimer::singleShot(250, Qt::PreciseTimer, this, &OnboardingCoordinator::scheduleRefresh);
    persist();
    refresh();
}

void OnboardingCoordinator::persist() {
    if (context_.save) context_.save();
}

void OnboardingCoordinator::navigate() {
    if (welcome_) return;
    const QString page = macroTour_ ? QStringLiteral("macro")
        : step_ == 0 ? QStringLiteral("driver") : step_ == 4 ? QStringLiteral("settings") : QStringLiteral("keys");
    if (context_.view->currentPage() == page) return;
    context_.view->navigateTo(page);
    if (page == QStringLiteral("driver")) emit context_.view->driverPage()->refreshRequested();
}

void OnboardingCoordinator::act() {
    if (!active_ || welcome_ || busy()) return;
    if (macroTour_) {
        if (step_ == 0) emit context_.view->macroPage()->addStepRequested();
        else if (step_ == 1) emit context_.view->macroPage()->hotkeyEditClicked();
        else if (step_ == 2 && macro() && !macro()->enabled)
            emit context_.view->macroPage()->macroEnabledToggled(true);
    } else if (step_ == 0) {
        if (context_.selfCheck) context_.selfCheck();
        emit context_.view->driverPage()->refreshRequested();
    } else if (step_ == 1) {
        emit context_.view->keyPage()->addRequested();
    } else if (step_ == 2) {
        auto* current = profile();
        if (current && !current->enabled) emit context_.view->keyPage()->profileEnabledToggled(true);
        else if (current && !current->bindings.empty()) emit context_.view->keyPage()->editRequested(current->bindings.front().id);
        else emit context_.view->keyPage()->addRequested();
    } else if (step_ == 3) {
        emit context_.view->keyPage()->hotkeyEditClicked();
    } else if (step_ == 4) {
        emit context_.view->settingsPage()->gameGateEnabledChanged(!context_.config->gameGateEnabled);
    } else if (step_ == 5) {
        emit context_.view->keyPage()->pauseKeyAddClicked();
    }
    scheduleRefresh();
}

void OnboardingCoordinator::secondaryAction() {
    if (!active_ || welcome_ || macroTour_ || busy() || step_ != 4) return;
    emit context_.view->settingsPage()->gameGateProcessesEditRequested();
    scheduleRefresh();
}

void OnboardingCoordinator::scheduleRefresh() {
    if (!initialized_ || refreshPending_) return;
    refreshPending_ = true;
    QTimer::singleShot(0, this, [this]() {
        refreshPending_ = false;
        refresh();
    });
}

bool OnboardingCoordinator::eventFilter(QObject* watched, QEvent* event) {
    if (qobject_cast<QWidget*>(watched)) {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::WindowActivate:
        case QEvent::WindowDeactivate:
        case QEvent::WindowStateChange:
            scheduleRefresh();
            break;
        default:
            break;
        }
    }
    return PageModel::eventFilter(watched, event);
}

void OnboardingCoordinator::refresh() {
    if (!initialized_) return;
    const bool available = context_.window->isVisible() && !context_.window->isMinimized() && !busy();
    if (active_ && available) navigate();
    if (!active_ && available && context_.view->currentPage() == QStringLiteral("macro")
        && !(context_.config->onboardingSeenTips & kMacroTip)) {
        active_ = true;
        macroTour_ = true;
        step_ = std::clamp(context_.config->onboardingMacroStep, 0, 2);
    }
    const auto* current = profile();
    const auto* currentMacro = macro();
    const bool hasBindings = current && !current->bindings.empty();
    const bool hasEnabledBinding = current && current->enabled && std::any_of(
        current->bindings.begin(), current->bindings.end(), [](const auto& binding) { return binding.enabled; });
    const bool driverReady = context_.driverReady && context_.driverReady();
    QString title, body, hint, target, actionText, secondaryText;
    bool canNext = true;
    if (welcome_) {
        title = tr("欢迎使用小叽按键");
        body = tr("跟着界面配好第一组按键，了解怎样启动、暂停和停止。设置会自动保存，你也可以随时跳过。");
        hint = driverReady ? tr("当前驱动已就绪，将从添加按键开始。") : tr("先确认驱动状态，再配置按键。暂时无法修复也可以先了解操作。");
    } else if (macroTour_) {
        if (step_ == 0) {
            title = tr("按顺序执行一组按键");
            body = tr("循环连发会依次执行列表中的按键，再按设置重复。点击“添加按键”录入步骤，使用行内箭头调整顺序。");
            hint = tr("每步会按下约 5ms 后松开，间隔是松开后到下一步的时间，默认 %1ms。").arg(kDefaultDelayMs);
            target = QStringLiteral("add");
            actionText = tr("添加步骤");
            canNext = currentMacro && !currentMacro->steps.empty();
            if (canNext) hint = tr("步骤已保存，可以继续设置循环的启停键。");
        } else if (step_ == 1) {
            title = tr("循环使用自己的启停键");
            body = tr("这里的快捷键控制当前循环配置，与按键连发分别启停。两者可以同时运行，暂停键也分别生效。");
            target = QStringLiteral("hotkey");
            actionText = tr("录制循环启停键");
            canNext = currentMacro && currentMacro->triggerKey.has_value();
            hint = canNext ? tr("快捷键已保存。") : tr("选择一个与按键步骤、暂停键不冲突的快捷键。");
        } else {
            title = tr("选择循环的执行方式");
            body = tr("开关模式：按一次启停键开始，再按一次停止。按压模式：按住启停键执行，松开停止。开关模式的循环次数为 0 时会无限循环。");
            hint = tr("完成引导后，由你主动启动。需要修改步骤或设置时，先停止当前循环。");
            target = QStringLiteral("options");
            if (currentMacro && !currentMacro->enabled) {
                hint = tr("当前循环配置已停用，请先启用。完成引导后，再主动启动循环。");
                actionText = tr("启用循环配置");
                canNext = false;
            }
        }
    } else {
        switch (step_) {
        case 0:
            title = tr("确认驱动可用");
            body = tr("DD、HID 是软件驱动，USB 需要连接配套设备。保持当前适合你的模式，状态异常时使用“一键自检”，按提示处理。");
            hint = driverReady ? tr("当前驱动已就绪，可以继续。") : tr("当前驱动尚未就绪。可以先了解配置，驱动修复后再启动连发。");
            target = QStringLiteral("driver");
            actionText = tr("一键自检");
            break;
        case 1:
            title = tr("添加第一个按键");
            body = tr("直接使用顶部的默认配置，点击“添加按键”，在弹窗中录入需要连发的键。键盘和鼠标按键都支持，确认后自动保存。");
            hint = hasBindings ? tr("按键已保存，可以继续查看模式和启用状态。") : tr("按键间隔默认 %1ms，数值越小连点越快；先保留默认值即可。").arg(kDefaultDelayMs);
            target = QStringLiteral("add");
            actionText = tr("添加按键");
            canNext = hasBindings;
            break;
        case 2:
            title = tr("确认模式与启用状态");
            body = tr("开关：启动后持续连点。按压：启动后按住该键才连点。长按：持续保持按下，不会反复点击。独立和切换模式会在选择时补充说明。");
            hint = hasEnabledBinding ? tr("配置和按键已启用。行内开关可单独停用按键，编辑按钮可修改模式和间隔。") : tr("顶部配置开关和至少一个按键的状态开关都需要开启。");
            target = current && !current->enabled ? QStringLiteral("profile") : hasBindings ? QStringLiteral("mode") : QStringLiteral("add");
            actionText = current && !current->enabled ? tr("启用配置") : hasBindings ? tr("编辑第一个按键") : tr("添加按键");
            canNext = hasEnabledBinding;
            break;
        case 3:
            title = tr("设置启停快捷键");
            body = tr("点击这里录制控制工具的快捷键，按一次启动，再按一次停止。它与需要连发的按键是不同用途，请避免冲突。");
            canNext = current && current->controlHotkey.has_value();
            hint = canNext ? tr("启停键已保存：%1。").arg(QString::fromStdWString(GetKeyDisplayName(*current->controlHotkey))) : tr("按下要使用的快捷键即可完成录制，录制期间不会触发连发。");
            target = QStringLiteral("hotkey");
            actionText = tr("录制启停键");
            break;
        case 4:
            title = tr("选择按键在哪里生效");
            body = tr("开启“仅游戏客户端前台时生效”后，切出游戏会暂停并松开按键，切回游戏恢复。已内置识别剑网 3 客户端，其他游戏或模拟器可添加进程。");
            hint = context_.config->gameGateEnabled ? tr("当前已开启：非游戏窗口中即使工具已启动，也不会输出按键。") : tr("当前未开启：按键会发送到当前前台窗口。可按你的使用场景选择。");
            target = QStringLiteral("scope");
            actionText = context_.config->gameGateEnabled ? tr("关闭前台限制") : tr("开启前台限制");
            secondaryText = tr("管理游戏进程");
            break;
        default:
            title = tr("准备好，再主动启动");
            body = tr("完成引导后，切到目标窗口按启停键开始，再按一次停止；右侧也能启动和停止。运行时编辑会锁定，修改前先停止。");
            hint = tr("列表下方可添加暂停键：按住时临时暂停，松开恢复；按压键仍可触发。工具已启动但没有输出时，先看模式和游戏前台限制。");
            target = QStringLiteral("run");
            actionText = tr("添加暂停键（可选）");
            break;
        }
    }
    QVariantMap updated{{"active", active_}, {"visible", active_ && available}, {"startBlocked", blocksStart()}, {"welcome", welcome_},
        {"title", title}, {"body", body}, {"hint", hint}, {"target", target}, {"actionText", actionText},
        {"secondaryText", secondaryText},
        {"nextText", step_ == (macroTour_ ? 2 : 5) ? tr("完成引导") : tr("下一步")},
        {"progress", welcome_ ? tr("首次使用") : macroTour_ ? tr("循环连发 · %1 / 3").arg(step_ + 1) : tr("新手引导 · %1 / 6").arg(step_ + 1)},
        {"canNext", canNext}, {"canPrevious", step_ > 0 && !welcome_}};
    if (state_ != updated) {
        state_ = std::move(updated);
        emit stateChanged();
    }
}

bool OnboardingCoordinator::modeTipSeen(RepeatMode mode) const {
    const int flag = mode == RepeatMode::Independent ? kIndependentTip : mode == RepeatMode::Sequence ? kSequenceTip : 0;
    return !flag || (context_.config->onboardingSeenTips & flag);
}

void OnboardingCoordinator::markModeTipSeen(RepeatMode mode) {
    const int flag = mode == RepeatMode::Independent ? kIndependentTip : mode == RepeatMode::Sequence ? kSequenceTip : 0;
    if (!flag || modeTipSeen(mode)) return;
    context_.config->onboardingSeenTips |= flag;
    persist();
}

void OnboardingCoordinator::prepareKeyEditor(KeyCaptureDialogBase& dialog) {
    dialog.setModeGuideCallbacks([this](RepeatMode mode) { return !modeTipSeen(mode); },
        [this](RepeatMode mode) { markModeTipSeen(mode); });
}
