#include "coordinators/OnboardingCoordinator.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/KeyCaptureDialogBase.h"
#include "dialogs/MacroStepDialog.h"
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
constexpr int kToggleTip = 8;
constexpr int kHoldTip = 16;
constexpr int kLongPressTip = 32;

int modeTipFlag(RepeatMode mode) {
    switch (mode) {
    case RepeatMode::Toggle: return kToggleTip;
    case RepeatMode::Hold: return kHoldTip;
    case RepeatMode::Independent: return kIndependentTip;
    case RepeatMode::Sequence: return kSequenceTip;
    case RepeatMode::LongPress: return kLongPressTip;
    default: return 0;
    }
}

int actionTipFlag(MacroAction action) {
    const int index = static_cast<int>(action);
    return index >= 0 && index <= static_cast<int>(MacroAction::Wheel) ? 1 << index : 0;
}
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
        AppMessageDialog::information(context_.window, tr("新手引导"), tr("请先停止连发，关掉正在编辑的窗口，再打开新手引导。"));
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
    context_.config->onboardingSeenActions = 0;
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
        body = tr("带你设置一个自动连点的按键，再学会怎样开始、暂停和停止。设置会自动保存，不想看时可以随时跳过。");
        hint = driverReady ? tr("已经可以使用，先来添加按键。") : tr("先看看驱动页有没有报错，再来添加按键。这里有问题时，也可以先学习后面的操作。");
    } else if (macroTour_) {
        if (step_ == 0) {
            title = tr("按顺序执行一组按键");
            body = tr("例如先按 1、再按 2，就按这个顺序添加两条步骤。循环运行时，会从上到下执行这些步骤。想换顺序，拖动每行左侧的拖动图标即可。");
            hint = tr("步骤里的“间隔”就是做完后等多久。1000 毫秒是 1 秒，填得越小，两步之间等得越短。");
            target = QStringLiteral("add");
            actionText = tr("添加步骤");
            canNext = currentMacro && !currentMacro->steps.empty();
            if (canNext) hint = tr("步骤已保存，接下来设置一个启动和停止循环的快捷键。");
        } else if (step_ == 1) {
            title = tr("给循环设置启动和停止键");
            body = tr("点下面的按钮，再按你想用的快捷键。它只控制“循环连发”，不会把“按键连发”一起启动或停止。");
            target = QStringLiteral("hotkey");
            actionText = tr("录制循环启停键");
            canNext = currentMacro && currentMacro->triggerKey.has_value();
            hint = canNext ? tr("快捷键已保存，可以继续。") : tr("选一个循环步骤里不会用到的键，也不要和暂停键重复。");
        } else {
            title = tr("选择怎么开始、重复几次");
            body = tr("“开关模式”：按一下快捷键开始，再按一下停止。\n“按压模式”：一直按着才运行，松开就停。\n“循环次数”填 0，就是一直重复，直到你手动停止。");
            hint = tr("完成引导后，你再自己启动。想改步骤或设置，先停止循环。");
            target = QStringLiteral("options");
            if (currentMacro && !currentMacro->enabled) {
                hint = tr("这个循环还没打开，请先点“启用循环配置”。完成引导后，你再自己启动。");
                actionText = tr("启用循环配置");
                canNext = false;
            }
        }
    } else {
        switch (step_) {
        case 0:
            title = tr("先确认软件可以按键");
            body = tr("这里决定软件怎样帮你按键，先保留当前模式即可。有报错时点“一键自检”，跟着结果提示处理。选择 USB 时，需要插上配套设备。");
            hint = driverReady ? tr("已经准备好，可以继续。") : tr("这里还没准备好。可以先学习后面的操作，开始连点前需要处理好这里的问题。");
            target = QStringLiteral("driver");
            actionText = tr("一键自检");
            break;
        case 1:
            title = tr("添加第一个按键");
            body = tr("点击“添加按键”，再点“点击录制”，按一下想要自动连点的键，比如 1。确认后会自动保存，鼠标按键也可以录制。");
            hint = hasBindings ? tr("第一个按键已经保存，接下来看看它是否打开。") : tr("第一次先保留默认间隔 %1 毫秒。以后想点得慢一点，就把间隔调大。").arg(kDefaultDelayMs);
            target = QStringLiteral("add");
            actionText = tr("添加按键");
            canNext = hasBindings;
            break;
        case 2:
            title = tr("看看按键怎么运行");
            body = tr("第一次可以先选“开关模式”：启动后自动连点，停止后结束。想用其他模式时再切换，弹窗里会用例子告诉你怎么操作。");
            hint = hasEnabledBinding ? tr("已经打开，可以继续。想修改按键，点这一行的编辑图标。") : tr("请打开顶部配置旁的开关，再打开列表里至少一个按键的开关。");
            target = current && !current->enabled ? QStringLiteral("profile") : hasBindings ? QStringLiteral("mode") : QStringLiteral("add");
            actionText = current && !current->enabled ? tr("启用配置") : hasBindings ? tr("编辑第一个按键") : tr("添加按键");
            canNext = hasEnabledBinding;
            break;
        case 3:
            title = tr("设置一个启动和停止键");
            body = tr("点下面的按钮，再按一下想用的快捷键。以后按它一下就开始连点，再按一下就停止。选一个和要连点的键不同的键，方便区分。");
            canNext = current && current->controlHotkey.has_value();
            hint = canNext ? tr("已设为 %1，可以继续。").arg(QString::fromStdWString(GetKeyDisplayName(*current->controlHotkey))) : tr("按一下你想用的快捷键即可。录制时不会真的开始连点。");
            target = QStringLiteral("hotkey");
            actionText = tr("录制启停键");
            break;
        case 4:
            title = tr("切出游戏时要不要暂停");
            body = tr("打开“仅游戏客户端前台时生效”：切去聊天或浏览器时会暂停，回到游戏再继续。剑网3已内置，其他游戏可以点“管理游戏进程”添加。");
            hint = context_.config->gameGateEnabled ? tr("现在已打开：切去聊天或浏览器时，不会帮你按键。") : tr("现在未打开：软件会对你正在使用的窗口按键。只在游戏里用，建议打开。");
            target = QStringLiteral("scope");
            actionText = context_.config->gameGateEnabled ? tr("关闭这项限制") : tr("开启这项限制");
            secondaryText = tr("管理游戏进程");
            break;
        default:
            title = tr("完成后，去游戏里试试");
            body = tr("完成引导后，切到你想使用的窗口，按刚才设置的快捷键开始，再按一次停止。也可以点右侧“启动”或“停止”。要改设置，先停止连发。");
            hint = tr("还可以在列表下方添加“暂停键”：按住时暂停，松开后继续。不知道选什么时，可以以后再设置。");
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
    const int flag = modeTipFlag(mode);
    return !flag || (context_.config->onboardingSeenTips & flag);
}

void OnboardingCoordinator::markModeTipSeen(RepeatMode mode) {
    const int flag = modeTipFlag(mode);
    if (!flag || modeTipSeen(mode)) return;
    context_.config->onboardingSeenTips |= flag;
    persist();
}

void OnboardingCoordinator::prepareKeyEditor(KeyCaptureDialogBase& dialog) {
    dialog.setModeGuideCallbacks([this](RepeatMode mode) { return !modeTipSeen(mode); },
        [this](RepeatMode mode) { markModeTipSeen(mode); });
    if (auto* stepDialog = qobject_cast<MacroStepDialog*>(&dialog)) {
        stepDialog->setActionGuideCallbacks([this](MacroAction action) { return !actionTipSeen(action); },
            [this](MacroAction action) { markActionTipSeen(action); });
    }
}

bool OnboardingCoordinator::actionTipSeen(MacroAction action) const {
    const int flag = actionTipFlag(action);
    return !flag || (context_.config->onboardingSeenActions & flag);
}

void OnboardingCoordinator::markActionTipSeen(MacroAction action) {
    const int flag = actionTipFlag(action);
    if (!flag || actionTipSeen(action)) return;
    context_.config->onboardingSeenActions |= flag;
    persist();
}
