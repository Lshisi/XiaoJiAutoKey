#include "dialogs/AppMessageDialog.h"

#include "dialogs/DialogChrome.h"
#include "FactionTheme.h"
#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <Windows.h>
#else
#include <QApplication>
#endif

namespace {

void playSystemDialogSound(AppMessageDialog::Kind kind) {
#ifdef Q_OS_WIN
    ::MessageBeep(kind == AppMessageDialog::Kind::Warning ? MB_ICONWARNING : MB_ICONINFORMATION);
#else
    QApplication::beep();
#endif
}

void bringDialogToFront(QDialog* dialog) {
    if (!dialog) {
        return;
    }

    dialog->raise();
    dialog->activateWindow();
#ifdef Q_OS_WIN
    if (HWND hwnd = reinterpret_cast<HWND>(dialog->winId())) {
        ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        ::SetForegroundWindow(hwnd);
    }
#endif
}

}  // namespace

AppMessageDialog::AppMessageDialog(Kind kind,
    const QString& title,
    const QString& message,
    const QString& primaryButtonText,
    const QString& secondaryButtonText,
    QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    DialogChrome::apply(this);
    setWindowFlag(Qt::Dialog, true);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowModality(Qt::ApplicationModal);
    setModal(true);
    setStyleSheet(
        "QLabel#messageIcon { border-radius: 12px; color: white; font-size: 14px; font-weight: 600; }"
    );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 16);
    root->setSpacing(14);

    auto* content = new QHBoxLayout();
    content->setSpacing(14);

    auto* icon = new QLabel(kind == Kind::Warning ? QStringLiteral("!") : QStringLiteral("i"), this);
    icon->setObjectName("messageIcon");
    icon->setFixedSize(24, 24);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QString("QLabel#messageIcon { background: %1; border-radius: 12px; color: white; font-size: 14px; font-weight: 600; }")
        .arg(kind == Kind::Warning ? QStringLiteral("#D83B01") : FactionTheme::instance().palette().accent.name()));
    content->addWidget(icon, 0, Qt::AlignTop);

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(6);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleLabel->setFixedHeight(24);
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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
    messageContainer->setObjectName(QStringLiteral("messageContainer"));
    messageContainer->setAutoFillBackground(true);
    QPalette messageContainerPalette = messageContainer->palette();
    messageContainerPalette.setColor(QPalette::Window, Fluent::Colors::cardBg());
    messageContainerPalette.setColor(QPalette::Base, Fluent::Colors::cardBg());
    messageContainer->setPalette(messageContainerPalette);
    auto* messageContainerLayout = new QVBoxLayout(messageContainer);
    messageContainerLayout->setContentsMargins(0, 0, 0, 0);
    messageContainerLayout->addWidget(messageLabel);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setAutoFillBackground(true);
    QPalette scrollPalette = scrollArea->palette();
    scrollPalette.setColor(QPalette::Window, Fluent::Colors::cardBg());
    scrollPalette.setColor(QPalette::Base, Fluent::Colors::cardBg());
    scrollArea->setPalette(scrollPalette);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidget(messageContainer);
    if (auto* viewport = scrollArea->viewport()) {
        viewport->setAutoFillBackground(true);
        viewport->setPalette(scrollPalette);
    }
    scrollArea->setStyleSheet(QString(
        "QScrollArea { background: %1; border: none; }"
        "QScrollArea > QWidget { background: %1; }"
        "QWidget#messageContainer { background: %1; }")
        .arg(Fluent::Colors::cardBg().name(QColor::HexArgb))
        + Fluent::Styles::scrollBarQss(Fluent::Styles::scrollBarHandle(),
            Fluent::Styles::scrollBarHandleHover(), QColor(), 22));
    constexpr int dialogWidth = 400;
    constexpr int messageWidth = 300;
    const int messageHeight = QFontMetrics(messageLabel->font()).boundingRect(
        QRect(0, 0, messageWidth, 1000),
        static_cast<int>(Qt::TextWordWrap) | static_cast<int>(Qt::AlignLeft),
        message).height();
    scrollArea->setFixedHeight(qBound(44, messageHeight + 4, 170));
    textLayout->addWidget(scrollArea);

    content->addLayout(textLayout, 1);
    root->addLayout(content);

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

    const int contentHeight = qMax(icon->height(),
        titleLabel->sizeHint().height() + textLayout->spacing() + scrollArea->height());
    const int dialogHeight = qBound(176,
        root->contentsMargins().top() + contentHeight + root->spacing() +
            Fluent::Spacing::buttonHeight + root->contentsMargins().bottom(),
        320);
    setFixedSize(dialogWidth, dialogHeight);

    if (secondaryBtn) {
        connect(secondaryBtn, &QPushButton::clicked, this, &QDialog::reject);
    }
    connect(primaryBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void AppMessageDialog::warning(QWidget* parent, const QString& title, const QString& message) {
    playSystemDialogSound(Kind::Warning);
    AppMessageDialog dialog(Kind::Warning, title, message, QString::fromWCharArray(L"确定"), QString(), parent);
    QTimer::singleShot(0, &dialog, [&dialog]() { bringDialogToFront(&dialog); });
    dialog.exec();
}

void AppMessageDialog::information(QWidget* parent, const QString& title, const QString& message) {
    playSystemDialogSound(Kind::Information);
    AppMessageDialog dialog(Kind::Information, title, message, QString::fromWCharArray(L"确定"), QString(), parent);
    QTimer::singleShot(0, &dialog, [&dialog]() { bringDialogToFront(&dialog); });
    dialog.exec();
}

bool AppMessageDialog::informationWithActions(QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& primaryButtonText,
    const QString& secondaryButtonText) {
    playSystemDialogSound(Kind::Information);
    AppMessageDialog dialog(Kind::Information, title, message, primaryButtonText, secondaryButtonText, parent);
    QTimer::singleShot(0, &dialog, [&dialog]() { bringDialogToFront(&dialog); });
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
