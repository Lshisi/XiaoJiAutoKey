#include "FluentSidebar.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QHBoxLayout>

namespace {
QPixmap tintPixmap(const QPixmap& source, const QColor& color) {
    if (source.isNull()) {
        return source;
    }

    QPixmap tinted(source.size());
    tinted.setDevicePixelRatio(source.devicePixelRatio());
    tinted.fill(Qt::transparent);

    QPainter painter(&tinted);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(0, 0, source);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(QRect(QPoint(0, 0), source.size()), color);
    return tinted;
}

void drawKeyboardIcon(QPainter& p, const QRectF& rect, const QColor& color) {
    QPen pen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect.adjusted(1.5, 4.5, -1.5, -4.5), 2.5, 2.5);
    p.setBrush(color);
    const qreal y = rect.center().y() + 1;
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 4; ++col) {
            p.drawRoundedRect(QRectF(rect.left() + 5 + col * 3.3, y - 3 + row * 4, 1.6, 1.6), 0.6, 0.6);
        }
    }
}

void drawListIcon(QPainter& p, const QRectF& rect, const QColor& color) {
    QPen pen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    for (int i = 0; i < 3; ++i) {
        const qreal y = rect.top() + 5.5 + i * 5.0;
        p.drawLine(QPointF(rect.left() + 5, y), QPointF(rect.right() - 4, y));
    }
}

void drawGearIcon(QPainter& p, const QRectF& rect, const QColor& color) {
    const QPointF c = rect.center();
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 8; ++i) {
        p.save();
        p.translate(c);
        p.rotate(i * 45);
        p.drawLine(QPointF(0, -8.0), QPointF(0, -6.0));
        p.restore();
    }
    p.drawEllipse(c, 5.7, 5.7);
    p.drawEllipse(c, 2.0, 2.0);
}

void drawInfoIcon(QPainter& p, const QRectF& rect, const QColor& color) {
    QPen pen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(rect.adjusted(3.0, 3.0, -3.0, -3.0));
    p.drawPoint(QPointF(rect.center().x(), rect.top() + 6.4));
    p.drawLine(QPointF(rect.center().x(), rect.top() + 9.5), QPointF(rect.center().x(), rect.bottom() - 5.0));
}

void drawNavIcon(QPainter& p, const QRectF& rect, const QString& iconId, const QColor& color) {
    if (iconId == QStringLiteral("keys")) {
        drawKeyboardIcon(p, rect, color);
    } else if (iconId == QStringLiteral("macro")) {
        drawListIcon(p, rect, color);
    } else if (iconId == QStringLiteral("settings")) {
        drawGearIcon(p, rect, color);
    } else if (iconId == QStringLiteral("about")) {
        drawInfoIcon(p, rect, color);
    } else {
        p.setPen(color);
        p.setFont(QFont("Segoe UI Symbol", 11));
        p.drawText(rect, Qt::AlignCenter, iconId);
    }
}

void showFallbackFactionBadge(QLabel* label, const QString& name, const FactionPalette& palette) {
    label->setPixmap(QPixmap());
    label->setText(name.left(1));
    label->setFont(Fluent::Typography::bodyStrong());
    label->setStyleSheet(QString(
        "background: %1;"
        "border: 1px solid %2;"
        "border-radius: 17px;"
        "color: %3;")
            .arg(palette.accentSubtle.name(QColor::HexArgb),
                palette.accentBorder.name(QColor::HexArgb),
                Fluent::Colors::textPrimary().name()));
    label->show();
}
}

// ── FluentNavButton ─────────────────────────────
// 横向按钮：左侧图标 + 右侧文字，类似 Windows 设置的导航列表

FluentNavButton::FluentNavButton(const QString& icon, const QString& label, QWidget* parent)
    : QPushButton(parent), icon_(icon), label_(label)
{
    setFixedHeight(36);
    setCursor(Qt::PointingHandCursor);
    setFlat(true);
}

void FluentNavButton::setActive(bool active) {
    active_ = active;
    update();
}

void FluentNavButton::enterEvent(QEvent*) {
    hovered_ = true;
    update();
}

void FluentNavButton::leaveEvent(QEvent*) {
    hovered_ = false;
    update();
}

void FluentNavButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto palette = FactionTheme::instance().palette();
    QRectF r = rect().adjusted(6, 2, -6, -2);

    // Background
    if (active_) {
        p.setBrush(palette.accentSoft);
    } else if (hovered_) {
        p.setBrush(palette.accentSubtle);
    } else {
        p.setBrush(Qt::transparent);
    }
    p.setPen(Qt::NoPen);
    QPainterPath path;
    path.addRoundedRect(r, 8, 8);
    p.drawPath(path);

    // Active indicator (left colored bar)
    if (active_) {
        p.setBrush(palette.accent);
        p.drawRoundedRect(QRectF(4, r.center().y() - 8, 3, 16), 1.5, 1.5);
    }

    // Icon
    drawNavIcon(p, QRectF(r.left() + 9, r.top() + 6, 20, r.height() - 12),
        icon_, active_ ? palette.accent : Fluent::Colors::textSecondary());

    // Label
    p.setFont(Fluent::Typography::body());
    p.setPen(active_ ? Fluent::Colors::textPrimary() : Fluent::Colors::textPrimary());
    p.drawText(QRectF(r.left() + 33, r.top(), r.width() - 35, r.height()), Qt::AlignVCenter | Qt::AlignLeft, label_);
}

// ── FluentSidebar ───────────────────────────────
// 带文字的列表式导航

FluentSidebar::FluentSidebar(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(128);

    navLayout_ = new QVBoxLayout(this);
    navLayout_->setContentsMargins(0, 12, 0, 12);
    navLayout_->setSpacing(2);
    navLayout_->setAlignment(Qt::AlignTop);

    factionHeader_ = new QWidget(this);
    factionHeader_->setObjectName("factionHeader");
    factionHeader_->setFixedHeight(58);
    auto* headerLayout = new QHBoxLayout(factionHeader_);
    headerLayout->setContentsMargins(12, 8, 10, 8);
    headerLayout->setSpacing(7);

    factionIconLabel_ = new QLabel(factionHeader_);
    factionIconLabel_->setFixedSize(34, 34);
    factionIconLabel_->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(factionIconLabel_);

    factionNameLabel_ = new QLabel(factionHeader_);
    factionNameLabel_->setFont(Fluent::Typography::bodyStrong());
    factionNameLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    headerLayout->addWidget(factionNameLabel_, 1);

    navLayout_->addWidget(factionHeader_);
    factionHeaderSpacer_ = new QWidget(this);
    factionHeaderSpacer_->setFixedHeight(6);
    navLayout_->addWidget(factionHeaderSpacer_);
    updateFactionIcon();
}

void FluentSidebar::addNavItem(const QString& icon, const QString& label, const QString& pageId) {
    auto* btn = new FluentNavButton(icon, label, this);
    navLayout_->addWidget(btn);
    items_.append({btn, pageId});

    connect(btn, &QPushButton::clicked, this, [this, pageId]() {
        setCurrentPage(pageId);
        emit pageSelected(pageId);
    });
}

void FluentSidebar::setCurrentPage(const QString& pageId) {
    for (auto& [btn, id] : items_) {
        btn->setActive(id == pageId);
    }
}

void FluentSidebar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const auto palette = FactionTheme::instance().palette();
    p.fillRect(rect(), palette.surfaceTint);
    // Right border
    p.setPen(Qt::NoPen);
    p.setBrush(Fluent::Colors::sidebarBorder());
    p.drawRect(width() - 1, 0, 1, height());
}

void FluentSidebar::updateFactionIcon() {
    const auto& faction = FactionTheme::instance().current();
    const auto palette = FactionTheme::instance().palette();
    factionHeader_->setVisible(true);
    if (factionHeaderSpacer_) {
        factionHeaderSpacer_->setVisible(true);
    }

    factionNameLabel_->setText(faction.name);
    factionNameLabel_->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textPrimary().name()));
    factionHeader_->setStyleSheet("QWidget#factionHeader { background: transparent; }");

    const QString iconPath = FactionTheme::instance().iconPath();
    if (iconPath.isEmpty()) {
        showFallbackFactionBadge(factionIconLabel_, faction.name, palette);
        return;
    }

    QPixmap pix(iconPath);
    if (pix.isNull()) {
        showFallbackFactionBadge(factionIconLabel_, faction.name, palette);
        return;
    }
    factionIconLabel_->setStyleSheet("background: transparent;");
    factionIconLabel_->setText(QString());
    QPixmap scaled = pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (Fluent::Theme::isDarkMode()) {
        scaled = tintPixmap(scaled, Fluent::Colors::textPrimary());
    }
    factionIconLabel_->setPixmap(scaled);
    factionIconLabel_->show();
}
