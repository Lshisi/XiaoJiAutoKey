#pragma once

#include <QWidget>

class QScrollArea;

class AboutPage : public QWidget {
    Q_OBJECT
public:
    explicit AboutPage(QWidget* parent = nullptr);

    void updateThemeColors();

private:
    QScrollArea* scrollArea_ = nullptr;
};
