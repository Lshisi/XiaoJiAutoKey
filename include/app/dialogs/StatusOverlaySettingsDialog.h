#pragma once

#include "AppTypes.h"

#include <QDialog>
#include <QPoint>

class QTabWidget;
class QVBoxLayout;

struct StatusOverlaySettings {
    RepeatOverlayOptions repeat;
    MacroOverlayOptions macro;
};

class StatusOverlaySettingsDialog : public QDialog {
    Q_OBJECT

public:
    StatusOverlaySettingsDialog(const AppConfig& config, bool macroPage, QWidget* parent);
    [[nodiscard]] StatusOverlaySettings settings() const;
    void refreshVisuals();

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QVBoxLayout* addPage(const QString& title);
    void addCheck(QVBoxLayout* layout, const QString& text, bool* value);

    StatusOverlaySettings settings_;
    QTabWidget* tabs_ = nullptr;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
