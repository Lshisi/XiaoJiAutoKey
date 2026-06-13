#pragma once

#include <QColor>
#include <QPixmap>
#include <QSize>
#include <QString>

class QWidget;

namespace FactionIcon {

QPixmap render(const QString& path, const QSize& logicalSize, const QWidget* referenceWidget, const QColor& tint = QColor());

}
