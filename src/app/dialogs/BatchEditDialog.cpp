#include "dialogs/BatchEditDialog.h"

BatchEditDialog::BatchEditDialog(bool macroMode, int count, QWidget* parent)
    : QmlDialog(parent), macroMode_(macroMode) {
    setWindowTitle(macroMode ? tr("批量修改步骤") : tr("批量修改按键"));
    QVariantMap fields{{"macroMode", macroMode}, {"count", count},
        {"modifyDelay", false}, {"delay", QString::number(kDefaultDelayMs)},
        {"modifyDuration", false}, {"duration", QStringLiteral("500")},
        {"modifyEnabled", false}, {"enabled", true},
        {"modifyMode", false}, {"mode", 0},
        {"modifyPriority", false}, {"priority", true},
        {"actions", QVariantList{button("accept", tr("确定")), button("cancel", tr("取消"), false)}}};
    present("DialogBatchEdit.qml", fields, macroMode ? QSize(360, 220) : QSize(420, 340), true);
    updateAcceptAction();
}

std::optional<bool> BatchEditDialog::enabled() const {
    return !value("modifyEnabled").toBool() ? std::nullopt : std::optional<bool>(value("enabled").toBool());
}

std::optional<RepeatMode> BatchEditDialog::mode() const {
    if (macroMode_ || !value("modifyMode").toBool()) return std::nullopt;
    switch (value("mode").toInt()) {
    case 1: return RepeatMode::Hold;
    case 2: return RepeatMode::Independent;
    case 3: return RepeatMode::Sequence;
    case 4: return RepeatMode::LongPress;
    default: return RepeatMode::Toggle;
    }
}

std::optional<int> BatchEditDialog::delayMs() const {
    if (!value("modifyDelay").toBool()) return std::nullopt;
    bool ok = false;
    const int delay = value("delay").toString().toInt(&ok);
    return ok && delay >= (macroMode_ ? 0 : kMinDelayMs) ? std::optional<int>(delay) : std::nullopt;
}

std::optional<bool> BatchEditDialog::participatePriority() const {
    return macroMode_ || !value("modifyPriority").toBool()
        ? std::nullopt : std::optional<bool>(value("priority").toBool());
}

void BatchEditDialog::handleAction(const QString& action, const QVariant& value) {
    if (action == QStringLiteral("accept") && canAccept()) accept();
    else QmlDialog::handleAction(action, value);
}

void BatchEditDialog::editField(const QString& field, const QVariant& argument) {
    if (field == QStringLiteral("mode")) {
        setValue(field, argument.toInt());
        if (argument.toInt() == static_cast<int>(RepeatMode::LongPress)) setValue("modifyDelay", false);
    } else if (field == QStringLiteral("delay") || field == QStringLiteral("duration")) {
        setValue(field, argument.toString());
    } else {
        setValue(field, argument.toBool());
    }
    updateAcceptAction();
}

bool BatchEditDialog::canAccept() const {
    bool changed = value("modifyDuration").toBool() || value("modifyDelay").toBool() || value("modifyEnabled").toBool();
    if (!macroMode_) {
        changed = changed || value("modifyMode").toBool() || value("modifyPriority").toBool();
    }
    if (!changed) return false;
    if (value("modifyDuration").toBool()) {
        bool parsed = false;
        const int duration = value("duration").toString().toInt(&parsed);
        if (!parsed || duration < 1) return false;
    }
    if (!value("modifyDelay").toBool()) return true;
    bool ok = false;
    const int delay = value("delay").toString().toInt(&ok);
    return ok && delay >= (macroMode_ ? 0 : kMinDelayMs);
}

void BatchEditDialog::updateAcceptAction() {
    auto actions = value("actions").toList();
    auto confirm = actions[0].toMap();
    confirm.insert("enabled", canAccept());
    actions[0] = confirm;
    setValue("actions", actions);
}

std::optional<int> BatchEditDialog::durationMs() const {
    if (!macroMode_ || !value("modifyDuration").toBool()) return std::nullopt;
    return value("duration").toString().toInt();
}
