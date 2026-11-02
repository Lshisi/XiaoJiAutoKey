#pragma once

#include "PreviewDialogModel.h"
#include "PreviewOnboardingModel.h"
#include "PreviewWorkspaceController.h"
#include "ui/models/AboutPageModel.h"
#include "ui/models/DriverPageModel.h"
#include "ui/models/SettingsPageModel.h"
#include "ui/models/ShareImportBannerModel.h"
#include "ui/models/UsagePageModel.h"
#include <QPointer>
#include <QPointF>
#include <QStringList>

class QWindow;

class PreviewController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(bool demoData READ demoData NOTIFY demoDataChanged)
    Q_PROPERTY(QString currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QVariantList factions READ factions CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString activityIcon READ activityIcon NOTIFY activityIconChanged)
    Q_PROPERTY(bool overlayVisible READ overlayVisible NOTIFY overlayChanged)
    Q_PROPERTY(QVariantMap overlayState READ overlayState NOTIFY overlayChanged)
    Q_PROPERTY(bool pickerVisible READ pickerVisible NOTIFY pickerChanged)
    Q_PROPERTY(QVariantMap hintState READ hintState NOTIFY pickerChanged)

public:
    explicit PreviewController(QObject* parent = nullptr);
    QVariantMap theme() const;
    QVariantList factions() const;
    bool demoData() const { return demoData_; }
    QString currentPage() const { return currentPage_; }
    QString version() const;
    QString title() const;
    QString activityIcon() const;
    bool overlayVisible() const;
    QVariantMap overlayState() const;
    bool pickerVisible() const { return pickerVisible_; }
    QVariantMap hintState() const;
    void attachWindow(QWindow* window);
    PreviewWorkspaceModel* keyPage() { return keys_.model(); }
    PreviewWorkspaceModel* macroPage() { return macros_.model(); }
    UsagePageModel* usagePage() { return &usagePage_; }
    DriverPageModel* driverPage() { return &driverPage_; }
    SettingsPageModel* settingsPage() { return &settingsPage_; }
    AboutPageModel* aboutPage() { return &aboutPage_; }
    ShareImportBannerModel* shareBanner() { return &shareBanner_; }
    PreviewOnboardingModel* onboarding() { return &onboarding_; }
    PreviewDialogModel* dialog() { return &dialog_; }
    Q_INVOKABLE void setDarkMode(bool dark);
    Q_INVOKABLE void setDemoData(bool enabled);
    Q_INVOKABLE void selectPage(const QString& page);
    Q_INVOKABLE void selectFaction(const QString& faction);
    Q_INVOKABLE void startWindowMove();
    Q_INVOKABLE void minimizeWindow();
    Q_INVOKABLE void closeWindow();
    Q_INVOKABLE void showExample(int example);
    Q_INVOKABLE void toggleLock();
    Q_INVOKABLE void setPickerPoint(qreal x, qreal y);
    Q_INVOKABLE void endPicking();

signals:
    void themeChanged();
    void demoDataChanged();
    void currentPageChanged();
    void activityIconChanged();
    void overlayChanged();
    void pickerChanged();

private:
    void navigate(const QString& page);
    void refreshDriver();
    void showDriverReport();
    void showProcesses();
    void showOverlaySettings(bool macro);
    void showUpdateProgress();
    PreviewDialogModel dialog_;
    PreviewWorkspaceController keys_;
    PreviewWorkspaceController macros_;
    UsagePageModel usagePage_;
    DriverPageModel driverPage_;
    SettingsPageModel settingsPage_;
    AboutPageModel aboutPage_;
    ShareImportBannerModel shareBanner_;
    PreviewOnboardingModel onboarding_;
    QPointer<QWindow> window_;
    QString currentPage_ = QStringLiteral("keys");
    DriverMode driverMode_ = DriverMode::Legacy;
    QStringList processes_{QStringLiteral("mumu*")};
    QVariantMap overlayOptions_;
    bool demoData_ = false;
    bool ddInstalled_ = true;
    bool hidInstalled_ = true;
    bool overlayLocked_ = false;
    bool pickerVisible_ = false;
    QPointF pickerPoint_{200, 160};
};
