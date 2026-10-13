#pragma once

#include "ShareCodec.h"

#include <QDialog>
#include <QPoint>

class FluentComboBox;
class QPlainTextEdit;
class QLabel;
class QPushButton;

// 导出窗口：把当前方案 / 当前宏编码为分享码或文本配方，一键复制。
class ShareExportDialog : public QDialog {
    Q_OBJECT

public:
    ShareExportDialog(const QString& title, const SharePayload& payload, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void refreshExport();

    FluentComboBox* formatCombo_ = nullptr;
    QPlainTextEdit* textEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* copyBtn_ = nullptr;

    SharePayload payload_;
    QString summary_;

    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
