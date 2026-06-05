#pragma once

#include <QTableWidget>
#include <QHeaderView>

class FluentTable : public QTableWidget {
    Q_OBJECT
public:
    explicit FluentTable(QWidget* parent = nullptr);
    FluentTable(int rows, int cols, QWidget* parent = nullptr);
    void refreshThemeStyle();

private:
    void init();
};
