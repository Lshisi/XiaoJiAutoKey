#pragma once

#include <QDialog>

struct CloseConfirmResult {
    bool exit = false;
    bool remember = false;
};

class CloseConfirmDialog : public QDialog {
    Q_OBJECT

public:
    explicit CloseConfirmDialog(QWidget* parent = nullptr);

    [[nodiscard]] CloseConfirmResult decision() const;

private:
    bool exitSelected_ = false;
    bool rememberSelected_ = false;
};
