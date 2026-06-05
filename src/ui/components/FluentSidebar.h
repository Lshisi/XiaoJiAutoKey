#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QVector>

class FluentNavButton : public QPushButton {
    Q_OBJECT
public:
    FluentNavButton(const QString& icon, const QString& label, QWidget* parent = nullptr);
    void setActive(bool active);
    bool isActive() const { return active_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString icon_;
    QString label_;
    bool active_ = false;
    bool hovered_ = false;
};

class FluentSidebar : public QWidget {
    Q_OBJECT
public:
    explicit FluentSidebar(QWidget* parent = nullptr);

    void addNavItem(const QString& icon, const QString& label, const QString& pageId);
    void setCurrentPage(const QString& pageId);
    void updateFactionIcon();

signals:
    void pageSelected(const QString& pageId);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVBoxLayout* navLayout_;
    QWidget* factionHeader_ = nullptr;
    QLabel* factionIconLabel_ = nullptr;
    QLabel* factionNameLabel_ = nullptr;
    QVector<QPair<FluentNavButton*, QString>> items_;
};
