#include "dialogs/QmlDialog.h"
#include "dialogs/DialogChrome.h"
#include "ui/styles/QmlTheme.h"
#include "ui/components/QmlIconProvider.h"

#include <QCloseEvent>
#include <QKeyEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScreen>
#include <QShowEvent>
#include <QSurfaceFormat>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <cmath>

QmlDialog::QmlDialog(QWidget* parent) : QDialog(parent) {
    DialogChrome::apply(this);
    setModal(true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    quick_ = new QQuickWidget(this);
    quick_->engine()->addImageProvider(QStringLiteral("app-icons"), new QmlIconProvider);
    quick_->setObjectName(QStringLiteral("dialogQuickView"));
    QSurfaceFormat format = quick_->format();
    format.setAlphaBufferSize(8);
    quick_->setFormat(format);
    quick_->setClearColor(Qt::transparent);
    quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quick_->setFocusPolicy(Qt::StrongFocus);
    quick_->rootContext()->setContextProperty(QStringLiteral("appView"), this);
    quick_->rootContext()->setContextProperty(QStringLiteral("dialogModel"), this);
    layout->addWidget(quick_);
}

QmlDialog::~QmlDialog() { delete quick_; }
QVariantMap QmlDialog::button(const QString& id, const QString& text, bool primary, bool danger) {
    return {{"id", id}, {"text", text}, {"primary", primary}, {"danger", danger}, {"enabled", true}};
}
QVariantMap QmlDialog::theme() const { return Fluent::qmlTheme(); }

void QmlDialog::present(const QString& page, QVariantMap state, const QSize& size, bool fitContent, bool fitWidth) {
    state_ = {{"page", page}, {"title", windowTitle()}, {"actions", QVariantList{}},
        {"closeAllowed", true}};
    for (auto it = state.cbegin(); it != state.cend(); ++it) state_.insert(it.key(), it.value());
    fitContent_ = fitContent;
    fitWidth_ = fitWidth;
    resize(size);
    quick_->setSource(QUrl(QStringLiteral("qrc:/qt/qml/XiaoJi/UI/DialogShell.qml")));
    if (quick_->status() != QQuickWidget::Ready) {
        QStringList errors;
        for (const auto& error : quick_->errors()) errors << error.toString();
        qFatal("QML dialog failed: %s", qPrintable(errors.join(QLatin1Char('\n'))));
    }
    connect(quick_->rootObject(), SIGNAL(preferredHeightChanged()), this, SLOT(fitToContent()), Qt::QueuedConnection);
    if (fitWidth_)
        connect(quick_->rootObject(), SIGNAL(preferredWidthChanged()), this, SLOT(fitToContent()), Qt::QueuedConnection);
}

void QmlDialog::setValue(const QString& key, const QVariant& value) {
    if (state_.value(key) == value) return;
    state_.insert(key, value);
    emit dialogStateChanged();
}

void QmlDialog::dispatch(const QString& action, const QVariant& value) { handleAction(action, value); }
void QmlDialog::edit(const QString& field, const QVariant& value) { editField(field, value); }
void QmlDialog::editField(const QString&, const QVariant&) {}
void QmlDialog::handleAction(const QString& action, const QVariant&) {
    if (action == QStringLiteral("cancel")) reject();
}
void QmlDialog::reject() { if (value("closeAllowed").toBool()) QDialog::reject(); }
void QmlDialog::startWindowMove() { if (windowHandle()) windowHandle()->startSystemMove(); }
void QmlDialog::startWindowResize(int edges) {
    if (value("resizable").toBool() && windowHandle()) windowHandle()->startSystemResize(Qt::Edges(edges));
}
void QmlDialog::refreshVisuals() {
    emit themeChanged();
    fitToContent();
    quick_->update();
}

void QmlDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    refreshVisuals();
    quick_->setFocus();
}
void QmlDialog::closeEvent(QCloseEvent* event) {
    if (!value("closeAllowed").toBool()) event->ignore();
    else QDialog::closeEvent(event);
}
void QmlDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && !value("closeAllowed").toBool()) event->accept();
    else QDialog::keyPressEvent(event);
}
bool QmlDialog::event(QEvent* event) {
    if (event->type() == QEvent::ScreenChangeInternal || event->type() == QEvent::DevicePixelRatioChange)
        QTimer::singleShot(0, this, [this] { refreshVisuals(); });
    return QDialog::event(event);
}

void QmlDialog::fitToContent() {
    const auto* currentScreen = screen();
    if (!currentScreen || !quick_->rootObject()) return;
    QSize limit = currentScreen->availableGeometry().size() - QSize(32, 32);
    if (fitWidth_) {
        limit.setWidth(qMin(limit.width(), 640));
        if (auto* parent = parentWidget(); parent && parent->window()->isVisible())
            limit = limit.boundedTo(parent->window()->size() - QSize(32, 32));
    }
    setMinimumSize(QSize(320, 180).boundedTo(limit));
    setMaximumSize(limit);
    const QPoint center = frameGeometry().center();
    if (fitContent_) {
        const int preferredWidth = fitWidth_ ? qBound(minimumWidth(), static_cast<int>(std::ceil(
            quick_->rootObject()->property("preferredWidth").toReal())), maximumWidth()) : width();
        resize(QSize(preferredWidth, static_cast<int>(std::ceil(
            quick_->rootObject()->property("preferredHeight").toReal()))).boundedTo(limit));
    } else resize(size().boundedTo(limit));
    DialogChrome::clampWithinScreen(this, center);
}
