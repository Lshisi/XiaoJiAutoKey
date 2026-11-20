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
    const QStringList titles{tr("先确认软件可以按键"), tr("添加第一个按键"), tr("设置一个启动和停止键"), tr("切出游戏时要不要暂停"), tr("启动与停止")};
    const QStringList descriptions{
        tr("先看这里有没有报错。正常时可以继续；有问题就点“一键自检”，按结果里的提示处理。"),
        tr("点“添加按键”，再点“点击录制”，按一下想自动连点的键，比如 1。第一次先用“开关模式”，间隔保留默认值。"),
        tr("设置一个和连点按键不同的快捷键。以后按一下就开始，再按一下就停止。"),
        tr("打开“仅游戏客户端前台时生效”，切去聊天或浏览器时会暂停，回到游戏再继续。剑网3已内置，其他游戏可在“管理进程”中按窗口标题添加。"),
        tr("右侧“启动”和“停止”在同一个位置。开始后，想改按键或设置，先点“停止”。")};
    const bool welcome = step_ < 0;
    const bool inStep = step_ >= 0 && step_ < pages.size();
    state_ = {{"active", active_}, {"visible", active_}, {"startBlocked", active_},
        {"welcome", welcome}, {"target", inStep ? targets[step_] : QString()},
        {"title", inStep ? titles[step_] : tr("欢迎使用小叽按键")},
        {"body", inStep ? descriptions[step_] : tr("先添加一个自动连点的按键，再学会怎样开始和停止。可以随时跳过，以后从“关于软件”重新查看。")},
        {"hint", tr("这里是界面预览，不会真的按键。")},
        {"progress", inStep ? tr("%1 / 5").arg(step_ + 1) : tr("新手引导")},
        {"actionText", QString()}, {"secondaryText", QString()}, {"canPrevious", step_ >= 0},
        {"canNext", true}, {"nextText", step_ == 4 ? tr("完成") : tr("下一步")}};
    if (active_ && inStep) emit navigateRequested(pages[step_]);
    emit stateChanged();
}
