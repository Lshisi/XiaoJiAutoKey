#pragma once
#include "dialogs/QmlDialog.h"
#include "MacroRecording.h"
#include <QTimer>
class GlobalInputHook;
class MacroRecordingDialog final : public QmlDialog {
    Q_OBJECT
public:
    MacroRecordingDialog(GlobalInputHook& hook, bool supportsPosition, std::size_t stepLimit, QWidget* parent = nullptr);
    ~MacroRecordingDialog() override;
    const std::vector<MacroStep>& steps() const { return steps_; }
    void reject() override;
signals:
    void recordingStateChanged(bool recording);
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
private:
    void stopRecording();
    GlobalInputHook& hook_;
    MacroRecording recording_;
    std::vector<MacroStep> steps_;
    QTimer countdown_;
    int seconds_ = 3;
    bool recordingActive_ = false;
};
