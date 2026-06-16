#pragma once

#include <QDialog>
#include <QPoint>
#include <QString>

class AppMessageDialog : public QDialog {
    Q_OBJECT

public:
    enum class Kind {
        Warning,
        Information,
    };

    AppMessageDialog(Kind kind,
        const QString& title,
        const QString& message,
        const QString& primaryButtonText = QString::fromWCharArray(L"确定"),
        const QString& secondaryButtonText = QString(),
        QWidget* parent = nullptr);

    static void warning(QWidget* parent, const QString& title, const QString& message);
    static void information(QWidget* parent, const QString& title, const QString& message);
    static bool informationWithActions(QWidget* parent,
        const QString& title,
        const QString& message,
        const QString& primaryButtonText,
        const QString& secondaryButtonText);

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
