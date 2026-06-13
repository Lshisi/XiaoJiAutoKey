#include "MainWindow.h"
#include "ui/styles/DesignTokens.h"
#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QStyleFactory>
#include <QtPlugin>
#include <Windows.h>
#include <mmsystem.h>

// Static Qt plugin imports
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)

#pragma comment(lib, "winmm.lib")

int main(int argc, char *argv[]) {
    // 单实例守护
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"XiaoJiAutoKey_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        MessageBoxW(nullptr, L"小叽按键已在运行中，请勿重复打开。", L"提示", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // 提升时钟精度到 1ms
    timeBeginPeriod(1);

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setPalette(Fluent::Theme::applicationPalette());

    // Set global font - compact UI with slight letter spacing
    QFont globalFont("Segoe UI", 9);
    globalFont.setStyleStrategy(QFont::PreferAntialias);
    globalFont.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    app.setFont(globalFont);

    MainWindow window;
    if (!window.Initialize()) {
        timeEndPeriod(1);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }
    window.show();

    int result = app.exec();

    timeEndPeriod(1);
    if (hMutex) CloseHandle(hMutex);
    return result;
}
