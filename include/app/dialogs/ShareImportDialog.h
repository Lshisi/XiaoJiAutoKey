#pragma once

#include "ShareCodec.h"

#include <QDialog>
#include <QPoint>

class QPlainTextEdit;
class QLabel;
class QPushButton;

// 导入配置：粘贴分享码或文本配方，预览确认后导入。
class ShareImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ShareImportDialog(QWidget* parent = nullptr);

    [[nodiscard]] bool hasImport() const noexcept { return hasImport_; }
    [[nodiscard]] const SharePayload& importedPayload() const noexcept { return importPayload_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void previewImport();

    QPlainTextEdit* textEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* previewBtn_ = nullptr;
    QPushButton* importBtn_ = nullptr;

    SharePayload importPayload_;
    bool hasImport_ = false;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
