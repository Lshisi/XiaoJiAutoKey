#pragma once

#include <QRect>
#include <QString>
#include <QStringList>

class QWidget;

namespace FluentPopupMenu {

struct Options {
    int minWidth = 112;
    int maxWidth = 220;
    int itemHeight = 28;
    int maxVisibleItems = 8;
    int anchorGap = 4;
    bool preferAbove = false;
    QString trailingText;
};

int exec(
    QWidget* parent,
    const QRect& globalAnchorRect,
    const QStringList& items,
    const Options& options = Options());

}
