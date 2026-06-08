#include "FluentComboBox.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionComboBox>

FluentComboBox::FluentComboBox(QWidget* parent) : QComboBox(parent) {
    refreshThemeStyle();
}

void FluentComboBox::refreshThemeStyle() {
    const bool hasCustomFixedHeight =
        minimumHeight() == maximumHeight() &&
        minimumHeight() > 0 &&
        minimumHeight() != Fluent::Spacing::inputHeight;
    if (!hasCustomFixedHeight) {
        setFixedHeight(Fluent::Spacing::inputHeight);
    }
    setFont(Fluent::Typography::body());

    const auto palette = FactionTheme::instance().palette();
    const QString border = Fluent::Colors::border().name(QColor::HexArgb);
    const QString hoverBorder = palette.accentBorder.name(QColor::HexArgb);
    const QString bg = Fluent::Colors::cardBg().name();
    const QString text = Fluent::Colors::textPrimary().name();
    const QString muted = Fluent::Colors::textSecondary().name();
    const QString itemHover = Fluent::Colors::hoverBg().name(QColor::HexArgb);
    setStyleSheet(QString(
        "QComboBox {"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  padding: 0 12px;"
        "  background: %4;"
        "  color: %5;"
        "}"
        "QComboBox:hover {"
        "  border-color: %6;"
        "}"
        "QComboBox:focus {"
        "  border: 2px solid %1;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 30px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: none;"
        "  width: 0; height: 0;"
        "  border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent;"
        "  border-top: 5px solid %7;"
        "}"
        "QComboBox QAbstractItemView {"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  background: %4;"
        "  color: %5;"
        "  selection-background-color: %2;"
        "  selection-color: %5;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  height: 36px;"
        "  padding: 0 12px;"
        "  border-radius: 6px;"
        "}"
        "QComboBox QAbstractItemView::item:hover {"
        "  background: %8;"
        "}"
    ).arg(palette.accentBorder.name(),
        palette.accentSoft.name(),
        border,
        bg,
        text,
        hoverBorder,
        muted,
        itemHover));
    updateGeometry();
    update();
}

void FluentComboBox::paintEvent(QPaintEvent* event) {
    QComboBox::paintEvent(event);
}
