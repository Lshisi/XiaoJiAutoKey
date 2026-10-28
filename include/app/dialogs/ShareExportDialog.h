#pragma once
#include "ShareCodec.h"
#include "dialogs/QmlDialog.h"
#include <functional>
class ShareExportDialog : public QmlDialog {
    Q_OBJECT
public:
    ShareExportDialog(const QString& title, const SharePayload& payload, QWidget* parent = nullptr,
        const std::function<void(const QString&)>& onCopied = {});
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
private:
    void refreshExport();
    SharePayload payload_;
    QString summary_;
    std::function<void(const QString&)> onCopied_;
};
