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
    const auto palette = FactionTheme::instance().palette();
    const QString headerBg = palette.accentSubtle.name();
    setStyleSheet(QString(
        "QTableWidget {"
        "  background: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QTableWidget::item {"
        "  padding: 0 8px;"
        "  border: none;"
        "  color: #1F1F1F;"
        "}"
        "QTableWidget::item:hover {"
        "  background: rgba(0,0,0,0.025);"
        "}"
        "QTableWidget::item:selected {"
        "  background: %1;"
        "  color: #1F1F1F;"
        "}"
        "QHeaderView::section {"
        "  background: %2;"
        "  color: #606060;"
        "  border: none;"
        "  border-bottom: 1px solid rgba(0,0,0,0.06);"
        "  padding: 0 8px;"
        "  font-size: 11px;"
        "}"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(0,0,0,0.12); border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(0,0,0,0.2); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    ).arg(palette.accentSoft.name(), headerBg));

    horizontalHeader()->setStyleSheet(QString(
        "QHeaderView::section {"
        "  background: %1;"
        "  color: #606060;"
        "  border: none;"
        "  border-bottom: 1px solid rgba(0,0,0,0.06);"
        "  padding: 0 8px;"
        "  font-size: 11px;"
        "}"
    ).arg(headerBg));
    horizontalHeader()->viewport()->update();
    horizontalHeader()->update();
}
