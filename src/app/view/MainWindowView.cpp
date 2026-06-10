#include "view/MainWindowView.h"

#include "ui/components/FluentButton.h"
#include "ui/components/FluentCard.h"
#include "ui/components/FluentComboBox.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/components/FluentSidebar.h"
#include "ui/components/FluentTable.h"
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
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QTimer>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>
#include <QLayout>

namespace {
constexpr int kBaseWindowWidth = 780;
constexpr int kBaseWindowHeight = 560;
constexpr int kBaseDpi = 96;

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

        QObject::connect(handle, &QWindow::screenChanged, window, [window](QScreen* screen) {
            applyScreenAwareWindowSize(window, screen, false);
            QTimer::singleShot(0, window, [window]() { runDeferredLayoutPass(window, false); });
            QTimer::singleShot(160, window, [window]() { runDeferredLayoutPass(window, false); });
        });
    });
}

void refreshLabelPalette(QLabel* label) {
    if (!label || label->text().isEmpty()) {
        return;
    }
    if (!label->pixmap(Qt::ReturnByValue).isNull() || label->movie()) {
        return;
    }

    QPalette palette = label->palette();
    const QColor current = palette.color(QPalette::WindowText);
    const auto isSameColor = [](const QColor& a, const QColor& b) {
        return a.red() == b.red() && a.green() == b.green() && a.blue() == b.blue();
    };
    const bool wasTertiary =
        isSameColor(current, QColor(0x90, 0x90, 0x90)) ||
        isSameColor(current, QColor(0x9A, 0x9A, 0x9A));
    const bool wasSecondary =
        isSameColor(current, QColor(0x60, 0x60, 0x60)) ||
        isSameColor(current, QColor(0xC8, 0xC8, 0xC8));

    const QColor color = wasTertiary ? Fluent::Colors::textTertiary()
        : wasSecondary ? Fluent::Colors::textSecondary()
                       : Fluent::Colors::textPrimary();
    for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
        palette.setColor(group, QPalette::WindowText, color);
        palette.setColor(group, QPalette::Text, color);
    }
    label->setPalette(palette);
    label->setAutoFillBackground(false);
    label->updateGeometry();
    label->update();
}

void refreshSeparatorWidget(QWidget* widget) {
    if (!widget || widget->minimumHeight() != 1 || widget->maximumHeight() != 1) {
        return;
    }
    if (!widget->styleSheet().contains(QStringLiteral("background"), Qt::CaseInsensitive)) {
        return;
    }
    widget->setStyleSheet(QString("background: %1;")
        .arg(Fluent::Colors::divider().name(QColor::HexArgb)));
}

void refreshScrollAreaStyle(QScrollArea* scrollArea) {
    if (!scrollArea || scrollArea->styleSheet().isEmpty()) {
        return;
    }
    scrollArea->setStyleSheet(
        QString("QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: %2; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
            .arg(Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.18)") : QStringLiteral("rgba(0,0,0,0.12)"),
                Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.28)") : QStringLiteral("rgba(0,0,0,0.2)")));
    if (auto* content = scrollArea->widget()) {
        content->updateGeometry();
        if (auto* layout = content->layout()) {
            layout->invalidate();
        }
        QTimer::singleShot(0, content, [content]() {
            if (auto* layout = content->layout()) {
                layout->activate();
            }
            content->updateGeometry();
            content->update();
        });
    }
    scrollArea->updateGeometry();
    if (auto* viewport = scrollArea->viewport()) {
        viewport->update();
    }
}

void refreshKnownWidget(QWidget* widget) {
    if (auto* card = qobject_cast<FluentCard*>(widget)) {
        card->refreshThemeStyle();
    }
    if (auto* titleBar = qobject_cast<FluentTitleBar*>(widget)) {
        titleBar->refreshThemeStyle();
    }
    if (auto* sidebar = qobject_cast<FluentSidebar*>(widget)) {
        sidebar->updateFactionIcon();
    }
    if (auto* button = qobject_cast<FluentButton*>(widget)) {
        button->refreshMetrics();
    }
    if (auto* combo = qobject_cast<FluentComboBox*>(widget)) {
        combo->refreshThemeStyle();
    }
    if (auto* lineEdit = qobject_cast<FluentLineEdit*>(widget)) {
        lineEdit->refreshThemeStyle();
    }
    if (auto* table = qobject_cast<FluentTable*>(widget)) {
        table->refreshThemeStyle();
    }
    if (auto* scrollArea = qobject_cast<QScrollArea*>(widget)) {
        refreshScrollAreaStyle(scrollArea);
    }
    if (auto* label = qobject_cast<QLabel*>(widget)) {
        refreshLabelPalette(label);
    }
    refreshSeparatorWidget(widget);
}

void refreshWidgetTree(QWidget* widget) {
    if (!widget) {
        return;
    }

    refreshKnownWidget(widget);
    widget->ensurePolished();
    if (auto* style = widget->style()) {
        style->unpolish(widget);
        style->polish(widget);
    }
    widget->updateGeometry();
    if (auto* layout = widget->layout()) {
        layout->invalidate();
        layout->activate();
    }
    widget->update();

    const auto children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* child : children) {
        refreshWidgetTree(child);
    }
}
}

void MainWindowView::setup(QMainWindow* window) {
    window->setWindowTitle(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e"));
    window->setWindowIcon(QIcon(":/assets/icon.png"));
    window->setWindowFlags((window->windowFlags() | Qt::FramelessWindowHint) & ~Qt::WindowMaximizeButtonHint);
    window->setAttribute(Qt::WA_TranslucentBackground, true);
    window->setAutoFillBackground(false);
    QPalette windowPalette = window->palette();
    windowPalette.setColor(QPalette::Window, FactionTheme::instance().palette().surfaceTint);
    window->setPalette(windowPalette);

    applyScreenAwareWindowSize(window, nullptr, true);
    window->setMinimumSize(kBaseWindowWidth, kBaseWindowHeight);
    bindScreenChangeResize(window);

    auto* centralWidget = new QWidget(window);
    centralWidget->setObjectName("mainWindowRoot");
    centralWidget->setAttribute(Qt::WA_StyledBackground, true);
    centralWidget->setAutoFillBackground(false);
    QPalette centralPalette = centralWidget->palette();
    centralPalette.setColor(QPalette::Window, FactionTheme::instance().palette().surfaceTint);
    centralWidget->setPalette(centralPalette);
    centralWidget->setStyleSheet(QString(
        "QWidget#mainWindowRoot {"
        "  background: %1;"
        "  border: none;"
        "  border-radius: 10px;"
        "}")
        .arg(FactionTheme::instance().palette().surfaceTint.name()));
    window->setCentralWidget(centralWidget);

    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
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
        "  border-top: 1px solid %2;"
        "  border-bottom-left-radius: 10px;"
        "  border-bottom-right-radius: 10px;"
        "}")
        .arg(FactionTheme::instance().palette().surfaceTint.name(),
            Fluent::Colors::divider().name(QColor::HexArgb)));
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

}

void MainWindowView::initializeTray(QMainWindow* window) {
    if (trayIcon_ || !window) {
        return;
    }

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
    QPalette windowPalette = window->palette();
    windowPalette.setColor(QPalette::Window, FactionTheme::instance().palette().surfaceTint);
    window->setPalette(windowPalette);
    if (window->centralWidget()) {
        QPalette centralPalette = window->centralWidget()->palette();
        centralPalette.setColor(QPalette::Window, FactionTheme::instance().palette().surfaceTint);
        window->centralWidget()->setPalette(centralPalette);
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
            "  border-top: 1px solid %2;"
            "  border-bottom-left-radius: 10px;"
            "  border-bottom-right-radius: 10px;"
            "}")
            .arg(FactionTheme::instance().palette().surfaceTint.name(),
                Fluent::Colors::divider().name(QColor::HexArgb)));
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

void MainWindowView::refreshRuntimeVisuals(QMainWindow* window, bool dpiChanged, bool refreshThemeColors, int nativeDpi) {
    if (!window) {
        return;
    }

    if (refreshThemeColors) {
        updateFactionVisuals(window);
    } else {
        QPalette windowPalette = window->palette();
        windowPalette.setColor(QPalette::Window, FactionTheme::instance().palette().surfaceTint);
        window->setPalette(windowPalette);
        if (window->centralWidget()) {
            QPalette centralPalette = window->centralWidget()->palette();
            centralPalette.setColor(QPalette::Window, FactionTheme::instance().palette().surfaceTint);
            window->centralWidget()->setPalette(centralPalette);
        }
    }

    if (dpiChanged) {
        QScreen* screen = nullptr;
        if (auto* handle = window->windowHandle()) {
            screen = handle->screen();
        }
        window->setMinimumSize(kBaseWindowWidth, kBaseWindowHeight);
        keepWindowWithinAvailableGeometry(window, screen, nativeDpi, true);
    }

    if (auto* central = window->centralWidget()) {
        refreshWidgetTree(central);
    }

    if (trayIcon_) {
        trayIcon_->setIcon(QIcon(":/assets/icon.png"));
    }

    if (auto* handle = window->windowHandle()) {
        handle->requestUpdate();
    }
    window->updateGeometry();
    window->update();

    if (dpiChanged) {
        QTimer::singleShot(0, window, [window]() {
            runDeferredLayoutPass(window, false);
        });
        QTimer::singleShot(180, window, [window]() {
            runDeferredLayoutPass(window, false);
        });
    }
}

void MainWindowView::hideTrayIcon() {
    if (trayIcon_) trayIcon_->hide();
}
