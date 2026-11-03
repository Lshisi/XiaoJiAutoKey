#include "dialogs/MacroRecordingDialog.h"
#include "input/GlobalInputHook.h"
#include "MacroActions.h"
#include "MacroTiming.h"
#include <Windows.h>
MacroRecordingDialog::MacroRecordingDialog(GlobalInputHook& hook, bool supportsPosition, std::size_t stepLimit, QWidget* parent)
    : QmlDialog(parent), hook_(hook), recording_(stepLimit) {
    setWindowTitle(tr("录制一轮"));
    present("DialogMacroRecording.qml", {{"stage", "ready"}, {"seconds", 3},
        {"count", 0}, {"positions", false}, {"supportsPosition", supportsPosition}, {"summary", QString()},
        {"steps", QStringList{}}, {"actions", QVariantList{button("start", tr("开始录制")), button("cancel", tr("取消"), false)}}}, QSize(420, 320), true);
    countdown_.setInterval(1000);
    connect(&countdown_, &QTimer::timeout, this, [this] {
        if (--seconds_ > 0) { setValue("seconds", seconds_); return; }
        countdown_.stop();
        recordingActive_ = true;
        hook_.setRecordingActive(true);
        setValue("stage", "recording");
        setValue("actions", QVariantList{button("stop", tr("停止录制 F10")), button("cancel", tr("取消"), false)});
    });
    connect(&hook_, &GlobalInputHook::inputRecorded, this, [this](const RecordedInput& input) {
        if (!recordingActive_) return;
        recording_.append(input, value("positions").toBool());
        setValue("count", static_cast<int>(recording_.eventCount()));
        if (recording_.atCapacity()) stopRecording();
    });
    connect(&hook_, &GlobalInputHook::recordingStopRequested, this, &MacroRecordingDialog::stopRecording);
}
MacroRecordingDialog::~MacroRecordingDialog() { hook_.setRecordingActive(false); }
void MacroRecordingDialog::reject() {
    countdown_.stop();
    recordingActive_ = false;
    hook_.setRecordingActive(false);
    QmlDialog::reject();
}
void MacroRecordingDialog::editField(const QString& field, const QVariant& value) {
    if (field == "positions") setValue(field, value.toBool());
}
void MacroRecordingDialog::stopRecording() {
    if (!recordingActive_) return;
    recordingActive_ = false;
    hook_.setRecordingActive(false);
    steps_ = recording_.finish(GetTickCount64());
    QStringList descriptions;
    for (std::size_t index = 0; index < steps_.size(); ++index) {
        const auto& step = steps_[index];
        descriptions.push_back(tr("%1. %2").arg(index + 1).arg(QString::fromStdWString(
            MacroStepText(step, MacroActionNeedsKey(step.action) ? GetKeyDisplayName(step.key) : L""))));
    }
    setValue("steps", descriptions);
    MacroConfig macro;
    macro.steps = steps_;
    setValue("summary", (recording_.atCapacity() ? tr("已达到单次录制容量，录制已停止。\n") : QString())
        + QString::fromStdWString(FormatMacroTimingSummary(macro)));
    setValue("stage", "review");
    setValue("actions", steps_.empty()
        ? QVariantList{button("start", tr("重新录制")), button("cancel", tr("取消"), false)}
        : QVariantList{button("accept", tr("追加到循环")), button("start", tr("重新录制"), false), button("cancel", tr("取消"), false)});
    showNormal();
    raise();
    activateWindow();
}
void MacroRecordingDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "start") {
        recording_.clear();
        steps_.clear();
        seconds_ = 3;
        setValue("seconds", seconds_);
        setValue("count", 0);
        setValue("stage", "countdown");
        setValue("actions", QVariantList{button("cancel", tr("取消"), false)});
        countdown_.start();
    } else if (action == "stop") stopRecording();
    else if (action == "accept" && !steps_.empty()) accept();
    else QmlDialog::handleAction(action, argument);
}
