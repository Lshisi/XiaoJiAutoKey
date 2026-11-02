#pragma once

#include "dialogs/QmlDialog.h"

class ConfigNoteDialog final : public QmlDialog {
public:
    ConfigNoteDialog(const QString& name, const QString& note, QWidget* parent = nullptr);
    QString noteText() const;

protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
};
