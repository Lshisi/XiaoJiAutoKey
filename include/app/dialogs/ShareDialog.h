#pragma once

#include "ShareCodec.h"

#include <QDialog>
#include <QPoint>

class FluentComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

// 配置分享：导出（分享码 / 文本配方）与导入（粘贴解析 + 预览）。
class ShareDialog : public QDialog {
    Q_OBJECT

public:
    enum class Mode {
        ExportProfile,
        ExportMacro,
        Import,
    };

    struct ExportData {
        SharePayload currentProfile;
        SharePayload currentMacro;
        SharePayload everything;
    };

    ShareDialog(Mode initialMode, const ExportData& exportData, QWidget* parent = nullptr);

    [[nodiscard]] bool hasImport() const noexcept { return hasImport_; }
    [[nodiscard]] const SharePayload& importedPayload() const noexcept { return importPayload_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void refreshMode();
    void refreshExport();
    void previewImport();

    FluentComboBox* modeCombo_ = nullptr;
    FluentComboBox* scopeCombo_ = nullptr;
    FluentComboBox* formatCombo_ = nullptr;
    QPlainTextEdit* textEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* copyBtn_ = nullptr;
    QPushButton* previewBtn_ = nullptr;
    QPushButton* importBtn_ = nullptr;

    ExportData exportData_;
    SharePayload importPayload_;
    bool hasImport_ = false;
    bool importing_ = false;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
