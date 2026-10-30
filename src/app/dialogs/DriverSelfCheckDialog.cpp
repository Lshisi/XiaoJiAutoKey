#include "dialogs/DriverSelfCheckDialog.h"
DriverSelfCheckDialog::DriverSelfCheckDialog(QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(tr("驱动自检"));
    present("DialogReport.qml", {{"heading", QString()}, {"body", QString()}, {"bodyRich", true}, {"status", QString()},
        {"severity", 0}, {"progressVisible", false}, {"progressIndeterminate", true}, {"progressValue", 0}, {"progressText", QString()}},
        QSize(360, 180), true, true);
}
void DriverSelfCheckDialog::beginCheck() {
    busy_ = true;
    actions_.clear();
    setValue("closeAllowed", false);
    setValue("heading", tr("正在检测驱动环境…"));
    setValue("body", tr("正在检查驱动运行状态与 Windows 内核隔离设置，请稍候。"));
    setValue("progressVisible", true);
    setValue("severity", 0);
    setValue("actions", QVariantList{});
}
void DriverSelfCheckDialog::setConclusion(const Conclusion& conclusion) {
    busy_ = false;
    actions_ = conclusion.actions;
    setValue("heading", conclusion.heading);
    setValue("body", conclusion.bodyHtml);
    setValue("severity", static_cast<int>(conclusion.severity));
    setValue("progressVisible", false);
    setValue("closeAllowed", true);
    QVariantList actions;
    for (qsizetype index = 0; index < actions_.size(); ++index) {
        auto entry = button(QStringLiteral("diagnose:%1").arg(static_cast<int>(actions_[index].first)), actions_[index].second, index == 0);
        actions << entry;
    }
    setValue("actions", actions);
}
void DriverSelfCheckDialog::handleAction(const QString& action, const QVariant& argument) {
    if (busy_) return;
    for (const auto& [id, text] : actions_) {
        Q_UNUSED(text)
        if (action == QStringLiteral("diagnose:%1").arg(static_cast<int>(id))) { emit actionRequested(static_cast<int>(id)); return; }
    }
    QmlDialog::handleAction(action, argument);
}
