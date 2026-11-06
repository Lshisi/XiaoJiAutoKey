#pragma once

#include "PreviewWorkspaceModel.h"
#include "PreviewDialogModel.h"

class PreviewWorkspaceController final : public QObject {
    Q_OBJECT
public:
    PreviewWorkspaceController(bool macro, PreviewDialogModel& dialog, PreviewDialogModel& overview, QObject* parent = nullptr);
    PreviewWorkspaceModel* model() { return &model_; }
    const PreviewWorkspaceModel* model() const { return &model_; }
    void reset(bool demo) { model_.reset(demo); }

signals:
    void overlaySettingsRequested();
    void aboutRequested();

private:
    void handle(const QString& action, const QVariant& argument);
    void editRow(const QVariant& argument, bool adding, int insertIndex = -1, bool focusCreated = false,
        const QVariantMap& initial = {});
    void editRows(const QVariantList& selected);
    void editProfile(const QString& action);
    void editShortcut(const QString& kind, int pauseIndex = -1);
    [[nodiscard]] int rowIndexById(const QString& id) const;

    PreviewWorkspaceModel model_;
    PreviewDialogModel& dialog_;
    PreviewDialogModel& overview_;
    bool editingFromOverview_ = false;
    QVariantMap sound_{{"soundOn", QString()}, {"soundOff", QString()}, {"volume", 80}};
    quint64 nextId_ = 100;
};
