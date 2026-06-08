#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

class FluentCard : public QWidget {
    Q_OBJECT
public:
    explicit FluentCard(QWidget* parent = nullptr);
    FluentCard(const QString& title, QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void refreshThemeStyle();
    QVBoxLayout* contentLayout() { return contentLayout_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void init();
    QLabel* titleLabel_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
};
