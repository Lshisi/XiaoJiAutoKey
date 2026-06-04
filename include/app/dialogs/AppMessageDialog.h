#pragma once

#include <QDialog>

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
};
