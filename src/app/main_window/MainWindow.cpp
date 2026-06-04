#include "MainWindow.h"

#include <QEvent>
#include <QMessageBox>
#include <stdexcept>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      appPaths_(),
      configCoordinator_(appPaths_.configPath(), this),
      profileController_(config_),
      macroController_(config_),
      engine_(driver_),
      macroEngine_(driver_),
      inputHook_(this),
      soundService_(appPaths_.exeDir()) {}

MainWindow::~MainWindow() {
    engineStateCoordinator_.stopThreads();
}

bool MainWindow::Initialize() {
    setupUi();
    configCoordinator_.load();

    if (!driver_.LoadFromDirectory(appPaths_.exeDir())) {
        std::wstring errorMsg = L"\u9519\u8bef\u539f\u56e0: " + driver_.GetLastError() +
            L"\n\n\u8bf7\u786e\u8ba4\uff1a\n1. \u662f\u5426\u5df2\u5b89\u88c5 DD \u9a71\u52a8\n2. DD64.dll / DD32.dll \u662f\u5426\u5728 exe \u540c\u76ee\u5f55\n\n\u7a0b\u5e8f\u5373\u5c06\u9000\u51fa\u3002";
        QMessageBox::critical(this, QString::fromWCharArray(L"DD \u9a71\u52a8\u52a0\u8f7d\u5931\u8d25"),
            QString::fromStdWString(errorMsg));
        return false;
    }

    try {
        setupHooks();
    } catch (const std::exception&) {
        QMessageBox::critical(this, QString::fromWCharArray(L"\u5168\u5c40\u94a9\u5b50\u521d\u59cb\u5316\u5931\u8d25"),
            QString::fromWCharArray(L"\u65e0\u6cd5\u5b89\u88c5\u5168\u5c40\u952e\u76d8/\u9f20\u6807\u94a9\u5b50"));
        return false;
    }
    engineStateCoordinator_.startThreads();
    engineStateCoordinator_.applyBindings();
    engineStateCoordinator_.applyMacros();
    pageRefreshCoordinator_.refreshAll();
    return true;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && captureCoordinator_.captureActive()) {
        captureCoordinator_.cancelCapture();
    }
    return QMainWindow::eventFilter(watched, event);
}

