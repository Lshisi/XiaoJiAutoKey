#include "dialogs/AppMessageDialog.h"

#include "dialogs/DialogChrome.h"
#include "FactionTheme.h"
#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

AppMessageDialog::AppMessageDialog(Kind kind,
    const QString& title,
    const QString& message,
    const QString& primaryButtonText,
    const QString& secondaryButtonText,
    QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    DialogChrome::apply(this);
    setModal(true);
    resize(420, 260);
    setMinimumSize(360, 170);
    setMaximumSize(520, 420);
    setStyleSheet(
        "QLabel#messageIcon { border-radius: 18px; color: white; font-size: 20px; font-weight: 600; }"
    );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 16);
    root->setSpacing(14);

    auto* content = new QHBoxLayout();
    content->setSpacing(14);

    auto* icon = new QLabel(kind == Kind::Warning ? QStringLiteral("!") : QStringLiteral("i"), this);
    icon->setObjectName("messageIcon");
    icon->setFixedSize(36, 36);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QString("QLabel#messageIcon { background: %1; border-radius: 18px; color: white; font-size: 20px; font-weight: 600; }")
        .arg(kind == Kind::Warning ? QStringLiteral("#D83B01") : FactionTheme::instance().palette().accent.name()));
    content->addWidget(icon, 0, Qt::AlignTop);

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(6);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    QPalette titlePal = titleLabel->palette();
    titlePal.setColor(QPalette::WindowText, Fluent::Colors::textPrimary());
    titleLabel->setPalette(titlePal);
    textLayout->addWidget(titleLabel);

    auto* messageLabel = new QLabel(message, this);
    messageLabel->setFont(Fluent::Typography::body());
    messageLabel->setWordWrap(true);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPalette msgPal = messageLabel->palette();
    msgPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    messageLabel->setPalette(msgPal);

    auto* messageContainer = new QWidget(this);
    auto* messageContainerLayout = new QVBoxLayout(messageContainer);
    messageContainerLayout->setContentsMargins(0, 0, 0, 0);
    messageContainerLayout->addWidget(messageLabel);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidget(messageContainer);
    scrollArea->setMinimumHeight(72);
    scrollArea->setStyleSheet(QString(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 22px; }"
        "QScrollBar::handle:vertical:hover { background: %2; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
        .arg(Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.18)") : QStringLiteral("rgba(0,0,0,0.16)"),
             Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.28)") : QStringLiteral("rgba(0,0,0,0.24)")));
    textLayout->addWidget(scrollArea, 1);

    content->addLayout(textLayout, 1);
    root->addLayout(content, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton* secondaryBtn = nullptr;
    if (!secondaryButtonText.isEmpty()) {
        secondaryBtn = new FluentButton(FluentButton::Secondary, secondaryButtonText, this);
        secondaryBtn->setFixedWidth(86);
        buttons->addWidget(secondaryBtn);
        buttons->addSpacing(8);
    }
    auto* primaryBtn = new FluentButton(FluentButton::Primary, primaryButtonText, this);
    primaryBtn->setFixedWidth(86);
    buttons->addWidget(primaryBtn);
    root->addLayout(buttons);

    if (secondaryBtn) {
        connect(secondaryBtn, &QPushButton::clicked, this, &QDialog::reject);
    }
    connect(primaryBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void AppMessageDialog::warning(QWidget* parent, const QString& title, const QString& message) {
    AppMessageDialog dialog(Kind::Warning, title, message, QString::fromWCharArray(L"确定"), QString(), parent);
    dialog.exec();
}

void AppMessageDialog::information(QWidget* parent, const QString& title, const QString& message) {
    AppMessageDialog dialog(Kind::Information, title, message, QString::fromWCharArray(L"确定"), QString(), parent);
    dialog.exec();
}

bool AppMessageDialog::informationWithActions(QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& primaryButtonText,
    const QString& secondaryButtonText) {
    AppMessageDialog dialog(Kind::Information, title, message, primaryButtonText, secondaryButtonText, parent);
    return dialog.exec() == QDialog::Accepted;
}

void AppMessageDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void AppMessageDialog::mousePressEvent(QMouseEvent* event) {
    if (DialogChrome::mousePress(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
    QDialog::mousePressEvent(event);
}

void AppMessageDialog::mouseMoveEvent(QMouseEvent* event) {
    if (DialogChrome::mouseMove(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
    QDialog::mouseMoveEvent(event);
}

void AppMessageDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (DialogChrome::mouseRelease(event, dragging_)) return;
    QDialog::mouseReleaseEvent(event);
}
