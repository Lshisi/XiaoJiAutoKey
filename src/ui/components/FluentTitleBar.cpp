#include "FluentTitleBar.h"
#include "../styles/DesignTokens.h"

#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QStyle>

FluentTitleBar::FluentTitleBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(Fluent::Spacing::titleBarHeight);
    setMouseTracking(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 8, 0);
    layout->setSpacing(0);

    // Icon
    iconLabel_ = new QLabel(this);
    iconLabel_->setFixedSize(24, 24);
    iconLabel_->setScaledContents(true);
    layout->addWidget(iconLabel_);

    layout->addSpacing(10);

    // Title
    titleLabel_ = new QLabel(this);
    titleLabel_->setFont(Fluent::Typography::titleBarTitle());
    QPalette pal = titleLabel_->palette();
    pal.setColor(QPalette::WindowText, Fluent::Colors::textPrimary());
    titleLabel_->setPalette(pal);
    layout->addWidget(titleLabel_);

    layout->addStretch();

    // Caption buttons
    minBtn_ = createCaptionButton(QStringLiteral("\u2500"), tr("Minimize"));
    maxBtn_ = createCaptionButton(QStringLiteral("\u25A1"), tr("Maximize"));
    closeBtn_ = createCaptionButton(QStringLiteral("\u2715"), tr("Close"));

    layout->addWidget(minBtn_);
    layout->addWidget(maxBtn_);
    layout->addWidget(closeBtn_);

    // Close button special hover
    closeBtn_->setProperty("captionClose", true);

    connect(minBtn_, &QPushButton::clicked, this, &FluentTitleBar::minimizeClicked);
    connect(maxBtn_, &QPushButton::clicked, this, &FluentTitleBar::maximizeClicked);
    connect(closeBtn_, &QPushButton::clicked, this, &FluentTitleBar::closeClicked);
}

void FluentTitleBar::setTitle(const QString& title) {
    titleLabel_->setText(title);
}

void FluentTitleBar::setIcon(const QIcon& icon) {
    iconLabel_->setPixmap(icon.pixmap(24, 24));
}

QPushButton* FluentTitleBar::createCaptionButton(const QString& icon, const QString& tooltip) {
    auto* btn = new QPushButton(icon, this);
    btn->setFixedSize(46, 32);
    btn->setToolTip(tooltip);
    btn->setFlat(true);
    btn->setCursor(Qt::ArrowCursor);
    btn->setFont(QFont("Segoe UI", 10));
    btn->setStyleSheet(
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "  border-radius: 0px;"
        "  color: #202020;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(0,0,0,0.06);"
        "}"
        "QPushButton:pressed {"
        "  background: rgba(0,0,0,0.09);"
        "}"
    );
    return btn;
}

void FluentTitleBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Fluent::Colors::titleBarBg());
    // Bottom border line
    p.setPen(Qt::NoPen);
    p.setBrush(Fluent::Colors::divider());
    p.drawRect(0, height() - 1, width(), 1);
}

void FluentTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit maximizeClicked();
    }
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
