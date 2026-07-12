#include "dialogs/CloseConfirmDialog.h"

#include "dialogs/DialogChrome.h"
#include "ui/components/FluentCheckBox.h"
#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPushButton>
#include <QVBoxLayout>

CloseConfirmDialog::CloseConfirmDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QString::fromWCharArray(L"\u5173\u95ed\u786e\u8ba4"));
    DialogChrome::apply(this);
    setFixedSize(360, 204);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(10);

    auto* titleRow = new QHBoxLayout();
    auto* titleLabel = new QLabel(windowTitle(), this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(DialogChrome::createCloseButton(this));
    layout->addLayout(titleRow);

    auto* label = new QLabel(QString::fromWCharArray(L"\u60a8\u5e0c\u671b\u5173\u95ed\u7a0b\u5e8f\u8fd8\u662f\u6700\u5c0f\u5316\u5230\u7cfb\u7edf\u6258\u76d8\uff1f"), this);
    label->setFont(Fluent::Typography::body());
    label->setWordWrap(true);
    layout->addWidget(label);

    auto* rememberCheck = new FluentCheckBox(QString::fromWCharArray(L"\u8bb0\u4f4f\u6211\u7684\u9009\u62e9"), this);
    rememberCheck->setFont(Fluent::Typography::body());
    layout->addWidget(rememberCheck);
    layout->addStretch();

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* minimizeBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u6700\u5c0f\u5316\u5230\u6258\u76d8"), this);
    auto* exitBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u9000\u51fa\u7a0b\u5e8f"), this);
    minimizeBtn->setFixedWidth(120);
    exitBtn->setFixedWidth(100);
    btnLayout->addWidget(minimizeBtn);
    btnLayout->addSpacing(8);
    btnLayout->addWidget(exitBtn);
    layout->addLayout(btnLayout);

    connect(minimizeBtn, &QPushButton::clicked, this, [this, rememberCheck]() {
        exitSelected_ = false;
        rememberSelected_ = rememberCheck->isChecked();
        accept();
    });
    connect(exitBtn, &QPushButton::clicked, this, [this, rememberCheck]() {
        exitSelected_ = true;
        rememberSelected_ = rememberCheck->isChecked();
        accept();
    });
}

CloseConfirmResult CloseConfirmDialog::decision() const {
    return {exitSelected_, rememberSelected_};
}

void CloseConfirmDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void CloseConfirmDialog::mousePressEvent(QMouseEvent* event) {
    if (DialogChrome::mousePress(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
    QDialog::mousePressEvent(event);
}

void CloseConfirmDialog::mouseMoveEvent(QMouseEvent* event) {
    if (DialogChrome::mouseMove(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
    QDialog::mouseMoveEvent(event);
}

void CloseConfirmDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (DialogChrome::mouseRelease(event, dragging_)) return;
    QDialog::mouseReleaseEvent(event);
}
