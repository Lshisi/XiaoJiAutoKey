#include "ScreenPointPicker.h"
#include "ui/styles/DesignTokens.h"
#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWidget>
#include <QScreen>
#include <QShowEvent>
#include <QSurfaceFormat>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>
#include <vector>

namespace {
bool isStatusOverlay(const QWidget* widget) {
    return widget && qstrcmp(widget->metaObject()->className(), "StatusOverlayWindow") == 0;
}
std::vector<std::pair<QPointer<QWidget>, qreal>> fadeAppWindows(QWidget* host) {
    std::vector<std::pair<QPointer<QWidget>, qreal>> faded;
    const auto fade = [&faded](QWidget* widget) {
        if (!widget || !widget->isVisible() || widget->windowOpacity() <= 0) return;
        for (const auto& item : faded) if (item.first == widget) return;
        faded.emplace_back(widget, widget->windowOpacity());
        widget->setWindowOpacity(0);
    };
    fade(host);
    if (host && host->parentWidget()) fade(host->parentWidget()->window());
    for (auto* top : QApplication::topLevelWidgets())
        if (isStatusOverlay(top) || (top->parentWidget() && isStatusOverlay(top->parentWidget()->window()))) fade(top);
    return faded;
}
}
ScreenPointPicker::ScreenPointPicker(QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    quick_ = new QQuickWidget(this);
    QSurfaceFormat format = quick_->format();
    format.setAlphaBufferSize(8);
    quick_->setFormat(format);
    quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quick_->setClearColor(Qt::transparent);
    quick_->setAttribute(Qt::WA_TransparentForMouseEvents);
    quick_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(quick_);
    quick_->rootContext()->setContextProperty("pickerModel", this);
    refreshHint();
    quick_->setSource(QUrl(QStringLiteral("qrc:/qt/qml/XiaoJi/UI/PointPicker.qml")));
    if (quick_->status() != QQuickWidget::Ready) {
        QStringList errors;
        for (const auto& error : quick_->errors()) errors << error.toString();
        qFatal("QML point picker failed: %s", qPrintable(errors.join(QLatin1Char('\n'))));
    }
}
ScreenPointPicker::~ScreenPointPicker() { delete quick_; }
std::optional<QPoint> ScreenPointPicker::pick(QWidget* host) {
    auto faded = fadeAppWindows(host);
    ScreenPointPicker picker;
    const bool accepted = picker.exec() == QDialog::Accepted;
    const QPoint point = picker.pickedPos();
    for (auto& [widget, opacity] : faded) if (widget) widget->setWindowOpacity(opacity);
    if (host) host->activateWindow();
    return accepted ? std::optional<QPoint>(point) : std::nullopt;
}
void ScreenPointPicker::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    QRect united;
    for (const auto* screen : QGuiApplication::screens()) united = united.united(screen->geometry());
    if (united.isValid()) setGeometry(united);
    activateWindow();
    setFocus();
    refreshHint();
}
void ScreenPointPicker::refreshHint() {
    const QPoint global = QCursor::pos();
    const auto* screen = QGuiApplication::screenAt(global);
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect geometry = screen ? screen->geometry() : QRect();
    const QPoint local = mapFromGlobal(global);
    const QPoint origin = mapFromGlobal(geometry.topLeft());
    QString coords;
    if (geometry.width() > 0 && geometry.height() > 0) {
        coords = tr("屏幕内：%1%，%2%    %3, %4")
            .arg(100.0 * (global.x() - geometry.left()) / geometry.width(), 0, 'f', 1)
            .arg(100.0 * (global.y() - geometry.top()) / geometry.height(), 0, 'f', 1)
            .arg(global.x()).arg(global.y());
    }
    hintState_ = {{"text", tr("左键选择位置 · 右键 / Esc 取消\n%1").arg(coords)}, {"x", local.x()}, {"y", local.y()},
        {"right", origin.x() + geometry.width()}, {"bottom", origin.y() + geometry.height()},
        {"screenWidth", geometry.width()}, {"surface", Fluent::Colors::workspacePanel()},
        {"textColor", Fluent::Colors::workspaceText()}, {"border", Fluent::Colors::workspaceLine()},
        {"fontFamily", Fluent::Typography::fontFamily()}};
    emit hintChanged();
}
void ScreenPointPicker::mouseMoveEvent(QMouseEvent* event) { refreshHint(); QDialog::mouseMoveEvent(event); }
void ScreenPointPicker::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) { pickedPos_ = QCursor::pos(); accept(); }
    else if (event->button() == Qt::RightButton) reject();
    else QDialog::mousePressEvent(event);
}
void ScreenPointPicker::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) reject();
    else QDialog::keyPressEvent(event);
}
