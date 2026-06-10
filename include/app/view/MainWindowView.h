#pragma once

#include <QMainWindow>
#include <QMenu>
#include <QStackedWidget>
#include <QSystemTrayIcon>

class AboutPage;
class FluentSidebar;
class KeyPage;
class MacroPage;
class SettingsPage;

class MainWindowView {
public:
    void setup(QMainWindow* window);
    void initializeTray(QMainWindow* window);

    void navigateTo(const QString& pageId);
    void showRepeatPage();
    void showMacroPage();
    void updateFactionVisuals(QMainWindow* window);
    void refreshRuntimeVisuals(QMainWindow* window, bool dpiChanged, bool refreshThemeColors, int nativeDpi = 0);
    void hideTrayIcon();

    [[nodiscard]] bool isRepeatPageActive() const;

    [[nodiscard]] FluentSidebar* sidebar() const { return sidebar_; }
    [[nodiscard]] KeyPage* keyPage() const { return keyPage_; }
    [[nodiscard]] MacroPage* macroPage() const { return macroPage_; }
    [[nodiscard]] SettingsPage* settingsPage() const { return settingsPage_; }
    [[nodiscard]] AboutPage* aboutPage() const { return aboutPage_; }
    [[nodiscard]] QSystemTrayIcon* trayIcon() const { return trayIcon_; }

private:
    FluentSidebar* sidebar_ = nullptr;
    QStackedWidget* pageStack_ = nullptr;
    KeyPage* keyPage_ = nullptr;
    MacroPage* macroPage_ = nullptr;
    SettingsPage* settingsPage_ = nullptr;
    AboutPage* aboutPage_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
};
