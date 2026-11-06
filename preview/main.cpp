#include "PreviewController.h"
#include "ui/components/QmlIconProvider.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QWindow>

int main(int argc, char* argv[]) {
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("XiaoJiPreview"));
    PreviewController controller;
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("app-icons"), new QmlIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("appView"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("keyPageModel"), controller.keyPage());
    engine.rootContext()->setContextProperty(QStringLiteral("macroPageModel"), controller.macroPage());
    engine.rootContext()->setContextProperty(QStringLiteral("usagePageModel"), controller.usagePage());
    engine.rootContext()->setContextProperty(QStringLiteral("driverPageModel"), controller.driverPage());
    engine.rootContext()->setContextProperty(QStringLiteral("settingsPageModel"), controller.settingsPage());
    engine.rootContext()->setContextProperty(QStringLiteral("aboutPageModel"), controller.aboutPage());
    engine.rootContext()->setContextProperty(QStringLiteral("shareBannerModel"), controller.shareBanner());
    engine.rootContext()->setContextProperty(QStringLiteral("onboardingModel"), controller.onboarding());
    engine.rootContext()->setContextProperty(QStringLiteral("dialogModel"), controller.dialog());
    engine.rootContext()->setContextProperty(QStringLiteral("overviewDialogModel"), controller.overviewDialog());
    engine.rootContext()->setContextProperty(QStringLiteral("overlayModel"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("pickerModel"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/XiaoJi/UI/PreviewWindow.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    controller.attachWindow(qobject_cast<QWindow*>(engine.rootObjects().first()));
    return app.exec();
}
