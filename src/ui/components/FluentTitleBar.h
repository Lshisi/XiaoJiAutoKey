#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

class FluentTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit FluentTitleBar(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setIcon(const QIcon& icon);

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPushButton* createCaptionButton(const QString& icon, const QString& tooltip);

    QLabel* iconLabel_;
    QLabel* titleLabel_;
    QPushButton* minBtn_;
    QPushButton* maxBtn_;
    QPushButton* closeBtn_;

    QPoint dragPos_;
    bool dragging_ = false;
};
