#pragma once

#include <QDialog>
#include <QPoint>

class FluentLineEdit;
class QPushButton;

class ConfigNameDialog : public QDialog {
public:
    ConfigNameDialog(const QString& title,
        const QString& label,
        const QString& initialName = QString(),
        QWidget* parent = nullptr);

    [[nodiscard]] QString nameText() const;

    static bool getName(QWidget* parent,
        const QString& title,
        const QString& label,
        const QString& initialName,
        QString& name);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    FluentLineEdit* nameEdit_ = nullptr;
    QPushButton* okButton_ = nullptr;
    bool dragging_ = false;
    QPoint dragStartGlobal_;
    QPoint dragStartFrame_;
};
