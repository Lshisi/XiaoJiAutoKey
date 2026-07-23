#include "dialogs/MacroStepDialog.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MacroStepDialog::MacroStepDialog(std::optional<MacroStepEditResult> existing, QWidget* parent)
    : KeyCaptureDialogBase(parent) {
    setPrimaryMouseButtonsAllowed(true);

    setWindowTitle(existing.has_value()
        ? QString::fromWCharArray(L"\u7f16\u8f91\u6b65\u9aa4")
        : QString::fromWCharArray(L"\u6dfb\u52a0\u6309\u952e"));
    setFixedSize(DialogChrome::macroStepEditorWidth, DialogChrome::macroStepEditorHeight);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->setSpacing(14);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* titleLabel = new QLabel(windowTitle(), this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(DialogChrome::createCloseButton(this));
    layout->addLayout(titleRow);

    auto* formLayout = new QGridLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(12);
    formLayout->setVerticalSpacing(14);
    formLayout->setColumnStretch(1, 1);

    auto* keyLabel = new QLabel(QString::fromWCharArray(L"\u6309\u952e:"), this);
    keyLabel->setFont(Fluent::Typography::body());
    keyLabel->setFixedWidth(62);
    auto* keyEdit = new FluentLineEdit(this);
    keyEdit->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236"));
    formLayout->addWidget(keyLabel, 0, 0);
    formLayout->addWidget(keyEdit, 0, 1);
    setCaptureEditor(keyEdit);

    auto* delayLabel = new QLabel(QString::fromWCharArray(L"\u95f4\u9694(ms):"), this);
    delayLabel->setFont(Fluent::Typography::body());
    delayLabel->setFixedWidth(62);
    delayEdit_ = new FluentLineEdit(this);
    formLayout->addWidget(delayLabel, 1, 0);
    formLayout->addWidget(delayEdit_, 1, 1);
    layout->addLayout(formLayout);

    layout->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), this);
    cancelBtn->setFixedWidth(88);
    auto* okBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u786e\u5b9a"), this);
    okBtn->setFixedWidth(88);
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(8);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    if (existing.has_value()) {
        setCapturedKey(existing->key);
        delayEdit_->setText(QString::number(existing->delayAfterMs));
    } else {
        delayEdit_->setText(QString::number(kDefaultDelayMs));
    }

    bool delayClearedOnClick = false;
    connect(delayEdit_, &FluentLineEdit::clicked, this, [this, delayClearedOnClick]() mutable {
        if (delayClearedOnClick) return;
        delayEdit_->clear();
        delayClearedOnClick = true;
    });

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        if (!capturedKey().has_value() || !capturedKey()->IsValid()) {
            AppMessageDialog::warning(this, QString::fromWCharArray(L"\u53c2\u6570\u65e0\u6548"),
                QString::fromWCharArray(L"\u8bf7\u5148\u5f55\u5236\u4e00\u4e2a\u6309\u952e\u3002"));
            return;
        }
        bool ok = false;
        int delay = delayEdit_->text().toInt(&ok);
        if (!ok || delay < kMinDelayMs) {
            AppMessageDialog::warning(this, QString::fromWCharArray(L"\u53c2\u6570\u65e0\u6548"),
                QString::fromWCharArray(L"\u95f4\u9694\u5fc5\u987b\u662f\u4e0d\u5c0f\u4e8e 1 \u7684\u6574\u6570\u3002"));
            return;
        }
        accept();
    });
}

MacroStepEditResult MacroStepDialog::editResult() const {
    MacroStepEditResult value;
    value.key = *capturedKey();
    bool ok = false;
    value.delayAfterMs = delayEdit_->text().toInt(&ok);
    if (!ok || value.delayAfterMs < kMinDelayMs) value.delayAfterMs = kDefaultDelayMs;
    return value;
}
