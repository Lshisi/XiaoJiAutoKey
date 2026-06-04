#pragma once

#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <functional>

class AboutPage;
class FluentSidebar;
class KeyPage;
class MacroPage;
class SettingsPage;

class MainWindowView {
public:
    void setup(QMainWindow* window);

    void navigateTo(const QString& pageId);
    void showRepeatPage();
    void showMacroPage();
    void updateModeSwitchStyle(bool anyEngineRunning);
    void updateFactionVisuals(QMainWindow* window);
    void hideTrayIcon();
    void connectModeSwitches(QObject* receiver, std::function<bool()> canSwitch, std::function<void(int)> modeChanged);

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
    QWidget* keySettingsContainer_ = nullptr;
    QStackedWidget* keyMacroStack_ = nullptr;
    QPushButton* modeRepeatBtn_ = nullptr;
    QPushButton* modeMacroBtn_ = nullptr;
    KeyPage* keyPage_ = nullptr;
    MacroPage* macroPage_ = nullptr;
    SettingsPage* settingsPage_ = nullptr;
    AboutPage* aboutPage_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
};
