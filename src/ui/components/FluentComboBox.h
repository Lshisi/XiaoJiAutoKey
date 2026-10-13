#pragma once

#include <QComboBox>

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QWidget;

class FluentComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit FluentComboBox(QWidget* parent = nullptr);
    void refreshThemeStyle();

    // 开启后可直接在框内打字过滤条目（模糊匹配），Esc 或选中后恢复。
    void setSearchable(bool searchable);
    bool searchable() const { return searchable_; }
    void appendSearchText(const QString& text);
    void backspaceSearchText();
    void clearSearchText();
    bool hasSearchText() const { return !filterText_.isEmpty(); }

    void showPopup() override;
    void hidePopup() override;
    void acceptPopupIndex(int row);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updatePopupItems();

    QWidget* popupWidget_ = nullptr;
    QString filterText_;
    bool searchable_ = false;
};
