#include "DashboardPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FluentCard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

DashboardPage::DashboardPage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(Fluent::Spacing::pageMargin, Fluent::Spacing::pageMargin,
                               Fluent::Spacing::pageMargin, Fluent::Spacing::pageMargin);
    layout->setSpacing(Fluent::Spacing::cardGap);

    // Page header
    auto* headerLabel = new QLabel(QString::fromStdWString(L"小叽按键"), this);
    headerLabel->setFont(Fluent::Typography::pageTitle());
    QPalette pal = headerLabel->palette();
    pal.setColor(QPalette::WindowText, Fluent::Colors::textPrimary());
    headerLabel->setPalette(pal);
    layout->addWidget(headerLabel);

    auto* subtitleLabel = new QLabel(QString::fromStdWString(L"轻量、高效的自动按键工具"), this);
    subtitleLabel->setFont(Fluent::Typography::body());
    QPalette subPal = subtitleLabel->palette();
    subPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    subtitleLabel->setPalette(subPal);
    layout->addWidget(subtitleLabel);

    layout->addSpacing(8);

    // Status card row
    auto* cardRow = new QHBoxLayout();
    cardRow->setSpacing(Fluent::Spacing::cardGap);

    // Status card
    auto* statusCard = new FluentCard(QString::fromStdWString(L"运行状态"), this);
    auto* statusRow = new QHBoxLayout();
    statusDot_ = new QLabel(this);
    statusDot_->setFixedSize(10, 10);
    statusRow->addWidget(statusDot_);
    statusText_ = new QLabel(QString::fromStdWString(L"已停止"), this);
    statusText_->setFont(Fluent::Typography::bodyStrong());
    statusRow->addWidget(statusText_);
    statusRow->addStretch();
    statusCard->contentLayout()->addLayout(statusRow);
    cardRow->addWidget(statusCard);

    // Hotkey card
    auto* hotkeyCard = new FluentCard(QString::fromStdWString(L"启停快捷键"), this);
    hotkeyValue_ = new QLabel(QString::fromStdWString(L"未设置"), this);
    hotkeyValue_->setFont(Fluent::Typography::bodyStrong());
    hotkeyCard->contentLayout()->addWidget(hotkeyValue_);
    cardRow->addWidget(hotkeyCard);

    // Binding count card
    auto* countCard = new FluentCard(QString::fromStdWString(L"按键数量"), this);
    bindingCountValue_ = new QLabel("0", this);
    bindingCountValue_->setFont(Fluent::Typography::bodyStrong());
    countCard->contentLayout()->addWidget(bindingCountValue_);
    cardRow->addWidget(countCard);

    layout->addLayout(cardRow);
    layout->addStretch();
}

void DashboardPage::updateStatus(bool running, const QString& hotkey, int bindingCount) {
    statusText_->setText(running ? QString::fromStdWString(L"运行中") : QString::fromStdWString(L"已停止"));

    QString dotStyle = QString("background: %1; border-radius: 5px;")
        .arg(running ? Fluent::Colors::success().name() : Fluent::Colors::textTertiary().name());
    statusDot_->setStyleSheet(dotStyle);

    hotkeyValue_->setText(hotkey.isEmpty() ? QString::fromStdWString(L"未设置") : hotkey);
    bindingCountValue_->setText(QString::number(bindingCount));
}
