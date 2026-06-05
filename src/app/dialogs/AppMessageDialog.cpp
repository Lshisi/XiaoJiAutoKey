#include "dialogs/AppMessageDialog.h"

#include "FactionTheme.h"
#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AppMessageDialog::AppMessageDialog(Kind kind, const QString& title, const QString& message, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setModal(true);
    setFixedSize(360, 170);
    setStyleSheet(
        "QDialog { background: #FFFFFF; }"
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
    QPalette msgPal = messageLabel->palette();
    msgPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    messageLabel->setPalette(msgPal);
    textLayout->addWidget(messageLabel);

    content->addLayout(textLayout, 1);
    root->addLayout(content, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* okBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u786e\u5b9a"), this);
    okBtn->setFixedWidth(86);
    buttons->addWidget(okBtn);
    root->addLayout(buttons);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void AppMessageDialog::warning(QWidget* parent, const QString& title, const QString& message) {
    AppMessageDialog dialog(Kind::Warning, title, message, parent);
    dialog.exec();
}

void AppMessageDialog::information(QWidget* parent, const QString& title, const QString& message) {
    AppMessageDialog dialog(Kind::Information, title, message, parent);
    dialog.exec();
}
