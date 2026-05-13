#pragma once

#include <QWidget>
#include <QLabel>

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget* parent = nullptr);

    void updateStatus(bool running, const QString& hotkey, int bindingCount);

private:
    QLabel* statusDot_;
    QLabel* statusText_;
    QLabel* hotkeyValue_;
    QLabel* bindingCountValue_;
};
