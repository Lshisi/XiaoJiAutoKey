#pragma once
#include "dialogs/QmlDialog.h"
class UpdateDialog : public QmlDialog {
    Q_OBJECT
public:
    UpdateDialog(const QString& version, const QString& notes, bool hasAsset, QWidget* parent = nullptr);
    void beginDownload();
    void setDownloadProgress(qint64 received, qint64 total);
    void setStagedReady();
    void setDownloadError(const QString& message);
signals:
    void confirmRequested();
    void restartRequested();
protected:
    void handleAction(const QString& action, const QVariant& value) override;
private:
    bool busy_ = false;
    bool staged_ = false;
};
