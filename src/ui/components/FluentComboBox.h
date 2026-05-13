#pragma once

#include <QComboBox>

class FluentComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit FluentComboBox(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};
