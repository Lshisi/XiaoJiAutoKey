#include "FluentCard.h"
#include "../styles/DesignTokens.h"

#include <QPainter>
#include <QPainterPath>

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
    titleLabel_->setFont(Fluent::Typography::cardTitle());
    QPalette pal = titleLabel_->palette();
    pal.setColor(QPalette::WindowText, Fluent::Colors::textPrimary());
    titleLabel_->setPalette(pal);
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

void FluentCard::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, Fluent::Spacing::cardRadius, Fluent::Spacing::cardRadius);

    // Clean card: white bg + subtle border, NO shadow (avoids text shadow artifacts)
    p.setBrush(Fluent::Colors::cardBg());
    p.setPen(QPen(Fluent::Colors::cardBorder(), 1));
    p.drawPath(path);
}
