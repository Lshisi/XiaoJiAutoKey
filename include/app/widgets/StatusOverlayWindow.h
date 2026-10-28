#pragma once

#include "AppTypes.h"

#include <QPointF>
#include <QPointer>
#include <QVariantMap>
#include <QWidget>

class QQuickWidget;
class QScreen;
struct RepeaterStatus;
struct MacroStatus;

class StatusOverlayWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QVariantMap overlayState READ overlayState NOTIFY overlayStateChanged)
    Q_PROPERTY(QPointF lightPosition READ lightPosition NOTIFY lightPositionChanged)

public:
    explicit StatusOverlayWindow(QWidget* parent = nullptr);
    ~StatusOverlayWindow() override;
    bool initialize(QWidget* dialogParent);

    void setOverlayEnabled(bool enabled);
    void setGameGatePaused(bool paused);
    void refreshForDpiChange(int nativeDpi = 0);
    void updateStatus(const AppConfig& config,
        const ProfileConfig* profile,
        const MacroConfig* macro,
        const RepeaterStatus& repeat,
        const MacroStatus& macroStatus);
    QVariantMap overlayState() const { return overlayState_; }
    QPointF lightPosition() const { return lightPosition_; }
    Q_INVOKABLE void toggleLock();

signals:
    void overlayStateChanged();
    void lightPositionChanged();
    void positionChanged(const QPoint& position);
    void lockChanged(bool locked);
    void keyConfigRequested();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool event(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void bindDpiRefreshSignals();
    void setLocked(bool locked);
    void setOverlayOpacity(int opacity);
    void setLightPosition(const QPointF& position);
    void setStateValue(const QString& key, const QVariant& value);
    void syncLockButton();
    void syncTopMostOrder();
    void applyClickThrough();
    QPoint defaultPosition() const;
    QPoint positionFromConfig(const AppConfig& config) const;
    QPoint boundedPosition(const QPoint& position) const;
    void moveToConfiguredPosition(const AppConfig& config);
    void applyVisibleState();
    void refreshTextLayout();

    QQuickWidget* quickWidget_ = nullptr;
    QQuickWidget* lockQuickWidget_ = nullptr;
    QWidget* lockButtonWindow_ = nullptr;
    QVariantMap overlayState_;
    QPointF lightPosition_{0.25, 0.08};
    QPointer<QScreen> boundScreen_;
    bool overlayEnabled_ = false;
    bool locked_ = false;
    bool gameGatePaused_ = false;
    bool dragging_ = false;
    bool positionInitialized_ = false;
    int opacity_ = 70;
    QPoint dragStartGlobal_;
    QPoint dragStartWindow_;
};
