#pragma once

#include "AppTypes.h"

#include <QDialog>
#include <optional>

class FluentLineEdit;

class KeyCaptureDialogBase : public QDialog {
    Q_OBJECT

public:
    using QDialog::QDialog;

    void handleCapturedKey(quint64 encodedKey);
    [[nodiscard]] std::optional<KeyDescriptor> capturedKey() const;

signals:
    void keyCaptureRequested();
    void keyAccepted();

protected:
    void setCaptureEditor(FluentLineEdit* editor);
    void setCapturedKey(KeyDescriptor key);

private:
    FluentLineEdit* keyEdit_ = nullptr;
    std::optional<KeyDescriptor> capturedKey_;
};
