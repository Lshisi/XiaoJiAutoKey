#pragma once

#include <QDialog>
#include <QPoint>
#include <QStringList>

class FluentComboBox;
class FluentLineEdit;
class QListWidget;
class QPushButton;

// 管理前台门控额外识别的游戏进程名（内置 JX3Client* 之外的自定义条目）。
class GameProcessDialog : public QDialog {
public:
    explicit GameProcessDialog(const QStringList& initialProcesses, QWidget* parent = nullptr);

    [[nodiscard]] QStringList processes() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void addProcess(const QString& name);
    void refreshListTheme();
    void refreshRunningProcesses();

    FluentComboBox* processCombo_ = nullptr;
    FluentLineEdit* processEdit_ = nullptr;
    QListWidget* listWidget_ = nullptr;
    QPushButton* okButton_ = nullptr;
    QStringList processes_;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
