#pragma once
#include "dialogs/QmlDialog.h"
#include <string>
class SoundService;
struct SoundSettingsResult { std::wstring soundOn; std::wstring soundOff; int volume = 100; };
class SoundSettingsDialog : public QmlDialog {
    Q_OBJECT
public:
    SoundSettingsDialog(SoundService& soundService, bool macroMode, SoundSettingsResult initial, QWidget* parent = nullptr);
    SoundSettingsResult settings() const;
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
private:
    SoundService& soundService_;
    bool macroMode_ = false;
};
