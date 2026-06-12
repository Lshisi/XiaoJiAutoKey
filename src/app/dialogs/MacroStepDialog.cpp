#include "dialogs/MacroStepDialog.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MacroStepDialog::MacroStepDialog(std::optional<MacroStepEditResult> existing, QWidget* parent)
    : KeyCaptureDialogBase(parent) {
    setWindowTitle(existing.has_value()
        ? QString::fromWCharArray(L"\u7f16\u8f91\u6b65\u9aa4")
        : QString::fromWCharArray(L"\u6dfb\u52a0\u6309\u952e"));
    setFixedSize(360, 218);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 18, 24, 16);
    layout->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* titleLabel = new QLabel(windowTitle(), this);
    titleLabel->setFont(Fluent::Typography::cardTitle());
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(DialogChrome::createCloseButton(this));
    layout->addLayout(titleRow);

    auto* keyRow = new QHBoxLayout();
    auto* keyLabel = new QLabel(QString::fromWCharArray(L"\u6309\u952e:"), this);
    keyLabel->setFont(Fluent::Typography::body());
    keyLabel->setFixedWidth(70);
    keyRow->addWidget(keyLabel);
    auto* keyEdit = new FluentLineEdit(this);
    keyEdit->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
    keyRow->addWidget(keyEdit);
    layout->addLayout(keyRow);
    setCaptureEditor(keyEdit);

    auto* delayRow = new QHBoxLayout();
    auto* delayLabel = new QLabel(QString::fromWCharArray(L"\u95f4\u9694(ms):"), this);
    delayLabel->setFont(Fluent::Typography::body());
    delayLabel->setFixedWidth(70);
    delayRow->addWidget(delayLabel);
    delayEdit_ = new FluentLineEdit(this);
    delayRow->addWidget(delayEdit_);
    layout->addLayout(delayRow);

    layout->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u53d6\u6d88"), this);
    cancelBtn->setFixedWidth(80);
    auto* okBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u786e\u5b9a"), this);
    okBtn->setFixedWidth(80);
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(8);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    if (existing.has_value()) {
        setCapturedKey(existing->key);
        delayEdit_->setText(QString::number(existing->delayAfterMs));
    } else {
        delayEdit_->setText("50");
    }

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        if (!capturedKey().has_value() || !capturedKey()->IsValid()) {
            AppMessageDialog::warning(this, QString::fromWCharArray(L"\u53c2\u6570\u65e0\u6548"),
                QString::fromWCharArray(L"\u8bf7\u5148\u5f55\u5236\u4e00\u4e2a\u6309\u952e\u3002"));
            return;
        }
        bool ok = false;
        int delay = delayEdit_->text().toInt(&ok);
        if (!ok || delay <= 0) {
            delayEdit_->setText("50");
        }
        accept();
    });
}

MacroStepEditResult MacroStepDialog::editResult() const {
    MacroStepEditResult value;
    value.key = *capturedKey();
    bool ok = false;
    value.delayAfterMs = delayEdit_->text().toInt(&ok);
    if (!ok || value.delayAfterMs <= 0) value.delayAfterMs = 50;
    return value;
}
