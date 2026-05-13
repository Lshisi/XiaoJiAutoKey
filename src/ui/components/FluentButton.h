#pragma once

#include <QPushButton>
#include <QPropertyAnimation>

class FluentButton : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
public:
    enum Style { Primary, Secondary, Danger, Icon };

    explicit FluentButton(const QString& text = QString(), QWidget* parent = nullptr);
    FluentButton(Style style, const QString& text, QWidget* parent = nullptr);

    void setButtonStyle(Style style);
    Style buttonStyle() const { return style_; }

    qreal hoverProgress() const { return hoverProgress_; }
    void setHoverProgress(qreal v);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void setupAnimation();

    Style style_ = Primary;
    qreal hoverProgress_ = 0.0;
    QPropertyAnimation* hoverAnim_ = nullptr;
};
