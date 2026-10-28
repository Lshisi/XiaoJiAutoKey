#pragma once

#include <QObject>
#include <QVariantMap>

class PageModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)

public:
    explicit PageModel(QObject* parent = nullptr) : QObject(parent) {}
    QVariantMap state() const { return state_; }

signals:
    void stateChanged();

protected:
    void setValue(const QString& name, const QVariant& value) {
        state_.insert(name, value);
        emit stateChanged();
    }
    QVariantMap state_;
};
