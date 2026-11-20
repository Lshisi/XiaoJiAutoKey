#pragma once
#include "dialogs/QmlDialog.h"
#include "AppTypes.h"
class SoundService;
class SoundSettingsDialog : public QmlDialog {
    Q_OBJECT
public:
    SoundSettingsDialog(SoundService& soundService, const AppConfig& config, SoundGroup group, QWidget* parent = nullptr);
    SoundSettings settings() const;
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
private:
    SoundService& soundService_;
    void refreshAvailability(QVariantMap& state) const;
    void applyState(QVariantMap state);
};
