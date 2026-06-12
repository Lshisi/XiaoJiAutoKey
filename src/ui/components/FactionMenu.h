#pragma once

#include <QPoint>
#include <QString>

class QWidget;

namespace FactionMenu {

struct Options {
    int minWidth = 112;
    int itemHeight = 24;
    bool drawScrollBar = true;
    int bottomMargin = 8;
    int bottomLimitGlobalY = 0;
};

QString exec(QWidget* parent, const QPoint& globalPos, const Options& options = Options());

}
