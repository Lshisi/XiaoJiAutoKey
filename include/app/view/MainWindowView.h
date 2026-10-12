#pragma once

#include <QMainWindow>
#include <QIcon>
#include <QMenu>
#include <QStackedWidget>
#include <QSystemTrayIcon>

class AboutPage;
class QAction;
class FluentSidebar;
class FluentTitleBar;
class KeyPage;
class MacroPage;
class DriverPage;
class SettingsPage;

class MainWindowView {
public:
    ~MainWindowView();
    void setup(QMainWindow* window);
    void initializeTray(QMainWindow* window);

    void navigateTo(const QString& pageId);
    void showRepeatPage();
    void showMacroPage();
    void updateFactionVisuals(QMainWindow* window);
    void updateActivityIcons(QMainWindow* window, bool repeatRunning, bool macroRunning);
    void refreshRuntimeVisuals(QMainWindow* window, bool dpiChanged, bool refreshThemeColors, int nativeDpi = 0);
    void hideTrayIcon();

    [[nodiscard]] bool isRepeatPageActive() const;

    [[nodiscard]] FluentSidebar* sidebar() const { return sidebar_; }
    [[nodiscard]] KeyPage* keyPage() const { return keyPage_; }
    [[nodiscard]] MacroPage* macroPage() const { return macroPage_; }
    [[nodiscard]] DriverPage* driverPage() const { return driverPage_; }
    [[nodiscard]] SettingsPage* settingsPage() const { return settingsPage_; }
    [[nodiscard]] AboutPage* aboutPage() const { return aboutPage_; }
    [[nodiscard]] QSystemTrayIcon* trayIcon() const { return trayIcon_; }
    [[nodiscard]] QAction* trayGameGateAction() const { return trayGateAction_; }

private:
    FluentSidebar* sidebar_ = nullptr;
    QStackedWidget* pageStack_ = nullptr;
    KeyPage* keyPage_ = nullptr;
    MacroPage* macroPage_ = nullptr;
    DriverPage* driverPage_ = nullptr;
    SettingsPage* settingsPage_ = nullptr;
    AboutPage* aboutPage_ = nullptr;
    FluentTitleBar* titleBar_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QAction* trayGateAction_ = nullptr;
    QIcon idleIcon_;
    QIcon activeIcon_;
    bool activityIconActive_ = false;
};
