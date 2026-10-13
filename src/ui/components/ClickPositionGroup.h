#pragma once

#include "AppTypes.h"

#include <QWidget>
#include <functional>

class FluentButton;
class QLabel;
class QRadioButton;

// 「点击位置」设置组：跟随鼠标当前位置 / 固定点位（全屏点击拾取）。
// 供按键编辑弹窗与宏步骤编辑弹窗共用；仅鼠标键时由调用方控制显示。
class ClickPositionGroup : public QWidget {
    Q_OBJECT

public:
    ClickPositionGroup(std::function<bool()> driverSupportsMouseMove,
        QWidget* parent = nullptr);

    // 同步内部点位、单选框选中态与已拾取文本。
    void setValue(const ClickPosition& pos);

    // 固定单选选中时返回固定点位，否则返回跟随鼠标（默认值）。
    [[nodiscard]] ClickPosition value() const;

    // 按驱动能力刷新固定点位可用态与提示文案。
    void applyDriverSupport();

private:
    // 全屏点击拾取：隐藏所属弹窗，左键确认 / 右键或 Esc 取消后恢复。
    void beginPick();
    // 把屏幕坐标按所在屏幕比例记录为固定点位。
    void applyPick(const QPoint& pos);
    void refreshPickButtonsEnabled();

    std::function<bool()> driverSupportsMouseMove_;
    ClickPosition clickPos_;

    QRadioButton* followRadio_ = nullptr;
    QRadioButton* fixedRadio_ = nullptr;
    FluentButton* pickBtn_ = nullptr;
    QLabel* pickedLabel_ = nullptr;
    QLabel* hintLabel_ = nullptr;
};
