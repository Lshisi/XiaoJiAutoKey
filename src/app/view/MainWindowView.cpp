#include "view/MainWindowView.h"
#include "dialogs/AppMessageDialog.h"
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickWidget>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMainWindow>
#include <QMenu>
#include <QUrl>

#include "ui/models/ShareImportBannerModel.h"
#include "ui/models/AboutPageModel.h"
#include "ui/models/UsagePageModel.h"
#include "ui/models/PluginCenterModel.h"
#include "ui/models/DriverPageModel.h"
#include "ui/models/KeyPageModel.h"
#include "ui/models/MacroPageModel.h"
#include "ui/models/SettingsPageModel.h"
#include "ui/styles/DesignTokens.h"
#include "ui/styles/QmlTheme.h"
#include "ui/components/QmlIconProvider.h"
#include "dialogs/QmlDialog.h"
#include "FactionTheme.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QLayout>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace {
constexpr int kBaseWindowWidth = 780;
constexpr int kBaseWindowHeight = 560;
constexpr int kBaseDpi = 96;

QColor appShellColor() {
    return FactionTheme::instance().palette().surfaceTint;
}

QRect currentAvailableGeometry() {
    if (auto* screen = QApplication::screenAt(QCursor::pos())) {
        return screen->availableGeometry();
    }
    if (auto* screen = QApplication::primaryScreen()) {
        return screen->availableGeometry();
    }
    return QRect(0, 0, 1920, 1080);
}

QSize scaledInitialWindowSize(const QRect& available) {
    constexpr int baseWidth = 780;
    constexpr int baseHeight = 560;
    constexpr int baseScreenWidth = 1920;
    constexpr int baseScreenHeight = 1080;

    const double widthScale = static_cast<double>(available.width()) / baseScreenWidth;
    const double heightScale = static_cast<double>(available.height()) / baseScreenHeight;
    const double widthSizeScale = qBound(1.0, qMin(widthScale, heightScale), 2.0);
    const int extraHeight = qMax(0, available.height() - baseScreenHeight);

    const int scaledWidth = static_cast<int>(baseWidth * widthSizeScale + 0.5);
    const int scaledHeight = baseHeight + static_cast<int>(extraHeight * 0.18 + 0.5);
    const int maxWidth = qMax(720, available.width() - 80);
    const int maxHeight = qMin(qMax(560, available.height() - 80), 700);

    return QSize(
        qBound(780, scaledWidth, maxWidth),
        qBound(560, scaledHeight, maxHeight)
    );
}

void applyScreenAwareWindowSize(QMainWindow* window, QScreen* screen, bool centerOnScreen) {
    if (!window || window->isMinimized()) {
        return;
    }

    const QRect available = screen ? screen->availableGeometry() : currentAvailableGeometry();
    const QSize size = scaledInitialWindowSize(available);
    const QPoint center = centerOnScreen ? available.center() : window->geometry().center();

    QRect geometry(QPoint(center.x() - size.width() / 2, center.y() - size.height() / 2), size);
    const int minX = available.left();
    const int minY = available.top();
    const int maxX = qMax(minX, available.right() - geometry.width() + 1);
    const int maxY = qMax(minY, available.bottom() - geometry.height() + 1);
    geometry.moveTopLeft(QPoint(
        qBound(minX, geometry.left(), maxX),
        qBound(minY, geometry.top(), maxY)
    ));

    window->setGeometry(geometry);
}

int minimumLogicalSizeForDpi(QMainWindow* window, int value, int nativeDpi) {
    Q_UNUSED(window)
    Q_UNUSED(nativeDpi)
    return value;
}

void keepWindowWithinAvailableGeometry(QMainWindow* window, QScreen* screen, int nativeDpi = 0, bool clampToDesignedSize = false) {
    if (!window || window->isMinimized()) {
        return;
    }

    window->setMinimumSize(kBaseWindowWidth, kBaseWindowHeight);
    const QRect available = screen ? screen->availableGeometry() : currentAvailableGeometry();
    QRect geometry = window->geometry();
    if (geometry.isEmpty()) {
        return;
    }

    const int minWidth = qMax(window->minimumWidth(), minimumLogicalSizeForDpi(window, kBaseWindowWidth, nativeDpi));
    const int minHeight = qMax(window->minimumHeight(), minimumLogicalSizeForDpi(window, kBaseWindowHeight, nativeDpi));
    const QSize designedSize = scaledInitialWindowSize(available);
    const int maxWidth = clampToDesignedSize ? qMax(minWidth, designedSize.width()) : qMax(minWidth, available.width());
    const int maxHeight = clampToDesignedSize ? qMax(minHeight, designedSize.height()) : qMax(minHeight, available.height());
    const int width = qBound(minWidth, geometry.width(), maxWidth);
    const int height = qBound(minHeight, geometry.height(), maxHeight);
    geometry.setSize(QSize(width, height));

    const int minX = available.left();
    const int minY = available.top();
    const int maxX = qMax(minX, available.right() - geometry.width() + 1);
    const int maxY = qMax(minY, available.bottom() - geometry.height() + 1);
    geometry.moveTopLeft(QPoint(
        qBound(minX, geometry.left(), maxX),
        qBound(minY, geometry.top(), maxY)
    ));

    if (geometry != window->geometry()) {
        window->setGeometry(geometry);
    }
}

void refreshLayoutTree(QWidget* widget, bool polishStyle) {
    if (!widget) {
        return;
    }

    if (polishStyle) {
        widget->ensurePolished();
        if (auto* style = widget->style()) {
            style->unpolish(widget);
            style->polish(widget);
        }
    } else {
        widget->ensurePolished();
    }
    widget->updateGeometry();
    if (auto* layout = widget->layout()) {
        layout->invalidate();
    }

    const auto children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* child : children) {
        refreshLayoutTree(child, polishStyle);
    }

    if (auto* layout = widget->layout()) {
        layout->activate();
    }
    widget->update();
}

void runDeferredLayoutPass(QMainWindow* window, bool polishStyle) {
    if (!window) {
        return;
    }

    if (auto* central = window->centralWidget()) {
        refreshLayoutTree(central, polishStyle);
    }
    window->ensurePolished();
    window->updateGeometry();
    window->update();
}

void bindScreenChangeResize(QMainWindow* window) {
    QTimer::singleShot(0, window, [window]() {
        auto* handle = window ? window->windowHandle() : nullptr;
        if (!handle) {
            return;
        }

        auto refreshLayouts = [window]() {
            runDeferredLayoutPass(window, false);
        };

        auto bindScreenSignals = [window, refreshLayouts](QScreen* screen) {
            if (!screen) return;
            QObject::connect(screen, &QScreen::logicalDotsPerInchChanged, window,
                [window, refreshLayouts, screen](qreal dpi) {
                    const int nativeDpi = qMax(kBaseDpi, qRound(dpi));
                    keepWindowWithinAvailableGeometry(window, screen, nativeDpi, false);
                    QTimer::singleShot(0, window, refreshLayouts);
                    QTimer::singleShot(160, window, refreshLayouts);
                });
            QObject::connect(screen, &QScreen::physicalDotsPerInchChanged, window,
                [window, refreshLayouts, screen](qreal dpi) {
                    const int nativeDpi = qMax(kBaseDpi, qRound(dpi));
                    keepWindowWithinAvailableGeometry(window, screen, nativeDpi, false);
                    QTimer::singleShot(0, window, refreshLayouts);
                    QTimer::singleShot(160, window, refreshLayouts);
                });
        };

        bindScreenSignals(handle->screen());
        QObject::connect(handle, &QWindow::screenChanged, window, [window, bindScreenSignals](QScreen* screen) {
            bindScreenSignals(screen);
            applyScreenAwareWindowSize(window, screen, false);
            QTimer::singleShot(0, window, [window]() { runDeferredLayoutPass(window, false); });
            QTimer::singleShot(160, window, [window]() { runDeferredLayoutPass(window, false); });
        });
    });
}

}

MainWindowView::~MainWindowView() {
    delete quickWidget_;
}

bool MainWindowView::setup(QMainWindow* window, QObject* onboarding) {
    window_ = window;
    window->setWindowTitle(tr("小叽按键"));
    connect(window, &QMainWindow::windowTitleChanged, this, &MainWindowView::titleChanged);
    idleIcon_ = QIcon(":/assets/icon.png");
    activeIcon_ = QIcon(":/assets/icon-active.png");
    qApp->setWindowIcon(idleIcon_);
    window->setWindowIcon(idleIcon_);
    window->setWindowFlags((window->windowFlags() | Qt::FramelessWindowHint) & ~Qt::WindowMaximizeButtonHint);
    window->setAttribute(Qt::WA_TranslucentBackground);
    window->setMinimumSize(kBaseWindowWidth, kBaseWindowHeight);
    applyScreenAwareWindowSize(window, nullptr, true);
    bindScreenChangeResize(window);

    keyPage_ = new KeyPageModel(this);
    macroPage_ = new MacroPageModel(this);
    driverPage_ = new DriverPageModel(this);
    settingsPage_ = new SettingsPageModel(this);
    aboutPage_ = new AboutPageModel(this);
    usagePage_ = new UsagePageModel(this);
    pluginCenter_ = new PluginCenterModel(this);
    connect(pluginCenter_, &PageModel::stateChanged, this, [this] {
        if (currentPage_.startsWith(QStringLiteral("plugin:")) && pluginCenter_->page(currentPage_).isEmpty())
            navigateTo(QStringLiteral("plugins"));
    });

    auto* central = new QWidget(window);
    central->setObjectName("mainWindowRoot");
    window->setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    shareBanner_ = new ShareImportBannerModel(this);
    quickWidget_ = new QQuickWidget(central);
    quickWidget_->engine()->addImageProvider(QStringLiteral("app-icons"), new QmlIconProvider);
    quickWidget_->setObjectName(QStringLiteral("mainQuickView"));
    quickWidget_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget_->setFocusPolicy(Qt::StrongFocus);
    quickWidget_->setClearColor(Qt::transparent);
    quickWidget_->setAcceptDrops(false);
    auto* context = quickWidget_->rootContext();
    context->setContextProperty("appView", this);
    context->setContextProperty("onboardingModel", onboarding);
    context->setContextProperty("keyPageModel", keyPage_);
    context->setContextProperty("macroPageModel", macroPage_);
    context->setContextProperty("driverPageModel", driverPage_);
    context->setContextProperty("settingsPageModel", settingsPage_);
    context->setContextProperty("aboutPageModel", aboutPage_);
    context->setContextProperty("usagePageModel", usagePage_);
    context->setContextProperty("pluginCenterModel", pluginCenter_);
    context->setContextProperty("shareBannerModel", shareBanner_);
    updateFactionVisuals(window);
    quickWidget_->setSource(QUrl(QStringLiteral("qrc:/qt/qml/XiaoJi/UI/Main.qml")));
    if (quickWidget_->status() != QQuickWidget::Ready) {
        QStringList errors;
        for (const auto& error : quickWidget_->errors()) errors.push_back(error.toString());
        AppMessageDialog::warning(window, tr("QML 界面加载失败"), errors.join(QLatin1Char('\n')));
        return false;
    }
    layout->addWidget(quickWidget_, 1);
    connect(aboutPage_, &AboutPageModel::usageGuideRequested, window, [window]() {
        const QString path = QDir(QApplication::applicationDirPath()).filePath(QStringLiteral("使用说明.mhtml"));
        if (!QFileInfo::exists(path)) {
            AppMessageDialog::warning(window, tr("无法打开使用说明"), tr("未找到使用说明文件，请重新解压完整发布包。"));
        } else if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
            AppMessageDialog::warning(window, tr("无法打开使用说明"), tr("请确认系统已为 MHTML 文件配置可用的浏览器。"));
        }
    });
    return true;
}

void MainWindowView::initializeTray(QMainWindow* window) {
    if (trayIcon_ || !window) return;
    trayIcon_ = new QSystemTrayIcon(activityIconActive_ ? activeIcon_ : idleIcon_, window);
    trayMenu_ = new QMenu(window);
    auto* showAction = trayMenu_->addAction(tr("显示主窗口"));
    auto* quitAction = trayMenu_->addAction(tr("退出"));
    trayIcon_->setContextMenu(trayMenu_);
    trayIcon_->show();
    connect(showAction, &QAction::triggered, window, [window]() {
        window->showNormal();
        window->raise();
        window->activateWindow();
    });
    connect(quitAction, &QAction::triggered, window, [this]() {
        emit exitRequested();
    });
    connect(trayIcon_, &QSystemTrayIcon::activated, window, [window](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            window->showNormal();
            window->raise();
            window->activateWindow();
        }
    });
}

void MainWindowView::navigateTo(const QString& pageId) {
    if (currentPage_ == pageId) return;
    if (pageId.startsWith(QStringLiteral("plugin:")) && pluginCenter_)
        pluginCenter_->dispatch(QStringLiteral("expanded"), {}, true);
    currentPage_ = pageId;
    emit currentPageChanged();
}

void MainWindowView::selectPage(const QString& pageId) {
    if (pageId != "keys" && pageId != "macro" && pageId != "usage" && pageId != "driver"
        && pageId != "settings" && pageId != "about" && pageId != "plugins"
        && (!pluginCenter_ || pluginCenter_->page(pageId).isEmpty())) return;
    emit pageSelected(pageId);
}

void MainWindowView::selectFaction(const QString& factionId) {
    if (FactionTheme::instance().find(factionId)) emit factionSelected(factionId);
}

void MainWindowView::startWindowMove() {
    if (window_ && window_->windowHandle()) window_->windowHandle()->startSystemMove();
}

void MainWindowView::minimizeWindow() { if (window_) window_->showMinimized(); }
void MainWindowView::closeWindow() { if (window_) window_->close(); }
QString MainWindowView::title() const { return window_ ? window_->windowTitle() : QString(); }
QString MainWindowView::activityIcon() const {
    return activityIconActive_ ? QStringLiteral("qrc:/assets/icon-active.png") : QStringLiteral("qrc:/assets/icon.png");
}

void MainWindowView::showRepeatPage() { navigateTo(QStringLiteral("keys")); }
void MainWindowView::showMacroPage() { navigateTo(QStringLiteral("macro")); }
bool MainWindowView::isRepeatPageActive() const { return currentPage_ == QStringLiteral("keys"); }
QString MainWindowView::version() const { return QString::fromLatin1(APP_VERSION); }

QVariantList MainWindowView::factions() const {
    QVariantList values;
    for (const auto& faction : FactionTheme::instance().factions()) {
        values.push_back(QVariantMap{{"id", faction.id}, {"name", faction.name},
            {"icon", QString(faction.iconPath).replace(QStringLiteral(":/"), QStringLiteral("qrc:/"))}});
    }
    return values;
}

void MainWindowView::updateActivityIcons(QMainWindow* window, bool repeatRunning, bool macroRunning) {
    const bool active = repeatRunning || macroRunning;
    if (activityIconActive_ == active) return;
    activityIconActive_ = active;
    const QIcon& icon = active ? activeIcon_ : idleIcon_;
    window->setWindowIcon(icon);
    emit activityIconChanged();
    if (trayIcon_) trayIcon_->setIcon(icon);
}

void MainWindowView::updateFactionVisuals(QMainWindow* window) {
    const auto& faction = FactionTheme::instance().current();
    qApp->setPalette(Fluent::Theme::applicationPalette());
    theme_ = Fluent::qmlTheme();
    emit themeChanged();
    QPalette palette = window->palette();
    palette.setColor(QPalette::Window, appShellColor());
    window->setPalette(palette);
    window->centralWidget()->setStyleSheet(QString("QWidget#mainWindowRoot { background: %1; border-radius: 10px; }").arg(appShellColor().name()));
    if (settingsPage_) settingsPage_->setFaction(faction.id);
    for (auto* dialog : window->findChildren<QmlDialog*>()) dialog->refreshVisuals();
    window->update();
}

void MainWindowView::refreshRuntimeVisuals(QMainWindow* window, bool dpiChanged, bool refreshThemeColors, int nativeDpi) {
    if (!window) return;
    if (refreshThemeColors) updateFactionVisuals(window);
    if (dpiChanged) {
        auto* handle = window->windowHandle();
        keepWindowWithinAvailableGeometry(window, handle ? handle->screen() : nullptr, nativeDpi, false);
    }
    if (window->centralWidget()) refreshLayoutTree(window->centralWidget(), true);
    for (auto* dialog : window->findChildren<QmlDialog*>()) dialog->refreshVisuals();
    if (quickWidget_) quickWidget_->update();
    if (auto* handle = window->windowHandle()) handle->requestUpdate();
    window->updateGeometry();
    window->update();
}

void MainWindowView::hideTrayIcon() {
    if (trayIcon_) trayIcon_->hide();
}
