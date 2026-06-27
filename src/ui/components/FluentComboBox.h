#pragma once

#include <QComboBox>

class QWidget;

class FluentComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit FluentComboBox(QWidget* parent = nullptr);
    void refreshThemeStyle();
    void showPopup() override;
    void hidePopup() override;
    void acceptPopupIndex(int row);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    QWidget* popupWidget_ = nullptr;
};
