#pragma once

#include "AppTypes.h"

#include <QPoint>
#include <QWidget>

class QLabel;
class QPushButton;
class QScreen;
class QWindow;

class StatusOverlayWindow : public QWidget {
    Q_OBJECT

public:
    explicit StatusOverlayWindow(QWidget* parent = nullptr);

    void setOverlayEnabled(bool enabled);
    void refreshForDpiChange(int nativeDpi = 0);
    void updateStatus(const AppConfig& config,
        const ProfileConfig* profile,
        const MacroConfig* macro,
        bool repeatRunning,
        bool macroRunning);

signals:
    void positionChanged(const QPoint& position);
    void lockChanged(bool locked);
    void keyConfigRequested();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool event(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

private:
    void bindDpiRefreshSignals();
    void refreshThemeStyle(bool repeatRunning, bool macroRunning);
    void setLocked(bool locked);
    void setOverlayOpacity(int opacity);
    void syncLockButton();
    void syncTopMostOrder();
    void applyClickThrough();
    QPoint defaultPosition() const;
    QPoint positionFromConfig(const AppConfig& config) const;
    QPoint boundedPosition(const QPoint& position) const;
    void moveToConfiguredPosition(const AppConfig& config);
    void applyVisibleState();
    int currentScreenDpi() const;

    QLabel* repeatIndicator_ = nullptr;
    QLabel* macroIndicator_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* repeatTitleLabel_ = nullptr;
    QLabel* repeatHotkeyLabel_ = nullptr;
    QLabel* macroTitleLabel_ = nullptr;
    QLabel* macroHotkeyLabel_ = nullptr;
    QWidget* repeatBlock_ = nullptr;
    QWidget* macroBlock_ = nullptr;
    QPushButton* lockButton_ = nullptr;
    QWidget* lockButtonWindow_ = nullptr;
    bool overlayEnabled_ = false;
    bool locked_ = false;
    int opacity_ = 70;
    bool dragging_ = false;
    bool positionInitialized_ = false;
    QWindow* boundWindowHandle_ = nullptr;
    QScreen* boundScreen_ = nullptr;
    int observedDpi_ = 0;
    QPoint dragStartGlobal_;
    QPoint dragStartWindow_;
};
