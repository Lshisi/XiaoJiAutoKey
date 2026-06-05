#include "AboutPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FluentCard.h"
#include "../components/FluentButton.h"
#include "FactionTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QClipboard>

AboutPage::AboutPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("AboutPage { background: %1; }").arg(FactionTheme::instance().palette().surfaceTint.name()));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(16, 12, 16, 12);
    outerLayout->setSpacing(0);

    // Outer big card
    auto* outerCard = new FluentCard(this);

    // Page title
    auto* pageTitle = new QLabel(QString::fromWCharArray(L"\u5173\u4e8e\u8f6f\u4ef6"), this);
    pageTitle->setFont(Fluent::Typography::pageTitle());
    outerCard->contentLayout()->addWidget(pageTitle);
    outerCard->contentLayout()->addSpacing(12);

    // ── Inner card 1: App info ──
    auto* infoCard = new FluentCard(this);
    auto* infoRow = new QHBoxLayout();
    auto* iconLabel = new QLabel(this);
    iconLabel->setPixmap(QPixmap(":/assets/icon.png").scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setFixedSize(48, 48);
    infoRow->addWidget(iconLabel);
    infoRow->addSpacing(12);
    auto* infoText = new QVBoxLayout();
    infoText->setSpacing(2);
    auto* appName = new QLabel(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e~"), this);
    appName->setFont(Fluent::Typography::cardTitle());
    infoText->addWidget(appName);
    auto* version = new QLabel(QString("v") + APP_VERSION, this);
    version->setFont(Fluent::Typography::caption());
    infoText->addWidget(version);
    auto* desc = new QLabel(QString::fromWCharArray(L"\u4e00\u4e2a\u8f7b\u91cf\u3001\u9ad8\u6548\u7684\u6309\u952e\u81ea\u52a8\u5316\u5de5\u5177"), this);
    desc->setFont(Fluent::Typography::body());
    infoText->addWidget(desc);
    infoRow->addLayout(infoText);
    infoRow->addStretch();
    infoCard->contentLayout()->addLayout(infoRow);
    outerCard->contentLayout()->addWidget(infoCard);
    outerCard->contentLayout()->addSpacing(12);

    // ── Inner card 2: Contact ──
    auto* contactCard = new FluentCard(QString::fromWCharArray(L"\u8054\u7cfb\u4f5c\u8005"), this);
    auto* qqRow = new QHBoxLayout();
    qqRow->addWidget(new QLabel("QQ: ", this));
    auto* qqLabel = new QLabel("2033653725", this);
    qqRow->addWidget(qqLabel);
    qqRow->addStretch();
    auto* copyQqBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u590d\u5236"), this);
    copyQqBtn->setFixedSize(50, 24);
    qqRow->addWidget(copyQqBtn);
    contactCard->contentLayout()->addLayout(qqRow);
    auto* emailRow = new QHBoxLayout();
    emailRow->addWidget(new QLabel(QString::fromWCharArray(L"\u90ae\u7bb1: "), this));
    auto* emailLabel = new QLabel("2033653725@qq.com", this);
    emailRow->addWidget(emailLabel);
    emailRow->addStretch();
    auto* copyEmailBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u590d\u5236"), this);
    copyEmailBtn->setFixedSize(50, 24);
    emailRow->addWidget(copyEmailBtn);
    contactCard->contentLayout()->addLayout(emailRow);
    auto* groupRow = new QHBoxLayout();
    groupRow->addWidget(new QLabel(QString::fromWCharArray(L"QQ\u7fa4: "), this));
    auto* groupLabel = new QLabel("REMOVED", this);
    groupRow->addWidget(groupLabel);
    groupRow->addStretch();
    auto* copyGroupBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u590d\u5236"), this);
    copyGroupBtn->setFixedSize(50, 24);
    groupRow->addWidget(copyGroupBtn);
    contactCard->contentLayout()->addLayout(groupRow);
    outerCard->contentLayout()->addWidget(contactCard);
    outerCard->contentLayout()->addSpacing(12);

    // ── Inner card 3: Disclaimer ──
    auto* disclaimerCard = new FluentCard(QString::fromWCharArray(L"\u514d\u8d23\u58f0\u660e"), this);
    auto* disclaimerLabel = new QLabel(
        QString::fromWCharArray(
            L"1. \u672c\u8f6f\u4ef6\u4ec5\u4f9b\u5b66\u4e60\u548c\u4ea4\u6d41\u4f7f\u7528\n"
            L"2. \u4f7f\u7528\u672c\u8f6f\u4ef6\u4ea7\u751f\u7684\u4e00\u5207\u540e\u679c\u7531\u7528\u6237\u81ea\u884c\u627f\u62c5\n"
            L"3. \u4f5c\u8005\u4e0d\u5bf9\u8f6f\u4ef6\u4f7f\u7528\u9020\u6210\u7684\u4efb\u4f55\u635f\u5931\u8d1f\u8d23\n"
            L"4. \u7981\u6b62\u4f7f\u7528\u672c\u8f6f\u4ef6\u8fdb\u884c\u4efb\u4f55\u8fdd\u6cd5\u6d3b\u52a8"),
        this);
    disclaimerLabel->setFont(Fluent::Typography::body());
    disclaimerLabel->setWordWrap(true);
    disclaimerCard->contentLayout()->addWidget(disclaimerLabel);
    outerCard->contentLayout()->addWidget(disclaimerCard);

    outerCard->contentLayout()->addStretch();
    outerLayout->addWidget(outerCard, 1);

    connect(copyQqBtn, &QPushButton::clicked, [qqLabel]() { QApplication::clipboard()->setText(qqLabel->text()); });
    connect(copyEmailBtn, &QPushButton::clicked, [emailLabel]() { QApplication::clipboard()->setText(emailLabel->text()); });
    connect(copyGroupBtn, &QPushButton::clicked, [groupLabel]() { QApplication::clipboard()->setText(groupLabel->text()); });
}
