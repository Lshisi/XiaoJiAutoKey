#include "PluginWindows.h"
#include "PluginPageContext.h"
#include "PluginQml.h"
#include <QEvent>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSurfaceFormat>
#include <algorithm>
#include <cmath>
#include <utility>

namespace Plugins {
PluginWindows::PluginWindows(Manifest manifest, PluginPageContext* api, QQmlEngine* engine, QVariantMap saved, QObject* parent)
    : QObject(parent), manifest_(std::move(manifest)), api_(api), engine_(engine), saved_(std::move(saved)) {
    QVariantMap retained;
    for (const auto& declaration : manifest_.windows) if (saved_.contains(declaration.id)) retained.insert(declaration.id, saved_.value(declaration.id));
    saved_ = retained;
    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(300);
    connect(&saveTimer_, &QTimer::timeout, this, [this] {
        for (const auto& [id, window] : windows_) { constrain(window); remember(id); }
        emit stateSaved(saved_); emit changed();
    });
    for (auto* screen : QGuiApplication::screens()) watchScreen(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, &PluginWindows::watchScreen);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] {
        QTimer::singleShot(0, this, [this] { for (const auto& [id, window] : windows_) { constrain(window); remember(id); } });
    });
}

PluginWindows::~PluginWindows() { closeAll(); }

void PluginWindows::watchScreen(QScreen* screen) {
    const auto refresh = [this] { for (const auto& [id, window] : windows_) { constrain(window); remember(id); } };
    connect(screen, &QScreen::availableGeometryChanged, this, refresh);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, refresh);
}

void PluginWindows::constrain(QQuickWindow* window) {
    auto* screen = QGuiApplication::screenAt(window->geometry().center());
    if (!screen) screen = window->screen();
    if (!screen || !QGuiApplication::screens().contains(screen)) screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const auto area = screen->availableGeometry();
    const auto margins = window->frameMargins();
    const QRect contentArea = area.marginsRemoved(margins);
    const QSize size(std::clamp(window->width(), 32, std::max(32, contentArea.width())),
        std::clamp(window->height(), 32, std::max(32, contentArea.height())));
    const QPoint position(std::clamp(window->x(), contentArea.left(), std::max(contentArea.left(), contentArea.right() - size.width() + 1)),
        std::clamp(window->y(), contentArea.top(), std::max(contentArea.top(), contentArea.bottom() - size.height() + 1)));
    window->setScreen(screen);
    window->setGeometry(QRect(position, size));
}

void PluginWindows::remember(const QString& id) {
    const auto found = windows_.find(id);
    if (found == windows_.end()) return;
    auto* window = found->second;
    const QVariantMap state{{"x", window->x()}, {"y", window->y()}, {"width", window->width()}, {"height", window->height()},
        {"screen", window->screen() ? window->screen()->name() : QString()},
        {"topmost", window->flags().testFlag(Qt::WindowStaysOnTopHint)}, {"clickThrough", window->flags().testFlag(Qt::WindowTransparentForInput)}};
    if (saved_.value(id).toMap() == state) return;
    saved_.insert(id, state);
    if (!closing_) saveTimer_.start();
}

bool PluginWindows::dispatch(const QString& operation, const QString& id, const QVariantMap& options) {
    if (closing_) return false;
    error_.clear();
    const auto declaration = std::find_if(manifest_.windows.cbegin(), manifest_.windows.cend(), [&](const auto& item) { return item.id == id; });
    if (declaration == manifest_.windows.cend()) { error_ = tr("插件未声明此窗口。"); return false; }
    auto found = windows_.find(id);
    if (operation == QStringLiteral("show") && found == windows_.end()) {
        QString error;
        auto* object = createQml(engine_, qmlSource(manifest_, declaration->file),
            {{"pluginApi", QVariant::fromValue(api_)}, {"windowId", id}}, &error);
        auto* item = qobject_cast<QQuickItem*>(object);
        if (!item) { delete object; error_ = error.isEmpty() ? tr("插件窗口内容根对象必须是 Item。"): error; return false; }
        auto* window = new QQuickWindow;
        window->setMinimumSize(QSize(32, 32));
        window->setMaximumSize(QSize(2048, 2048));
        QQmlEngine::setObjectOwnership(item, QQmlEngine::CppOwnership);
        window->setTitle(tr("%1 · %2").arg(manifest_.name, declaration->title));
        const auto restored = saved_.value(id).toMap();
        const bool topmost = restored.value("topmost", declaration->topmost).toBool();
        const bool clickThrough = restored.value("clickThrough", false).toBool();
        Qt::WindowFlags flags = Qt::Tool;
        if (declaration->frameless) flags |= Qt::FramelessWindowHint;
        if (topmost) flags |= Qt::WindowStaysOnTopHint;
        if (clickThrough) flags |= Qt::WindowTransparentForInput;
        if (clickThrough || !declaration->focusable) flags |= Qt::WindowDoesNotAcceptFocus;
        window->setFlags(flags);
        if (declaration->transparent) { auto format = window->format(); format.setAlphaBufferSize(8); window->setFormat(format); }
        window->setColor(declaration->transparent ? QColor(Qt::transparent) : api_->theme().value("surface").value<QColor>());
        if (!declaration->transparent) connect(api_, &PluginPageContext::themeChanged, window, [this, window] {
            window->setColor(api_->theme().value("surface").value<QColor>());
        });
        auto* screen = QGuiApplication::primaryScreen();
        for (auto* candidate : QGuiApplication::screens()) if (candidate->name() == restored.value("screen").toString()) screen = candidate;
        if (screen) window->setScreen(screen);
        const auto area = screen ? screen->availableGeometry() : QRect(0, 0, 800, 600);
        window->resize(std::clamp(restored.value("width", declaration->width).toInt(), 32, 2048),
            std::clamp(restored.value("height", declaration->height).toInt(), 32, 2048));
        window->setPosition(restored.value("x", area.center().x() - window->width() / 2).toInt(),
            restored.value("y", area.center().y() - window->height() / 2).toInt());
        item->setParent(window->contentItem());
        item->setParentItem(window->contentItem());
        item->setSize(window->size());
        const auto resized = [item, window] { item->setSize(window->size()); };
        const auto moved = [this, id] { remember(id); };
        connect(window, &QQuickWindow::widthChanged, this, resized);
        connect(window, &QQuickWindow::heightChanged, this, resized);
        connect(window, &QWindow::xChanged, this, moved);
        connect(window, &QWindow::yChanged, this, moved);
        connect(window, &QWindow::widthChanged, this, moved);
        connect(window, &QWindow::heightChanged, this, moved);
        connect(window, &QWindow::visibleChanged, this, [this] { emit changed(); });
        connect(window, &QWindow::screenChanged, this, [this, window, id] { constrain(window); remember(id); });
        window->installEventFilter(this);
        windows_.emplace(id, window);
        constrain(window);
        found = windows_.find(id);
    }
    if (operation == QStringLiteral("close")) { close(id); return true; }
    if (found == windows_.end()) { error_ = tr("请先打开插件窗口。"); return false; }
    auto* window = found->second;
    if (operation == QStringLiteral("show")) { window->show(); constrain(window); window->raise(); }
    else if (operation == QStringLiteral("hide")) window->hide();
    else if (operation == QStringLiteral("move")) return window->startSystemMove();
    else if (operation == QStringLiteral("configure")) {
        for (auto it = options.cbegin(); it != options.cend(); ++it) {
            if (it.key() == QStringLiteral("topmost") || it.key() == QStringLiteral("clickThrough") || it.key() == QStringLiteral("resetPosition")) {
                if (it.value().metaType().id() != QMetaType::Bool) { error_ = tr("窗口开关必须是布尔值。"); return false; }
            } else if (it.key() == QStringLiteral("x") || it.key() == QStringLiteral("y") || it.key() == QStringLiteral("width") || it.key() == QStringLiteral("height")) {
                bool ok = false;
                const double number = it.value().toDouble(&ok);
                if (!ok || !std::isfinite(number) || std::abs(number) > 100000 || std::floor(number) != number
                    || ((it.key() == QStringLiteral("width") || it.key() == QStringLiteral("height")) && (number < 32 || number > 2048))) {
                    error_ = tr("窗口位置或尺寸无效。"); return false;
                }
            } else { error_ = tr("不支持此窗口选项。"); return false; }
        }
        const bool visible = window->isVisible();
        auto flags = window->flags();
        if (options.contains("topmost")) flags.setFlag(Qt::WindowStaysOnTopHint, options.value("topmost").toBool());
        if (options.contains("clickThrough")) {
            flags.setFlag(Qt::WindowTransparentForInput, options.value("clickThrough").toBool());
            flags.setFlag(Qt::WindowDoesNotAcceptFocus, options.value("clickThrough").toBool() || !declaration->focusable);
        }
        if (flags != window->flags()) window->setFlags(flags);
        window->setGeometry(options.value("x", window->x()).toInt(), options.value("y", window->y()).toInt(),
            options.value("width", window->width()).toInt(), options.value("height", window->height()).toInt());
        if (options.value("resetPosition").toBool()) {
            if (auto* screen = QGuiApplication::primaryScreen()) {
                window->setScreen(screen);
                const auto area = screen->availableGeometry();
                window->setPosition(area.right() - window->width() - 23, area.bottom() - window->height() - 23);
            }
        }
        constrain(window);
        if (visible) window->show();
    } else { error_ = tr("不支持此窗口操作。"); return false; }
    remember(id);
    emit changed();
    return true;
}

bool PluginWindows::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Close) {
        for (const auto& [id, window] : windows_) if (window == watched) {
            QTimer::singleShot(0, this, [this, id] { close(id); });
            event->ignore();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void PluginWindows::close(const QString& id) {
    const auto found = windows_.find(id);
    if (found == windows_.end()) return;
    remember(id);
    auto* window = found->second;
    windows_.erase(found);
    window->removeEventFilter(this);
    window->disconnect(this);
    delete window;
    if (!closing_) emit changed();
}

void PluginWindows::closeAll() {
    closing_ = true;
    while (!windows_.empty()) close(windows_.begin()->first);
    saveTimer_.stop();
    emit stateSaved(saved_);
    closing_ = false;
    emit changed();
}

QVariantList PluginWindows::presentation() const {
    QVariantList rows;
    for (const auto& declaration : manifest_.windows) {
        const auto found = windows_.find(declaration.id);
        const auto* window = found == windows_.end() ? nullptr : found->second;
        rows.append(QVariantMap{{"id", declaration.id}, {"title", declaration.title}, {"open", window != nullptr},
            {"visible", window && window->isVisible()}, {"clickThrough", window && window->flags().testFlag(Qt::WindowTransparentForInput)},
            {"topmost", window ? window->flags().testFlag(Qt::WindowStaysOnTopHint) : declaration.topmost},
            {"focusable", declaration.focusable}, {"width", window ? window->width() : declaration.width},
            {"height", window ? window->height() : declaration.height}});
    }
    return rows;
}
}
