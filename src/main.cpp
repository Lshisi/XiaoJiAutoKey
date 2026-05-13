#include "MainWindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QtPlugin>
#include <Windows.h>
#include <mmsystem.h>

// Static Qt plugin imports
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)

#pragma comment(lib, "winmm.lib")

int main(int argc, char *argv[]) {
    // 单实例守护：防止打开多个进程
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"XiaoJiAutoKey_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        MessageBoxW(nullptr, L"小叽按键已在运行中，请勿重复打开。", L"提示", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // 提升 Windows 系统时钟精度到 1ms，解决 Sleep 默认 15.6ms 误差导致的连发卡顿问题
    timeBeginPeriod(1);

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

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