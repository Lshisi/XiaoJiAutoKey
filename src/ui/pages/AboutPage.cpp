#include "AboutPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FluentCard.h"
#include "../components/FluentButton.h"
#include "FactionTheme.h"
#include "dialogs/AppMessageDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QUrl>

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
    setAttribute(Qt::WA_StyledBackground, true);

    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* scrollContent = new QWidget(scrollArea_);
    scrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollContent->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap,
        Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap);
    contentLayout->setSpacing(0);

    // Outer big card
    auto* outerCard = new FluentCard(scrollContent);
    outerCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (auto* outerCardLayout = qobject_cast<QVBoxLayout*>(outerCard->layout())) {
        outerCardLayout->setAlignment(Qt::AlignTop);
    }
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

    // Offline configuration guide
    auto* usageCard = new FluentCard(QString::fromWCharArray(L"\u914d\u7f6e\u8bf4\u660e"), outerCard);
    auto* usageRow = new QHBoxLayout();
    auto* usageLabel = new QLabel(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e\u914d\u7f6e\u8bf4\u660e"), usageCard);
    usageLabel->setWordWrap(true);
    usageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    usageRow->addWidget(usageLabel, 1);
    auto* openUsageBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u6253\u5f00"), usageCard);
    openUsageBtn->setFixedSize(64, 24);
    usageRow->addWidget(openUsageBtn);
    usageCard->contentLayout()->addLayout(usageRow);
    outerCard->contentLayout()->addWidget(usageCard);

    // Disclaimer
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
    contentLayout->addWidget(outerCard, 1);
    scrollArea_->setWidget(scrollContent);
    if (auto* viewport = scrollArea_->viewport()) {
        viewport->setStyleSheet(QStringLiteral("background: transparent;"));
    }
    pageLayout->addWidget(scrollArea_);

    connect(copyGroupBtn, &QPushButton::clicked, [copyGroupBtn, groupLabel]() { copyWithFeedback(copyGroupBtn, groupLabel->text()); });
    connect(openUsageBtn, &QPushButton::clicked, this, [this]() {
        const QString guidePath = QDir(QApplication::applicationDirPath()).filePath(
            QString::fromWCharArray(L"\u4f7f\u7528\u8bf4\u660e.mhtml"));
        if (!QFileInfo::exists(guidePath)) {
            AppMessageDialog::warning(this,
                QString::fromWCharArray(L"\u65e0\u6cd5\u6253\u5f00\u4f7f\u7528\u8bf4\u660e"),
                QString::fromWCharArray(L"\u672a\u627e\u5230\u4f7f\u7528\u8bf4\u660e\u6587\u4ef6\uff0c\u8bf7\u91cd\u65b0\u89e3\u538b\u5b8c\u6574\u53d1\u5e03\u5305\u3002"));
            return;
        }
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(guidePath))) {
            AppMessageDialog::warning(this,
                QString::fromWCharArray(L"\u65e0\u6cd5\u6253\u5f00\u4f7f\u7528\u8bf4\u660e"),
                QString::fromWCharArray(L"\u8bf7\u786e\u8ba4\u7cfb\u7edf\u5df2\u4e3a MHTML \u6587\u4ef6\u914d\u7f6e\u53ef\u7528\u7684\u6d4f\u89c8\u5668\u3002"));
        }
    });
    updateThemeColors();
}

void AboutPage::updateThemeColors() {
    setStyleSheet(QString("AboutPage { background: %1; }")
        .arg(FactionTheme::instance().palette().surfaceTint.name()));
    if (scrollArea_) {
        scrollArea_->setStyleSheet(
            QString("QScrollArea { background: transparent; border: none; }"
            "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
            "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; }"
            "QScrollBar::handle:vertical:hover { background: %2; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
                .arg(Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.18)") : QStringLiteral("rgba(0,0,0,0.12)"),
                    Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.28)") : QStringLiteral("rgba(0,0,0,0.2)")));
        if (auto* viewport = scrollArea_->viewport()) {
            viewport->setStyleSheet(QStringLiteral("background: transparent;"));
        }
    }
    update();
}
