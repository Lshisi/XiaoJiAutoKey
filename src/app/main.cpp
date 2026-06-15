#include "MainWindow.h"
#include "ui/styles/DesignTokens.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QStringList>
#include <QStyleFactory>
#include <QtPlugin>
#include <Windows.h>
#include <mmsystem.h>

// Static Qt plugin imports
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)

#pragma comment(lib, "winmm.lib")

int main(int argc, char *argv[]) {
    const QStringList args(argv, argv + argc);
    if (args.contains(QStringLiteral("--restart-wait"))) {
        Sleep(1500);
    }

    // 鍗曞疄渚嬪畧鎶?
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"XiaoJiAutoKey_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        MessageBoxW(nullptr, L"\u5c0f\u53fd\u6309\u952e\u5df2\u5728\u8fd0\u884c\u4e2d\uff0c\u8bf7\u52ff\u91cd\u590d\u6253\u5f00\u3002", L"\u63d0\u793a", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // 鎻愬崌鏃堕挓绮惧害鍒?1ms
    timeBeginPeriod(1);

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setPalette(Fluent::Theme::applicationPalette());

    // Set global font - compact UI with crisp CJK rendering.
    QFont globalFont = Fluent::Typography::body();
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
