#include "FluentSidebar.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QHBoxLayout>

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
    QRectF r = rect().adjusted(8, 2, -8, -2);

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
    QFont iconFont("Segoe UI", 11);
    p.setFont(iconFont);
    p.setPen(active_ ? palette.accent : Fluent::Colors::textSecondary());
    p.drawText(QRectF(r.left() + 10, r.top(), 20, r.height()), Qt::AlignCenter, icon_);

    // Label
    p.setFont(Fluent::Typography::body());
    p.setPen(active_ ? Fluent::Colors::textPrimary() : Fluent::Colors::textPrimary());
    p.drawText(QRectF(r.left() + 34, r.top(), r.width() - 38, r.height()), Qt::AlignVCenter | Qt::AlignLeft, label_);
}

// ── FluentSidebar ───────────────────────────────
// 宽度 160px，带文字的列表式导航

FluentSidebar::FluentSidebar(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(156);

    navLayout_ = new QVBoxLayout(this);
    navLayout_->setContentsMargins(0, 12, 0, 12);
    navLayout_->setSpacing(2);
    navLayout_->setAlignment(Qt::AlignTop);

    factionHeader_ = new QWidget(this);
    factionHeader_->setObjectName("factionHeader");
    factionHeader_->setFixedHeight(58);
    auto* headerLayout = new QHBoxLayout(factionHeader_);
    headerLayout->setContentsMargins(14, 8, 12, 8);
    headerLayout->setSpacing(8);

    factionIconLabel_ = new QLabel(factionHeader_);
    factionIconLabel_->setFixedSize(34, 34);
    factionIconLabel_->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(factionIconLabel_);

    factionNameLabel_ = new QLabel(factionHeader_);
    factionNameLabel_->setFont(Fluent::Typography::bodyStrong());
    factionNameLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    headerLayout->addWidget(factionNameLabel_, 1);

    navLayout_->addWidget(factionHeader_);
    navLayout_->addSpacing(6);
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
    p.setBrush(QColor(0, 0, 0, 28));
    p.drawRect(width() - 1, 0, 1, height());
}

void FluentSidebar::updateFactionIcon() {
    const auto& faction = FactionTheme::instance().current();
    const auto palette = FactionTheme::instance().palette();
    factionNameLabel_->setText(faction.name);
    factionNameLabel_->setStyleSheet(QString("color: %1;").arg(Fluent::Colors::textPrimary().name()));
    factionHeader_->setStyleSheet("QWidget#factionHeader { background: transparent; }");

    const QString iconPath = FactionTheme::instance().iconPath();
    if (iconPath.isEmpty()) {
        factionIconLabel_->setText(QString::fromWCharArray(L"\u5c0f"));
        factionIconLabel_->setFont(Fluent::Typography::bodyStrong());
        factionIconLabel_->setStyleSheet(QString("background: %1; color: %2; border-radius: 17px;")
            .arg(palette.accent.name(), palette.textOnAccent.name()));
        factionIconLabel_->show();
        return;
    }

    QPixmap pix(iconPath);
    if (pix.isNull()) {
        factionIconLabel_->hide();
        return;
    }
    factionIconLabel_->setStyleSheet("background: transparent;");
    factionIconLabel_->setPixmap(pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    factionIconLabel_->show();
}
