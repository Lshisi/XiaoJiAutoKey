#include "MainWindow.h"

#include "dialogs/AppMessageDialog.h"
#include "ui/styles/DesignTokens.h"

#include <QApplication>
#include <QFont>
#include <QEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QString>
#include <QTimer>
#include <QtGlobal>
#include <QWindow>
#include <Windows.h>
#include <stdexcept>

namespace {
constexpr int kBaseWindowWidth = 780;
constexpr int kBaseWindowHeight = 560;
constexpr int kBaseDpi = 96;

int dpiScaled(int value, int dpi) {
    return qMax(value, (value * dpi + kBaseDpi / 2) / kBaseDpi);
}

int dpiFromWParam(WPARAM value) {
    const int dpiX = LOWORD(value);
    const int dpiY = HIWORD(value);
    return qMax(kBaseDpi, dpiX > 0 ? dpiX : dpiY);
}

#ifdef Q_OS_WIN
int effectiveMonitorDpi(HWND hwnd) {
    if (!hwnd) {
        return 0;
    }

    using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static const auto getDpiForMonitor = []() -> GetDpiForMonitorFn {
        const HMODULE module = LoadLibraryW(L"Shcore.dll");
        if (!module) {
            return nullptr;
        }
        return reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(module, "GetDpiForMonitor"));
    }();

    if (!getDpiForMonitor) {
        return 0;
    }

    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!monitor) {
        return 0;
    }

    UINT dpiX = 0;
    UINT dpiY = 0;
    constexpr int kEffectiveDpi = 0;
    if (FAILED(getDpiForMonitor(monitor, kEffectiveDpi, &dpiX, &dpiY))) {
        return 0;
    }
    return qMax(kBaseDpi, static_cast<int>(dpiX > 0 ? dpiX : dpiY));
}
#endif

void expandSuggestedRectForDpi(RECT* rect, int dpi) {
    if (!rect) {
        return;
    }

    const int minWidth = dpiScaled(kBaseWindowWidth, qMax(kBaseDpi, dpi));
    const int minHeight = dpiScaled(kBaseWindowHeight, qMax(kBaseDpi, dpi));
    const int width = rect->right - rect->left;
    const int height = rect->bottom - rect->top;
    if (width < minWidth) {
        rect->right = rect->left + minWidth;
    }
    if (height < minHeight) {
        rect->bottom = rect->top + minHeight;
    }
}

void applySuggestedDpiRect(HWND hwnd, const RECT* rect) {
    if (!hwnd || !rect) {
        return;
    }

    const int width = rect->right - rect->left;
    const int height = rect->bottom - rect->top;
    if (width <= 0 || height <= 0) {
        return;
    }

    SetWindowPos(hwnd, nullptr,
        rect->left,
        rect->top,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      appPaths_(),
      configCoordinator_(appPaths_.configPath(), this),
      profileController_(config_),
      macroController_(config_),
      engine_(driver_),
      macroEngine_(driver_),
      inputHook_(this),
      soundService_(appPaths_.exeDir()),
      statusOverlay_(nullptr) {
    runtimeVisualRefreshTimer_.setSingleShot(true);
    connect(&runtimeVisualRefreshTimer_, &QTimer::timeout, this, [this]() {
        applyRuntimeVisualRefresh();
    });
}

MainWindow::~MainWindow() {
    engineStateCoordinator_.stopThreads();
}

bool MainWindow::Initialize() {
    setupUi();
    configCoordinator_.load();
    soundService_.warmUpToggleSounds(
        config_.customSoundOn,
        config_.customSoundOff,
        config_.macroSoundOn,
        config_.macroSoundOff);

    if (!driver_.LoadFromDirectory(appPaths_.exeDir())) {
        std::wstring errorMsg = L"\u9519\u8bef\u539f\u56e0: " + driver_.GetLastError() +
            L"\n\n\u8bf7\u786e\u8ba4\uff1a\n1. \u662f\u5426\u5df2\u5b89\u88c5 DD \u9a71\u52a8\n2. DD64.dll / DD32.dll \u662f\u5426\u5728 exe \u540c\u76ee\u5f55\n\n\u7a0b\u5e8f\u5373\u5c06\u9000\u51fa\u3002";
        AppMessageDialog::warning(this, QString::fromWCharArray(L"DD \u9a71\u52a8\u52a0\u8f7d\u5931\u8d25"),
            QString::fromStdWString(errorMsg));
        return false;
    }

    try {
        setupHooks();
    } catch (const std::exception&) {
        AppMessageDialog::warning(this, QString::fromWCharArray(L"\u5168\u5c40\u94a9\u5b50\u521d\u59cb\u5316\u5931\u8d25"),
            QString::fromWCharArray(L"\u65e0\u6cd5\u5b89\u88c5\u5168\u5c40\u952e\u76d8/\u9f20\u6807\u94a9\u5b50"));
        return false;
    }
    engineStateCoordinator_.startThreads();
    engineStateCoordinator_.applyBindings();
    engineStateCoordinator_.applyMacros();
    pageRefreshCoordinator_.refreshAll();
    refreshStatusOverlay();
    bindDpiRefreshSignals();
    observedScreenDpi_ = currentScreenDpi();
    QTimer::singleShot(250, this, [this]() {
        view_.initializeTray(this);
    });
    return true;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && captureCoordinator_.captureActive()) {
        captureCoordinator_.cancelCapture();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
}

bool MainWindow::event(QEvent* event) {
    switch (event->type()) {
    case QEvent::ScreenChangeInternal:
        refreshIfScreenDpiChanged();
        break;
    default:
        break;
    }
    return QMainWindow::event(event);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result) {
    Q_UNUSED(eventType)
#ifdef Q_OS_WIN
    const auto* msg = static_cast<MSG*>(message);
    if (msg) {
        switch (msg->message) {
        case WM_THEMECHANGED:
            Fluent::Theme::refreshDarkModeCache();
            scheduleRuntimeVisualRefresh(false, true);
            break;
        case WM_SETTINGCHANGE: {
            const auto* area = reinterpret_cast<const wchar_t*>(msg->lParam);
            const QString setting = area ? QString::fromWCharArray(area) : QString();
            const bool themeSetting =
                setting.compare(QStringLiteral("ImmersiveColorSet"), Qt::CaseInsensitive) == 0 ||
                setting.compare(QStringLiteral("WindowsThemeElement"), Qt::CaseInsensitive) == 0;
            const bool dpiSetting =
                setting.isEmpty() ||
                setting.compare(QStringLiteral("Display"), Qt::CaseInsensitive) == 0 ||
                setting.compare(QStringLiteral("UserPreferences"), Qt::CaseInsensitive) == 0 ||
                setting.compare(QStringLiteral("WindowMetrics"), Qt::CaseInsensitive) == 0 ||
                setting.compare(QStringLiteral("ConvertibleSlateMode"), Qt::CaseInsensitive) == 0;
            if (dpiSetting) {
                const int dpi = currentScreenDpi();
                if (observedScreenDpi_ == 0 || qAbs(dpi - observedScreenDpi_) > 2) {
                    runtimeDpiOverride_ = dpi;
                }
                runtimeDpiSyncWaitsRemaining_ = 0;
                scheduleRuntimeVisualRefresh(true, false);
            }
            if (themeSetting) {
                Fluent::Theme::refreshDarkModeCache();
                scheduleRuntimeVisualRefresh(false, true);
            }
            break;
        }
        case WM_DPICHANGED:
            runtimeDpiOverride_ = dpiFromWParam(msg->wParam);
            if (auto* suggestedRect = reinterpret_cast<RECT*>(msg->lParam)) {
                expandSuggestedRectForDpi(suggestedRect, runtimeDpiOverride_);
                applySuggestedDpiRect(reinterpret_cast<HWND>(winId()), suggestedRect);
            }
            runtimeDpiSyncWaitsRemaining_ = 0;
            enforceNativeWindowSizeForDpi(runtimeDpiOverride_);
            scheduleRuntimeVisualRefresh(true, false);
            for (const int delayMs : {0, 120, 320}) {
                const int dpi = runtimeDpiOverride_;
                QTimer::singleShot(delayMs, this, [this, dpi]() {
                    runtimeDpiOverride_ = dpi;
                    runtimeDpiSyncWaitsRemaining_ = 0;
                    scheduleRuntimeVisualRefresh(true, false);
                });
            }
            break;
        case WM_DISPLAYCHANGE:
            refreshIfScreenDpiChanged();
            break;
        default:
            break;
        }
    }
#else
    Q_UNUSED(message)
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::scheduleRuntimeVisualRefresh(bool dpiChanged, bool themeChanged) {
    runtimeDpiRefreshPending_ = runtimeDpiRefreshPending_ || dpiChanged;
    runtimeThemeRefreshPending_ = runtimeThemeRefreshPending_ || themeChanged;
    runtimeVisualRefreshPassesRemaining_ = qMax(runtimeVisualRefreshPassesRemaining_, dpiChanged ? 6 : 2);
    if (applyingRuntimeVisualRefresh_) {
        return;
    }
    runtimeVisualRefreshTimer_.start(dpiChanged ? 0 : 80);
}

void MainWindow::applyRuntimeVisualRefresh() {
    if (applyingRuntimeVisualRefresh_) {
        return;
    }

    applyingRuntimeVisualRefresh_ = true;
    if (runtimeDpiRefreshPending_) {
        runtimeDpiRefreshActive_ = true;
    }
    const bool dpiChanged = runtimeDpiRefreshActive_;
    const bool themeChanged = runtimeThemeRefreshPending_;
    runtimeDpiRefreshPending_ = false;
    runtimeThemeRefreshPending_ = false;

    if (runtimeVisualRefreshPassesRemaining_ > 0) {
        --runtimeVisualRefreshPassesRemaining_;
    }

    QFont globalFont = Fluent::Typography::body();
    qApp->setFont(globalFont);

    qApp->setPalette(Fluent::Theme::applicationPalette());

    const int screenDpi = currentScreenDpi();
    const int dpi = runtimeDpiOverride_ > 0 ? runtimeDpiOverride_ : screenDpi;
    if (dpiChanged) {
        recreateWindowSurfaceAfterDpiChange(dpi);
        observedScreenDpi_ = dpi;
        enforceNativeWindowSizeForDpi(dpi);
    }
    view_.refreshRuntimeVisuals(this, dpiChanged, themeChanged, dpiChanged ? dpi : 0);
    if (dpiChanged) {
        statusOverlay_.refreshForDpiChange(dpi);
    }
    if (themeChanged) {
        pageRefreshCoordinator_.refreshAll();
        view_.refreshRuntimeVisuals(this, false, false);
    }
    refreshStatusOverlay();

    applyingRuntimeVisualRefresh_ = false;
    if (runtimeVisualRefreshPassesRemaining_ > 0) {
        runtimeVisualRefreshTimer_.start(dpiChanged ? 160 : 220);
    } else if (runtimeDpiRefreshActive_) {
        runtimeDpiRefreshActive_ = false;
        runtimeDpiSyncWaitsRemaining_ = 0;
        runtimeDpiOverride_ = 0;
        QTimer::singleShot(0, this, [this]() {
            view_.refreshRuntimeVisuals(this, false, false);
        });
    }
}

void MainWindow::bindDpiRefreshSignals() {
    QTimer::singleShot(0, this, [this]() {
        auto* handle = windowHandle();
        if (!handle) {
            return;
        }

        auto bindScreen = [this](QScreen* screen) {
            if (!screen) {
                return;
            }
            connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this](qreal dpi) {
                runtimeDpiOverride_ = qMax(qMax(kBaseDpi, qRound(dpi)), currentScreenDpi());
                runtimeDpiSyncWaitsRemaining_ = 0;
                scheduleRuntimeVisualRefresh(true, false);
            }, Qt::UniqueConnection);
            connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this](qreal dpi) {
                Q_UNUSED(dpi)
                runtimeDpiOverride_ = currentScreenDpi();
                runtimeDpiSyncWaitsRemaining_ = 0;
                scheduleRuntimeVisualRefresh(true, false);
            }, Qt::UniqueConnection);
        };

        bindScreen(handle->screen());
        connect(handle, &QWindow::screenChanged, this, [this, bindScreen](QScreen* screen) {
            bindScreen(screen);
            refreshIfScreenDpiChanged();
        }, Qt::UniqueConnection);
    });
}

int MainWindow::currentScreenDpi() const {
    QScreen* screen = nullptr;
    if (auto* handle = windowHandle()) {
        screen = handle->screen();
    }
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    int qtDpi = screen ? qMax(kBaseDpi, qRound(screen->logicalDotsPerInch())) : kBaseDpi;
    if (screen) {
        qtDpi = qMax(qtDpi, qRound(screen->devicePixelRatio() * kBaseDpi));
    }
    if (auto* handle = windowHandle()) {
        qtDpi = qMax(qtDpi, qRound(handle->devicePixelRatio() * kBaseDpi));
    }
    qtDpi = qMax(qtDpi, qRound(devicePixelRatioF() * kBaseDpi));

#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        const int monitorDpi = effectiveMonitorDpi(hwnd);
        if (monitorDpi > 0 && observedScreenDpi_ > 0 && qAbs(monitorDpi - observedScreenDpi_) > 2) {
            return monitorDpi;
        }

        const UINT dpi = GetDpiForWindow(hwnd);
        if (dpi > 0) {
            const int nativeDpi = qMax(kBaseDpi, static_cast<int>(dpi));
            if (observedScreenDpi_ > 0 && qAbs(qtDpi - observedScreenDpi_) > 2) {
                return qtDpi;
            }
            return nativeDpi;
        }
    }
#endif

    return qtDpi;
}

void MainWindow::refreshIfScreenDpiChanged() {
    const int dpi = currentScreenDpi();
    if (observedScreenDpi_ == 0) {
        observedScreenDpi_ = dpi;
        return;
    }
    if (qAbs(dpi - observedScreenDpi_) < 6) {
        return;
    }
    observedScreenDpi_ = dpi;
    scheduleRuntimeVisualRefresh(true, false);
}

void MainWindow::enforceNativeWindowSizeForDpi(int dpi) {
    if (dpi <= 0 || isMinimized()) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(winId());
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }

    const int minWidth = dpiScaled(kBaseWindowWidth, qMax(kBaseDpi, dpi));
    const int minHeight = dpiScaled(kBaseWindowHeight, qMax(kBaseDpi, dpi));
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width >= minWidth && height >= minHeight) {
        return;
    }

    SetWindowPos(hwnd, nullptr,
        rect.left,
        rect.top,
        qMax(width, minWidth),
        qMax(height, minHeight),
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainWindow::recreateWindowSurfaceAfterDpiChange(int dpi) {
    if (dpi <= kBaseDpi || dpi % kBaseDpi == 0 || isMinimized()) {
        return;
    }

    if (recreatedFractionalDpiSurface_) {
        return;
    }
    recreatedFractionalDpiSurface_ = true;

    const QRect originalGeometry = geometry();
    if (originalGeometry.isEmpty()) {
        return;
    }

    QTimer::singleShot(0, this, [this, originalGeometry, dpi]() {
        if (isMinimized()) {
            return;
        }

        const bool wasVisible = isVisible();
        if (wasVisible) {
            hide();
        }

        destroy(true, true);
        setGeometry(originalGeometry);
        createWinId();

        if (wasVisible) {
            show();
        }

        enforceNativeWindowSizeForDpi(dpi);
        view_.refreshRuntimeVisuals(this, true, false, dpi);
        if (const HWND hwnd = reinterpret_cast<HWND>(winId())) {
            RedrawWindow(hwnd, nullptr, nullptr,
                RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        QTimer::singleShot(120, this, [this, dpi]() {
            view_.refreshRuntimeVisuals(this, true, false, dpi);
            if (const HWND hwnd = reinterpret_cast<HWND>(winId())) {
                RedrawWindow(hwnd, nullptr, nullptr,
                    RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
            }
        });
    });
}

void MainWindow::syncQtGeometryWithNativeWindow() {
    if (isMinimized()) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(winId());
    RECT rect{};
    if (!hwnd || !GetWindowRect(hwnd, &rect)) {
        return;
    }

    const qreal scale = devicePixelRatioF();
    if (scale <= 0.0 || qFuzzyCompare(scale, 1.0)) {
        return;
    }

    const QRect logicalGeometry(
        qRound(rect.left / scale),
        qRound(rect.top / scale),
        qRound((rect.right - rect.left) / scale),
        qRound((rect.bottom - rect.top) / scale));
    if (!logicalGeometry.isEmpty() && geometry() != logicalGeometry) {
        setGeometry(logicalGeometry);
    }
}

void MainWindow::refreshStatusOverlay() {
    auto* profile = profileController_.activeProfile();
    auto* macro = macroController_.activeMacro();
    const bool repeatRunning = engine_.IsToolRunning();
    const bool macroRunning = macro ? macroEngine_.IsRunning(macro->id) : false;
    statusOverlay_.updateStatus(config_, profile, macro, repeatRunning, macroRunning);
    view_.updateActivityIcons(this, repeatRunning, macroRunning);
}

