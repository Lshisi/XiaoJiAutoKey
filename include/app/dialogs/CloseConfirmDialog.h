#pragma once
#include "dialogs/QmlDialog.h"
struct CloseConfirmResult { bool exit = false; bool remember = false; };
class CloseConfirmDialog : public QmlDialog {
    Q_OBJECT
public:
    explicit CloseConfirmDialog(QWidget* parent = nullptr);
    CloseConfirmResult decision() const;
protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;
private:
    bool exitSelected_ = false;
};
