#include "FluentTable.h"
#include "../styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QHeaderView>

FluentTable::FluentTable(QWidget* parent) : QTableWidget(parent) { init(); }
FluentTable::FluentTable(int rows, int cols, QWidget* parent) : QTableWidget(rows, cols, parent) { init(); }

void FluentTable::init() {
    setFont(Fluent::Typography::body());
    setShowGrid(false);
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->setVisible(false);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::NoFrame);

    verticalHeader()->setDefaultSectionSize(Fluent::Spacing::tableRowH);
    horizontalHeader()->setFixedHeight(Fluent::Spacing::tableHeaderH);
    horizontalHeader()->setFont(Fluent::Typography::caption());
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    refreshThemeStyle();
}

void FluentTable::refreshThemeStyle() {
    setFont(Fluent::Typography::body());
    verticalHeader()->setDefaultSectionSize(Fluent::Spacing::tableRowH);
    horizontalHeader()->setFixedHeight(Fluent::Spacing::tableHeaderH);
    horizontalHeader()->setFont(Fluent::Typography::caption());

    const auto palette = FactionTheme::instance().palette();
    const QString headerBg = palette.accentSubtle.name();
    const QString text = Fluent::Colors::textPrimary().name();
    const QString muted = Fluent::Colors::textSecondary().name();
    const QString divider = Fluent::Colors::divider().name(QColor::HexArgb);
    const QString rowHover = Fluent::Colors::rowHover().name(QColor::HexArgb);
    const QString scrollQss = Fluent::Styles::scrollBarQss(
        Fluent::Styles::scrollBarHandle(), Fluent::Styles::scrollBarHandleHover());
    setStyleSheet(QString(
        "QTableWidget {"
        "  background: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QTableWidget::item {"
        "  padding: 0 8px;"
        "  border: none;"
        "  color: %3;"
        "}"
        "QTableWidget::item:hover {"
        "  background: %4;"
        "}"
        "QTableWidget::item:selected {"
        "  background: %1;"
        "  color: %3;"
        "}"
        "QHeaderView::section {"
        "  background: %2;"
        "  color: %5;"
        "  border: none;"
        "  border-bottom: 1px solid %6;"
        "  padding: 0 8px;"
        "  font-size: 11px;"
        "}"
    ).arg(palette.accentSoft.name(),
        headerBg,
        text,
        rowHover,
        muted,
        divider)
        + scrollQss);
    horizontalHeader()->setStyleSheet(QString(
        "QHeaderView::section {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-bottom: 1px solid %3;"
        "  padding: 0 8px;"
        "  font-size: 11px;"
        "}"
    ).arg(headerBg, muted, divider));
    horizontalHeader()->viewport()->update();
    horizontalHeader()->update();
    viewport()->update();
    updateGeometry();
    update();
}
