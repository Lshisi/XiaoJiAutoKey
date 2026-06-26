#include "dialogs/KeyCaptureDialogBase.h"

#include "dialogs/DialogChrome.h"
#include "input/InputKeyUtils.h"
#include "ui/components/FluentLineEdit.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <Windows.h>

KeyCaptureDialogBase::KeyCaptureDialogBase(QWidget* parent)
    : QDialog(parent) {
    DialogChrome::apply(this);
}

void KeyCaptureDialogBase::handleCapturedKey(quint64 encodedKey) {
    KeyDescriptor key = DecodeKey(encodedKey);
    if (!allowPrimaryMouseButtons_
        && key.device == InputDevice::Mouse
        && (key.code == VK_LBUTTON || key.code == VK_RBUTTON)) {
        if (keyEdit_ && capturedKey_.has_value()) {
            keyEdit_->setText(QString::fromStdWString(GetKeyDisplayName(*capturedKey_)));
        } else if (keyEdit_) {
            keyEdit_->setText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
        }
        return;
    }
    setCapturedKey(key);
    emit keyAccepted();
}

std::optional<KeyDescriptor> KeyCaptureDialogBase::capturedKey() const {
    return capturedKey_;
}

void KeyCaptureDialogBase::setPrimaryMouseButtonsAllowed(bool allowed) {
    allowPrimaryMouseButtons_ = allowed;
}

void KeyCaptureDialogBase::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void KeyCaptureDialogBase::mousePressEvent(QMouseEvent* event) {
    if (DialogChrome::mousePress(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
    QDialog::mousePressEvent(event);
}

void KeyCaptureDialogBase::mouseMoveEvent(QMouseEvent* event) {
    if (DialogChrome::mouseMove(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
    QDialog::mouseMoveEvent(event);
}

void KeyCaptureDialogBase::mouseReleaseEvent(QMouseEvent* event) {
    if (DialogChrome::mouseRelease(event, dragging_)) return;
    QDialog::mouseReleaseEvent(event);
}

void KeyCaptureDialogBase::setCaptureEditor(FluentLineEdit* editor) {
    keyEdit_ = editor;
    keyEdit_->installEventFilter(this);
    keyEdit_->setReadOnly(true);
    connect(keyEdit_, &FluentLineEdit::clicked, this, [this]() {
        if (keyEdit_) keyEdit_->setText(QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e..."));
        emit keyCaptureRequested();
    });
}

void KeyCaptureDialogBase::setCapturedKey(KeyDescriptor key) {
    capturedKey_ = key;
    if (keyEdit_) keyEdit_->setText(QString::fromStdWString(GetKeyDisplayName(key)));
}
