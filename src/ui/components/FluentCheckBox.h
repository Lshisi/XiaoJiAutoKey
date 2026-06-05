#pragma once

#include <QCheckBox>

class FluentCheckBox : public QCheckBox {
public:
    explicit FluentCheckBox(const QString& text = QString(), QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
};
