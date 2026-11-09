#include "dialogs/DriverDiagnosticDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QTimer>

DriverDiagnosticDialog::DriverDiagnosticDialog(QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(tr("诊断信息"));
    present("DialogShare.qml", {{"exporting", true}, {"format", 0}, {"formatVisible", false},
        {"diagnostic", true}, {"resizable", true},
        {"code", QString()}, {"namePreview", QString()}, {"status", QString()}},
        QSize(480, 360));
}

void DriverDiagnosticDialog::beginReport() {
    setValue("closeAllowed", true);
    setValue("code", tr("正在后台读取诊断信息…"));
    setValue("status", QString::fromWCharArray(L"仅汇总当前状态，不访问剪贴板。"));
    setValue("actions", QVariantList{button(QStringLiteral("cancel"), tr("关闭"), false)});
}

void DriverDiagnosticDialog::setReport(const QString& report) {
    report_ = report;
    setValue("closeAllowed", true);
    setValue("code", report);
    setValue("status", QString::fromWCharArray(L"可先检查内容，再点击“复制”。"));
    setValue("actions", QVariantList{
        button(QStringLiteral("copy"), tr("复制")),
        button(QStringLiteral("cancel"), tr("关闭"), false)});
}

void DriverDiagnosticDialog::handleAction(const QString& action, const QVariant& value) {
    if (action == QStringLiteral("copy")) {
        QTimer::singleShot(0, this, [this]() {
            QApplication::clipboard()->setText(report_);
            setValue("status", tr("已复制到剪贴板"));
        });
        return;
    }
    QmlDialog::handleAction(action, value);
}
