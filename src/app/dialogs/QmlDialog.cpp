#include "dialogs/QmlDialog.h"
#include "dialogs/DialogChrome.h"
#include "ui/styles/QmlTheme.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMoveEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWidget>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QSurfaceFormat>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <cmath>
#include <utility>

class DialogBackdropProvider final : public QQuickImageProvider {
public:
    DialogBackdropProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    void setImage(QImage image) { image_ = std::move(image); }
    QImage requestImage(const QString&, QSize* size, const QSize&) override {
        if (size) *size = image_.size();
        return image_;
    }
private:
    QImage image_;
};

QmlDialog::QmlDialog(QWidget* parent) : QDialog(parent) {
    DialogChrome::apply(this);
    setModal(true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    quick_ = new QQuickWidget(this);
    quick_->setObjectName(QStringLiteral("dialogQuickView"));
    QSurfaceFormat format = quick_->format();
    format.setAlphaBufferSize(8);
    quick_->setFormat(format);
    quick_->setClearColor(Qt::transparent);
    quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quick_->setFocusPolicy(Qt::StrongFocus);
    quick_->rootContext()->setContextProperty(QStringLiteral("appView"), this);
    quick_->rootContext()->setContextProperty(QStringLiteral("dialogModel"), this);
    backdrop_ = new DialogBackdropProvider;
    quick_->engine()->addImageProvider(QStringLiteral("dialogBackdrop"), backdrop_);
    layout->addWidget(quick_);
}

QmlDialog::~QmlDialog() { delete quick_; }
QVariantMap QmlDialog::button(const QString& id, const QString& text, bool primary, bool danger) {
    return {{"id", id}, {"text", text}, {"primary", primary}, {"danger", danger}, {"enabled", true}};
}
QVariantMap QmlDialog::theme() const { return Fluent::qmlTheme(); }
QString QmlDialog::backdropUrl() const { return QStringLiteral("image://dialogBackdrop/%1").arg(backdropRevision_); }

void QmlDialog::present(const QString& page, QVariantMap state, const QSize& size, bool fitContent) {
    state_ = {{"page", page}, {"title", windowTitle()}, {"actions", QVariantList{}},
        {"closeAllowed", true}, {"hasBackdrop", false}};
    for (auto it = state.cbegin(); it != state.cend(); ++it) state_.insert(it.key(), it.value());
    fitContent_ = fitContent;
    resize(size);
    quick_->setSource(QUrl(QStringLiteral("qrc:/qt/qml/XiaoJi/UI/DialogShell.qml")));
    if (quick_->status() != QQuickWidget::Ready) {
        QStringList errors;
        for (const auto& error : quick_->errors()) errors << error.toString();
        qFatal("QML dialog failed: %s", qPrintable(errors.join(QLatin1Char('\n'))));
    }
    connect(quick_->rootObject(), SIGNAL(preferredHeightChanged()), this, SLOT(fitToContent()), Qt::QueuedConnection);
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
void QmlDialog::refreshVisuals() {
    emit themeChanged();
    fitToContent();
    quick_->update();
    if (isVisible()) captureBackdrop();
}

void QmlDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    refreshVisuals();
    quick_->setFocus();
}
void QmlDialog::moveEvent(QMoveEvent* event) { QDialog::moveEvent(event); updateBackdropRegion(); }
void QmlDialog::resizeEvent(QResizeEvent* event) { QDialog::resizeEvent(event); updateBackdropRegion(); }
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
    const QSize limit = currentScreen->availableGeometry().size() - QSize(32, 32);
    setMinimumSize(QSize(320, 180).boundedTo(limit));
    setMaximumSize(limit);
    const QPoint center = frameGeometry().center();
    if (fitContent_) resize(QSize(width(), static_cast<int>(std::ceil(
        quick_->rootObject()->property("preferredHeight").toReal()))).boundedTo(limit));
    else resize(size().boundedTo(limit));
    DialogChrome::clampWithinScreen(this, center);
}

void QmlDialog::captureBackdrop() {
    if (!parentWidget()) return;
    auto* source = parentWidget()->window()->findChild<QQuickWidget*>();
    if (!source || source == quick_ || source->status() != QQuickWidget::Ready) return;
    QImage image = source->grabFramebuffer();
    if (image.isNull()) return;
    backdropSize_ = source->size();
    backdropOrigin_ = source->mapToGlobal(QPoint(0, 0));
    backdrop_->setImage(std::move(image));
    ++backdropRevision_;
    updateBackdropRegion();
    setValue("hasBackdrop", true);
    emit backdropChanged();
}

void QmlDialog::updateBackdropRegion() {
    if (backdropSize_.isEmpty()) return;
    const QPoint offset = mapToGlobal(QPoint(0, 0)) - backdropOrigin_;
    backdropRegion_ = QVector4D(offset.x() / backdropSize_.width(), offset.y() / backdropSize_.height(),
        width() / backdropSize_.width(), height() / backdropSize_.height());
    emit backdropChanged();
}
