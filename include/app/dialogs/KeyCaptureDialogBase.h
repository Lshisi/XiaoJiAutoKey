#pragma once

#include "AppTypes.h"

#include <QDialog>
#include <QPoint>
#include <optional>

class FluentLineEdit;

class KeyCaptureDialogBase : public QDialog {
    Q_OBJECT

public:
    explicit KeyCaptureDialogBase(QWidget* parent = nullptr);

    void handleCapturedKey(quint64 encodedKey);
    [[nodiscard]] std::optional<KeyDescriptor> capturedKey() const;
    void setPrimaryMouseButtonsAllowed(bool allowed);

signals:
    void keyCaptureRequested();
    void keyAccepted();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void setCaptureEditor(FluentLineEdit* editor);
    void setCapturedKey(KeyDescriptor key);

private:
    FluentLineEdit* keyEdit_ = nullptr;
    std::optional<KeyDescriptor> capturedKey_;
    bool allowPrimaryMouseButtons_ = false;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
