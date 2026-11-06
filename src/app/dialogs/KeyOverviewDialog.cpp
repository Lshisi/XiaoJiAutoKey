#include "dialogs/KeyOverviewDialog.h"

#include "ui/models/KeyOverviewData.h"
#include "ui/models/PageModel.h"
#include <QTimer>

KeyOverviewDialog::KeyOverviewDialog(PageModel& page, bool macroMode, QWidget* parent)
    : QmlDialog(parent), page_(page), macroMode_(macroMode) {
    setWindowTitle(macroMode ? tr("循环按键总览") : tr("按键总览"));
    setObjectName(macroMode ? QStringLiteral("macroKeyOverview") : QStringLiteral("repeatKeyOverview"));
    setModal(false);
    setWindowModality(Qt::NonModal);
    setAttribute(Qt::WA_DeleteOnClose);
    overview_ = KeyOverviewData::build(page_.state(), macroMode_, selectedId_);
    present(QStringLiteral("DialogKeyOverview.qml"), {{"overview", overview_}, {"resizable", true}}, QSize(1100, 440), true);
    connect(&page_, &PageModel::stateChanged, this, &KeyOverviewDialog::refreshOverview);
}

void KeyOverviewDialog::refreshOverview() {
    overview_ = KeyOverviewData::build(page_.state(), macroMode_, selectedId_);
    setValue(QStringLiteral("overview"), overview_);
}

void KeyOverviewDialog::editField(const QString& field, const QVariant& value) {
    if (field != QStringLiteral("selectedKey")) return;
    selectedId_ = value.toString();
    refreshOverview();
}

void KeyOverviewDialog::handleAction(const QString& action, const QVariant& argument) {
    if (action != QStringLiteral("navigate")) {
        QmlDialog::handleAction(action, argument);
        return;
    }
    refreshOverview();
    const auto target = KeyOverviewData::action(overview_, argument.toString());
    if (target.isEmpty()) return;
    QTimer::singleShot(0, this, [this, target] {
        if (!page_.state().value("running").toBool()) emit navigationRequested(target);
        refreshOverview();
        raise();
        activateWindow();
    });
}
