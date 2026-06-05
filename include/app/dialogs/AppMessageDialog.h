#pragma once

#include <QDialog>
#include <QPoint>

class AppMessageDialog : public QDialog {
    Q_OBJECT

public:
    enum class Kind {
        Warning,
        Information,
    };

    AppMessageDialog(Kind kind, const QString& title, const QString& message, QWidget* parent = nullptr);

    static void warning(QWidget* parent, const QString& title, const QString& message);
    static void information(QWidget* parent, const QString& title, const QString& message);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
