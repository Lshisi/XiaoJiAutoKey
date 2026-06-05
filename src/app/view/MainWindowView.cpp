#include "view/MainWindowView.h"

#include "ui/components/FluentSidebar.h"
#include "ui/pages/AboutPage.h"
#include "ui/pages/KeyPage.h"
#include "ui/pages/MacroPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/styles/DesignTokens.h"
#include "FactionTheme.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

void MainWindowView::setup(QMainWindow* window) {
    window->setWindowTitle(QString::fromWCharArray(L"\u5c0f\u53fd\u6309\u952e~"));
    window->setWindowIcon(QIcon(":/assets/icon.png"));
    window->resize(780, 560);
    window->setMinimumSize(720, 560);

    auto* centralWidget = new QWidget(window);
    centralWidget->setObjectName("mainWindowRoot");
    centralWidget->setStyleSheet(QString("QWidget#mainWindowRoot { background: %1; }")
        .arg(FactionTheme::instance().palette().surfaceTint.name()));
    window->setCentralWidget(centralWidget);

    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    sidebar_ = new FluentSidebar(window);
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2328"), QString::fromWCharArray(L"\u6309\u952e\u8fde\u53d1"), "keys");
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2630"), QString::fromWCharArray(L"\u5b8f\u5faa\u73af"), "macro");
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2699"), QString::fromWCharArray(L"\u5168\u5c40\u8bbe\u7f6e"), "settings");
    sidebar_->addNavItem(QString::fromWCharArray(L"\u2139"), QString::fromWCharArray(L"\u5173\u4e8e\u8f6f\u4ef6"), "about");
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
    statusBar->setStyleSheet(QString("QWidget#appStatusBar { background: %1; border-top: 1px solid rgba(0,0,0,0.08); }")
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
        window->centralWidget()->setStyleSheet(QString("QWidget#mainWindowRoot { background: %1; }")
            .arg(FactionTheme::instance().palette().surfaceTint.name()));
    }
    if (auto* statusBar = window->findChild<QWidget*>("appStatusBar")) {
        statusBar->setStyleSheet(QString("QWidget#appStatusBar { background: %1; border-top: 1px solid rgba(0,0,0,0.08); }")
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
