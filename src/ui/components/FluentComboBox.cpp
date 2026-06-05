#include "FluentComboBox.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionComboBox>

FluentComboBox::FluentComboBox(QWidget* parent) : QComboBox(parent) {
    setFixedHeight(Fluent::Spacing::inputHeight);
    setFont(Fluent::Typography::body());
    refreshThemeStyle();
}

void FluentComboBox::refreshThemeStyle() {
    const auto palette = FactionTheme::instance().palette();
    setStyleSheet(QString(
        "QComboBox {"
        "  border: 1px solid rgba(0,0,0,0.08);"
        "  border-radius: 8px;"
        "  padding: 0 12px;"
        "  background: #FFFFFF;"
        "  color: #1F1F1F;"
        "}"
        "QComboBox:hover {"
        "  border-color: rgba(0,0,0,0.12);"
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
        "  border-top: 5px solid #606060;"
        "}"
        "QComboBox QAbstractItemView {"
        "  border: 1px solid rgba(0,0,0,0.08);"
        "  border-radius: 8px;"
        "  background: #FFFFFF;"
        "  selection-background-color: %2;"
        "  selection-color: #1F1F1F;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  height: 36px;"
        "  padding: 0 12px;"
        "  border-radius: 6px;"
        "}"
        "QComboBox QAbstractItemView::item:hover {"
        "  background: rgba(0,0,0,0.04);"
        "}"
    ).arg(palette.accentBorder.name(), palette.accentSoft.name()));
}

void FluentComboBox::paintEvent(QPaintEvent* event) {
    QComboBox::paintEvent(event);
}
