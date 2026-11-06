#pragma once
#include "AppTypes.h"
#include "dialogs/QmlDialog.h"
#include <functional>
#include <optional>
#include <QSize>
class KeyCaptureDialogBase : public QmlDialog {
    Q_OBJECT
public:
    explicit KeyCaptureDialogBase(QWidget* parent = nullptr);
    void handleCapturedKey(quint64 encodedKey);
    void handleModifierReleased(KeyDescriptor key);
    std::optional<KeyDescriptor> capturedKey() const { return capturedKey_; }
    void setInitialKey(KeyDescriptor key) { setCapturedKey(key); }
    void setPrimaryMouseButtonsAllowed(bool allowed) { allowPrimaryMouseButtons_ = allowed; }
    void setModeGuideCallbacks(std::function<bool(RepeatMode)> shouldShow,
        std::function<void(RepeatMode)> dismissed);
signals:
    void keyCaptureRequested();
    void keyAccepted();
protected:
    void setCapturedKey(KeyDescriptor key);
    void prepareEditor(QVariantMap fields, std::function<bool()> supportsMouseMove = {},
        const QSize& initialSize = QSize(420, 290));
    ClickPosition clickPosition() const;
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
private:
    void updateModeGuide();
    void dismissModeGuide();
    void showModifierPending(const KeyDescriptor& key);
    std::optional<KeyDescriptor> capturedKey_;
    std::function<bool()> supportsMouseMove_;
    ClickPosition clickPos_;
    bool allowPrimaryMouseButtons_ = false;
    std::function<bool(RepeatMode)> shouldShowModeGuide_;
    std::function<void(RepeatMode)> modeGuideDismissed_;
};
