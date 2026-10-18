#pragma once

#include <QColor>
#include <QDialog>
#include <QPair>
#include <QPoint>
#include <QString>
#include <QVector>

class FluentButton;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;

// 驱动自检弹窗：检测中 → 结论（内核隔离拦截 / 驱动异常 / 一切正常）→ 执行修复 → 结果。
// 弹窗自身不做检测与修复，所有状态由 DriverSelfCheckCoordinator 驱动（仿 UpdateDialog）。
class DriverSelfCheckDialog : public QDialog {
    Q_OBJECT

public:
    enum class Action {
        None = 0,
        OpenSecurityCenter,  // 打开 Windows 安全中心（内核隔离页）
        RepairDd,            // 修复 DD 驱动
        SwitchToHid,         // 卸载 DD 并切换到 HID
        ReinstallHid,        // 重装 HID 驱动
        SwitchToDd,          // 切换到 DD 驱动（复用完整切换流程）
        RebootComputer,      // 立即重启电脑
    };
    Q_ENUM(Action)

    enum class Severity {
        Info = 0,  // 中性（检测中 / 执行中）
        Ok,        // 绿：一切正常
        Warning,   // 黄：可运行但存在风险
        Danger,    // 红：内核隔离拦截 / 驱动故障
    };

    struct Conclusion {
        Severity severity = Severity::Info;
        QString heading;
        QString bodyHtml;
        QVector<QPair<Action, QString>> actions;  // 底部动作按钮（动作 + 文案），首项为主按钮
    };

    explicit DriverSelfCheckDialog(QWidget* parent = nullptr);

    // 进入检测中状态（忙碌，不允许关闭）。
    void beginCheck();
    // 展示诊断结论与动作按钮。
    void setConclusion(const Conclusion& conclusion);
    // 进入执行修复状态（忙碌，不允许关闭）；期间可用 setExecuteMessage 更新阶段文案。
    void beginExecute(const QString& title);
    void setExecuteMessage(const QString& message);
    // 展示执行结果与后续动作按钮。
    void finishExecute(bool ok, const QString& resultHtml,
        const QVector<QPair<Action, QString>>& actions);

signals:
    void actionRequested(int action);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void rebuildActionButtons(const QVector<QPair<Action, QString>>& actions);
    void setBusy(bool busy);
    void refreshVisuals();

    bool busy_ = false;
    Severity severity_ = Severity::Info;
    QColor severityColor_;

    QLabel* headingLabel_ = nullptr;
    QScrollArea* bodyScroll_ = nullptr;
    QLabel* bodyLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QHBoxLayout* buttonsRow_ = nullptr;
    QVector<FluentButton*> actionButtons_;
    QPushButton* closeButton_ = nullptr;

    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
