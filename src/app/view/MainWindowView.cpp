#include "view/MainWindowView.h"

#include "ui/components/FluentSidebar.h"
#include "ui/components/FluentTitleBar.h"
#include "ui/pages/AboutPage.h"
#include "ui/pages/KeyPage.h"
#include "ui/pages/MacroPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace {
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

void bindScreenChangeResize(QMainWindow* window) {
    QTimer::singleShot(0, window, [window]() {
        auto* handle = window ? window->windowHandle() : nullptr;
        if (!handle) {
            return;
        }

        QObject::connect(handle, &QWindow::screenChanged, window, [window](QScreen* screen) {
            applyScreenAwareWindowSize(window, screen, false);
        });
    });
}
}

void MainWindowView::setup(QMainWindow* window) {
    window->setWindowTitle(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e~"));
    window->setWindowIcon(QIcon(":/assets/icon.png"));
    window->setWindowFlags((window->windowFlags() | Qt::FramelessWindowHint) & ~Qt::WindowMaximizeButtonHint);
    window->setAttribute(Qt::WA_TranslucentBackground, true);

    applyScreenAwareWindowSize(window, nullptr, true);
    window->setMinimumSize(720, 560);
    bindScreenChangeResize(window);

    auto* centralWidget = new QWidget(window);
    centralWidget->setObjectName("mainWindowRoot");
    centralWidget->setStyleSheet(QString(
        "QWidget#mainWindowRoot {"
        "  background: %1;"
        "  border: none;"
        "  border-radius: 10px;"
        "}")
        .arg(FactionTheme::instance().palette().surfaceTint.name()));
    window->setCentralWidget(centralWidget);

    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(1, 1, 1, 1);
    rootLayout->setSpacing(0);

    auto* titleBar = new FluentTitleBar(window);
    titleBar->setTitle(window->windowTitle());
    titleBar->setIcon(window->windowIcon());
    rootLayout->addWidget(titleBar);
    QObject::connect(titleBar, &FluentTitleBar::minimizeClicked, window, &QMainWindow::showMinimized);
    QObject::connect(titleBar, &FluentTitleBar::closeClicked, window, &QMainWindow::close);

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    sidebar_ = new FluentSidebar(window);
    sidebar_->addNavItem(QStringLiteral("keys"), QString::fromWCharArray(L"\u6309\u952e\u8fde\u53d1"), "keys");
    sidebar_->addNavItem(QStringLiteral("macro"), QString::fromWCharArray(L"\u5b8f\u5faa\u73af"), "macro");
    sidebar_->addNavItem(QStringLiteral("settings"), QString::fromWCharArray(L"\u5168\u5c40\u8bbe\u7f6e"), "settings");
    sidebar_->addNavItem(QStringLiteral("about"), QString::fromWCharArray(L"\u5173\u4e8e\u8f6f\u4ef6"), "about");
    sidebar_->setCurrentPage("keys");
    bodyLayout->addWidget(sidebar_);

    pageStack_ = new QStackedWidget(window);
    keyPage_ = new KeyPage(window);
    macroPage_ = new MacroPage(window);
    settingsPage_ = new SettingsPage(window);
    aboutPage_ = new AboutPage(window);
    pageStack_->addWidget(keyPage_);
    pageStack_->addWidget(macroPage_);
    pageStack_->addWidget(settingsPage_);
    pageStack_->addWidget(aboutPage_);
    pageStack_->setCurrentWidget(keyPage_);
    bodyLayout->addWidget(pageStack_, 1);
    rootLayout->addLayout(bodyLayout, 1);

    auto* statusBar = new QWidget(window);
    statusBar->setObjectName("appStatusBar");
    statusBar->setFixedHeight(24);
    statusBar->setStyleSheet(QString(
        "QWidget#appStatusBar {"
        "  background: %1;"
        "  border-top: 1px solid rgba(0,0,0,0.08);"
        "  border-bottom-left-radius: 10px;"
        "  border-bottom-right-radius: 10px;"
        "}")
        .arg(FactionTheme::instance().palette().surfaceTint.name()));
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    auto* versionLabel = new QLabel(QString("v") + QString(APP_VERSION), statusBar);
    versionLabel->setFont(Fluent::Typography::smallText());
    QPalette statusPalette = versionLabel->palette();
    statusPalette.setColor(QPalette::WindowText, Fluent::Colors::textTertiary());
    versionLabel->setPalette(statusPalette);
    statusLayout->addWidget(versionLabel);
    statusLayout->addStretch();
    auto* authorLabel = new QLabel(QString::fromWCharArray(L"By\uff1a\u9f99\u864e-\u95fb\u6210\u753b"), statusBar);
    authorLabel->setFont(Fluent::Typography::smallText());
    authorLabel->setPalette(statusPalette);
    statusLayout->addWidget(authorLabel);
    rootLayout->addWidget(statusBar);

    trayIcon_ = new QSystemTrayIcon(QIcon(":/assets/icon.png"), window);
    trayMenu_ = new QMenu(window);
    auto* showAction = trayMenu_->addAction(QString::fromWCharArray(L"\u663e\u793a\u4e3b\u7a97\u53e3"));
    auto* quitAction = trayMenu_->addAction(QString::fromWCharArray(L"\u9000\u51fa"));
    trayIcon_->setContextMenu(trayMenu_);
    trayIcon_->setToolTip(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e"));
    trayIcon_->show();

    QObject::connect(showAction, &QAction::triggered, window, [window]() {
        window->showNormal();
        window->activateWindow();
    });
    QObject::connect(quitAction, &QAction::triggered, window, [this]() {
        hideTrayIcon();
        qApp->quit();
    });
    QObject::connect(trayIcon_, &QSystemTrayIcon::activated, window,
        [window](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                window->showNormal();
                window->activateWindow();
            }
        });
}

void MainWindowView::navigateTo(const QString& pageId) {
    if (pageId == "keys") pageStack_->setCurrentWidget(keyPage_);
    else if (pageId == "macro") pageStack_->setCurrentWidget(macroPage_);
    else if (pageId == "settings") pageStack_->setCurrentWidget(settingsPage_);
    else if (pageId == "about") pageStack_->setCurrentWidget(aboutPage_);
}

void MainWindowView::showRepeatPage() {
    pageStack_->setCurrentWidget(keyPage_);
    sidebar_->setCurrentPage("keys");
}

void MainWindowView::showMacroPage() {
    pageStack_->setCurrentWidget(macroPage_);
    sidebar_->setCurrentPage("macro");
}

bool MainWindowView::isRepeatPageActive() const {
    return pageStack_->currentWidget() == keyPage_;
}

void MainWindowView::updateFactionVisuals(QMainWindow* window) {
    if (window->centralWidget()) {
        window->centralWidget()->setStyleSheet(QString(
            "QWidget#mainWindowRoot {"
            "  background: %1;"
            "  border: none;"
            "  border-radius: 10px;"
            "}")
            .arg(FactionTheme::instance().palette().surfaceTint.name()));
    }
    if (auto* statusBar = window->findChild<QWidget*>("appStatusBar")) {
        statusBar->setStyleSheet(QString(
            "QWidget#appStatusBar {"
            "  background: %1;"
            "  border-top: 1px solid rgba(0,0,0,0.08);"
            "  border-bottom-left-radius: 10px;"
            "  border-bottom-right-radius: 10px;"
            "}")
            .arg(FactionTheme::instance().palette().surfaceTint.name()));
    }
    sidebar_->updateFactionIcon();
    sidebar_->update();
    for (auto* child : sidebar_->findChildren<QWidget*>()) child->update();
    keyPage_->updateThemeColors();
    keyPage_->update();
    macroPage_->updateThemeColors();
    settingsPage_->setFaction(FactionTheme::instance().current().id);
    aboutPage_->setStyleSheet(QString("AboutPage { background: %1; }").arg(FactionTheme::instance().palette().surfaceTint.name()));
    window->update();
}

void MainWindowView::hideTrayIcon() {
    if (trayIcon_) trayIcon_->hide();
}
