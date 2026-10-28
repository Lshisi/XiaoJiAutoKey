#pragma once

#include <QDialog>
#include <QVariantMap>
#include <QVector4D>

class QQuickWidget;
class DialogBackdropProvider;

class QmlDialog : public QDialog {
    Q_OBJECT
    Q_PROPERTY(QVariantMap dialogState READ dialogState NOTIFY dialogStateChanged)
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QVector4D backdropRegion READ backdropRegion NOTIFY backdropChanged)
    Q_PROPERTY(QString backdropUrl READ backdropUrl NOTIFY backdropChanged)

public:
    explicit QmlDialog(QWidget* parent = nullptr);
    ~QmlDialog() override;
    QVariantMap dialogState() const { return state_; }
    QVariantMap theme() const;
    QVector4D backdropRegion() const { return backdropRegion_; }
    QString backdropUrl() const;
    void refreshVisuals();
    Q_INVOKABLE void dispatch(const QString& action, const QVariant& value = {});
    Q_INVOKABLE void edit(const QString& field, const QVariant& value);
    Q_INVOKABLE void startWindowMove();

public slots:
    void reject() override;

signals:
    void dialogStateChanged();
    void themeChanged();
    void backdropChanged();

protected:
    static QVariantMap button(const QString& id, const QString& text, bool primary = true, bool danger = false);
    void present(const QString& page, QVariantMap state, const QSize& size, bool fitContent = false);
    void setValue(const QString& key, const QVariant& value);
    QVariant value(const QString& key) const { return state_.value(key); }
    virtual void handleAction(const QString& action, const QVariant& value);
    virtual void editField(const QString& field, const QVariant& value);
    void showEvent(QShowEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;

private slots:
    void fitToContent();

private:
    void captureBackdrop();
    void updateBackdropRegion();
    QQuickWidget* quick_ = nullptr;
    DialogBackdropProvider* backdrop_ = nullptr;
    QVariantMap state_;
    QSizeF backdropSize_;
    QPoint backdropOrigin_;
    QVector4D backdropRegion_{0, 0, 1, 1};
    int backdropRevision_ = 0;
    bool fitContent_ = false;
};
