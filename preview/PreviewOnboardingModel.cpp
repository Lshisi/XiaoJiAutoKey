#include "PreviewOnboardingModel.h"
#include <QStringList>

PreviewOnboardingModel::PreviewOnboardingModel(QObject* parent) : PageModel(parent) { refresh(); }
void PreviewOnboardingModel::show() { active_ = true; step_ = -1; refresh(); }
void PreviewOnboardingModel::begin() { step_ = 0; refresh(); }
void PreviewOnboardingModel::next() { if (++step_ >= 5) active_ = false; refresh(); }
void PreviewOnboardingModel::previous() { if (step_ > -1) --step_; refresh(); }
void PreviewOnboardingModel::skip() { active_ = false; refresh(); }

void PreviewOnboardingModel::refresh() {
    const QStringList pages{"driver", "keys", "keys", "settings", "keys"};
    const QStringList targets{"driver", "add", "hotkey", "scope", "run"};
    const QStringList titles{tr("确认驱动"), tr("添加按键"), tr("设置启停快捷键"), tr("设置生效范围"), tr("启动与停止")};
    const QStringList descriptions{
        tr("在驱动管理页查看当前驱动、切换模式或打开自检。"),
        tr("点击添加按键，选择连发模式和间隔；也可以编辑或删除已有按键。"),
        tr("在右侧启动触发区域设置快捷键，并选择提示音和悬浮窗。"),
        tr("可以限制仅在游戏客户端位于前台时生效，并管理额外进程。"),
        tr("启动和停止始终位于右侧的同一个位置；运行时相关编辑操作会锁定。")};
    const bool welcome = step_ < 0;
    const bool inStep = step_ >= 0 && step_ < pages.size();
    state_ = {{"active", active_}, {"visible", active_}, {"startBlocked", active_},
        {"welcome", welcome}, {"target", inStep ? targets[step_] : QString()},
        {"title", inStep ? titles[step_] : tr("欢迎使用小叽按键")},
        {"body", inStep ? descriptions[step_] : tr("跟随引导了解驱动、按键配置、快捷键和启动操作。")},
        {"hint", tr("当前为界面预览，引导中的操作不会启动按键引擎。")},
        {"progress", inStep ? tr("%1 / 5").arg(step_ + 1) : tr("新手引导")},
        {"actionText", QString()}, {"secondaryText", QString()}, {"canPrevious", step_ >= 0},
        {"canNext", true}, {"nextText", step_ == 4 ? tr("完成") : tr("下一步")}};
    if (active_ && inStep) emit navigateRequested(pages[step_]);
    emit stateChanged();
}
