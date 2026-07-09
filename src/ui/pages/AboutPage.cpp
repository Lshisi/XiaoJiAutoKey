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
#include <QSizePolicy>
#include <QTimer>

namespace {

void copyWithFeedback(FluentButton* button, const QString& text) {
    QApplication::clipboard()->setText(text);
    button->setText(QString::fromWCharArray(L"\u5df2\u590d\u5236"));
    button->setEnabled(false);
    QTimer::singleShot(1200, button, [button]() {
        button->setText(QString::fromWCharArray(L"\u590d\u5236"));
        button->setEnabled(true);
    });
}

}

AboutPage::AboutPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("AboutPage { background: %1; }").arg(FactionTheme::instance().palette().surfaceTint.name()));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap,
        Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap);
    pageLayout->setSpacing(0);

    // Outer big card
    auto* outerCard = new FluentCard(this);
    outerCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    outerCard->contentLayout()->setSpacing(10);

    // Page title
    auto* pageTitle = new QLabel(QString::fromWCharArray(L"\u5173\u4e8e\u8f6f\u4ef6"), outerCard);
    pageTitle->setFont(Fluent::Typography::pageTitle());
    outerCard->contentLayout()->addWidget(pageTitle);

    // ── Inner card 1: App info ──
    auto* infoCard = new FluentCard(outerCard);
    auto* infoRow = new QHBoxLayout();
    auto* iconLabel = new QLabel(infoCard);
    iconLabel->setPixmap(QPixmap(":/assets/icon.png").scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setFixedSize(48, 48);
    infoRow->addWidget(iconLabel);
    infoRow->addSpacing(12);
    auto* infoText = new QVBoxLayout();
    infoText->setSpacing(2);
    auto* appName = new QLabel(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e"), infoCard);
    appName->setFont(Fluent::Typography::cardTitle());
    appName->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    infoText->addWidget(appName);
    auto* version = new QLabel(QString("V") + APP_VERSION, infoCard);
    version->setFont(Fluent::Typography::caption());
    version->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    infoText->addWidget(version);
    auto* desc = new QLabel(QString::fromWCharArray(L"\u4e00\u4e2a\u8f7b\u91cf\u3001\u9ad8\u6548\u7684\u6309\u952e\u81ea\u52a8\u5316\u5de5\u5177"), infoCard);
    desc->setFont(Fluent::Typography::body());
    desc->setWordWrap(false);
    desc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    infoText->addWidget(desc);
    infoRow->addLayout(infoText, 1);
    infoCard->contentLayout()->addLayout(infoRow);
    outerCard->contentLayout()->addWidget(infoCard);

    // ── Inner card 2: Contact ──
    auto* contactCard = new FluentCard(QString::fromWCharArray(L"\u8054\u7cfb\u4f5c\u8005"), outerCard);
    auto* qqRow = new QHBoxLayout();
    auto* qqPrefix = new QLabel("QQ: ", contactCard);
    qqPrefix->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    qqRow->addWidget(qqPrefix);
    auto* qqLabel = new QLabel("2033653725", contactCard);
    qqLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    qqRow->addWidget(qqLabel, 1);
    auto* copyQqBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u590d\u5236"), contactCard);
    copyQqBtn->setFixedSize(64, 24);
    qqRow->addWidget(copyQqBtn);
    contactCard->contentLayout()->addLayout(qqRow);
    auto* emailRow = new QHBoxLayout();
    auto* emailPrefix = new QLabel(QString::fromWCharArray(L"\u90ae\u7bb1: "), contactCard);
    emailPrefix->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    emailRow->addWidget(emailPrefix);
    auto* emailLabel = new QLabel("2033653725@qq.com", contactCard);
    emailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    emailRow->addWidget(emailLabel, 1);
    auto* copyEmailBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u590d\u5236"), contactCard);
    copyEmailBtn->setFixedSize(64, 24);
    emailRow->addWidget(copyEmailBtn);
    contactCard->contentLayout()->addLayout(emailRow);
    auto* groupRow = new QHBoxLayout();
    auto* groupPrefix = new QLabel(QString::fromWCharArray(L"QQ\u7fa4: "), contactCard);
    groupPrefix->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    groupRow->addWidget(groupPrefix);
    auto* groupLabel = new QLabel("REMOVED", contactCard);
    groupLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    groupRow->addWidget(groupLabel, 1);
    auto* copyGroupBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u590d\u5236"), contactCard);
    copyGroupBtn->setFixedSize(64, 24);
    groupRow->addWidget(copyGroupBtn);
    contactCard->contentLayout()->addLayout(groupRow);
    outerCard->contentLayout()->addWidget(contactCard);

    // ── Inner card 3: Disclaimer ──
    auto* disclaimerCard = new FluentCard(QString::fromWCharArray(L"\u514d\u8d23\u58f0\u660e"), outerCard);
    auto* disclaimerLabel = new QLabel(
        QString::fromWCharArray(
            L"1. \u672c\u8f6f\u4ef6\u4ec5\u4f9b\u5b66\u4e60\u548c\u4ea4\u6d41\u4f7f\u7528\n"
            L"2. \u4f7f\u7528\u672c\u8f6f\u4ef6\u4ea7\u751f\u7684\u4e00\u5207\u540e\u679c\u7531\u7528\u6237\u81ea\u884c\u627f\u62c5\n"
            L"3. \u4f5c\u8005\u4e0d\u5bf9\u8f6f\u4ef6\u4f7f\u7528\u9020\u6210\u7684\u4efb\u4f55\u635f\u5931\u8d1f\u8d23\n"
            L"4. \u7981\u6b62\u4f7f\u7528\u672c\u8f6f\u4ef6\u8fdb\u884c\u4efb\u4f55\u8fdd\u6cd5\u6d3b\u52a8"),
        disclaimerCard);
    disclaimerLabel->setFont(Fluent::Typography::body());
    disclaimerLabel->setWordWrap(true);
    disclaimerLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    disclaimerCard->contentLayout()->addWidget(disclaimerLabel);
    outerCard->contentLayout()->addWidget(disclaimerCard);
    outerCard->contentLayout()->addStretch(1);

    pageLayout->addWidget(outerCard);

    connect(copyQqBtn, &QPushButton::clicked, [copyQqBtn, qqLabel]() { copyWithFeedback(copyQqBtn, qqLabel->text()); });
    connect(copyEmailBtn, &QPushButton::clicked, [copyEmailBtn, emailLabel]() { copyWithFeedback(copyEmailBtn, emailLabel->text()); });
    connect(copyGroupBtn, &QPushButton::clicked, [copyGroupBtn, groupLabel]() { copyWithFeedback(copyGroupBtn, groupLabel->text()); });
}
