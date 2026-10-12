#pragma once

#include <QWidget>

class QScrollArea;
class QLabel;

class AboutPage : public QWidget {
    Q_OBJECT
public:
    explicit AboutPage(QWidget* parent = nullptr);

    void updateThemeColors();
    void refreshIcon();

protected:
    void showEvent(QShowEvent* event) override;

private:
    QScrollArea* scrollArea_ = nullptr;
    QLabel* iconLabel_ = nullptr;
};
