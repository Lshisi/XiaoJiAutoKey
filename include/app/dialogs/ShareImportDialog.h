#pragma once
#include "ShareCodec.h"
#include "dialogs/QmlDialog.h"
class ShareImportDialog : public QmlDialog {
    Q_OBJECT
public:
    explicit ShareImportDialog(QWidget* parent = nullptr);
    void setText(const QString& text);
    bool hasImport() const noexcept { return hasImport_; }
    const SharePayload& importedPayload() const noexcept { return importPayload_; }
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
private:
    void previewImport();
    SharePayload importPayload_;
    bool hasImport_ = false;
};
