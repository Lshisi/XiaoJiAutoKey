#include "AboutPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FluentCard.h"
#include "../components/FluentButton.h"
#include "../components/FactionIcon.h"
#include "FactionTheme.h"
#include "dialogs/AppMessageDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProgressBar>
#include <QScrollArea>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QUrl>

namespace {

QString formatMegabytes(qint64 bytes) {
    return QString::number(static_cast<double>(bytes) / 1024.0 / 1024.0, 'f', 1) + QStringLiteral(" MB");
}

}  // namespace

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
    iconLabel_ = new QLabel(infoCard);
    iconLabel_->setFixedSize(48, 48);
    iconLabel_->setAlignment(Qt::AlignCenter);
    infoRow->addWidget(iconLabel_);
    infoRow->addSpacing(12);
    auto* infoText = new QVBoxLayout();
    infoText->setSpacing(2);
    auto* appName = new QLabel(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e"), infoCard);
    appName->setFont(Fluent::Typography::cardTitle());
    appName->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    infoText->addWidget(appName);
    auto* desc = new QLabel(QString::fromWCharArray(L"\u4e00\u4e2a\u8f7b\u91cf\u3001\u9ad8\u6548\u7684\u6309\u952e\u81ea\u52a8\u5316\u5de5\u5177"), infoCard);
    desc->setFont(Fluent::Typography::body());
    desc->setWordWrap(false);
    desc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    infoText->addWidget(desc);
    infoRow->addLayout(infoText, 1);
    infoCard->contentLayout()->addLayout(infoRow);
    outerCard->contentLayout()->addWidget(infoCard);

    // ── Inner card 2: Software update ──
    auto* updateCard = new FluentCard(QString::fromWCharArray(L"\u8f6f\u4ef6\u66f4\u65b0"), outerCard);

    auto* updateHeaderRow = new QHBoxLayout();
    auto* versionCaption = new QLabel(QString::fromWCharArray(L"\u5f53\u524d\u7248\u672c: "), updateCard);
    versionCaption->setFont(Fluent::Typography::body());
    updateHeaderRow->addWidget(versionCaption);
    versionValueLabel_ = new QLabel(QString("V") + APP_VERSION, updateCard);
    versionValueLabel_->setFont(Fluent::Typography::body());
    updateHeaderRow->addWidget(versionValueLabel_);
    updateHeaderRow->addStretch();
    checkUpdateBtn_ = new FluentButton(FluentButton::Secondary,
        QString::fromWCharArray(L"\u68c0\u67e5\u66f4\u65b0"), updateCard);
    checkUpdateBtn_->setFixedHeight(28);
    checkUpdateBtn_->setMinimumWidth(88);
    updateHeaderRow->addWidget(checkUpdateBtn_);
    updateCard->contentLayout()->addLayout(updateHeaderRow);

    updateStatusLabel_ = new QLabel(updateCard);
    updateStatusLabel_->setFont(Fluent::Typography::body());
    updateStatusLabel_->setWordWrap(true);
    updateStatusLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    updateCard->contentLayout()->addWidget(updateStatusLabel_);

    auto* updateActionRow = new QHBoxLayout();
    downloadUpdateBtn_ = new FluentButton(FluentButton::Primary,
        QString::fromWCharArray(L"\u4e0b\u8f7d\u5e76\u5b89\u88c5"), updateCard);
    downloadUpdateBtn_->setFixedHeight(28);
    downloadUpdateBtn_->setMinimumWidth(104);
    downloadUpdateBtn_->hide();
    updateActionRow->addWidget(downloadUpdateBtn_);
    restartUpdateBtn_ = new FluentButton(FluentButton::Primary,
        QString::fromWCharArray(L"\u7acb\u5373\u91cd\u542f\u66f4\u65b0"), updateCard);
    restartUpdateBtn_->setFixedHeight(28);
    restartUpdateBtn_->setMinimumWidth(112);
    restartUpdateBtn_->hide();
    updateActionRow->addWidget(restartUpdateBtn_);
    updateActionRow->addStretch();
    updateCard->contentLayout()->addLayout(updateActionRow);

    downloadProgressBar_ = new QProgressBar(updateCard);
    downloadProgressBar_->setTextVisible(false);
    downloadProgressBar_->setFixedHeight(6);
    downloadProgressBar_->hide();
    updateCard->contentLayout()->addWidget(downloadProgressBar_);

    downloadProgressLabel_ = new QLabel(updateCard);
    downloadProgressLabel_->setFont(Fluent::Typography::caption());
    downloadProgressLabel_->hide();
    updateCard->contentLayout()->addWidget(downloadProgressLabel_);

    outerCard->contentLayout()->addWidget(updateCard);

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

    connect(checkUpdateBtn_, &QPushButton::clicked, this, [this]() {
        emit checkUpdateRequested();
    });
    connect(downloadUpdateBtn_, &QPushButton::clicked, this, [this]() {
        // An empty URL means the release only has no attached package; the
        // coordinator opens the release page instead of downloading.
        emit downloadUpdateRequested(pendingAssetUrl_);
    });
    connect(restartUpdateBtn_, &QPushButton::clicked, this, [this]() {
        emit restartForUpdateRequested();
    });
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

    setUpdateIdle();
    updateThemeColors();
}

void AboutPage::refreshIcon() {
    if (!iconLabel_) return;
    iconLabel_->setPixmap(FactionIcon::render(QStringLiteral(":/assets/icon.png"), iconLabel_->size(), iconLabel_));
}

void AboutPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refreshIcon();
}

void AboutPage::setUpdateStatus(const QString& text, UpdateStatusKind kind) {
    updateStatusKind_ = kind;
    updateStatusLabel_->setText(text);
    updateStatusLabel_->setVisible(!text.isEmpty());
    applyUpdateCardStyles();
}

void AboutPage::applyUpdateCardStyles() {
    QColor statusColor = Fluent::Colors::textSecondary();
    if (updateStatusKind_ == UpdateStatusKind::Accent) {
        statusColor = FactionTheme::instance().palette().accent;
    } else if (updateStatusKind_ == UpdateStatusKind::Error) {
        statusColor = Fluent::Colors::danger();
    }
    if (updateStatusLabel_) {
        updateStatusLabel_->setStyleSheet(
            QString("QLabel { color: %1; background: transparent; border: none; }")
                .arg(statusColor.name(QColor::HexArgb)));
    }
    if (downloadProgressBar_) {
        downloadProgressBar_->setStyleSheet(
            QString("QProgressBar { background: %1; border: none; border-radius: 3px; }"
                "QProgressBar::chunk { background: %2; border-radius: 3px; }")
                .arg(Fluent::Theme::isDarkMode()
                        ? QStringLiteral("rgba(255,255,255,0.14)")
                        : QStringLiteral("rgba(0,0,0,0.10)"),
                    FactionTheme::instance().palette().accent.name()));
    }
    if (downloadProgressLabel_) {
        downloadProgressLabel_->setStyleSheet(
            QString("QLabel { color: %1; background: transparent; border: none; }")
                .arg(Fluent::Colors::textSecondary().name(QColor::HexArgb)));
    }
}

void AboutPage::setUpdateIdle() {
    pendingAssetUrl_.clear();
    checkUpdateBtn_->setEnabled(true);
    checkUpdateBtn_->setText(QString::fromWCharArray(L"\u68c0\u67e5\u66f4\u65b0"));
    downloadUpdateBtn_->hide();
    restartUpdateBtn_->hide();
    downloadProgressBar_->hide();
    downloadProgressLabel_->hide();
    setUpdateStatus(QString(), UpdateStatusKind::Info);
}

void AboutPage::setChecking() {
    checkUpdateBtn_->setEnabled(false);
    checkUpdateBtn_->setText(QString::fromWCharArray(L"\u68c0\u67e5\u4e2d..."));
    downloadUpdateBtn_->hide();
    restartUpdateBtn_->hide();
    downloadProgressBar_->hide();
    downloadProgressLabel_->hide();
    setUpdateStatus(QString(), UpdateStatusKind::Info);
}

void AboutPage::setNoUpdate() {
    pendingAssetUrl_.clear();
    checkUpdateBtn_->setEnabled(false);
    checkUpdateBtn_->setText(QString::fromWCharArray(L"\u65e0\u66f4\u65b0"));
    downloadUpdateBtn_->hide();
    restartUpdateBtn_->hide();
    downloadProgressBar_->hide();
    downloadProgressLabel_->hide();
    setUpdateStatus(QString(), UpdateStatusKind::Info);
    // Briefly show the result on the button, then offer a fresh check.
    QTimer::singleShot(3000, this, [this]() {
        if (!checkUpdateBtn_->isEnabled() &&
            checkUpdateBtn_->text() == QString::fromWCharArray(L"\u65e0\u66f4\u65b0")) {
            setUpdateIdle();
        }
    });
}

void AboutPage::setUpdateAvailable(const QString& latestVersion, const QString& assetUrl) {
    checkUpdateBtn_->setEnabled(true);
    checkUpdateBtn_->setText(QString::fromWCharArray(L"\u68c0\u67e5\u66f4\u65b0"));
    restartUpdateBtn_->hide();
    downloadProgressBar_->hide();
    downloadProgressLabel_->hide();
    pendingAssetUrl_ = assetUrl;
    downloadUpdateBtn_->setText(assetUrl.isEmpty()
        ? QString::fromWCharArray(L"\u524d\u5f80\u4e0b\u8f7d")
        : QString::fromWCharArray(L"\u4e0b\u8f7d\u5e76\u5b89\u88c5"));
    downloadUpdateBtn_->show();
    setUpdateStatus(QString::fromWCharArray(L"\u53d1\u73b0\u65b0\u7248\u672c V") + latestVersion,
        UpdateStatusKind::Accent);
}

void AboutPage::setDownloading() {
    checkUpdateBtn_->setEnabled(true);
    checkUpdateBtn_->setText(QString::fromWCharArray(L"\u68c0\u67e5\u66f4\u65b0"));
    downloadUpdateBtn_->hide();
    restartUpdateBtn_->hide();
    downloadProgressBar_->setRange(0, 100);
    downloadProgressBar_->setValue(0);
    downloadProgressBar_->show();
    downloadProgressLabel_->clear();
    downloadProgressLabel_->show();
    setUpdateStatus(QString::fromWCharArray(L"\u6b63\u5728\u4e0b\u8f7d\u66f4\u65b0\u5305..."),
        UpdateStatusKind::Info);
}

void AboutPage::showDownloadProgress(qint64 received, qint64 total) {
    if (total > 0) {
        downloadProgressBar_->setRange(0, 100);
        downloadProgressBar_->setValue(
            static_cast<int>(received * 100 / total));
        downloadProgressLabel_->setText(
            formatMegabytes(received) + QStringLiteral(" / ") + formatMegabytes(total));
    } else {
        downloadProgressBar_->setRange(0, 0);
        downloadProgressLabel_->setText(formatMegabytes(received));
    }
}

void AboutPage::setUpdateStaged() {
    checkUpdateBtn_->setEnabled(true);
    checkUpdateBtn_->setText(QString::fromWCharArray(L"\u68c0\u67e5\u66f4\u65b0"));
    downloadUpdateBtn_->hide();
    downloadProgressBar_->hide();
    downloadProgressLabel_->hide();
    restartUpdateBtn_->show();
    setUpdateStatus(QString::fromWCharArray(L"\u66f4\u65b0\u5305\u5df2\u5c31\u7eea\uff0c\u91cd\u542f\u540e\u5b8c\u6210\u66f4\u65b0"),
        UpdateStatusKind::Accent);
}

void AboutPage::setUpdateError(const QString& message) {
    pendingAssetUrl_.clear();
    checkUpdateBtn_->setEnabled(true);
    checkUpdateBtn_->setText(QString::fromWCharArray(L"\u68c0\u67e5\u66f4\u65b0"));
    downloadUpdateBtn_->hide();
    restartUpdateBtn_->hide();
    downloadProgressBar_->hide();
    downloadProgressLabel_->hide();
    setUpdateStatus(message, UpdateStatusKind::Error);
}

void AboutPage::updateThemeColors() {
    refreshIcon();
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
    applyUpdateCardStyles();
    update();
}
