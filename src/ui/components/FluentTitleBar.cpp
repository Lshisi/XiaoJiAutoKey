#include "FluentTitleBar.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <QStyle>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace {

class CaptionButton final : public QPushButton {
public:
    CaptionButton(bool close, QWidget* parent) : QPushButton(parent), close_(close) {}

protected:
    void paintEvent(QPaintEvent* event) override {
        QPushButton::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QColor color = !isEnabled() ? Fluent::Colors::textTertiary()
            : close_ && (underMouse() || isDown()) ? QColor(Qt::white) : Fluent::Colors::textPrimary();
        painter.setPen(QPen(color, 1.1, Qt::SolidLine, Qt::RoundCap));
        const QPointF center(width() / 2.0, height() / 2.0);
        if (close_) {
            painter.drawLine(center + QPointF(-3.5, -3.5), center + QPointF(3.5, 3.5));
            painter.drawLine(center + QPointF(-3.5, 3.5), center + QPointF(3.5, -3.5));
        } else {
            painter.drawLine(center + QPointF(-4.0, 0), center + QPointF(4.0, 0));
        }
    }

private:
    bool close_;
};

QColor mixColor(const QColor& a, const QColor& b, double t) {
    const double clamped = qBound(0.0, t, 1.0);
    return QColor(
        static_cast<int>(a.red() + (b.red() - a.red()) * clamped),
        static_cast<int>(a.green() + (b.green() - a.green()) * clamped),
        static_cast<int>(a.blue() + (b.blue() - a.blue()) * clamped)
    );
}

QColor titleBarFillColor() {
    const auto palette = FactionTheme::instance().palette();
    const QColor base = Fluent::Colors::titleBarBg();
    if (Fluent::Theme::isDarkMode()) {
        return mixColor(base, palette.accent, 0.09);
    }

    return mixColor(base, palette.accent, 0.12);
}

QColor titleBarDividerColor() {
    const auto palette = FactionTheme::instance().palette();
    if (Fluent::Theme::isDarkMode()) {
        QColor divider = mixColor(Fluent::Colors::divider(), palette.accentBorder, 0.36);
        divider.setAlpha(76);
        return divider;
    }

    const QColor base = mixColor(Fluent::Colors::titleBarBg(), palette.accentBorder, 0.12);
    QColor divider = base;
    divider.setAlpha(26);
    return divider;
}

}  // namespace

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
    minBtn_ = createCaptionButton(false);
    closeBtn_ = createCaptionButton(true);

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

QPushButton* FluentTitleBar::createCaptionButton(bool close) {
    auto* btn = new CaptionButton(close, this);
    btn->setAccessibleName(close ? QStringLiteral("\u5173\u95ed") : QStringLiteral("\u6700\u5c0f\u5316"));
    btn->setToolTip(btn->accessibleName());
    btn->setFixedSize(40, 28);
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
    p.fillPath(bg, titleBarFillColor());

    // Bottom divider kept inset and light to avoid a heavy seam.
    p.setRenderHint(QPainter::Antialiasing, false);
    const qreal dividerWidth = Fluent::Painting::alignedStrokeWidth(1.0, devicePixelRatioF());
    p.setPen(QPen(titleBarDividerColor(), dividerWidth));
    const qreal dividerY = height() - dividerWidth / 2.0;
    p.drawLine(QPointF(10, dividerY), QPointF(width() - 10, dividerY));
}

void FluentTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    event->ignore();
}

void FluentTitleBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
#ifdef Q_OS_WIN
        ReleaseCapture();
        SendMessageW(reinterpret_cast<HWND>(window()->winId()), WM_NCLBUTTONDOWN, HTCAPTION, 0);
        event->accept();
#else
        dragging_ = true;
        dragPos_ = event->globalPos();
#endif
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
