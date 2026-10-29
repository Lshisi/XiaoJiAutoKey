#include "dialogs/UpdateDialog.h"
#include <QStringList>
#include <algorithm>
namespace {
QString formatMegabytes(qint64 bytes) {
    return QString::number(static_cast<double>(bytes) / 1024.0 / 1024.0, 'f', 1)
        + QStringLiteral(" MB");
}

// 先转义 Gitee 发行版描述，再把轻量 Markdown 标题、列表转成富文本。
QString formatNotes(const QString& notes) {
    if (notes.trimmed().isEmpty()) {
        return QString::fromWCharArray(
            L"\u6682\u65e0\u66f4\u65b0\u65e5\u5fd7\u3002");
    }
    QStringList lines;
    for (QString line : notes.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        line = line.toHtmlEscaped();
        if (line.startsWith(QStringLiteral("### "))) {
            line = line.mid(4);
        } else if (line.startsWith(QStringLiteral("## "))) {
            line = QStringLiteral("<b>") + line.mid(3) + QStringLiteral("</b>");
        } else if (line.startsWith(QStringLiteral("# "))) {
            line = QStringLiteral("<b>") + line.mid(2) + QStringLiteral("</b>");
        }
        if (line.startsWith(QStringLiteral("- "))) {
            line = QStringLiteral("\u2022 ") + line.mid(2);
        }
        lines.push_back(line);
    }
    return lines.join(QStringLiteral("<br>"));
}


}

UpdateDialog::UpdateDialog(const QString& version, const QString& notes, bool hasAsset, QWidget* parent) : QmlDialog(parent) {
    setWindowTitle(tr("软件更新"));
    present("DialogReport.qml", {{"heading", tr("发现新版本 V%1").arg(version)}, {"body", formatNotes(notes)},
        {"bodyRich", true}, {"status", QString()}, {"severity", 0}, {"progressVisible", false},
        {"progressIndeterminate", false}, {"progressValue", 0}, {"progressText", QString()},
        {"actions", QVariantList{button("confirm", hasAsset ? tr("确认更新") : tr("前往下载")), button("cancel", tr("取消"), false)}}},
        QSize(440, 404));
}
void UpdateDialog::beginDownload() {
    busy_ = true;
    staged_ = false;
    setValue("closeAllowed", false);
    setValue("actions", QVariantList{});
    setValue("status", tr("正在下载更新包…"));
    setValue("severity", 0);
    setValue("progressVisible", true);
    setValue("progressIndeterminate", true);
    setValue("progressText", QString());
}
void UpdateDialog::setDownloadProgress(qint64 received, qint64 total) {
    setValue("progressIndeterminate", total <= 0);
    if (total > 0) setValue("progressValue", std::clamp(100.0 * received / total, 0.0, 100.0));
    setValue("progressText", total > 0 ? tr("%1 / %2").arg(formatMegabytes(received), formatMegabytes(total)) : formatMegabytes(received));
}
void UpdateDialog::setStagedReady() {
    busy_ = false;
    staged_ = true;
    setValue("closeAllowed", true);
    setValue("progressVisible", false);
    setValue("status", tr("更新包已就绪，重启后自动完成安装。"));
    setValue("severity", 1);
    setValue("actions", QVariantList{button("restart", tr("立即重启更新")), button("cancel", tr("稍后"), false)});
}
void UpdateDialog::setDownloadError(const QString& message) {
    busy_ = false;
    staged_ = false;
    setValue("closeAllowed", true);
    setValue("progressVisible", false);
    setValue("status", message);
    setValue("severity", 3);
    setValue("actions", QVariantList{button("confirm", tr("重试")), button("cancel", tr("取消"), false)});
}
void UpdateDialog::handleAction(const QString& action, const QVariant& argument) {
    if (busy_) return;
    if (action == "confirm" && !staged_) emit confirmRequested();
    else if (action == "restart" && staged_) emit restartRequested();
    else QmlDialog::handleAction(action, argument);
}
