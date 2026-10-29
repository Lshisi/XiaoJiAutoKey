#pragma once

#include "AppTypes.h"

#include <QElapsedTimer>
#include <QPointF>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>
#include <QWidget>
#include <memory>

class QQuickWidget;
class QScreen;
class QTimer;
class OverlayBackdropProvider;
class OverlayBackdropCapture;
struct RepeaterStatus;
struct MacroStatus;

class StatusOverlayWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QVariantMap overlayState READ overlayState NOTIFY overlayStateChanged)
    Q_PROPERTY(QUrl backdropSource READ backdropSource NOTIFY backdropChanged)
    Q_PROPERTY(bool backdropAvailable READ backdropAvailable NOTIFY backdropChanged)

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
    QUrl backdropSource() const { return backdropSource_; }
    bool backdropAvailable() const { return backdropAvailable_; }
    Q_INVOKABLE void toggleLock();

signals:
    void overlayStateChanged();
    void backdropChanged();
    void positionChanged(const QPoint& position);
    void lockChanged(bool locked);
    void keyConfigRequested();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool event(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void bindDpiRefreshSignals();
    void setLocked(bool locked);
    void setOverlayOpacity(int opacity);
    void finishDragging(bool savePosition);
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
    void clearBackdrop();
    void scheduleBackdropRefresh();
    void refreshBackdrop();
    void configureBackdropCapture();
    void refreshGlassPalette();

    QQuickWidget* quickWidget_ = nullptr;
    QQuickWidget* lockQuickWidget_ = nullptr;
    QWidget* lockButtonWindow_ = nullptr;
    QTimer* backdropTimer_ = nullptr;
    QTimer* dragReleaseTimer_ = nullptr;
    std::unique_ptr<OverlayBackdropCapture> backdropCapture_;
    OverlayBackdropProvider* backdropProvider_ = nullptr;
    QUrl backdropSource_{QStringLiteral("image://overlay-backdrop/0")};
    QElapsedTimer backdropClock_;
    quint64 backdropRevision_ = 0;
    bool backdropAvailable_ = false;
    bool backdropRefreshPending_ = false;
    bool backdropCapturing_ = false;
    bool realRefraction_ = true;
    bool captureExcluded_ = false;
    bool backdropDark_ = false;
    QVariantMap overlayState_;
    QPointer<QScreen> boundScreen_;
    bool overlayEnabled_ = false;
    bool locked_ = false;
    bool gameGatePaused_ = false;
    bool dragging_ = false;
    bool dragPressed_ = false;
    bool positionInitialized_ = false;
    int opacity_ = 70;
    QPoint dragStartGlobal_;
    QPoint dragStartWindow_;
};
