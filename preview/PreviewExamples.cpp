#include "PreviewController.h"

#include "FactionTheme.h"
#include "ui/styles/DesignTokens.h"
#include <QTimer>
#include <QWindow>

void PreviewController::showDriverReport() {
    dialog_.present("DialogReport.qml", tr("驱动自检"),
        {{"heading", tr("驱动自检结果（示例）")}, {"body", driverPage_.state().value("details")},
            {"bodyRich", false}, {"status", tr("此窗口仅展示自检结果，不访问系统驱动。")},
            {"severity", demoData_ ? 1 : 2}, {"progressVisible", false},
            {"progressIndeterminate", false}, {"progressValue", 0.0}, {"progressText", QString()},
            {"actions", QVariantList{
                QVariantMap{{"id", "accept"}, {"text", tr("诊断信息")}, {"primary", true},
                    {"danger", false}, {"enabled", true}},
                QVariantMap{{"id", "cancel"}, {"text", tr("关闭")}, {"primary", false},
                    {"danger", false}, {"enabled", true}}}}},
        [this](const QVariantMap&) {
            QTimer::singleShot(0, this, &PreviewController::showDiagnosticReport);
        }, 500);
}

void PreviewController::showDiagnosticReport() {
    const QString mode = QStringList{"DD", "HID", "USB"}[static_cast<int>(driverMode_)];
    const bool ready = driverMode_ == DriverMode::Legacy ? ddInstalled_
        : driverMode_ == DriverMode::Hid ? hidInstalled_ : demoData_;
    const QString report = tr("［小叽按键诊断信息（界面预览示例）］\n"
        "生成时间: 示例时间\n\n"
        "［基础环境］\n"
        "软件版本: V%1\n"
        "操作系统: Windows（示例）\n"
        "系统架构: x64\n"
        "进程架构: x64\n\n"
        "［驱动状态］\n"
        "当前模式: %2\n"
        "驱动就绪: %3\n"
        "主动卸载后停用自动安装: 否\n\n"
        "［运行库与设备］\n"
        "驱动目录: 示例状态\n"
        "DD32.dll / DD64.dll: 示例状态\n"
        "HID 运行库与服务: 示例状态\n"
        "USB 设备: 示例状态\n\n"
        "［最近错误］\n无\n\n"
        "［安全环境］\n"
        "内存完整性运行中: 示例状态\n"
        "易受攻击驱动阻止列表: 示例状态\n"
        "近 7 天相关拦截记录: 无\n\n"
        "［运行配置］\n"
        "按键配置启用: %4\n"
        "循环配置启用: %5\n"
        "按键引擎运行中: %6\n"
        "循环引擎运行中: %7\n"
        "仅游戏前台生效: 否\n\n"
        "说明: 正式程序不包含个人路径、剪贴板内容和完整配置；此处仅展示界面预览。")
        .arg(version(), mode, ready ? tr("是") : tr("否"),
            keys_.model()->state().value("profileEnabled").toBool() ? tr("是") : tr("否"),
            macros_.model()->state().value("profileEnabled").toBool() ? tr("是") : tr("否"),
            keys_.model()->state().value("running").toBool() ? tr("是") : tr("否"),
            macros_.model()->state().value("running").toBool() ? tr("是") : tr("否"));

    const int request = ++diagnosticPreviewRequest_;
    dialog_.present("DialogShare.qml", tr("诊断信息"),
        {{"exporting", true}, {"format", 0}, {"formatVisible", false}, {"diagnostic", true}, {"resizable", true},
            {"code", tr("正在后台读取诊断信息…")}, {"namePreview", QString()},
            {"status", tr("界面预览示例，不访问系统驱动。")},
            {"actions", QVariantList{QVariantMap{{"id", "cancel"}, {"text", tr("关闭")},
                {"primary", false}, {"danger", false}, {"enabled", true}}}}}, {}, 500);
    QTimer::singleShot(300, this, [this, request, report]() {
        if (request != diagnosticPreviewRequest_ || !dialog_.visible()
            || !dialog_.dialogState().value("diagnostic").toBool()) return;
        dialog_.edit("code", report);
        dialog_.edit("actions", QVariantList{
            QVariantMap{{"id", "copy"}, {"text", tr("复制")}, {"primary", true},
                {"danger", false}, {"enabled", true}},
            QVariantMap{{"id", "cancel"}, {"text", tr("关闭")}, {"primary", false},
                {"danger", false}, {"enabled", true}}});
    });
}

void PreviewController::showProcesses() {
    dialog_.present("DialogProcesses.qml", tr("管理游戏进程"),
        {{"runningProcesses", QStringList{"jx3clientx64.exe", "mumuplayer.exe", "game.exe"}},
            {"processes", processes_}, {"processName", QString()}},
        [this](const QVariantMap& values) { processes_ = values.value("processes").toStringList(); }, 500);
}

void PreviewController::showOverlaySettings(bool macro) {
    auto fields = overlayOptions_;
    fields.insert("tab", macro ? 1 : 0);
    dialog_.present("DialogOverlaySettings.qml", tr("悬浮窗设置"), fields,
        [this](const QVariantMap& values) { overlayOptions_ = values; emit overlayChanged(); });
}

void PreviewController::showUpdateProgress() {
    dialog_.present("DialogReport.qml", tr("下载更新"),
        {{"heading", tr("正在下载更新（示例）")}, {"body", tr("更新包下载和安装过程的界面预览。")},
            {"bodyRich", false}, {"status", tr("演示进度，不发起网络请求或安装更新。")},
            {"severity", 0}, {"progressVisible", true}, {"progressIndeterminate", false},
            {"progressValue", 0.65}, {"progressText", tr("65% · 示例更新包")}});
}

void PreviewController::showExample(int example) {
    switch (example) {
    case 1:
        dialog_.present("DialogClose.qml", tr("关闭确认"), {{"remember", false}}, {}, 360);
        dialog_.edit("actions", QVariantList{
            QVariantMap{{"id", "exit"}, {"text", tr("退出程序")}, {"primary", true}, {"danger", false}, {"enabled", true}},
            QVariantMap{{"id", "tray"}, {"text", tr("最小化到托盘")}, {"primary", false}, {"danger", false}, {"enabled", true}}});
        break;
    case 2:
        dialog_.present("DialogShare.qml", tr("导入配置"), {{"exporting", false}, {"format", 1},
            {"namePreview", tr("方案：打本配置\n1：主输出\n侧键1：接管")},
            {"code", QString()}, {"status", tr("此处展示导入窗口，内容不会保存到用户配置。")}});
        break;
    case 3: showUpdateProgress(); break;
    case 4: shareBanner_.showForText(QStringLiteral("preview")); break;
    case 5: dialog_.resetEditorGuides(); onboarding_.show(); break;
    case 6: pickerVisible_ = true; emit pickerChanged(); break;
    default: break;
    }
}

bool PreviewController::overlayVisible() const {
    return keys_.model()->state().value("overlayEnabled").toBool()
        || macros_.model()->state().value("overlayEnabled").toBool();
}

QVariantMap PreviewController::overlayState() const {
    const auto colors = FactionTheme::instance().palette();
    const bool dark = Fluent::Theme::isDarkMode();
    const auto repeat = keys_.model()->state();
    const auto macro = macros_.model()->state();
    QStringList repeatLines;
    QStringList macroLines;
    if (overlayOptions_.value("repeatHotkey").toBool()) repeatLines << tr("启动键：%1").arg(repeat.value("hotkey").toString());
    for (const auto& value : keys_.model()->rows()) {
        const auto row = value.toMap();
        const QString option = QStringList{"repeatToggle", "repeatHold", "repeatIndependent", "repeatSequence", "repeatLongPress"}[row.value("mode").toInt()];
        if (overlayOptions_.value(option).toBool())
            repeatLines << tr("%1：%2").arg(row.value("modeText").toString(),
                row.value("name").toString().isEmpty() ? row.value("keyText").toString()
                    : tr("%1（%2）").arg(row.value("name").toString(), row.value("keyText").toString()));
    }
    if (overlayOptions_.value("repeatSequenceHotkey").toBool() && repeat.value("hasSequenceKey").toBool())
        repeatLines << tr("切换键：%1").arg(repeat.value("sequenceKey").toString());
    if (overlayOptions_.value("repeatPauseKeys").toBool() && !repeat.value("pauseKeys").toList().isEmpty())
        repeatLines << tr("暂停键：%1").arg(repeat.value("pauseKeys").toStringList().join("、"));
    if (overlayOptions_.value("macroHotkey").toBool()) macroLines << tr("触发键：%1").arg(macro.value("hotkey").toString());
    if (overlayOptions_.value("macroTriggerMode").toBool()) macroLines << (macro.value("triggerMode").toInt() == 0 ? tr("开关触发") : tr("按压触发"));
    if (overlayOptions_.value("macroLoopCount").toBool())
        macroLines << (macro.value("loopCount").toInt() == 0 ? tr("循环次数：无限") : tr("循环次数：%1").arg(macro.value("loopCount").toInt()));
    if (overlayOptions_.value("macroCurrentStep").toBool()) {
        for (const auto& value : macros_.model()->rows()) {
            const auto step = value.toMap();
            if (!step.value("enabled").toBool()) continue;
            macroLines << (step.value("name").toString().isEmpty()
                ? tr("当前：%1").arg(step.value("keyText").toString())
                : tr("当前：%1（%2）").arg(step.value("name").toString(), step.value("keyText").toString()));
            break;
        }
    }
    if (overlayOptions_.value("macroPauseKeys").toBool() && !macro.value("pauseKeys").toList().isEmpty())
        macroLines << tr("暂停键：%1").arg(macro.value("pauseKeys").toStringList().join("、"));
    return {{"showRepeat", keys_.model()->state().value("overlayEnabled")},
        {"showMacro", macros_.model()->state().value("overlayEnabled")},
        {"repeatActive", keys_.model()->state().value("running")},
        {"macroActive", macros_.model()->state().value("running")}, {"gameGatePaused", false},
        {"repeatTitle", tr("按键连发")}, {"macroTitle", tr("循环连发")},
        {"repeatLines", repeatLines}, {"macroLines", macroLines},
        {"contentPadding", 8}, {"fontFamily", QStringLiteral("Microsoft YaHei UI")}, {"fontPixelSize", 12},
        {"text", Fluent::Colors::overlayText(dark)}, {"muted", Fluent::Colors::overlayTextSecondary(dark)},
        {"panelColor", Fluent::Colors::overlayPanel(dark)}, {"accent", colors.accent},
        {"accentBorder", colors.accentBorder}, {"warning", Fluent::Colors::warning()},
        {"opacity", overlayOptions_.value("opacity", 70)}, {"locked", overlayLocked_}};
}

void PreviewController::toggleLock() { overlayLocked_ = !overlayLocked_; emit overlayChanged(); }
void PreviewController::setPickerPoint(qreal x, qreal y) { pickerPoint_ = QPointF(x, y); emit pickerChanged(); }
void PreviewController::endPicking() { pickerVisible_ = false; emit pickerChanged(); }

QVariantMap PreviewController::hintState() const {
    return {{"x", pickerPoint_.x()}, {"y", pickerPoint_.y()},
        {"right", window_ ? window_->width() : 780}, {"bottom", window_ ? window_->height() : 600},
        {"screenWidth", window_ ? window_->width() : 780},
        {"text", tr("坐标拾取界面预览\n点击或按 Esc 返回")}, {"surface", Fluent::Colors::workspacePanel()},
        {"textColor", Fluent::Colors::workspaceText()}, {"border", Fluent::Colors::workspaceLine()},
        {"fontFamily", QStringLiteral("Microsoft YaHei UI")}};
}
