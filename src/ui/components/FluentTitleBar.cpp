#include "FluentTitleBar.h"
#include "../styles/DesignTokens.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <QStyle>

FluentTitleBar::FluentTitleBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(Fluent::Spacing::titleBarHeight);
    setMouseTracking(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 8, 0);
    layout->setSpacing(0);

    // Icon
    iconLabel_ = new QLabel(this);
    iconLabel_->setFixedSize(20, 20);
    iconLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel_);

    layout->addSpacing(8);

    // Title
    titleLabel_ = new QLabel(this);
    applyTitlePalette();
    layout->addWidget(titleLabel_);

    layout->addStretch();

    // Caption buttons
    minBtn_ = createCaptionButton(QStringLiteral("\u2500"), tr("Minimize"));
    closeBtn_ = createCaptionButton(QStringLiteral("\u2715"), tr("Close"));

    layout->addWidget(minBtn_);
    layout->addWidget(closeBtn_);

    // Close button special hover
    closeBtn_->setProperty("captionClose", true);
    closeBtn_->setStyleSheet(
        QString("QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "  border-radius: 6px;"
        "  color: %1;"
        "}"
        "QPushButton:hover {"
        "  background: #E81123;"
        "  color: white;"
        "}"
        "QPushButton:pressed {"
        "  background: #C50F1F;"
        "  color: white;"
        "}")
        .arg(Fluent::Colors::textPrimary().name()));

    connect(minBtn_, &QPushButton::clicked, this, &FluentTitleBar::minimizeClicked);
    connect(closeBtn_, &QPushButton::clicked, this, &FluentTitleBar::closeClicked);
}

void FluentTitleBar::setTitle(const QString& title) {
    titleLabel_->setText(title);
}

void FluentTitleBar::setIcon(const QIcon& icon) {
    icon_ = icon;
    iconLabel_->setPixmap(icon_.pixmap(QSize(20, 20)));
}

void FluentTitleBar::refreshThemeStyle() {
    setFixedHeight(Fluent::Spacing::titleBarHeight);
    applyTitlePalette();

    if (!icon_.isNull()) {
        iconLabel_->setPixmap(icon_.pixmap(QSize(20, 20)));
    }

    const auto captionStyle = QString(
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "  border-radius: 6px;"
        "  color: %1;"
        "}"
        "QPushButton:hover {"
        "  background: %2;"
        "}"
        "QPushButton:pressed {"
        "  background: %3;"
        "}")
        .arg(Fluent::Colors::textPrimary().name(),
            Fluent::Colors::hoverBg().name(QColor::HexArgb),
            Fluent::Colors::selectedBg().name(QColor::HexArgb));
    minBtn_->setStyleSheet(captionStyle);
    closeBtn_->setStyleSheet(
        QString("QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "  border-radius: 6px;"
        "  color: %1;"
        "}"
        "QPushButton:hover {"
        "  background: #E81123;"
        "  color: white;"
        "}"
        "QPushButton:pressed {"
        "  background: #C50F1F;"
        "  color: white;"
        "}")
        .arg(Fluent::Colors::textPrimary().name()));
    updateGeometry();
    update();
}

void FluentTitleBar::applyTitlePalette() {
    titleLabel_->setFont(Fluent::Typography::titleBarTitle());
    titleLabel_->setAutoFillBackground(false);
    titleLabel_->setStyleSheet(QString("QLabel { color: %1; background: transparent; }")
        .arg(Fluent::Colors::textPrimary().name()));

    QPalette pal = titleLabel_->palette();
    const QColor text = Fluent::Colors::textPrimary();
    for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
        pal.setColor(group, QPalette::WindowText, text);
        pal.setColor(group, QPalette::Text, text);
    }
    titleLabel_->setPalette(pal);
}

QPushButton* FluentTitleBar::createCaptionButton(const QString& icon, const QString& tooltip) {
    auto* btn = new QPushButton(icon, this);
    btn->setFixedSize(40, 28);
    btn->setToolTip(tooltip);
    btn->setFlat(true);
    btn->setCursor(Qt::ArrowCursor);
    btn->setFont(QFont("Segoe UI", 9));
    btn->setStyleSheet(
        QString("QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "  border-radius: 6px;"
        "  color: %1;"
        "}"
        "QPushButton:hover {"
        "  background: %2;"
        "}"
        "QPushButton:pressed {"
        "  background: %3;"
        "}")
        .arg(Fluent::Colors::textPrimary().name(),
            Fluent::Colors::hoverBg().name(QColor::HexArgb),
            Fluent::Colors::selectedBg().name(QColor::HexArgb)));
    return btn;
}

void FluentTitleBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath bg;
    bg.addRoundedRect(QRectF(0, 0, width(), height() + 10), 10, 10);
    p.fillPath(bg, Fluent::Colors::titleBarBg());

    // Bottom border line
    p.setPen(Qt::NoPen);
    p.setBrush(Fluent::Colors::divider());
    p.drawRect(0, height() - 1, width(), 1);
}

void FluentTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    event->ignore();
}

void FluentTitleBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragPos_ = event->globalPos();
    }
}

void FluentTitleBar::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        QWidget* win = window();
        if (win->isMaximized()) {
            win->showNormal();
        }
        QPoint diff = event->globalPos() - dragPos_;
        win->move(win->pos() + diff);
        dragPos_ = event->globalPos();
    }
}

void FluentTitleBar::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
}
