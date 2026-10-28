#pragma once
#include <QDialog>
#include <QVariantMap>
#include <optional>
class QQuickWidget;
class ScreenPointPicker : public QDialog {
    Q_OBJECT
    Q_PROPERTY(QVariantMap hintState READ hintState NOTIFY hintChanged)
public:
    explicit ScreenPointPicker(QWidget* parent = nullptr);
    ~ScreenPointPicker() override;
    QPoint pickedPos() const noexcept { return pickedPos_; }
    QVariantMap hintState() const { return hintState_; }
    static std::optional<QPoint> pick(QWidget* host);
signals:
    void hintChanged();
protected:
    void showEvent(QShowEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
private:
    void refreshHint();
    QQuickWidget* quick_ = nullptr;
    QVariantMap hintState_;
    QPoint pickedPos_;
};
