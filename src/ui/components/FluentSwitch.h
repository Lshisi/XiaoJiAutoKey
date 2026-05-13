#pragma once

#include <QWidget>
#include <QPropertyAnimation>

class FluentSwitch : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition)
public:
    explicit FluentSwitch(QWidget* parent = nullptr);

    bool isChecked() const { return checked_; }
    void setChecked(bool checked);

    qreal position() const { return position_; }
    void setPosition(qreal pos);

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool checked_ = false;
    bool hovered_ = false;
    qreal position_ = 0.0;
    QPropertyAnimation* anim_;
};
