#pragma once

#include <QComboBox>

class FluentComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit FluentComboBox(QWidget* parent = nullptr);
    void refreshThemeStyle();

protected:
    void paintEvent(QPaintEvent* event) override;
};
