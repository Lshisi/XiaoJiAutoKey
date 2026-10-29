#include "dialogs/StatusOverlaySettingsDialog.h"
#include <algorithm>
StatusOverlaySettingsDialog::StatusOverlaySettingsDialog(const AppConfig& config, bool macroPage, QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(tr("悬浮窗设置"));
    present("DialogOverlaySettings.qml", {
        {"tab", macroPage ? 1 : 0}, {"opacity", config.statusOverlayOpacity},
        {"repeatHotkey", config.repeatOverlay.showHotkey}, {"repeatIndependent", config.repeatOverlay.showIndependent},
        {"repeatHold", config.repeatOverlay.showHold}, {"repeatToggle", config.repeatOverlay.showToggle},
        {"repeatSequence", config.repeatOverlay.showSequence}, {"repeatSequenceHotkey", config.repeatOverlay.showSequenceHotkey},
        {"repeatPauseKeys", config.repeatOverlay.showPauseKeys},
        {"macroHotkey", config.macroOverlay.showHotkey}, {"macroTriggerMode", config.macroOverlay.showTriggerMode},
        {"macroLoopCount", config.macroOverlay.showLoopCount}, {"macroPauseKeys", config.macroOverlay.showPauseKeys},
        {"actions", QVariantList{button("accept", tr("保存")), button("cancel", tr("取消"), false)}}
    }, QSize(400, 400));
}
StatusOverlaySettings StatusOverlaySettingsDialog::settings() const {
    return {{value("repeatHotkey").toBool(), value("repeatIndependent").toBool(), value("repeatHold").toBool(),
        value("repeatToggle").toBool(), value("repeatSequence").toBool(), value("repeatSequenceHotkey").toBool(), value("repeatPauseKeys").toBool()},
        {value("macroHotkey").toBool(), value("macroTriggerMode").toBool(), value("macroLoopCount").toBool(), value("macroPauseKeys").toBool()},
        value("opacity").toInt()};
}
void StatusOverlaySettingsDialog::editField(const QString& field, const QVariant& argument) {
    if (field == "opacity") setValue(field, std::clamp(argument.toInt(), 30, 100));
    else if (field == "tab") setValue(field, std::clamp(argument.toInt(), 0, 2));
    else if (field.startsWith("repeat") || field.startsWith("macro")) {
        if (dialogState().contains(field)) setValue(field, argument.toBool());
    }
}
void StatusOverlaySettingsDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action == "accept") accept();
    else QmlDialog::handleAction(action, argument);
}
