#include "PreviewDialogModel.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QWindow>
#include <QDateTime>
#include <QWheelEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include "MacroActions.h"
#include "PreviewKeyInput.h"
#include "ui/models/KeyOverviewData.h"
#include "ui/models/PageModel.h"

PreviewDialogModel::PreviewDialogModel(QObject* parent) : QObject(parent) {
    qApp->installEventFilter(this);
    recordingCountdown_.setInterval(1000);
    connect(&recordingCountdown_, &QTimer::timeout, this, [this] {
        int seconds = state_.value("seconds").toInt() - 1;
        edit("seconds", seconds);
        if (seconds > 0) return;
        recordingCountdown_.stop();
        edit("stage", "recording");
        edit("actions", QVariantList{
            QVariantMap{{"id", "stop"}, {"text", tr("停止录制 F10")}, {"primary", true}, {"enabled", true}},
            QVariantMap{{"id", "cancel"}, {"text", tr("取消")}, {"enabled", true}}});
    });
}

void PreviewDialogModel::present(const QString& page, const QString& title, QVariantMap fields,
    std::function<void(const QVariantMap&)> accepted, int width, bool danger) {
    disconnect(overviewConnection_);
    overviewPage_.clear();
    overviewSelection_.clear();
    overviewNavigation_ = {};
    accepted_ = std::move(accepted);
    recording_ = false;
    fields.insert("page", page);
    fields.insert("title", title);
    fields.insert("width", width);
    fields.insert("closeAllowed", true);
    QVariantList actions{QVariantMap{{"id", "accept"}, {"text", accepted_ ? tr("确定") : tr("关闭")},
        {"primary", !danger}, {"danger", danger}, {"enabled", true}}};
    if (accepted_) actions.append(QVariantMap{{"id", "cancel"}, {"text", tr("取消")},
        {"primary", false}, {"danger", false}, {"enabled", true}});
    if (!fields.contains("actions")) fields.insert("actions", actions);
    state_ = std::move(fields);
    emit dialogStateChanged();
    visible_ = true;
    emit visibleChanged();
}

void PreviewDialogModel::message(const QString& title, const QString& text, std::function<void()> accepted) {
    std::function<void(const QVariantMap&)> action;
    if (accepted) action = [accepted = std::move(accepted)](const QVariantMap&) { accepted(); };
    present("DialogMessage.qml", title, {{"message", text}}, std::move(action));
}

void PreviewDialogModel::edit(const QString& field, const QVariant& value) {
    if (overviewPage_ && field == QStringLiteral("selectedKey")) {
        overviewSelection_ = value.toString();
        refreshOverview();
        return;
    }
    state_.insert(field, value);
    if (field == "mode") state_.insert("needsDelay", value.toInt() != 4);
    if (field == "mode" && state_.value("page").toString() == QStringLiteral("DialogBatchEdit.qml")
        && value.toInt() == 4) state_.insert("modifyDelay", false);
    if (field == "format") state_.insert("code", tr("【界面预览示例】\n默认配置\n按键 1：开关模式，10ms\n按键 侧键1：按压模式，10ms"));
    updateBatchEditActions();
    emit dialogStateChanged();
}

void PreviewDialogModel::updateBatchEditActions() {
    if (state_.value("page").toString() != QStringLiteral("DialogBatchEdit.qml")) return;

    const bool macroMode = state_.value("macroMode").toBool();
    bool changed = state_.value("modifyDuration").toBool() || state_.value("modifyDelay").toBool() || state_.value("modifyEnabled").toBool();
    if (!macroMode) {
        changed = changed || state_.value("modifyMode").toBool() || state_.value("modifyPriority").toBool();
    }
    bool validDelay = true;
    bool validDuration = true;
    if (state_.value("modifyDelay").toBool()) {
        bool ok = false;
        const int delay = state_.value("delay").toString().toInt(&ok);
        validDelay = ok && delay >= (macroMode ? 0 : 1);
    }
    if (macroMode && state_.value("modifyDuration").toBool()) {
        bool ok = false;
        const int duration = state_.value("duration").toString().toInt(&ok);
        validDuration = ok && duration >= 1;
    }

    auto actions = state_.value("actions").toList();
    auto confirm = actions[0].toMap();
    confirm.insert("enabled", changed && validDelay && validDuration);
    actions[0] = confirm;
    state_.insert("actions", actions);
}

void PreviewDialogModel::dispatch(const QString& action, const QVariant& argument) {
    if (overviewPage_ && action == QStringLiteral("navigate")) {
        refreshOverview();
        const auto target = KeyOverviewData::action(state_.value("overview").toMap(), argument.toString());
        if (target.isEmpty()) return;
        QTimer::singleShot(0, this, [this, target] {
            if (overviewPage_ && !overviewPage_->state().value("running").toBool()) overviewNavigation_(target);
        });
        return;
    }
    if (state_.value("page").toString() == "DialogMacroRecording.qml") {
        if (action == "start") {
            macroRecording_.clear();
            edit("stage", "countdown");
            edit("seconds", 3);
            edit("count", 0);
            edit("actions", QVariantList{QVariantMap{{"id", "cancel"}, {"text", tr("取消")}, {"enabled", true}}});
            recordingCountdown_.start();
            return;
        }
        if (action == "stop") { finishMacroRecording(); return; }
    }
    if (action == "accept" || action == "cancel" || action == "exit" || action == "tray") {
        recordingCountdown_.stop();
        disconnect(overviewConnection_);
        overviewPage_.clear();
        auto accepted = std::move(accepted_);
        visible_ = false;
        recording_ = false;
        emit visibleChanged();
        if (action == "accept" && accepted) accepted(state_);
    } else if (action == "record") {
        recording_ = true;
        pendingModifierKeys_.clear();
        edit("keyText", tr("请按键或点击鼠标…"));
        edit("captureHint", tr("先按住 Ctrl／Shift／Alt，再按主键；单独使用修饰键时直接松开。"));
    } else if (action == "pick") {
        edit("positionHint", tr("示例固定点位：50%，50%"));
    } else if (action == "dismissModeGuide") {
        edit("modeGuideVisible", false);
    } else if (action == "chooseSound" || action == "resetSound") {
        edit(argument.toBool() ? "soundOn" : "soundOff", action == "chooseSound" ? tr("示例提示音.wav") : QString());
    } else if (action == "add" || action == "selectProcess") {
        const QString name = action == "add" ? state_.value("processName").toString().trimmed() : argument.toString();
        auto processes = state_.value("processes").toList();
        if (!name.isEmpty() && !processes.contains(name)) processes.append(name);
        state_.insert("processName", QString());
        edit("processes", processes);
    } else if (action == "remove") {
        auto processes = state_.value("processes").toList();
        const int index = argument.toInt();
        if (index >= 0 && index < processes.size()) processes.removeAt(index);
        edit("processes", processes);
    } else if (action == "copy") {
        edit("status", tr("已复制（界面预览示例，不访问系统剪贴板）"));
    }
}

void PreviewDialogModel::startWindowMove() {
    if (auto* window = QGuiApplication::focusWindow()) window->startSystemMove();
}

void PreviewDialogModel::startWindowResize(int edges) {
    if (state_.value("resizable").toBool()) {
        if (auto* window = QGuiApplication::focusWindow()) window->startSystemResize(Qt::Edges(edges));
    }
}

void PreviewDialogModel::presentOverview(PageModel& page, bool macroMode, std::function<void(const QVariantMap&)> navigation) {
    QString selected;
    present(QStringLiteral("DialogKeyOverview.qml"), macroMode ? tr("循环按键总览") : tr("按键总览"),
        {{"overview", KeyOverviewData::build(page.state(), macroMode, selected)}, {"resizable", true}, {"actions", QVariantList{}}}, {}, 1100);
    overviewPage_ = &page;
    overviewMacroMode_ = macroMode;
    overviewSelection_ = selected;
    overviewNavigation_ = std::move(navigation);
    overviewConnection_ = connect(&page, &PageModel::stateChanged, this, &PreviewDialogModel::refreshOverview);
}

void PreviewDialogModel::refreshOverview() {
    state_.insert("overview", KeyOverviewData::build(overviewPage_->state(), overviewMacroMode_, overviewSelection_));
    emit dialogStateChanged();
}

bool PreviewDialogModel::eventFilter(QObject* watched, QEvent* event) {
    if (visible_ && state_.value("page").toString() == "DialogMacroRecording.qml"
        && state_.value("stage").toString() == "recording") {
        auto* window = qobject_cast<QQuickWindow*>(watched);
        if (!window) return QObject::eventFilter(watched, event);
        RecordedInput input;
        input.timeMs = static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch());
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            const auto* key = static_cast<QKeyEvent*>(event);
            if (key->isAutoRepeat()) return true;
            if (key->key() == Qt::Key_F10) {
                if (event->type() == QEvent::KeyPress) finishMacroRecording();
                return true;
            }
            input.key = {InputDevice::Keyboard, static_cast<std::uint32_t>(key->key())};
            input.pressed = event->type() == QEvent::KeyPress;
        } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            QQuickItem* item = window->contentItem();
            while (auto* child = item->childAt(item->mapFromScene(mouse->position()).x(),
                item->mapFromScene(mouse->position()).y())) item = child;
            for (; item; item = item->parentItem()) {
                if (item->objectName().startsWith("dialogAction_") || item->objectName() == "dialogCancel")
                    return QObject::eventFilter(watched, event);
            }
            input.key = {InputDevice::Mouse, static_cast<std::uint32_t>(mouse->button())};
            input.pressed = event->type() == QEvent::MouseButtonPress;
            input.position = {true, 0.5, 0.5};
        } else if (event->type() == QEvent::Wheel) input.wheelDelta = static_cast<QWheelEvent*>(event)->angleDelta().y();
        else return QObject::eventFilter(watched, event);
        macroRecording_.append(input, state_.value("positions").toBool());
        edit("count", static_cast<int>(macroRecording_.eventCount()));
        if (macroRecording_.atCapacity()) finishMacroRecording();
        return event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease;
    }
    if (!visible_ || !recording_) return QObject::eventFilter(watched, event);
    const auto isModifierKey = [](int key) {
        return key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt;
    };
    QString key;
    if (event->type() == QEvent::KeyPress) {
        const auto* pressed = static_cast<QKeyEvent*>(event);
        if (pressed->key() == Qt::Key_Escape) { dispatch("cancel"); return true; }
        if (isModifierKey(pressed->key())) {
            pendingModifierKeys_.append(pressed->key());
            edit("keyText", QKeySequence(pressed->keyCombination()).toString(QKeySequence::PortableText)
                + tr(" 已按住，再按主键"));
            return true;
        }
        key = QKeySequence(pressed->keyCombination()).toString(QKeySequence::PortableText);
    } else if (event->type() == QEvent::KeyRelease) {
        const auto* released = static_cast<QKeyEvent*>(event);
        if (!isModifierKey(released->key()) || pendingModifierKeys_.isEmpty()) {
            return QObject::eventFilter(watched, event);
        }
        pendingModifierKeys_.removeAll(released->key());
        if (!pendingModifierKeys_.isEmpty()) return true;
        key = QKeySequence(released->key()).toString(QKeySequence::PortableText);
    } else if (event->type() == QEvent::MouseButtonPress) {
        const auto* pressed = static_cast<QMouseEvent*>(event);
        QString prefix;
        if ((pressed->modifiers() & Qt::ControlModifier) != 0) prefix += QStringLiteral("Ctrl+");
        if ((pressed->modifiers() & Qt::ShiftModifier) != 0) prefix += QStringLiteral("Shift+");
        if ((pressed->modifiers() & Qt::AltModifier) != 0) prefix += QStringLiteral("Alt+");
        switch (pressed->button()) {
        case Qt::LeftButton: key = tr("左键"); break;
        case Qt::RightButton: key = tr("右键"); break;
        case Qt::MiddleButton: key = tr("中键"); break;
        case Qt::XButton1: key = tr("侧键1"); break;
        case Qt::XButton2: key = tr("侧键2"); break;
        default: return false;
        }
        key = prefix + key;
    }
    if (key.isEmpty()) return QObject::eventFilter(watched, event);
    recording_ = false;
    pendingModifierKeys_.clear();
    state_.insert("isMouse", event->type() == QEvent::MouseButtonPress);
    const KeyDescriptor descriptor = event->type() == QEvent::MouseButtonPress
        ? PreviewMouseKey(static_cast<QMouseEvent*>(event)->button(), static_cast<QMouseEvent*>(event)->modifiers())
        : PreviewKeyboardKey(static_cast<QKeyEvent*>(event)->key(), static_cast<QKeyEvent*>(event)->modifiers());
    state_.insert("keyData", KeyOverviewData::key(descriptor, key));
    edit("keyText", key);
    return true;
}

void PreviewDialogModel::finishMacroRecording() {
    edit("stage", "review");
    const auto steps = macroRecording_.finish(static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch()));
    QVariantList rows;
    QStringList descriptions;
    for (const auto& step : steps) {
        QString key = step.key.device == InputDevice::Keyboard
            ? QKeySequence(static_cast<int>(step.key.code)).toString(QKeySequence::PortableText) : tr("鼠标键");
        const QString actionText = QString::fromStdWString(MacroStepText(step, key.toStdWString()));
        auto descriptor = step.key.device == InputDevice::Keyboard
            ? PreviewKeyboardKey(static_cast<int>(step.key.code))
            : PreviewMouseKey(static_cast<Qt::MouseButton>(step.key.code));
        descriptor.modifiers = step.key.modifiers;
        descriptions.append(actionText);
        rows.push_back(QVariantMap{{"enabled", true}, {"rawKeyText", key}, {"keyText", actionText},
            {"key", KeyOverviewData::key(descriptor, key)},
            {"action", static_cast<int>(step.action)}, {"duration", step.durationMs}, {"wheelDelta", step.wheelDelta},
            {"delay", step.delayAfterMs}, {"name", QString()}, {"positionHint", QString()}});
    }
    edit("recordedRows", rows);
    edit("steps", descriptions);
    edit("summary", tr("录制完成 · %1 步（仅预览窗口内输入）").arg(rows.size()));
    edit("actions", QVariantList{
        QVariantMap{{"id", "accept"}, {"text", tr("追加到循环")}, {"primary", true}, {"enabled", !rows.empty()}},
        QVariantMap{{"id", "start"}, {"text", tr("重新录制")}, {"enabled", true}},
        QVariantMap{{"id", "cancel"}, {"text", tr("取消")}, {"enabled", true}}});
}
