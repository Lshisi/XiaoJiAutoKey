#include "FluentTable.h"
#include "../styles/DesignTokens.h"

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

    setStyleSheet(
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
        "  background: rgba(0,120,212,0.08);"
        "  color: #1F1F1F;"
        "}"
        "QHeaderView::section {"
        "  background: #FAFAFA;"
        "  color: #606060;"
        "  border: none;"
        "  border-bottom: 1px solid rgba(0,0,0,0.06);"
        "  padding: 0 8px;"
        "  font-size: 11px;"
        "}"
    );
}
