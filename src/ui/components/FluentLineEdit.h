#pragma once

#include <QLineEdit>
#include <QPropertyAnimation>

class FluentLineEdit : public QLineEdit {
    Q_OBJECT
    Q_PROPERTY(qreal focusProgress READ focusProgress WRITE setFocusProgress)
public:
    explicit FluentLineEdit(QWidget* parent = nullptr);
    FluentLineEdit(const QString& text, QWidget* parent = nullptr);

    qreal focusProgress() const { return focusProgress_; }
    void setFocusProgress(qreal v);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void init();
    qreal focusProgress_ = 0.0;
    QPropertyAnimation* focusAnim_;
};
