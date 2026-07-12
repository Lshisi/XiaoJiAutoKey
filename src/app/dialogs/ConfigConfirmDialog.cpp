#include "dialogs/ConfigConfirmDialog.h"

#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

ConfigConfirmDialog::ConfigConfirmDialog(const QString& title,
    const QString& message,
    const QString& confirmText,
    QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    DialogChrome::apply(this);
    setModal(true);
    setFixedSize(380, 176);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(12);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleLabel->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textPrimary().name()));
    root->addWidget(titleLabel);

    auto* messageLabel = new QLabel(message, this);
    messageLabel->setFont(Fluent::Typography::body());
    messageLabel->setWordWrap(true);
    messageLabel->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textSecondary().name()));
    root->addWidget(messageLabel, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* cancelButton = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), this);
    auto* confirmButton = new FluentButton(FluentButton::Danger, confirmText, this);
    cancelButton->setFixedWidth(78);
    confirmButton->setFixedWidth(92);
    buttons->addWidget(cancelButton);
    buttons->addSpacing(8);
    buttons->addWidget(confirmButton);
    root->addLayout(buttons);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmButton, &QPushButton::clicked, this, &QDialog::accept);
}

bool ConfigConfirmDialog::confirm(QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& confirmText) {
    ConfigConfirmDialog dialog(title, message, confirmText, parent);
    return dialog.exec() == QDialog::Accepted;
}

void ConfigConfirmDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void ConfigConfirmDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStartGlobal_ = event->globalPos();
        dragStartFrame_ = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void ConfigConfirmDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(dragStartFrame_ + event->globalPos() - dragStartGlobal_);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void ConfigConfirmDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}
