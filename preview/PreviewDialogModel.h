#pragma once

#include <QObject>
#include <QVariantMap>
#include <functional>

class PreviewDialogModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap dialogState READ dialogState NOTIFY dialogStateChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
public:
    explicit PreviewDialogModel(QObject* parent = nullptr);
    QVariantMap dialogState() const { return state_; }
    bool visible() const { return visible_; }
    void present(const QString& page, const QString& title, QVariantMap fields,
        std::function<void(const QVariantMap&)> accepted = {}, int width = 440, bool danger = false);
    void message(const QString& title, const QString& text, std::function<void()> accepted = {});
    Q_INVOKABLE void edit(const QString& field, const QVariant& value);
    Q_INVOKABLE void dispatch(const QString& action, const QVariant& argument = {});
    Q_INVOKABLE void startWindowMove();

signals:
    void dialogStateChanged();
    void visibleChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateBatchEditActions();
    QVariantMap state_;
    std::function<void(const QVariantMap&)> accepted_;
    bool visible_ = false;
    bool recording_ = false;
};
