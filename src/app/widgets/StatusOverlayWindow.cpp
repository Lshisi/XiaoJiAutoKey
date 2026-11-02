#include "widgets/StatusOverlayWindow.h"

#include "FactionTheme.h"
#include "MacroEngine.h"
#include "RepeaterEngine.h"
#include "dialogs/AppMessageDialog.h"
#include "ui/styles/DesignTokens.h"
#include "ui/components/QmlIconProvider.h"

#include <Windows.h>
#include <dwmapi.h>
#include <QApplication>
#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScreen>
#include <QShowEvent>
#include <QStringList>
#include <QSurfaceFormat>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr int kContentPadding = 10;
constexpr int kBaseWidth = 178;
constexpr int kLockSize = 32;
constexpr int kFontPixelSize = 12;

struct AccentPolicy {
    int state;
    DWORD flags;
    DWORD color;
    DWORD animation;
};

struct CompositionAttributeData {
    int attribute;
    void* data;
    SIZE_T size;
};

bool setNativeBlur(HWND window, bool enabled) {
    using SetCompositionAttribute = BOOL(WINAPI*)(HWND, CompositionAttributeData*);
    static const auto setAttribute = reinterpret_cast<SetCompositionAttribute>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (!setAttribute) return false;
    // Windows 10's native blur uses WCA_ACCENT_POLICY (19), state 3.
    AccentPolicy policy{enabled ? 3 : 0, 0, 0, 0};
    CompositionAttributeData attribute{19, &policy, sizeof(policy)};
    return setAttribute(window, &attribute) != FALSE;
}

HRGN createOverlayRegion(int width, int height, int radius) {
    HRGN region = CreateRectRgn(0, radius, width, height - radius);
    HRGN strip = CreateRectRgn(0, 0, 0, 0);
    if (!region || !strip) {
        if (region) DeleteObject(region);
        if (strip) DeleteObject(strip);
        return nullptr;
    }
    // Keep the existing power-4 corners, with the native region as the only clip.
    for (int y = 0; y < radius; ++y) {
        const double distance = (radius - y - 0.5) / radius;
        const int inset = qRound(radius * (1.0 - std::pow(1.0 - std::pow(distance, 4.0), 0.25)));
        SetRectRgn(strip, inset, y, width - inset, y + 1);
        CombineRgn(region, region, strip, RGN_OR);
        SetRectRgn(strip, inset, height - y - 1, width - inset, height - y);
        CombineRgn(region, region, strip, RGN_OR);
    }
    DeleteObject(strip);
    return region;
}

bool dragButtonDown() {
    const int button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
    return (GetAsyncKeyState(button) & 0x8000) != 0;
}

void configureQuickSurface(QQuickWidget* view) {
    view->engine()->addImageProvider(QStringLiteral("app-icons"), new QmlIconProvider);
    QSurfaceFormat format = view->format();
    format.setAlphaBufferSize(8);
    view->setFormat(format);
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setClearColor(Qt::transparent);
    view->setAttribute(Qt::WA_TranslucentBackground, true);
    view->setAutoFillBackground(false);
    view->setFocusPolicy(Qt::NoFocus);
}

QFont overlayFont() {
    QFont font(Fluent::Typography::fontFamily());
    font.setPixelSize(kFontPixelSize);
    return font;
}

QString keyText(const std::optional<KeyDescriptor>& key) {
    return key ? QString::fromStdWString(GetKeyDisplayName(*key)) : QStringLiteral("[-]");
}

QString keySummary(const std::vector<KeyDescriptor>& keys, const QString& prefix,
    const QFontMetrics& metrics, int availableWidth) {
    QStringList names;
    for (std::size_t i = 0; i < std::min<std::size_t>(2, keys.size()); ++i)
        names << QString::fromStdWString(GetKeyDisplayName(keys[i]));
    const QString suffix = keys.size() > 2
        ? QObject::tr(" 等%1键").arg(static_cast<qulonglong>(keys.size())) : QString();
    const int budget = std::max(0, availableWidth - metrics.horizontalAdvance(prefix + suffix));
    return prefix + metrics.elidedText(names.join(QStringLiteral(", ")), Qt::ElideRight, budget) + suffix;
}

QStringList repeatLines(const ProfileConfig* profile, const RepeaterStatus& status,
    const RepeatOverlayOptions& options, const QFontMetrics& metrics, int availableWidth) {
    if (!profile) return {};
    QStringList lines;
    if (options.showHotkey)
        lines << QObject::tr("启停：%1").arg(keyText(profile->enabled ? profile->controlHotkey : std::nullopt));
    const auto hasMode = [profile](RepeatMode mode) {
        return std::any_of(profile->bindings.begin(), profile->bindings.end(), [mode](const KeyBinding& binding) {
            return binding.enabled && binding.mode == mode;
        });
    };
    const QString modeState = !profile->enabled ? QObject::tr("（停用）")
        : !status.running ? QObject::tr("（关闭）")
        : status.paused ? QObject::tr("（暂停）")
        : status.holdPriorityActive ? QObject::tr("（让位）") : QString();
    if (options.showIndependent && hasMode(RepeatMode::Independent))
        lines << QObject::tr("独立%1：%2").arg(modeState, status.independentKey
            ? keyText(status.independentKey) : QObject::tr("未激活"));
    if (options.showHold && hasMode(RepeatMode::Hold))
        lines << (status.holdKeys.empty() ? QObject::tr("按压：未按压")
            : keySummary(status.holdKeys, QObject::tr("按压："), metrics, availableWidth));
    if (options.showToggle && hasMode(RepeatMode::Toggle)) {
        std::vector<KeyDescriptor> keys;
        for (const auto& binding : profile->bindings)
            if (binding.enabled && binding.mode == RepeatMode::Toggle) keys.push_back(binding.key);
        lines << keySummary(keys, QObject::tr("开关%1：").arg(modeState), metrics, availableWidth);
    }
    if (options.showLongPress && hasMode(RepeatMode::LongPress)) {
        std::vector<KeyDescriptor> keys;
        for (const auto& binding : profile->bindings)
            if (binding.enabled && binding.mode == RepeatMode::LongPress) keys.push_back(binding.key);
        lines << keySummary(keys, QObject::tr("长按%1：").arg(modeState), metrics, availableWidth);
    }
    if (options.showSequence && hasMode(RepeatMode::Sequence)) {
        QString selected = QObject::tr("未激活");
        for (const auto& binding : profile->bindings) {
            if (binding.enabled && binding.mode == RepeatMode::Sequence
                && status.sequenceTaskId && binding.id == *status.sequenceTaskId) {
                selected = QString::fromStdWString(GetKeyDisplayName(binding.key));
                break;
            }
        }
        lines << QObject::tr("切换%1：%2").arg(modeState, selected);
    }
    if (options.showSequenceHotkey && profile->sequenceSwitchKey)
        lines << QObject::tr("切换键：%1").arg(keyText(profile->sequenceSwitchKey));
    if (options.showPauseKeys && !profile->pauseKeys.empty())
        lines << keySummary(profile->pauseKeys, QObject::tr("暂停键："), metrics, availableWidth);
    return lines;
}

QStringList macroLines(const MacroConfig* macro, const MacroOverlayOptions& options,
    const QFontMetrics& metrics, int availableWidth) {
    if (!macro) return {};
    QStringList lines;
    QString trigger;
    if (options.showHotkey)
        trigger = QObject::tr("启停：%1").arg(keyText(macro->enabled ? macro->triggerKey : std::nullopt));
    if (options.showTriggerMode) {
        const QString mode = macro->triggerMode == MacroTriggerMode::Hold ? QObject::tr("按压") : QObject::tr("开关");
        trigger = trigger.isEmpty() ? QObject::tr("触发：%1").arg(mode)
            : QObject::tr("%1（%2）").arg(trigger, mode);
    }
    if (options.showLoopCount && macro->triggerMode == MacroTriggerMode::Toggle) {
        const QString loops = macro->loopCount <= 0 ? QObject::tr("无限循环")
            : QObject::tr("%1次循环").arg(macro->loopCount);
        const QString combined = trigger.isEmpty() ? loops : QObject::tr("%1  %2").arg(trigger, loops);
        if (metrics.horizontalAdvance(combined) <= availableWidth) trigger = combined;
        else {
            if (!trigger.isEmpty()) lines << trigger;
            trigger = loops;
        }
    }
    if (!trigger.isEmpty()) lines << trigger;
    if (options.showPauseKeys && !macro->pauseKeys.empty())
        lines << keySummary(macro->pauseKeys, QObject::tr("暂停键："), metrics, availableWidth);
    return lines;
}

const QScreen* nearestScreenForPoint(const QPoint& point) {
    if (const auto* screen = QGuiApplication::screenAt(point)) return screen;
    const QScreen* nearest = nullptr;
    qint64 nearestDistance = std::numeric_limits<qint64>::max();
    for (const auto* screen : QGuiApplication::screens()) {
        const QRect area = screen->geometry();
        const int dx = point.x() < area.left() ? area.left() - point.x()
            : point.x() > area.right() ? point.x() - area.right() : 0;
        const int dy = point.y() < area.top() ? area.top() - point.y()
            : point.y() > area.bottom() ? point.y() - area.bottom() : 0;
        const qint64 distance = static_cast<qint64>(dx) * dx + static_cast<qint64>(dy) * dy;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = screen;
        }
    }
    return nearest ? nearest : QGuiApplication::primaryScreen();
}
}

StatusOverlayWindow::StatusOverlayWindow(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
    setCursor(Qt::SizeAllCursor);
    setFixedWidth(kBaseWidth);
    overlayState_ = {
        {"showRepeat", false}, {"showMacro", false}, {"repeatTitle", QString()}, {"macroTitle", QString()},
        {"repeatLines", QStringList{}}, {"macroLines", QStringList{}},
        {"repeatActive", false}, {"macroActive", false}, {"gameGatePaused", false},
        {"locked", false}, {"opacity", opacity_}, {"dark", Fluent::Theme::isDarkMode()},
        {"nativeBlur", nativeBlur_}, {"blurActive", false},
        {"accent", FactionTheme::instance().palette().accent},
        {"accentBorder", FactionTheme::instance().palette().accentBorder},
        {"text", Fluent::Colors::overlayText(Fluent::Theme::isDarkMode())},
        {"muted", Fluent::Colors::overlayTextSecondary()},
        {"panelColor", Fluent::Colors::overlayPanel(Fluent::Theme::isDarkMode())},
        {"warning", Fluent::Colors::warning()},
        {"fontFamily", Fluent::Typography::fontFamily()}, {"fontPixelSize", kFontPixelSize},
        {"contentPadding", kContentPadding}
    };
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    quickWidget_ = new QQuickWidget(this);
    configureQuickSurface(quickWidget_);
    quickWidget_->setAttribute(Qt::WA_TransparentForMouseEvents);
    root->addWidget(quickWidget_);

    lockButtonWindow_ = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    lockButtonWindow_->setAttribute(Qt::WA_TranslucentBackground, true);
    lockButtonWindow_->setAttribute(Qt::WA_ShowWithoutActivating, true);
    lockButtonWindow_->setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
    lockButtonWindow_->setFixedSize(kLockSize, kLockSize);
    auto* lockLayout = new QVBoxLayout(lockButtonWindow_);
    lockLayout->setContentsMargins(0, 0, 0, 0);
    lockQuickWidget_ = new QQuickWidget(lockButtonWindow_);
    configureQuickSurface(lockQuickWidget_);
    lockLayout->addWidget(lockQuickWidget_);
    setStyleSheet(QStringLiteral("StatusOverlayWindow { background: transparent; }"));
    dragReleaseTimer_ = new QTimer(this);
    dragReleaseTimer_->setInterval(20);
    connect(dragReleaseTimer_, &QTimer::timeout, this, [this] {
        if (!dragButtonDown()) finishDragging(true);
    });
}

StatusOverlayWindow::~StatusOverlayWindow() {
    if (quickWidget_->rootObject()) disconnect(quickWidget_->rootObject(), nullptr, this, nullptr);
    delete lockButtonWindow_;
    delete quickWidget_;
}

bool StatusOverlayWindow::initialize(QWidget* dialogParent) {
    quickWidget_->rootContext()->setContextProperty("overlayModel", this);
    lockQuickWidget_->rootContext()->setContextProperty("overlayModel", this);
    quickWidget_->setSource(QUrl(QStringLiteral("qrc:/qt/qml/XiaoJi/UI/OverlayWindow.qml")));
    lockQuickWidget_->setSource(QUrl(QStringLiteral("qrc:/qt/qml/XiaoJi/UI/OverlayLock.qml")));
    if (quickWidget_->status() != QQuickWidget::Ready || lockQuickWidget_->status() != QQuickWidget::Ready) {
        QStringList errors;
        for (auto* view : {quickWidget_, lockQuickWidget_})
            for (const auto& error : view->errors()) errors << error.toString();
        AppMessageDialog::warning(dialogParent, tr("QML 悬浮窗加载失败"), errors.join(QLatin1Char('\n')));
        return false;
    }
    connect(quickWidget_->rootObject(), &QQuickItem::implicitHeightChanged,
        this, &StatusOverlayWindow::refreshTextLayout, Qt::QueuedConnection);
    winId();
    connect(windowHandle(), &QWindow::screenChanged, this, [this](QScreen*) {
        bindDpiRefreshSignals();
        refreshForDpiChange();
    });
    refreshTextLayout();
    return true;
}

void StatusOverlayWindow::setStateValue(const QString& key, const QVariant& value) {
    if (overlayState_.value(key) == value) return;
    overlayState_.insert(key, value);
    emit overlayStateChanged();
}

void StatusOverlayWindow::setOverlayEnabled(bool enabled) {
    if (overlayEnabled_ == enabled) return;
    overlayEnabled_ = enabled;
    applyVisibleState();
}

void StatusOverlayWindow::setGameGatePaused(bool paused) {
    if (gameGatePaused_ == paused) return;
    gameGatePaused_ = paused;
    setStateValue("gameGatePaused", paused);
    refreshTextLayout();
}

void StatusOverlayWindow::refreshForDpiChange(int nativeDpi) {
    Q_UNUSED(nativeDpi)
    refreshTextLayout();
    move(boundedPosition(pos()));
    syncLockButton();
    quickWidget_->update();
    lockQuickWidget_->update();
    applyNativeBlur();
}

void StatusOverlayWindow::updateStatus(const AppConfig& config, const ProfileConfig* profile,
    const MacroConfig* macro, const RepeaterStatus& repeat, const MacroStatus& macroStatus) {
    if (!config.statusOverlayShowRepeat && !config.statusOverlayShowMacro) {
        setOverlayEnabled(false);
        return;
    }
    setLocked(config.statusOverlayLocked);
    setOverlayOpacity(config.statusOverlayOpacity);
    if (nativeBlur_ != config.statusOverlayNativeBlur) {
        nativeBlur_ = config.statusOverlayNativeBlur;
        setStateValue("nativeBlur", nativeBlur_);
        applyNativeBlur();
    }
    const QFontMetrics metrics(overlayFont());
    const int desiredWidth = std::max(kBaseWidth, metrics.horizontalAdvance(tr("连发：配置停用"))
        + 2 * kContentPadding + 22 + 24);
    const auto* currentScreen = nearestScreenForPoint(pos());
    const int targetWidth = currentScreen ? std::min(desiredWidth, currentScreen->availableGeometry().width()) : desiredWidth;
    if (width() != targetWidth) setFixedWidth(targetWidth);
    const int textWidth = width() - 2 * kContentPadding - 24;

    const bool hasPausableBindings = profile && std::any_of(profile->bindings.begin(), profile->bindings.end(),
        [](const KeyBinding& binding) { return binding.enabled && binding.mode != RepeatMode::Hold; });
    const bool repeatPaused = repeat.running && repeat.paused && hasPausableBindings;
    const QString repeatState = !profile ? tr("无配置") : !profile->enabled ? tr("配置停用")
        : repeatPaused ? (repeat.holdKeys.empty() ? tr("已暂停") : tr("部分暂停"))
        : repeat.running ? tr("已开启") : tr("已关闭");
    const QString macroState = !macro ? tr("无配置") : !macro->enabled ? tr("配置停用")
        : macroStatus.paused ? tr("已暂停") : macroStatus.running ? tr("已开启") : tr("已关闭");

    QVariantMap state = overlayState_;
    state.insert("showRepeat", config.statusOverlayShowRepeat);
    state.insert("showMacro", config.statusOverlayShowMacro);
    state.insert("repeatTitle", tr("连发：%1").arg(repeatState));
    state.insert("macroTitle", tr("循环：%1").arg(macroState));
    state.insert("repeatLines", repeatLines(profile, repeat, config.repeatOverlay, metrics, textWidth));
    state.insert("macroLines", macroLines(macro, config.macroOverlay, metrics, textWidth));
    state.insert("repeatActive", repeat.running && (!repeatPaused || !repeat.holdKeys.empty()));
    state.insert("macroActive", macroStatus.running && !macroStatus.paused);
    const bool materialDark = Fluent::Theme::isDarkMode();
    state.insert("dark", materialDark);
    state.insert("accent", FactionTheme::instance().palette().accent);
    state.insert("accentBorder", FactionTheme::instance().palette().accentBorder);
    state.insert("text", Fluent::Colors::overlayText(materialDark));
    state.insert("muted", Fluent::Colors::overlayTextSecondary(materialDark));
    state.insert("panelColor", Fluent::Colors::overlayPanel(materialDark));
    state.insert("warning", Fluent::Colors::warning());
    if (overlayState_ != state) {
        overlayState_ = std::move(state);
        emit overlayStateChanged();
    }
    refreshTextLayout();
    moveToConfiguredPosition(config);
    setOverlayEnabled(true);
    if (!dragging_) move(boundedPosition(pos()));
    syncLockButton();
}

void StatusOverlayWindow::refreshTextLayout() {
    if (!quickWidget_->rootObject()) return;
    const int targetHeight = static_cast<int>(std::ceil(quickWidget_->rootObject()->implicitHeight()));
    if (targetHeight <= 0) return;
    if (height() != targetHeight) {
        setFixedHeight(targetHeight);
        layout()->invalidate();
        layout()->activate();
    }
    if (isVisible() && !dragging_) move(boundedPosition(pos()));
    syncLockButton();
}

bool StatusOverlayWindow::updateNativeRegion() {
    const WId window = winId();
    RECT bounds{};
    if (!GetWindowRect(reinterpret_cast<HWND>(window), &bounds)) return false;
    const QSize size(bounds.right - bounds.left, bounds.bottom - bounds.top);
    const int radius = std::min({qRound(Fluent::Spacing::cardRadius * devicePixelRatioF()),
        size.width() / 2, size.height() / 2});
    if (size.isEmpty() || radius <= 0) return false;
    if (nativeRegionWindow_ == window && nativeRegionSize_ == size && nativeRegionRadius_ == radius)
        return true;
    HRGN region = createOverlayRegion(size.width(), size.height(), radius);
    if (!region) return false;
    if (!SetWindowRgn(reinterpret_cast<HWND>(window), region, TRUE)) {
        DeleteObject(region);
        return false;
    }
    // Windows owns the region after SetWindowRgn succeeds.
    nativeRegionWindow_ = window;
    nativeRegionSize_ = size;
    nativeRegionRadius_ = radius;
    return true;
}

void StatusOverlayWindow::applyNativeBlur() {
    if (!windowHandle()) return;
    const bool regionReady = updateNativeRegion();
    BOOL compositionEnabled = FALSE;
    const bool enabled = nativeBlur_ && isVisible() && regionReady
        && SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)) && compositionEnabled;
    const bool applied = setNativeBlur(reinterpret_cast<HWND>(winId()), enabled);
    setStateValue("blurActive", enabled && applied);
}

void StatusOverlayWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (locked_ || event->button() != Qt::LeftButton) return QWidget::mouseDoubleClickEvent(event);
    finishDragging(false);
    emit keyConfigRequested();
    event->accept();
}

void StatusOverlayWindow::mousePressEvent(QMouseEvent* event) {
    if (locked_ || event->button() != Qt::LeftButton) return QWidget::mousePressEvent(event);
    finishDragging(true);
    dragPressed_ = true;
    dragStartGlobal_ = event->globalPosition().toPoint();
    dragStartWindow_ = pos();
    grabMouse();
    dragReleaseTimer_->start();
    event->accept();
}

void StatusOverlayWindow::mouseMoveEvent(QMouseEvent* event) {
    if (locked_) return QWidget::mouseMoveEvent(event);
    if (!dragPressed_) return QWidget::mouseMoveEvent(event);
    if (!(event->buttons() & Qt::LeftButton) || !dragButtonDown()) {
        finishDragging(true);
        return;
    }
    const QPoint delta = event->globalPosition().toPoint() - dragStartGlobal_;
    if (!dragging_ && delta.manhattanLength() < QApplication::startDragDistance()) {
        event->accept();
        return;
    }
    dragging_ = true;
    move(boundedPosition(dragStartWindow_ + delta));
    syncLockButton();
    event->accept();
}

void StatusOverlayWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !dragPressed_) return QWidget::mouseReleaseEvent(event);
    finishDragging(true);
    event->accept();
}

void StatusOverlayWindow::finishDragging(bool savePosition) {
    if (!dragPressed_ && !dragging_) return;
    const bool moved = dragging_;
    dragPressed_ = false;
    dragging_ = false;
    dragReleaseTimer_->stop();
    if (QWidget::mouseGrabber() == this) releaseMouse();
    if (moved && savePosition) {
        positionInitialized_ = true;
        move(boundedPosition(pos()));
        syncLockButton();
        emit positionChanged(pos());
    }
}

void StatusOverlayWindow::toggleLock() {
    setLocked(!locked_);
    emit lockChanged(locked_);
}

void StatusOverlayWindow::setLocked(bool locked) {
    if (locked_ == locked) return;
    finishDragging(true);
    locked_ = locked;
    setStateValue("locked", locked);
    applyClickThrough();
    setCursor(locked_ ? Qt::ArrowCursor : Qt::SizeAllCursor);
    syncLockButton();
}

void StatusOverlayWindow::setOverlayOpacity(int opacity) {
    opacity = std::clamp(opacity, 30, 100);
    if (opacity_ == opacity) return;
    opacity_ = opacity;
    setStateValue("opacity", opacity);
}

void StatusOverlayWindow::applyClickThrough() {
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED;
    if (locked_) style |= WS_EX_TRANSPARENT;
    else style &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style);
}

void StatusOverlayWindow::syncLockButton() {
    if (!isVisible()) {
        lockButtonWindow_->hide();
        return;
    }
    lockButtonWindow_->move(pos() + QPoint(width() - 8 - kLockSize, 8));
    if (!lockButtonWindow_->isVisible()) {
        lockButtonWindow_->show();
        syncTopMostOrder();
    }
}

void StatusOverlayWindow::syncTopMostOrder() {
    constexpr UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW;
    SetWindowPos(reinterpret_cast<HWND>(winId()), HWND_TOPMOST, 0, 0, 0, 0, flags);
    SetWindowPos(reinterpret_cast<HWND>(lockButtonWindow_->winId()), HWND_TOPMOST, 0, 0, 0, 0, flags);
}

void StatusOverlayWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    bindDpiRefreshSignals();
    applyClickThrough();
    refreshForDpiChange();
}

bool StatusOverlayWindow::event(QEvent* event) {
    if (event->type() == QEvent::ScreenChangeInternal || event->type() == QEvent::DevicePixelRatioChange)
        QTimer::singleShot(0, this, [this] { refreshForDpiChange(); });
    if (event->type() == QEvent::Resize && quickWidget_) applyNativeBlur();
    if (event->type() == QEvent::UngrabMouse || event->type() == QEvent::WindowDeactivate)
        finishDragging(true);
    if (event->type() == QEvent::Hide) finishDragging(false);
    if (event->type() == QEvent::Hide && quickWidget_) applyNativeBlur();
    return QWidget::event(event);
}

bool StatusOverlayWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    const auto* msg = static_cast<MSG*>(message);
    if (msg && msg->message == WM_DPICHANGED) {
        const int dpi = std::max<int>(96, LOWORD(msg->wParam));
        QTimer::singleShot(0, this, [this, dpi]() { refreshForDpiChange(dpi); });
    }
    if (msg && (msg->message == WM_DWMCOMPOSITIONCHANGED || msg->message == WM_DISPLAYCHANGE))
        QTimer::singleShot(0, this, [this] { applyNativeBlur(); });
    if (msg && (msg->message == WM_CAPTURECHANGED || msg->message == WM_CANCELMODE))
        finishDragging(true);
    return QWidget::nativeEvent(eventType, message, result);
}

QPoint StatusOverlayWindow::defaultPosition() const {
    const auto* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry().topLeft() + QPoint(6, 6) : QPoint(6, 6);
}

QPoint StatusOverlayWindow::positionFromConfig(const AppConfig& config) const {
    return config.statusOverlayPositionSaved
        ? QPoint(config.statusOverlayX, config.statusOverlayY) : defaultPosition();
}

QPoint StatusOverlayWindow::boundedPosition(const QPoint& position) const {
    const auto* screen = nearestScreenForPoint(dragging_
        ? QCursor::pos() : position + QPoint(width() / 2, height() / 2));
    if (!screen) return position;
    const QRect area = screen->geometry();
    return QPoint(
        std::clamp(position.x(), area.left(), std::max(area.left(), area.right() - width() + 1)),
        std::clamp(position.y(), area.top(), std::max(area.top(), area.bottom() - height() + 1)));
}

void StatusOverlayWindow::moveToConfiguredPosition(const AppConfig& config) {
    if (positionInitialized_ || dragging_) return;
    move(boundedPosition(positionFromConfig(config)));
    positionInitialized_ = true;
}

void StatusOverlayWindow::applyVisibleState() {
    if (overlayEnabled_) {
        if (!positionInitialized_) move(defaultPosition());
        show();
        bindDpiRefreshSignals();
        refreshForDpiChange();
    } else {
        hide();
        lockButtonWindow_->hide();
        positionInitialized_ = false;
    }
}

void StatusOverlayWindow::bindDpiRefreshSignals() {
    auto* screen = windowHandle() ? windowHandle()->screen() : nullptr;
    if (!screen || boundScreen_ == screen) return;
    if (boundScreen_) disconnect(boundScreen_.data(), nullptr, this, nullptr);
    boundScreen_ = screen;
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this](qreal) { refreshForDpiChange(); });
    connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this](qreal) { refreshForDpiChange(); });
}
