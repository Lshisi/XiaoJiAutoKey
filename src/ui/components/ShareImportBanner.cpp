#include "ui/components/ShareImportBanner.h"

#include "FactionTheme.h"
#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>

namespace {

constexpr int kBannerHeight = 52;
constexpr int kBannerWidth = 360;
// 距底边留出状态栏（24px）+ 8px 间隙，距右边界用页面边距。
constexpr int kBottomInset = 32;
constexpr int kAutoHideMs = 8000;

}  // namespace

ShareImportBanner::ShareImportBanner(QWidget* parent) : QWidget(parent) {
    setFixedSize(kBannerWidth, kBannerHeight);
    setCursor(Qt::PointingHandCursor);
    hide();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 8, 10, 8);
    layout->setSpacing(8);

    iconLabel_ = new QLabel(this);
    iconLabel_->setFont(QFont("Segoe MDL2 Assets", 11));
    layout->addWidget(iconLabel_);

    textLabel_ = new QLabel(QString::fromWCharArray(
        L"\u68c0\u6d4b\u5230\u526a\u8d34\u677f\u4e2d\u7684\u5206\u4eab\u914d\u7f6e"), this);
    textLabel_->setFont(Fluent::Typography::body());
    layout->addWidget(textLabel_);
    layout->addStretch();

    actionLabel_ = new QLabel(QString::fromWCharArray(L"\u70b9\u51fb\u5bfc\u5165"), this);
    actionLabel_->setFont(Fluent::Typography::bodyStrong());
    layout->addWidget(actionLabel_);

    closeButton_ = new FluentButton(FluentButton::Icon, QString(QChar(0x00D7)), this);
    QFont closeFont = Fluent::Typography::body();
    closeFont.setPointSize(13);
    closeButton_->setFont(closeFont);
    closeButton_->setFixedSize(24, 24);
    closeButton_->setToolTip(QString::fromWCharArray(L"\u5173\u95ed"));
    closeButton_->setCursor(Qt::ArrowCursor);
    layout->addWidget(closeButton_);

    connect(closeButton_, &QPushButton::clicked, this, [this]() {
        const QString text = shareText_;
        hideBanner();
        emit dismissed(text);
    });

    autoHideTimer_ = new QTimer(this);
    autoHideTimer_->setSingleShot(true);
    autoHideTimer_->setInterval(kAutoHideMs);
    connect(autoHideTimer_, &QTimer::timeout, this, &ShareImportBanner::hideBanner);

    applyTheme();
}

void ShareImportBanner::showForText(const QString& text) {
    shareText_ = text;
    reposition();
    show();
    raise();
    autoHideTimer_->start();
}

void ShareImportBanner::hideBanner() {
    autoHideTimer_->stop();
    shareText_.clear();
    hide();
}

void ShareImportBanner::repositionToHost() {
    if (isVisible()) {
        reposition();
    }
}

void ShareImportBanner::reposition() {
    if (auto* host = parentWidget()) {
        move(host->width() - width() - Fluent::Spacing::pageMargin,
            host->height() - height() - kBottomInset);
    }
}

void ShareImportBanner::applyTheme() {
    // 跟随当前门派配色，而不是固定的 Fluent 蓝。
    const auto palette = FactionTheme::instance().palette();

    QPalette iconPalette = iconLabel_->palette();
    iconPalette.setColor(QPalette::WindowText, palette.accent);
    iconLabel_->setPalette(iconPalette);
    iconLabel_->setText(QString(QChar(0xE896)));  // download icon

    QPalette textPalette = textLabel_->palette();
    textPalette.setColor(QPalette::WindowText, Fluent::Colors::textPrimary());
    textLabel_->setPalette(textPalette);

    QPalette actionPalette = actionLabel_->palette();
    actionPalette.setColor(QPalette::WindowText, palette.accent);
    actionLabel_->setPalette(actionPalette);

    update();
}

void ShareImportBanner::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal borderWidth = Fluent::Painting::alignedStrokeWidth(1.0, devicePixelRatioF());
    const QRectF cardArea = Fluent::Painting::alignedStrokeRect(rect(), borderWidth);

    const auto palette = FactionTheme::instance().palette();
    QPainterPath path;
    path.addRoundedRect(cardArea, Fluent::Spacing::cardRadius, Fluent::Spacing::cardRadius);
    painter.fillPath(path, Fluent::Colors::cardBg());
    if (underMouse()) {
        // hover 叠一层门派强调色的浅色调，与侧边栏选中态一致。
        painter.fillPath(path, palette.accentSoft);
    }
    painter.setPen(QPen(palette.accentBorder, borderWidth));
    painter.drawPath(path);
}

void ShareImportBanner::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !shareText_.isEmpty()) {
        const QString text = shareText_;
        hideBanner();
        emit importRequested(text);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}
