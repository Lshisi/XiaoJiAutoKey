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
        {"isMouse", capturedKey_ && capturedKey_->device == InputDevice::Mouse},
        {"positionVisible", true},
        {"supportsPosition", supportsMouseMove_ && supportsMouseMove_()}, {"positionHint", tr("未拾取")},
        {"fixed", false}, {"modeVisible", false}, {"needsDelay", true}, {"mode", 0},
        {"enabled", true}, {"priority", true}, {"delay", QString::number(kDefaultDelayMs)}};
    QVariantMap values = defaults;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) values.insert(it.key(), it.value());
    clickPos_.fixedPosition = values.value("fixed").toBool();
    clickPos_.anchorX = values.take("anchorX").toDouble();
    clickPos_.anchorY = values.take("anchorY").toDouble();
    if (clickPos_.IsFixed()) values.insert("positionHint", tr("屏幕内：%1%，%2%").arg(clickPos_.anchorX * 100, 0, 'f', 1).arg(clickPos_.anchorY * 100, 0, 'f', 1));
    values.insert("actions", QVariantList{button("accept", tr("确定")), button("cancel", tr("取消"), false)});
    present("DialogKeyEditor.qml", values, initialSize, true);
}
void KeyCaptureDialogBase::setCapturedKey(KeyDescriptor key) {
    capturedKey_ = key;
    setValue("keyText", QString::fromStdWString(GetKeyDisplayName(key)));
    setValue("isMouse", key.device == InputDevice::Mouse);
}
void KeyCaptureDialogBase::handleCapturedKey(quint64 encodedKey) {
    const auto key = DecodeKey(encodedKey);
    if (!allowPrimaryMouseButtons_ && key.device == InputDevice::Mouse && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
        setValue("keyText", capturedKey_ ? QString::fromStdWString(GetKeyDisplayName(*capturedKey_)) : tr("点击录制"));
        return;
    }
    setCapturedKey(key);
    emit keyAccepted();
}
ClickPosition KeyCaptureDialogBase::clickPosition() const {
    return capturedKey_ && capturedKey_->device == InputDevice::Mouse && value("fixed").toBool() ? clickPos_ : ClickPosition{};
}
void KeyCaptureDialogBase::editField(const QString& field, const QVariant& argument) {
    if (field == "delay") setValue(field, argument.toString());
    else if (field == "mode" && argument.toInt() >= 0
        && argument.toInt() <= static_cast<int>(RepeatMode::LongPress)) {
        setValue(field, argument.toInt());
        setValue("needsDelay", argument.toInt() != static_cast<int>(RepeatMode::LongPress));
    } else if (field == "enabled" || field == "priority") setValue(field, argument.toBool());
    else if (field == "fixed") {
        if (argument.toBool() && !(supportsMouseMove_ && supportsMouseMove_())) {
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
        emit keyCaptureRequested();
    } else if (action == "pick" && supportsMouseMove_ && supportsMouseMove_() && value("fixed").toBool()) {
        const auto point = ScreenPointPicker::pick(this);
        if (!point) return;
        const auto* screen = QGuiApplication::screenAt(*point);
        if (!screen) return;
        const QRect area = screen->geometry();
        clickPos_.fixedPosition = true;
        clickPos_.anchorX = std::clamp(double(point->x() - area.left()) / area.width(), 0.0, 1.0);
        clickPos_.anchorY = std::clamp(double(point->y() - area.top()) / area.height(), 0.0, 1.0);
        setValue("positionHint", tr("屏幕内：%1%，%2%").arg(clickPos_.anchorX * 100, 0, 'f', 1).arg(clickPos_.anchorY * 100, 0, 'f', 1));
    } else if (action == "accept") {
        if (!capturedKey_ || !capturedKey_->IsValid()) {
            AppMessageDialog::warning(this, tr("参数无效"), tr("请先录制一个按键或鼠标键。"));
            return;
        }
        if (value("needsDelay").toBool()) {
            bool ok = false;
            const int delay = value("delay").toString().toInt(&ok);
            if (!ok || delay < kMinDelayMs) {
                AppMessageDialog::warning(this, tr("参数无效"), tr("间隔必须是不小于 1 的整数。"));
                return;
            }
        }
        accept();
    } else QmlDialog::handleAction(action, argument);
}
