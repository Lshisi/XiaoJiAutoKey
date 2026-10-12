#include "dialogs/BindingEditDialog.h"

#include "ClickPositioning.h"
#include "dialogs/AppMessageDialog.h"
#include "dialogs/DialogChrome.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentComboBox.h"
#include "ui/components/FluentLineEdit.h"
#include "ui/components/FluentSwitch.h"
#include "ui/styles/DesignTokens.h"

#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QRadioButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kDialogBaseHeight = 250;
constexpr int kClickGroupExtraHeight = 168;

QString AnchorText(double x, double y, bool relativeToGame) {
    return QString::fromWCharArray(relativeToGame
            ? L"\u5df2\u62fe\u53d6: \u6e38\u620f\u7a97\u53e3\u5185 (%1%, %2%)"
            : L"\u5df2\u62fe\u53d6: \u4e3b\u5c4f\u5e55 (%1%, %2%)")
        .arg(QString::number(x * 100.0, 'f', 1), QString::number(y * 100.0, 'f', 1));
}

}  // namespace

BindingEditDialog::BindingEditDialog(std::optional<BindingEditResult> existing, int defaultDelayMs,
    GameWindowProvider* gameWindows, std::function<bool()> driverSupportsMouseMove, QWidget* parent)
    : KeyCaptureDialogBase(parent),
      gameWindows_(gameWindows),
      driverSupportsMouseMove_(std::move(driverSupportsMouseMove)) {
    setPrimaryMouseButtonsAllowed(true);

    setWindowTitle(existing.has_value()
        ? QString::fromWCharArray(L"\u7f16\u8f91\u6309\u952e")
        : QString::fromWCharArray(L"\u6dfb\u52a0\u6309\u952e"));
    setFixedSize(DialogChrome::bindingEditorWidth, kDialogBaseHeight);

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
    auto* suppressionLabel = new QLabel(QString::fromWCharArray(L"\u53c2\u4e0e\u6309\u538b\u4f18\u5148"), mixedModeSuppressionRow_);
    suppressionLabel->setFont(Fluent::Typography::body());
    suppressionRow->addWidget(suppressionLabel);
    suppressionRow->addStretch();
    mixedModeSuppressionSwitch_ = new FluentSwitch(mixedModeSuppressionRow_);
    suppressionRow->addWidget(mixedModeSuppressionSwitch_);
    formLayout->addWidget(mixedModeSuppressionRow_, 2, 2, 1, 2);
    layout->addLayout(formLayout);

    // ── 点击位置（仅鼠标键显示）──
    clickPosGroup_ = new QWidget(this);
    auto* groupLayout = new QVBoxLayout(clickPosGroup_);
    groupLayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->setSpacing(6);

    auto* groupTitle = new QLabel(QString::fromWCharArray(L"\u70b9\u51fb\u4f4d\u7f6e"), clickPosGroup_);
    groupTitle->setFont(Fluent::Typography::caption());
    QPalette groupTitlePalette = groupTitle->palette();
    groupTitlePalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    groupTitle->setPalette(groupTitlePalette);
    groupLayout->addWidget(groupTitle);

    clickFollowRadio_ = new QRadioButton(QString::fromWCharArray(L"\u8ddf\u968f\u9f20\u6807\u5f53\u524d\u4f4d\u7f6e"), clickPosGroup_);
    clickFollowRadio_->setFont(Fluent::Typography::body());
    clickFollowRadio_->setFixedHeight(22);
    clickFollowRadio_->setChecked(true);
    groupLayout->addWidget(clickFollowRadio_);

    auto* fixedRow = new QHBoxLayout();
    fixedRow->setContentsMargins(0, 0, 0, 0);
    fixedRow->setSpacing(8);
    clickFixedRadio_ = new QRadioButton(QString::fromWCharArray(L"\u56fa\u5b9a\u70b9\u4f4d"), clickPosGroup_);
    clickFixedRadio_->setFont(Fluent::Typography::body());
    clickFixedRadio_->setFixedHeight(22);
    fixedRow->addWidget(clickFixedRadio_);
    pickPosBtn_ = new FluentButton(FluentButton::Secondary,
        QString::fromWCharArray(L"\u62fe\u53d6\u5750\u6807"), clickPosGroup_);
    pickPosBtn_->setFixedHeight(24);
    fixedRow->addWidget(pickPosBtn_);
    fixedRow->addStretch();
    groupLayout->addLayout(fixedRow);

    pickPosLabel_ = new QLabel(QString::fromWCharArray(L"\u672a\u62fe\u53d6"), clickPosGroup_);
    pickPosLabel_->setFont(Fluent::Typography::caption());
    QPalette pickPalette = pickPosLabel_->palette();
    pickPalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    pickPosLabel_->setPalette(pickPalette);
    groupLayout->addWidget(pickPosLabel_);

    clickPosHintLabel_ = new QLabel(clickPosGroup_);
    clickPosHintLabel_->setFont(Fluent::Typography::caption());
    clickPosHintLabel_->setWordWrap(true);
    QPalette hintPalette = clickPosHintLabel_->palette();
    hintPalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    clickPosHintLabel_->setPalette(hintPalette);
    groupLayout->addWidget(clickPosHintLabel_);
    layout->addWidget(clickPosGroup_);

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
        clickPos_ = existing->clickPos;
    } else {
        modeCombo_->setCurrentIndex(0);
        delayEdit_->setText(QString::number(defaultDelayMs));
        enabledSwitch_->setChecked(true);
        mixedModeSuppressionSwitch_->setChecked(true);
    }
    updateMixedModeSuppressionVisible();
    updateClickPosSectionVisible();

    pickTimer_ = new QTimer(this);
    pickTimer_->setInterval(1000);
    connect(pickTimer_, &QTimer::timeout, this, &BindingEditDialog::advancePickCountdown);

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
    connect(this, &KeyCaptureDialogBase::keyAccepted, this, [this]() {
        updateClickPosSectionVisible();
    });
    connect(pickPosBtn_, &QPushButton::clicked, this, [this]() {
        beginPickPosition();
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

BindingEditDialog::~BindingEditDialog() = default;

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
    value.clickPos = value.key.device == InputDevice::Mouse ? clickPos_ : ClickPosition{};
    if (value.clickPos.IsFixed() && !(clickFixedRadio_ && clickFixedRadio_->isChecked())) {
        value.clickPos = ClickPosition{};
    }
    return value;
}

void BindingEditDialog::updateMixedModeSuppressionVisible() {
    if (!mixedModeSuppressionRow_ || !mixedModeSuppressionSwitch_) return;
    const bool isHoldMode = modeCombo_ && modeCombo_->currentIndex() == 1;
    mixedModeSuppressionRow_->setVisible(isHoldMode);
}

void BindingEditDialog::updateClickPosSectionVisible() {
    if (!clickPosGroup_) return;
    const auto key = capturedKey();
    const bool isMouse = key.has_value() && key->device == InputDevice::Mouse;
    clickPosGroup_->setVisible(isMouse);
    setFixedSize(DialogChrome::bindingEditorWidth,
        kDialogBaseHeight + (isMouse ? kClickGroupExtraHeight : 0));

    const bool supported = driverSupportsMouseMove_ ? driverSupportsMouseMove_() : false;
    if (clickFixedRadio_) {
        clickFixedRadio_->setEnabled(supported);
    }
    if (pickPosBtn_) {
        pickPosBtn_->setEnabled(supported && clickFixedRadio_ && clickFixedRadio_->isChecked());
    }
    if (clickPosHintLabel_) {
        clickPosHintLabel_->setText(supported
            ? QString::fromWCharArray(
                L"\u56fa\u5b9a\u70b9\u4f4d\u6309\u6e38\u620f\u7a97\u53e3\u6bd4\u4f8b\u8bb0\u5f55\uff0c"
                L"\u5207\u6362\u5206\u8fa8\u7387\u6216\u79fb\u52a8\u7a97\u53e3\u540e\u81ea\u52a8\u6362\u7b97\u3002")
            : QString::fromWCharArray(
                L"\u5f53\u524d\u9a71\u52a8\u4e0d\u652f\u6301\u9f20\u6807\u79fb\u52a8\uff0c"
                L"\u65e0\u6cd5\u4f7f\u7528\u56fa\u5b9a\u70b9\u4f4d\u3002"));
    }
    if (clickPos_.IsFixed() && pickPosLabel_) {
        pickPosLabel_->setText(AnchorText(clickPos_.anchorX, clickPos_.anchorY, true));
    }
    if (clickFollowRadio_ && clickFixedRadio_) {
        // 单选框始终反映当前存储状态；驱动不支持时固定项为禁用态，
        // 已有固定点位不会被清掉。
        if (clickPos_.IsFixed()) {
            clickFixedRadio_->setChecked(true);
        } else {
            clickFollowRadio_->setChecked(true);
        }
    }
    if (clickFixedRadio_) {
        disconnect(clickFixedRadio_, nullptr, nullptr, nullptr);
        connect(clickFixedRadio_, &QRadioButton::toggled, this, [this](bool checked) {
            if (pickPosBtn_) {
                pickPosBtn_->setEnabled(checked && driverSupportsMouseMove_ && driverSupportsMouseMove_());
            }
            if (!checked) {
                clickPos_.fixedPosition = false;
            }
        });
    }
}

void BindingEditDialog::beginPickPosition() {
    if (pickTimer_ && !pickTimer_->isActive()) {
        pickCountdown_ = 3;
        if (pickPosBtn_) {
            pickPosBtn_->setText(QString::number(pickCountdown_) + QString("..."));
            pickPosBtn_->setEnabled(false);
        }
        pickTimer_->start();
    }
}

void BindingEditDialog::advancePickCountdown() {
    if (!pickTimer_) return;
    --pickCountdown_;
    if (pickCountdown_ > 0) {
        if (pickPosBtn_) pickPosBtn_->setText(QString::number(pickCountdown_) + QString("..."));
        return;
    }
    pickTimer_->stop();
    finishPickPosition();
}

void BindingEditDialog::finishPickPosition() {
    const QPoint pos = QCursor::pos();
    double x = 0.5;
    double y = 0.5;
    bool relativeToGame = false;

    RECT rect{};
    if (gameWindows_ != nullptr && gameWindows_->TryGetGameClientRect(rect)
        && rect.right > rect.left && rect.bottom > rect.top) {
        x = static_cast<double>(pos.x() - rect.left) / static_cast<double>(rect.right - rect.left);
        y = static_cast<double>(pos.y() - rect.top) / static_cast<double>(rect.bottom - rect.top);
        relativeToGame = true;
    } else if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect geometry = screen->geometry();
        if (geometry.width() > 0 && geometry.height() > 0) {
            x = static_cast<double>(pos.x() - geometry.left()) / static_cast<double>(geometry.width());
            y = static_cast<double>(pos.y() - geometry.top()) / static_cast<double>(geometry.height());
        }
    }

    clickPos_.fixedPosition = true;
    clickPos_.anchorX = std::clamp(x, 0.0, 1.0);
    clickPos_.anchorY = std::clamp(y, 0.0, 1.0);
    if (pickPosLabel_) {
        pickPosLabel_->setText(AnchorText(clickPos_.anchorX, clickPos_.anchorY, relativeToGame));
    }
    if (pickPosBtn_) {
        pickPosBtn_->setText(QString::fromWCharArray(L"\u62fe\u53d6\u5750\u6807"));
        pickPosBtn_->setEnabled(true);
    }
}
