#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class ShareImportBannerModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)

public:
    explicit ShareImportBannerModel(QObject* parent = nullptr);
    bool visible() const { return !shareText_.isEmpty(); }
    QString shareText() const { return shareText_; }
    void showForText(const QString& text);
    void hideBanner();
    Q_INVOKABLE void importShare();
    Q_INVOKABLE void dismiss();

signals:
    void visibleChanged();
    void importRequested(const QString& text);
    void dismissed(const QString& text);

private:
    QTimer autoHideTimer_;
    QString shareText_;
};
