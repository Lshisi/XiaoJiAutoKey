#include "PreviewDialogModel.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QWindow>

PreviewDialogModel::PreviewDialogModel(QObject* parent) : QObject(parent) {
    qApp->installEventFilter(this);
}

void PreviewDialogModel::present(const QString& page, const QString& title, QVariantMap fields,
    std::function<void(const QVariantMap&)> accepted, int width, bool danger) {
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
    fields.insert("actions", actions);
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
    bool changed = state_.value("modifyDelay").toBool() || state_.value("modifyEnabled").toBool();
    if (!macroMode) {
        changed = changed || state_.value("modifyMode").toBool() || state_.value("modifyPriority").toBool();
    }
    bool validDelay = true;
    if (state_.value("modifyDelay").toBool()) {
        bool ok = false;
        const int delay = state_.value("delay").toString().toInt(&ok);
        validDelay = ok && delay >= 1;
    }

    auto actions = state_.value("actions").toList();
    auto confirm = actions[0].toMap();
    confirm.insert("enabled", changed && validDelay);
    actions[0] = confirm;
    state_.insert("actions", actions);
}

void PreviewDialogModel::dispatch(const QString& action, const QVariant& argument) {
    if (action == "accept" || action == "cancel" || action == "exit" || action == "tray") {
        auto accepted = std::move(accepted_);
        visible_ = false;
        recording_ = false;
        emit visibleChanged();
        if (action == "accept" && accepted) accepted(state_);
    } else if (action == "record") {
        recording_ = true;
        edit("keyText", tr("请按键或点击鼠标…"));
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
    }
}

void PreviewDialogModel::startWindowMove() {
    if (auto* window = QGuiApplication::focusWindow()) window->startSystemMove();
}

bool PreviewDialogModel::eventFilter(QObject* watched, QEvent* event) {
    if (!visible_ || !recording_) return QObject::eventFilter(watched, event);
    QString key;
    if (event->type() == QEvent::KeyPress) {
        const auto* pressed = static_cast<QKeyEvent*>(event);
        if (pressed->key() == Qt::Key_Escape) { dispatch("cancel"); return true; }
        key = QKeySequence(pressed->keyCombination()).toString(QKeySequence::PortableText);
    } else if (event->type() == QEvent::MouseButtonPress) {
        switch (static_cast<QMouseEvent*>(event)->button()) {
        case Qt::LeftButton: key = tr("左键"); break;
        case Qt::RightButton: key = tr("右键"); break;
        case Qt::MiddleButton: key = tr("中键"); break;
        case Qt::XButton1: key = tr("侧键1"); break;
        case Qt::XButton2: key = tr("侧键2"); break;
        default: return false;
        }
    }
    if (key.isEmpty()) return QObject::eventFilter(watched, event);
    recording_ = false;
    state_.insert("isMouse", event->type() == QEvent::MouseButtonPress);
    edit("keyText", key);
    return true;
}
