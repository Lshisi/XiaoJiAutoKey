#pragma once

#include <QLineEdit>
#include <QPropertyAnimation>

class FluentLineEdit : public QLineEdit {
    Q_OBJECT
    Q_PROPERTY(qreal focusProgress READ focusProgress WRITE setFocusProgress)
public:
    explicit FluentLineEdit(QWidget* parent = nullptr);
    FluentLineEdit(const QString& text, QWidget* parent = nullptr);

    qreal focusProgress() const { return focusProgress_; }
    void setFocusProgress(qreal v);
    void refreshThemeStyle();
    // 录制框等由程序管理内容的输入框：抑制右键编辑菜单。
    void setCaptureOnly(bool captureOnly) { captureOnly_ = captureOnly; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void init();
    qreal focusProgress_ = 0.0;
    bool captureOnly_ = false;
    QPropertyAnimation* focusAnim_;
};
