#include "dialogs/KeyCaptureDialogBase.h"
#include "dialogs/AppMessageDialog.h"
#include "input/InputKeyUtils.h"
#include "ui/components/ScreenPointPicker.h"
#include <Windows.h>
#include <QGuiApplication>
#include <QScreen>
#include <algorithm>
KeyCaptureDialogBase::KeyCaptureDialogBase(QWidget* parent) : QmlDialog(parent) {}
void KeyCaptureDialogBase::prepareEditor(QVariantMap fields, std::function<bool()> supportsMouseMove,
    const QSize& initialSize) {
    supportsMouseMove_ = std::move(supportsMouseMove);
    const QVariantMap defaults{{"keyText", capturedKey_ ? QString::fromStdWString(GetKeyDisplayName(*capturedKey_)) : tr("点击录制")},
        {"captureHint", tr("组合键如 Ctrl+1，先按住 Ctrl 再按 1；只录制 Ctrl，按一下再松开即可。")},
        {"isMouse", capturedKey_ && capturedKey_->device == InputDevice::Mouse},
        {"positionVisible", true},
        {"supportsPosition", supportsMouseMove_ && supportsMouseMove_()}, {"positionHint", tr("未拾取")},
        {"fixed", false}, {"modeVisible", false}, {"needsDelay", true}, {"mode", 0},
        {"enabled", true}, {"priority", true}, {"modeGuideVisible", false},
        {"nameVisible", false}, {"name", QString()},
        {"delay", QString::number(kDefaultDelayMs)}};
    QVariantMap values = defaults;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) values.insert(it.key(), it.value());
    const bool canPosition = capturedKey_ && CanUseClickPosition(*capturedKey_);
    values.insert("canPosition", canPosition);
    clickPos_.fixedPosition = values.value("fixed").toBool();
    clickPos_.anchorX = values.take("anchorX").toDouble();
    clickPos_.anchorY = values.take("anchorY").toDouble();
    if (!canPosition) {
        clickPos_ = {};
        values.insert("fixed", false);
        values.insert("positionHint", tr("未拾取"));
    }
    if (clickPos_.IsFixed()) values.insert("positionHint", tr("屏幕内：%1%，%2%").arg(clickPos_.anchorX * 100, 0, 'f', 1).arg(clickPos_.anchorY * 100, 0, 'f', 1));
    values.insert("actions", QVariantList{button("accept", tr("确定")), button("cancel", tr("取消"), false)});
    present("DialogKeyEditor.qml", values, initialSize, true);
}
void KeyCaptureDialogBase::setCapturedKey(KeyDescriptor key) {
    capturedKey_ = key;
    setValue("keyText", QString::fromStdWString(GetKeyDisplayName(key)));
    setValue("isMouse", key.device == InputDevice::Mouse);
    const bool canPosition = CanUseClickPosition(key);
    setValue("canPosition", canPosition);
    if (!canPosition) {
        clickPos_ = {};
        setValue("fixed", false);
        setValue("positionHint", tr("未拾取"));
    }
}
void KeyCaptureDialogBase::handleCapturedKey(quint64 encodedKey) {
    const auto key = DecodeKey(encodedKey);
    if (IsKeyboardModifierKey(key)) {
        showModifierPending(key);
        return;
    }
    if (!allowPrimaryMouseButtons_ && key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
        setValue("keyText", capturedKey_ ? QString::fromStdWString(GetKeyDisplayName(*capturedKey_)) : tr("点击录制"));
        return;
    }
    setCapturedKey(key);
    emit keyAccepted();
}

void KeyCaptureDialogBase::showModifierPending(const KeyDescriptor& key) {
    setValue("keyText", QString::fromStdWString(ModifierPrefix(ModifierMaskFromKey(key)))
        + tr("已按住，再按另一个键"));
    setValue("captureHint", tr("再按一个键，就能录成组合键。只想录制刚才按住的 Ctrl、Shift 或 Alt，直接松开即可。"));
}

void KeyCaptureDialogBase::handleModifierReleased(KeyDescriptor key) {
    setCapturedKey(key);
    emit keyAccepted();
}
ClickPosition KeyCaptureDialogBase::clickPosition() const {
    return capturedKey_ && CanUseClickPosition(*capturedKey_) && value("fixed").toBool() ? clickPos_ : ClickPosition{};
}
void KeyCaptureDialogBase::editField(const QString& field, const QVariant& argument) {
    if (field == "delay") setValue(field, argument.toString());
    else if (field == "name") setValue(field, argument.toString());
    else if (field == "mode" && argument.toInt() >= 0
        && argument.toInt() <= static_cast<int>(RepeatMode::LongPress)) {
        setValue(field, argument.toInt());
        setValue("needsDelay", argument.toInt() != static_cast<int>(RepeatMode::LongPress));
        updateModeGuide();
    } else if (field == "enabled" || field == "priority" || field == "martialAssist" || field == "preventAccidentalStop") {
        setValue(field, argument.toBool());
    } else if (field == "fixed") {
        if (argument.toBool() && (!capturedKey_ || !CanUseClickPosition(*capturedKey_)
            || !(supportsMouseMove_ && supportsMouseMove_()))) {
            emit dialogStateChanged();
            return;
        }
        if (!argument.toBool()) {
            clickPos_.fixedPosition = false;
            setValue("positionHint", tr("未拾取"));
        }
        setValue(field, argument.toBool());
    }
}
void KeyCaptureDialogBase::handleAction(const QString& action, const QVariant& argument) {
    if (action == "record") {
        setValue("keyText", tr("录制中"));
        setValue("captureHint", tr("组合键如 Ctrl+1，先按住 Ctrl 再按 1；只录制 Ctrl，按一下再松开即可。"));
        emit keyCaptureRequested();
    } else if (action == "pick" && capturedKey_ && CanUseClickPosition(*capturedKey_)
        && supportsMouseMove_ && supportsMouseMove_() && value("fixed").toBool()) {
        const auto point = ScreenPointPicker::pick(this);
        if (!point) return;
        const auto* screen = QGuiApplication::screenAt(*point);
        if (!screen) return;
        const QRect area = screen->geometry();
        clickPos_.fixedPosition = true;
        clickPos_.anchorX = std::clamp(double(point->x() - area.left()) / area.width(), 0.0, 1.0);
        clickPos_.anchorY = std::clamp(double(point->y() - area.top()) / area.height(), 0.0, 1.0);
        setValue("positionHint", tr("屏幕内：%1%，%2%").arg(clickPos_.anchorX * 100, 0, 'f', 1).arg(clickPos_.anchorY * 100, 0, 'f', 1));
    } else if (action == "dismissModeGuide") {
        dismissModeGuide();
    } else if (action == "accept") {
        if (!capturedKey_ || !capturedKey_->IsValid()) {
            AppMessageDialog::warning(this, tr("参数无效"), tr("请先录制一个按键或鼠标键。"));
            return;
        }
        if (value("needsDelay").toBool()) {
            bool ok = false;
            const int delay = value("delay").toString().toInt(&ok);
            if (!ok || delay < kMinDelayMs) {
                AppMessageDialog::warning(this, tr("参数无效"),
                    value("macroStep").toBool()
                        ? tr("松开后间隔必须是不小于 1 的整数。")
                        : tr("间隔必须是不小于 1 的整数。"));
                return;
            }
        }
        dismissModeGuide();
        accept();
    } else QmlDialog::handleAction(action, argument);
}

void KeyCaptureDialogBase::setModeGuideCallbacks(std::function<bool(RepeatMode)> shouldShow,
    std::function<void(RepeatMode)> dismissed) {
    shouldShowModeGuide_ = std::move(shouldShow);
    modeGuideDismissed_ = std::move(dismissed);
    updateModeGuide();
}

void KeyCaptureDialogBase::updateModeGuide() {
    const auto mode = static_cast<RepeatMode>(value("mode").toInt());
    setValue("modeGuideVisible", value("modeVisible").toBool()
        && shouldShowModeGuide_ && shouldShowModeGuide_(mode));
}

void KeyCaptureDialogBase::dismissModeGuide() {
    if (!value("modeGuideVisible").toBool()) return;
    if (modeGuideDismissed_) modeGuideDismissed_(static_cast<RepeatMode>(value("mode").toInt()));
    setValue("modeGuideVisible", false);
}
