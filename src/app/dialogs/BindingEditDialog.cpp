#include "dialogs/BindingEditDialog.h"

#include "dialogs/AppMessageDialog.h"
#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentComboBox.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/components/FluentSwitch.h"
#include "ui/styles/DesignTokens.h"

#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>

BindingEditDialog::BindingEditDialog(std::optional<BindingEditResult> existing, int defaultDelayMs, QWidget* parent)
    : KeyCaptureDialogBase(parent) {
    setPrimaryMouseButtonsAllowed(true);

    setWindowTitle(existing.has_value()
        ? QString::fromWCharArray(L"\u7f16\u8f91\u6309\u952e")
        : QString::fromWCharArray(L"\u6dfb\u52a0\u6309\u952e"));
    setFixedSize(DialogChrome::bindingEditorWidth, DialogChrome::bindingEditorHeight);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 18, 28, 20);
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
    formLayout->setColumnStretch(3, 1);

    auto* keyLabel = new QLabel(QString::fromWCharArray(L"\u6309\u952e:"), this);
    keyLabel->setFont(Fluent::Typography::body());
    keyLabel->setFixedWidth(62);
    auto* keyEdit = new FluentLineEdit(this);
    keyEdit->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236"));
    formLayout->addWidget(keyLabel, 0, 0);
    formLayout->addWidget(keyEdit, 0, 1);
    setCaptureEditor(keyEdit);

    auto* modeLabel = new QLabel(QString::fromWCharArray(L"\u6a21\u5f0f:"), this);
    modeLabel->setFont(Fluent::Typography::body());
    modeLabel->setFixedWidth(62);
    modeCombo_ = new FluentComboBox(this);
    modeCombo_->addItem(QString::fromWCharArray(L"\u5f00\u5173\u6a21\u5f0f"));
    modeCombo_->addItem(QString::fromWCharArray(L"\u6309\u538b\u6a21\u5f0f"));
    modeCombo_->addItem(QString::fromWCharArray(L"\u72ec\u7acb\u6a21\u5f0f"));
    modeCombo_->addItem(QString::fromWCharArray(L"\u5207\u6362\u6a21\u5f0f"));
    formLayout->addWidget(modeLabel, 0, 2);
    formLayout->addWidget(modeCombo_, 0, 3);

    auto* delayLabel = new QLabel(QString::fromWCharArray(L"\u95f4\u9694(ms):"), this);
    delayLabel->setFont(Fluent::Typography::body());
    delayLabel->setFixedWidth(62);
    delayEdit_ = new FluentLineEdit(this);
    formLayout->addWidget(delayLabel, 1, 0);
    formLayout->addWidget(delayEdit_, 1, 1, 1, 3);

    auto* enableContainer = new QWidget(this);
    auto* enableRow = new QHBoxLayout(enableContainer);
    enableRow->setContentsMargins(0, 0, 0, 0);
    auto* enableLabel = new QLabel(QString::fromWCharArray(L"\u542f\u7528\u6b64\u6309\u952e"), this);
    enableLabel->setFont(Fluent::Typography::body());
    enableRow->addWidget(enableLabel);
    enableRow->addStretch();
    enabledSwitch_ = new FluentSwitch(this);
    enableRow->addWidget(enabledSwitch_);
    formLayout->addWidget(enableContainer, 2, 0, 1, 2);

    mixedModeSuppressionRow_ = new QWidget(this);
    auto* suppressionRow = new QHBoxLayout(mixedModeSuppressionRow_);
    suppressionRow->setContentsMargins(0, 0, 0, 0);
    auto* suppressionLabel = new QLabel(QString::fromWCharArray(L"\u53c2\u4e0e\u8fde\u53d1\u4f18\u5148"), mixedModeSuppressionRow_);
    suppressionLabel->setFont(Fluent::Typography::body());
    suppressionLabel->setToolTip(QString::fromWCharArray(L"\u5f00\u542f\u540e\uff0c\u6309\u4f4f\u8be5\u6309\u538b\u952e\u65f6\u4f1a\u6309\u8fde\u53d1\u4f18\u5148\u6682\u505c\u5f00\u5173\u6a21\u5f0f\u8fde\u53d1\u3002"));
    suppressionRow->addWidget(suppressionLabel);
    suppressionRow->addStretch();
    mixedModeSuppressionSwitch_ = new FluentSwitch(mixedModeSuppressionRow_);
    suppressionRow->addWidget(mixedModeSuppressionSwitch_);
    formLayout->addWidget(mixedModeSuppressionRow_, 2, 2, 1, 2);
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
        if (existing->mode == RepeatMode::Hold) {
            modeCombo_->setCurrentIndex(1);
        } else if (existing->mode == RepeatMode::Independent) {
            modeCombo_->setCurrentIndex(2);
        } else if (existing->mode == RepeatMode::Sequence) {
            modeCombo_->setCurrentIndex(3);
        } else {
            modeCombo_->setCurrentIndex(0);
        }
        delayEdit_->setText(QString::number(existing->delayMs));
        enabledSwitch_->setChecked(existing->enabled);
        mixedModeSuppressionSwitch_->setChecked(existing->mode != RepeatMode::Hold || !existing->suppressMixedMode);
    } else {
        modeCombo_->setCurrentIndex(0);
        delayEdit_->setText(QString::number(defaultDelayMs));
        enabledSwitch_->setChecked(true);
        mixedModeSuppressionSwitch_->setChecked(true);
    }
    updateMixedModeSuppressionVisible();

    bool delayClearedOnClick = false;
    connect(delayEdit_, &FluentLineEdit::clicked, this, [this, delayClearedOnClick]() mutable {
        if (delayClearedOnClick) return;
        delayEdit_->clear();
        delayClearedOnClick = true;
    });

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        updateMixedModeSuppressionVisible();
    });
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        if (!capturedKey().has_value() || !capturedKey()->IsValid()) {
            AppMessageDialog::warning(this, QString::fromWCharArray(L"\u53c2\u6570\u65e0\u6548"),
                QString::fromWCharArray(L"\u8bf7\u5148\u5f55\u5236\u4e00\u4e2a\u6309\u952e\u6216\u9f20\u6807\u952e\u3002"));
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

BindingEditResult BindingEditDialog::editResult() const {
    BindingEditResult value;
    value.key = *capturedKey();
    switch (modeCombo_->currentIndex()) {
    case 1:
        value.mode = RepeatMode::Hold;
        break;
    case 2:
        value.mode = RepeatMode::Independent;
        break;
    case 3:
        value.mode = RepeatMode::Sequence;
        break;
    default:
        value.mode = RepeatMode::Toggle;
        break;
    }
    value.delayMs = delayEdit_->text().toInt();
    value.enabled = enabledSwitch_->isChecked();
    value.suppressMixedMode = value.mode == RepeatMode::Hold && !mixedModeSuppressionSwitch_->isChecked();
    return value;
}

void BindingEditDialog::updateMixedModeSuppressionVisible() {
    if (!mixedModeSuppressionRow_ || !mixedModeSuppressionSwitch_) return;
    const bool isHoldMode = modeCombo_ && modeCombo_->currentIndex() == 1;
    mixedModeSuppressionRow_->setVisible(isHoldMode);
}
