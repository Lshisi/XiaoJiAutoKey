#include "FluentSidebar.h"
#include "../styles/DesignTokens.h"
#include "../../include/FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

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

    QRectF r = rect().adjusted(6, 2, -6, -2);

    // Background
    if (active_) {
        QColor activeBg = FactionTheme::instance().secondaryColor();
        p.setBrush(activeBg);
    } else if (hovered_) {
        p.setBrush(Fluent::Colors::hoverBg());
    } else {
        p.setBrush(Qt::transparent);
    }
    p.setPen(Qt::NoPen);
    QPainterPath path;
    path.addRoundedRect(r, 8, 8);
    p.drawPath(path);

    // Active indicator (left colored bar)
    if (active_) {
        p.setBrush(FactionTheme::instance().primaryColor());
        p.drawRoundedRect(QRectF(2, r.center().y() - 8, 3, 16), 1.5, 1.5);
    }

    // Icon
    QFont iconFont("Segoe UI", 9);
    p.setFont(iconFont);
    p.setPen(active_ ? FactionTheme::instance().primaryColor() : Fluent::Colors::textSecondary());
    p.drawText(QRectF(r.left() + 10, r.top(), 20, r.height()), Qt::AlignCenter, icon_);

    // Label
    p.setFont(Fluent::Typography::body());
    p.setPen(active_ ? FactionTheme::instance().primaryColor() : Fluent::Colors::textPrimary());
    p.drawText(QRectF(r.left() + 34, r.top(), r.width() - 38, r.height()), Qt::AlignVCenter | Qt::AlignLeft, label_);
}

// ── FluentSidebar ───────────────────────────────
// 宽度 160px，带文字的列表式导航

FluentSidebar::FluentSidebar(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(130);

    navLayout_ = new QVBoxLayout(this);
    navLayout_->setContentsMargins(0, 12, 0, 12);
    navLayout_->setSpacing(2);
    navLayout_->setAlignment(Qt::AlignTop);

    // Faction icon at top
    factionIconLabel_ = new QLabel(this);
    factionIconLabel_->setFixedSize(40, 40);
    factionIconLabel_->setAlignment(Qt::AlignCenter);
    factionIconLabel_->setStyleSheet("background: transparent;");
    navLayout_->addWidget(factionIconLabel_, 0, Qt::AlignHCenter);
    navLayout_->addSpacing(8);
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
    p.fillRect(rect(), Fluent::Colors::sidebarBg());
    // Right border
    p.setPen(Qt::NoPen);
    p.setBrush(Fluent::Colors::sidebarBorder());
    p.drawRect(width() - 1, 0, 1, height());
}

void FluentSidebar::updateFactionIcon() {
    const QString iconPath = FactionTheme::instance().iconPath();
    if (iconPath.isEmpty()) {
        factionIconLabel_->hide();
        return;
    }

    QPixmap pix(iconPath);
    if (pix.isNull()) {
        factionIconLabel_->hide();
        return;
    }
    factionIconLabel_->setPixmap(pix.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    factionIconLabel_->show();
}
