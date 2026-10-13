#pragma once

#include <QDialog>
#include <QPoint>

#include <functional>

class FluentLineEdit;
class QPushButton;

class ConfigNameDialog : public QDialog {
public:
    // onImport 非空时在按钮行显示「导入」：点击后执行回调并关闭对话框，不返回名称。
    ConfigNameDialog(const QString& title,
        const QString& label,
        const QString& initialName = QString(),
        QWidget* parent = nullptr,
        const std::function<void()>& onImport = {});

    [[nodiscard]] QString nameText() const;

    static bool getName(QWidget* parent,
        const QString& title,
        const QString& label,
        const QString& initialName,
        QString& name,
        const std::function<void()>& onImport = {});

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    FluentLineEdit* nameEdit_ = nullptr;
    QPushButton* okButton_ = nullptr;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
