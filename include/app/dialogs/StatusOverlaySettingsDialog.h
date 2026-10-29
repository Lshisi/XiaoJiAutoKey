#pragma once
#include "AppTypes.h"
#include "dialogs/QmlDialog.h"
struct StatusOverlaySettings {
    RepeatOverlayOptions repeat;
    MacroOverlayOptions macro;
    int opacity = 70;
    bool nativeBlur = true;
};
class StatusOverlaySettingsDialog : public QmlDialog {
    Q_OBJECT
public:
    StatusOverlaySettingsDialog(const AppConfig& config, bool macroPage, QWidget* parent);
    StatusOverlaySettings settings() const;
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
};
