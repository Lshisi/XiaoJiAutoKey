#include "FluentCard.h"
#include "../styles/DesignTokens.h"

#include <QPainter>
#include <QPainterPath>

namespace {
void applyCardTitlePalette(QLabel* label) {
    if (!label) {
        return;
    }

    label->setFont(Fluent::Typography::cardTitle());
    label->setAutoFillBackground(false);
    label->setForegroundRole(QPalette::WindowText);
    label->setStyleSheet(QString("QLabel#fluentCardTitle { color: %1; background: transparent; }")
        .arg(Fluent::Colors::textPrimary().name()));

    const QColor text = Fluent::Colors::textPrimary();
    const QPalette current = label->palette();
    if (current.color(QPalette::Active, QPalette::WindowText) == text &&
        current.color(QPalette::Inactive, QPalette::WindowText) == text &&
        current.color(QPalette::Disabled, QPalette::WindowText) == text &&
        current.color(QPalette::Active, QPalette::Text) == text &&
        current.color(QPalette::Inactive, QPalette::Text) == text &&
        current.color(QPalette::Disabled, QPalette::Text) == text) {
        return;
    }

    QPalette pal = current;
    for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
        pal.setColor(group, QPalette::WindowText, text);
        pal.setColor(group, QPalette::Text, text);
    }
    label->setPalette(pal);
}
}

FluentCard::FluentCard(QWidget* parent) : QWidget(parent) {
    init();
}

FluentCard::FluentCard(const QString& title, QWidget* parent) : QWidget(parent) {
    init();
    setTitle(title);
}

void FluentCard::init() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(
        Fluent::Spacing::cardPadding,
        Fluent::Spacing::cardPadding,
        Fluent::Spacing::cardPadding,
        Fluent::Spacing::cardPadding
    );
    mainLayout->setSpacing(8);

    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName(QStringLiteral("fluentCardTitle"));
    applyCardTitlePalette(titleLabel_);
    titleLabel_->hide();
    mainLayout->addWidget(titleLabel_);

    contentLayout_ = new QVBoxLayout();
    contentLayout_->setSpacing(6);
    mainLayout->addLayout(contentLayout_);
}

void FluentCard::setTitle(const QString& title) {
    titleLabel_->setText(title);
    titleLabel_->setVisible(!title.isEmpty());
}

void FluentCard::refreshThemeStyle() {
    if (auto* mainLayout = qobject_cast<QVBoxLayout*>(layout())) {
        mainLayout->setContentsMargins(
            Fluent::Spacing::cardPadding,
            Fluent::Spacing::cardPadding,
            Fluent::Spacing::cardPadding,
            Fluent::Spacing::cardPadding
        );
        mainLayout->setSpacing(8);
    }
    if (contentLayout_) {
        contentLayout_->setSpacing(6);
    }
    if (titleLabel_) {
        applyCardTitlePalette(titleLabel_);
        titleLabel_->updateGeometry();
        titleLabel_->update();
    }
    updateGeometry();
    update();
}

void FluentCard::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal borderWidth = Fluent::Painting::alignedStrokeWidth(1.0, devicePixelRatioF());
    const QRectF r = Fluent::Painting::alignedStrokeRect(rect(), borderWidth);
    QPainterPath path;
    path.addRoundedRect(r, Fluent::Spacing::cardRadius, Fluent::Spacing::cardRadius);

    p.setBrush(Fluent::Colors::cardBg());
    p.setPen(Qt::NoPen);
    p.drawPath(path);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Fluent::Colors::cardBorder(), borderWidth));
    p.drawPath(path);
}
