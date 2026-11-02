#pragma once

#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QVariantMap>

class QMainWindow;
class QMenu;
class QQuickWidget;
class AboutPageModel;
class UsagePageModel;
class KeyPageModel;
class MacroPageModel;
class DriverPageModel;
class SettingsPageModel;
class ShareImportBannerModel;

class MainWindowView : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QVariantList factions READ factions CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString activityIcon READ activityIcon NOTIFY activityIconChanged)

public:
    ~MainWindowView() override;
    bool setup(QMainWindow* window, QObject* onboarding);
    void initializeTray(QMainWindow* window);
    void navigateTo(const QString& pageId);
    void showRepeatPage();
    void showMacroPage();
    void updateFactionVisuals(QMainWindow* window);
    void updateActivityIcons(QMainWindow* window, bool repeatRunning, bool macroRunning);
    void refreshRuntimeVisuals(QMainWindow* window, bool dpiChanged, bool refreshThemeColors, int nativeDpi = 0);
    void hideTrayIcon();

    QString currentPage() const { return currentPage_; }
    QVariantMap theme() const { return theme_; }
    QVariantList factions() const;
    QString version() const;
    QString title() const;
    QString activityIcon() const;
    bool isRepeatPageActive() const;
    Q_INVOKABLE void selectPage(const QString& pageId);
    Q_INVOKABLE void selectFaction(const QString& factionId);
    Q_INVOKABLE void startWindowMove();
    Q_INVOKABLE void minimizeWindow();
    Q_INVOKABLE void closeWindow();

    ShareImportBannerModel* shareBanner() const { return shareBanner_; }
    KeyPageModel* keyPage() const { return keyPage_; }
    MacroPageModel* macroPage() const { return macroPage_; }
    DriverPageModel* driverPage() const { return driverPage_; }
    SettingsPageModel* settingsPage() const { return settingsPage_; }
    AboutPageModel* aboutPage() const { return aboutPage_; }
    UsagePageModel* usagePage() const { return usagePage_; }
    QSystemTrayIcon* trayIcon() const { return trayIcon_; }

signals:
    void currentPageChanged();
    void themeChanged();
    void titleChanged();
    void activityIconChanged();
    void exitRequested();
    void pageSelected(const QString& pageId);
    void factionSelected(const QString& factionId);

private:
    QQuickWidget* quickWidget_ = nullptr;
    QPointer<QMainWindow> window_;
    ShareImportBannerModel* shareBanner_ = nullptr;
    KeyPageModel* keyPage_ = nullptr;
    MacroPageModel* macroPage_ = nullptr;
    DriverPageModel* driverPage_ = nullptr;
    SettingsPageModel* settingsPage_ = nullptr;
    AboutPageModel* aboutPage_ = nullptr;
    UsagePageModel* usagePage_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QIcon idleIcon_;
    QIcon activeIcon_;
    bool activityIconActive_ = false;
    QString currentPage_ = QStringLiteral("keys");
    QVariantMap theme_;
};
