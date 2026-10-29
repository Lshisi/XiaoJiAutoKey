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
    std::optional<KeyDescriptor> capturedKey() const { return capturedKey_; }
    void setPrimaryMouseButtonsAllowed(bool allowed) { allowPrimaryMouseButtons_ = allowed; }
signals:
    void keyCaptureRequested();
    void keyAccepted();
protected:
    void setCapturedKey(KeyDescriptor key);
    void prepareEditor(QVariantMap fields, std::function<bool()> supportsMouseMove = {},
        const QSize& initialSize = QSize(420, 250));
    ClickPosition clickPosition() const;
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
private:
    std::optional<KeyDescriptor> capturedKey_;
    std::function<bool()> supportsMouseMove_;
    ClickPosition clickPos_;
    bool allowPrimaryMouseButtons_ = false;
};
