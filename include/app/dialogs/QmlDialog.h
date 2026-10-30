#pragma once

#include <QDialog>
#include <QVariantMap>

class QQuickWidget;

class QmlDialog : public QDialog {
    Q_OBJECT
    Q_PROPERTY(QVariantMap dialogState READ dialogState NOTIFY dialogStateChanged)
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)

public:
    explicit QmlDialog(QWidget* parent = nullptr);
    ~QmlDialog() override;
    QVariantMap dialogState() const { return state_; }
    QVariantMap theme() const;
    void refreshVisuals();
    Q_INVOKABLE void dispatch(const QString& action, const QVariant& value = {});
    Q_INVOKABLE void edit(const QString& field, const QVariant& value);
    Q_INVOKABLE void startWindowMove();

public slots:
    void reject() override;

signals:
    void dialogStateChanged();
    void themeChanged();

protected:
    static QVariantMap button(const QString& id, const QString& text, bool primary = true, bool danger = false);
    void present(const QString& page, QVariantMap state, const QSize& size, bool fitContent = false, bool fitWidth = false);
    void setValue(const QString& key, const QVariant& value);
    QVariant value(const QString& key) const { return state_.value(key); }
    virtual void handleAction(const QString& action, const QVariant& value);
    virtual void editField(const QString& field, const QVariant& value);
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;

private slots:
    void fitToContent();

private:
    QQuickWidget* quick_ = nullptr;
    QVariantMap state_;
    bool fitContent_ = false;
    bool fitWidth_ = false;
};
