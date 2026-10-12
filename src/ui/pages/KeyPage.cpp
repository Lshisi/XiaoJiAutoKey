#include "KeyPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FluentTable.h"
#include "../components/FluentButton.h"
#include "../components/FluentLineEdit.h"
#include "../components/FluentSwitch.h"
#include "../components/FluentCard.h"
#include "../components/FluentComboBox.h"
#include "../components/FluentCheckBox.h"
#include "../components/FluentPopupMenu.h"
#include "FactionTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QEvent>
#include <QCheckBox>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QTimer>

namespace {
constexpr int kSequenceKeyLabelMaxWidth = 64;
constexpr int kMaxVisiblePauseKeys = 3;
constexpr int kPauseKeyLabelMaxWidth = 72;
constexpr int kPauseTagChromeWidth = 28;
constexpr int kPauseListSpacing = 4;
constexpr int kPauseMoreButtonWidth = 42;

class PriorityIconWidget : public QWidget {
public:
    enum class Kind { None, Disabled, Enabled };

    explicit PriorityIconWidget(Kind kind, QWidget* parent = nullptr)
        : QWidget(parent), kind_(kind) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setStyleSheet("background: transparent;");
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPointF c = rect().center();
        const QColor color = kind_ == Kind::Enabled
            ? Fluent::Colors::textPrimary()
            : Fluent::Colors::textSecondary();
        QPen pen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);

        if (kind_ == Kind::None) {
            p.drawLine(QPointF(c.x() - 3.8, c.y()), QPointF(c.x() + 3.8, c.y()));
        } else if (kind_ == Kind::Disabled) {
            p.drawLine(QPointF(c.x() - 3.6, c.y() - 3.6), QPointF(c.x() + 3.6, c.y() + 3.6));
            p.drawLine(QPointF(c.x() + 3.6, c.y() - 3.6), QPointF(c.x() - 3.6, c.y() + 3.6));
        } else {
            QPainterPath path;
            path.moveTo(c.x() - 4.4, c.y() + 0.4);
            path.lineTo(c.x() - 1.2, c.y() + 3.4);
            path.lineTo(c.x() + 4.8, c.y() - 4.0);
            p.drawPath(path);
        }
    }

private:
    Kind kind_;
};
}

KeyPage::KeyPage(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_StyledBackground, true);
    applyPageTheme();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap,
        Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap);
    layout->setSpacing(Fluent::Spacing::cardGap);

    // Left: table card
    auto* tableCard = new FluentCard(this);
    tableCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // -- Profile selector row --
    auto* profileRow = new QHBoxLayout();
    profileRow->setSpacing(6);
    auto* profileLabel = new QLabel(QString::fromWCharArray(L"\u6309\u952e\u914d\u7f6e:"), this);
    profileLabel->setFont(Fluent::Typography::caption());
    QPalette pLabelPal = profileLabel->palette();
    pLabelPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    profileLabel->setPalette(pLabelPal);
    profileRow->addWidget(profileLabel);
    profileCombo_ = new FluentComboBox(this);
    profileCombo_->setMinimumWidth(120);
    profileCombo_->setFixedHeight(28);
    profileRow->addWidget(profileCombo_, 1);
    profileEnabledLabel_ = new QLabel(QString::fromWCharArray(L"\u542f\u7528\u914d\u7f6e"), this);
    profileEnabledLabel_->setFont(Fluent::Typography::caption());
    QPalette enabledLabelPal = profileEnabledLabel_->palette();
    enabledLabelPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    profileEnabledLabel_->setPalette(enabledLabelPal);
    profileRow->addWidget(profileEnabledLabel_);
    profileEnabledSwitch_ = new FluentSwitch(this);
    profileRow->addWidget(profileEnabledSwitch_);
    profileAddBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE710)), this);  // + icon
    profileAddBtn_->setFixedSize(28, 28);
    profileAddBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileRow->addWidget(profileAddBtn_);
    profileRenameBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE8AC)), this);  // rename icon
    profileRenameBtn_->setFixedSize(28, 28);
    profileRenameBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileRow->addWidget(profileRenameBtn_);
    profileDeleteBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE74D)), this);  // delete icon
    profileDeleteBtn_->setFixedSize(28, 28);
    profileDeleteBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileRow->addWidget(profileDeleteBtn_);
    profileShareBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE72D)), this);  // share icon
    profileShareBtn_->setFixedSize(28, 28);
    profileShareBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileRow->addWidget(profileShareBtn_);
    tableCard->contentLayout()->addLayout(profileRow);
    tableCard->contentLayout()->addSpacing(8);

    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(QString::fromWCharArray(L"\u6309\u952e\u5217\u8868"), this);
    title->setFont(Fluent::Typography::pageTitle());
    headerRow->addWidget(title);
    updateBadge_ = new QLabel(this);
    updateBadge_->setFont(Fluent::Typography::caption());
    updateBadge_->setCursor(Qt::PointingHandCursor);
    updateBadge_->hide();
    updateBadge_->installEventFilter(this);
    headerRow->addSpacing(8);
    headerRow->addWidget(updateBadge_);
    headerRow->addStretch();
    addBtn_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"+  \u6dfb\u52a0\u6309\u952e"), this);
    addBtn_->setFixedHeight(28);
    addBtn_->setMinimumWidth(92);
    headerRow->addWidget(addBtn_);
    tableCard->contentLayout()->addLayout(headerRow);

    table_ = new FluentTable(0, 6, this);
    table_->setHorizontalHeaderLabels({
        QString::fromWCharArray(L"\u72b6\u6001"),
        QString::fromWCharArray(L"\u6309\u952e"),
        QString::fromWCharArray(L"\u6a21\u5f0f"),
        QString::fromWCharArray(L"\u4f18\u5148"),
        QString::fromWCharArray(L"\u5ef6\u8fdf"),
        QString::fromWCharArray(L"\u64cd\u4f5c")
    });
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    table_->setColumnWidth(0, 42);
    table_->setColumnWidth(5, 80);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tableCard->contentLayout()->addWidget(table_, 1);

    // -- 暂停按键区域（表格下方） --
    auto* pauseSeparator = new QWidget(this);
    pauseSeparator->setFixedHeight(1);
    pauseSeparator->setStyleSheet(QString("background: %1;").arg(Fluent::Colors::divider().name(QColor::HexArgb)));
    tableCard->contentLayout()->addWidget(pauseSeparator);
    tableCard->contentLayout()->addSpacing(8);

    auto* keyToolsRow = new QHBoxLayout();
    keyToolsRow->setContentsMargins(0, 0, 0, 0);
    keyToolsRow->setSpacing(6);

    sequenceSwitchLabel_ = new QLabel(QString::fromWCharArray(L"\u5207\u6362\u952e:"), this);
    sequenceSwitchLabel_->setFont(Fluent::Typography::caption());
    QPalette sequenceSwitchPalette = sequenceSwitchLabel_->palette();
    sequenceSwitchPalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    sequenceSwitchLabel_->setPalette(sequenceSwitchPalette);
    keyToolsRow->addWidget(sequenceSwitchLabel_);

    sequenceSwitchKeyWidget_ = new QWidget(this);
    sequenceSwitchKeyWidget_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    sequenceSwitchKeyLayout_ = new QHBoxLayout(sequenceSwitchKeyWidget_);
    sequenceSwitchKeyLayout_->setContentsMargins(0, 0, 0, 0);
    sequenceSwitchKeyLayout_->setSpacing(4);
    keyToolsRow->addWidget(sequenceSwitchKeyWidget_);

    sequenceSwitchAddBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE710)), this);
    sequenceSwitchAddBtn_->setFixedSize(24, 24);
    sequenceSwitchAddBtn_->setFont(QFont("Segoe MDL2 Assets", 9));
    keyToolsRow->addWidget(sequenceSwitchAddBtn_);

    keyToolsDivider_ = new QWidget(this);
    keyToolsDivider_->setFixedSize(1, 18);
    keyToolsDivider_->setStyleSheet(QString("background: %1;")
        .arg(Fluent::Colors::divider().name(QColor::HexArgb)));
    keyToolsRow->addSpacing(4);
    keyToolsRow->addWidget(keyToolsDivider_);
    keyToolsRow->addSpacing(4);

    pauseKeyLabel_ = new QLabel(QString::fromWCharArray(L"\u6682\u505c\u952e:"), this);
    pauseKeyLabel_->setFont(Fluent::Typography::caption());
    QPalette pauseLabelPal = pauseKeyLabel_->palette();
    pauseLabelPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    pauseKeyLabel_->setPalette(pauseLabelPal);
    keyToolsRow->addWidget(pauseKeyLabel_);

    pauseKeyListWidget_ = new QWidget(this);
    pauseKeyListWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    pauseKeyListWidget_->setMinimumWidth(kPauseMoreButtonWidth);
    pauseKeyListLayout_ = new QHBoxLayout(pauseKeyListWidget_);
    pauseKeyListLayout_->setContentsMargins(0, 0, 0, 0);
    pauseKeyListLayout_->setSpacing(4);
    pauseKeyListWidget_->installEventFilter(this);
    keyToolsRow->addWidget(pauseKeyListWidget_, 1);

    auto* pauseAddBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE710)), this);
    pauseAddBtn->setFixedSize(24, 24);
    pauseAddBtn->setFont(QFont("Segoe MDL2 Assets", 9));
    keyToolsRow->addWidget(pauseAddBtn);
    tableCard->contentLayout()->addLayout(keyToolsRow);

    layout->addWidget(tableCard, 5);

    // Right panel
    auto* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(Fluent::Spacing::cardGap);

    // -- Card 1: 蹇嵎鎺у埗 --
    auto* controlCard = new FluentCard(QString::fromWCharArray(L"\u5feb\u6377\u63a7\u5236"), this);
    controlCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    startBtn_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u25B6  \u542f\u52a8"), this);
    startBtn_->setFixedHeight(32);
    controlCard->contentLayout()->addWidget(startBtn_);
    rightLayout->addWidget(controlCard);

    // -- Card 2: startup trigger --
    auto* triggerCard = new FluentCard(QString::fromWCharArray(L"\u542f\u52a8\u89e6\u53d1"), this);
    triggerCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    triggerCard->layout()->setContentsMargins(12, 6, 12, 6);
    triggerCard->contentLayout()->setSpacing(4);
    hotkeyEdit_ = new FluentLineEdit(this);
    hotkeyEdit_->setReadOnly(true);
    hotkeyEdit_->setAccessibleName(QString::fromWCharArray(L"\u542f\u505c\u5feb\u6377\u952e"));
    hotkeyEdit_->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236"));
    auto* hotkeyRow = new QHBoxLayout();
    hotkeyRow->setSpacing(6);
    hotkeyRow->addWidget(hotkeyEdit_, 1);
    hotkeyClearBtn_ = new FluentButton(FluentButton::Secondary, QString(QChar(0xE74D)), this);
    hotkeyClearBtn_->setFixedSize(32, 32);
    hotkeyClearBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    hotkeyRow->addWidget(hotkeyClearBtn_);
    triggerCard->contentLayout()->addLayout(hotkeyRow);
    triggerCard->contentLayout()->addSpacing(4);
    auto* beepRow = new QHBoxLayout();
    beepCheck_ = new FluentCheckBox(QString::fromWCharArray(L"\u5f00\u542f\u63d0\u793a\u97f3"), this);
    beepCheck_->setFont(Fluent::Typography::body());
    beepRow->addWidget(beepCheck_);
    beepRow->addStretch();
    soundSettingsBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE189)), this);  // sound icon
    soundSettingsBtn_->setFixedSize(32, 28);
    soundSettingsBtn_->setFont(QFont("Segoe MDL2 Assets", 12));
    beepRow->addWidget(soundSettingsBtn_);
    triggerCard->contentLayout()->addLayout(beepRow);
    statusOverlayCheck_ = new FluentCheckBox(QString::fromWCharArray(L"\u663e\u793a\u5230\u6d6e\u7a97"), this);
    statusOverlayCheck_->setFont(Fluent::Typography::body());
    auto* overlayRow = new QHBoxLayout();
    overlayRow->addWidget(statusOverlayCheck_);
    overlayRow->addStretch();
    auto* overlaySettingsBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE713)), this);
    overlaySettingsBtn->setFixedSize(32, 28);
    overlaySettingsBtn->setFont(QFont("Segoe MDL2 Assets", 12));
    overlaySettingsBtn->setToolTip(QString::fromWCharArray(L"\u60ac\u6d6e\u7a97\u8bbe\u7f6e"));
    overlaySettingsBtn->setAccessibleName(overlaySettingsBtn->toolTip());
    overlayRow->addWidget(overlaySettingsBtn);
    triggerCard->contentLayout()->addLayout(overlayRow);
    connect(overlaySettingsBtn, &QPushButton::clicked, this, &KeyPage::statusOverlaySettingsClicked);
    rightLayout->addWidget(triggerCard);

    // -- Card 3: 按压优先 --
    auto* mixedCard = new FluentCard(QString::fromWCharArray(L"\u8bbe\u7f6e"), this);
    auto* mixedSection = new QVBoxLayout();
    mixedSection->setSpacing(Fluent::Spacing::cardGap / 2);
    mixedModeCheck_ = new FluentCheckBox(QString::fromWCharArray(L"\u6309\u538b\u4f18\u5148"), this);
    mixedModeCheck_->setFont(Fluent::Typography::body());
    mixedSection->addWidget(mixedModeCheck_);
    auto* mixedDesc = new QLabel(QString::fromWCharArray(L"\u6309\u4f4f\u6309\u538b\u952e\uff0c\u6682\u505c\u5176\u4ed6\u8fde\u53d1\u3002"), this);
    mixedDesc->setFont(Fluent::Typography::caption());
    mixedDesc->setWordWrap(true);
    QPalette mPal = mixedDesc->palette();
    mPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    mixedDesc->setPalette(mPal);
    mixedSection->addWidget(mixedDesc);
    mixedCard->contentLayout()->addLayout(mixedSection);
    auto* wasdSeparator = new QWidget(this);
    wasdSeparator->setFixedHeight(1);
    wasdSeparator->setStyleSheet(QString("background: %1;").arg(Fluent::Colors::divider().name(QColor::HexArgb)));
    mixedCard->contentLayout()->addWidget(wasdSeparator);
    auto* wasdSection = new QVBoxLayout();
    wasdSection->setSpacing(Fluent::Spacing::cardGap / 2);
    wasdProtectionCheck_ = new FluentCheckBox(QString::fromWCharArray(L"WASD \u9632\u5361"), this);
    wasdProtectionCheck_->setFont(Fluent::Typography::body());
    wasdSection->addWidget(wasdProtectionCheck_);
    auto* wasdDesc = new QLabel(QString::fromWCharArray(L"\u79fb\u52a8\u65f6\u964d\u901f\uff0c\u677e\u5f00\u6062\u590d\u3002"), this);
    wasdDesc->setFont(Fluent::Typography::caption());
    wasdDesc->setWordWrap(true);
    QPalette wPal = wasdDesc->palette();
    wPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    wasdDesc->setPalette(wPal);
    wasdSection->addWidget(wasdDesc);
    mixedCard->contentLayout()->addLayout(wasdSection);
    sequenceStartDivider_ = new QWidget(mixedCard);
    sequenceStartDivider_->setFixedHeight(1);
    mixedCard->contentLayout()->addWidget(sequenceStartDivider_);
    auto* sequenceSection = new QVBoxLayout();
    sequenceSection->setSpacing(Fluent::Spacing::cardGap / 2);
    rememberSequencePositionCheck_ = new FluentCheckBox(QString::fromWCharArray(L"\u8bb0\u4f4f\u5207\u6362\u4f4d\u7f6e"), mixedCard);
    rememberSequencePositionCheck_->setFont(Fluent::Typography::body());
    rememberSequencePositionCheck_->setChecked(true);
    sequenceSection->addWidget(rememberSequencePositionCheck_);
    auto* sequenceDesc = new QLabel(QString::fromWCharArray(L"\u52fe\u9009\u7eed\u7528\u4e0a\u6b21\uff0c\u53d6\u6d88\u4ece\u5934\u5f00\u59cb\u3002"), mixedCard);
    sequenceDesc->setFont(Fluent::Typography::caption());
    sequenceDesc->setWordWrap(true);
    QPalette sequencePalette = sequenceDesc->palette();
    sequencePalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    sequenceDesc->setPalette(sequencePalette);
    sequenceSection->addWidget(sequenceDesc);
    mixedCard->contentLayout()->addLayout(sequenceSection);
    mixedCard->contentLayout()->addStretch();
    mixedCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    rightLayout->addWidget(mixedCard, 1);
    rightLayout->setStretch(0, 0);
    rightLayout->setStretch(1, 0);
    rightLayout->setStretch(2, 1);
    layout->addLayout(rightLayout, 3);

    // Connections
    connect(addBtn_, &QPushButton::clicked, this, &KeyPage::addRequested);
    connect(startBtn_, &QPushButton::clicked, this, &KeyPage::startStopClicked);
    connect(beepCheck_, &QCheckBox::toggled, this, &KeyPage::beepToggled);
    connect(statusOverlayCheck_, &QCheckBox::toggled, this, &KeyPage::statusOverlayToggled);
    connect(mixedModeCheck_, &QCheckBox::toggled, this, &KeyPage::mixedModeToggled);
    connect(wasdProtectionCheck_, &QCheckBox::toggled, this, &KeyPage::wasdProtectionToggled);
    connect(rememberSequencePositionCheck_, &QCheckBox::toggled, this, &KeyPage::rememberSequencePositionChanged);
    connect(sequenceSwitchAddBtn_, &QPushButton::clicked, this, &KeyPage::sequenceSwitchKeyAddClicked);
    connect(pauseAddBtn, &QPushButton::clicked, this, &KeyPage::pauseKeyAddClicked);
    connect(soundSettingsBtn_, &QPushButton::clicked, this, &KeyPage::soundSettingsClicked);
    connect(hotkeyClearBtn_, &QPushButton::clicked, this, &KeyPage::hotkeyClearClicked);
    hotkeyEdit_->installEventFilter(this);

    // Profile connections
    connect(profileCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) return;
        auto id = static_cast<quint64>(profileCombo_->itemData(index).toULongLong());
        emit profileSwitched(id);
    });
    connect(profileAddBtn_, &QPushButton::clicked, this, &KeyPage::profileAddRequested);
    connect(profileRenameBtn_, &QPushButton::clicked, this, &KeyPage::profileRenameRequested);
    connect(profileDeleteBtn_, &QPushButton::clicked, this, &KeyPage::profileDeleteRequested);
    connect(profileShareBtn_, &QPushButton::clicked, this, &KeyPage::shareRequested);
    connect(profileEnabledSwitch_, &FluentSwitch::toggled, this, &KeyPage::profileEnabledToggled);

    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item->column() == 0) {
            auto id = item->data(Qt::UserRole).toULongLong();
            emit enableToggled(id, item->checkState() == Qt::Checked);
        }
    });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < (int)bindings_.size()) emit editRequested(bindings_[row].id);
    });

    refreshSequenceSwitchKey();
}

void KeyPage::setBindings(const std::vector<KeyBinding>& bindings) {
    bindings_ = bindings;
    refreshTable();
}

void KeyPage::setProfileEnabled(bool enabled) {
    profileEnabled_ = enabled;
    profileEnabledSwitch_->blockSignals(true);
    profileEnabledSwitch_->setChecked(enabled);
    profileEnabledSwitch_->blockSignals(false);
    startBtn_->setEnabled(running_ || profileEnabled_);
}

void KeyPage::setRunning(bool running) {
    running_ = running;
    // Enable/disable buttons based on state
    startBtn_->setEnabled(running || profileEnabled_);
    startBtn_->setButtonStyle(running ? FluentButton::Danger : FluentButton::Primary);
    updateStartBtnText();

    // Disable editing controls while running
    addBtn_->setEnabled(!running);
    hotkeyEdit_->setEnabled(!running);
    profileCombo_->setEnabled(!running);
    profileAddBtn_->setEnabled(!running);
    profileRenameBtn_->setEnabled(!running);
    profileDeleteBtn_->setEnabled(!running);
    table_->setEnabled(!running);
    if (sequenceSwitchAddBtn_) sequenceSwitchAddBtn_->setEnabled(!running && !sequenceSwitchRecording_);
    if (sequenceSwitchClearBtn_) sequenceSwitchClearBtn_->setEnabled(!running);
    beepCheck_->setEnabled(!running);
    if (statusOverlayCheck_) statusOverlayCheck_->setEnabled(!running);
    if (soundSettingsBtn_) soundSettingsBtn_->setEnabled(!running);
    if (hotkeyClearBtn_) {
        hotkeyClearBtn_->setEnabled(!running && !hotkeyName_.isEmpty()
            && hotkeyName_ != QString::fromWCharArray(L"\u672a\u8bbe\u7f6e")
            && hotkeyName_ != QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e..."));
    }
    mixedModeCheck_->setEnabled(!running);
    if (wasdProtectionCheck_) wasdProtectionCheck_->setEnabled(!running);
    if (rememberSequencePositionCheck_) rememberSequencePositionCheck_->setEnabled(!running);
}

void KeyPage::setHotkeyText(const QString& text) {
    hotkeyEdit_->setText(text);
    hotkeyName_ = text;
    if (hotkeyClearBtn_) {
        hotkeyClearBtn_->setEnabled(!running_ && !text.isEmpty()
            && text != QString::fromWCharArray(L"\u672a\u8bbe\u7f6e")
            && text != QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e..."));
    }
    updateStartBtnText();
}

void KeyPage::setBeepEnabled(bool enabled) {
    beepCheck_->blockSignals(true);
    beepCheck_->setChecked(enabled);
    beepCheck_->blockSignals(false);
}

void KeyPage::setStatusOverlayEnabled(bool enabled) {
    if (!statusOverlayCheck_) return;
    statusOverlayCheck_->blockSignals(true);
    statusOverlayCheck_->setChecked(enabled);
    statusOverlayCheck_->blockSignals(false);
}

void KeyPage::setMixedMode(bool enabled) {
    mixedModeCheck_->blockSignals(true);
    mixedModeCheck_->setChecked(enabled);
    mixedModeCheck_->blockSignals(false);
}

void KeyPage::setWasdProtection(bool enabled) {
    if (!wasdProtectionCheck_) return;
    wasdProtectionCheck_->blockSignals(true);
    wasdProtectionCheck_->setChecked(enabled);
    wasdProtectionCheck_->blockSignals(false);
}

void KeyPage::setRememberSequencePosition(bool enabled) {
    if (!rememberSequencePositionCheck_) return;
    rememberSequencePositionCheck_->blockSignals(true);
    rememberSequencePositionCheck_->setChecked(enabled);
    rememberSequencePositionCheck_->blockSignals(false);
}

void KeyPage::setSequenceSwitchKey(std::optional<KeyDescriptor> key, bool recording) {
    sequenceSwitchKey_ = key;
    sequenceSwitchRecording_ = recording;
    refreshSequenceSwitchKey();
}

void KeyPage::updateThemeColors() {
    applyPageTheme();
    table_->refreshThemeStyle();
    profileCombo_->refreshThemeStyle();
    if (rememberSequencePositionCheck_) rememberSequencePositionCheck_->update();
    if (sequenceStartDivider_) {
        sequenceStartDivider_->setStyleSheet(QString("background: %1;")
            .arg(Fluent::Colors::divider().name(QColor::HexArgb)));
    }
    if (profileEnabledLabel_) {
        QPalette labelPalette = profileEnabledLabel_->palette();
        labelPalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
        profileEnabledLabel_->setPalette(labelPalette);
    }
    if (profileEnabledSwitch_) profileEnabledSwitch_->update();
    if (sequenceSwitchLabel_) {
        QPalette palette = sequenceSwitchLabel_->palette();
        palette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
        sequenceSwitchLabel_->setPalette(palette);
    }
    if (pauseKeyLabel_) {
        QPalette palette = pauseKeyLabel_->palette();
        palette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
        pauseKeyLabel_->setPalette(palette);
    }
    if (keyToolsDivider_) {
        keyToolsDivider_->setStyleSheet(QString("background: %1;")
            .arg(Fluent::Colors::divider().name(QColor::HexArgb)));
    }
    if (updateBadge_ && updateBadge_->isVisible()) {
        applyUpdateBadgeStyle();
    }
    beepCheck_->update();
    if (statusOverlayCheck_) statusOverlayCheck_->update();
    mixedModeCheck_->update();
    if (wasdProtectionCheck_) wasdProtectionCheck_->update();
    // Table checkboxes will be updated on next refreshTable()
    refreshTable();
    refreshSequenceSwitchKey();
    refreshPauseKeyList();
}

void KeyPage::applyPageTheme() {
    const auto palette = FactionTheme::instance().palette();
    setStyleSheet(QString("KeyPage { background: %1; }").arg(palette.surfaceTint.name()));
}

void KeyPage::setPauseKeys(const std::vector<KeyDescriptor>& keys) {
    pauseKeys_ = keys;
    refreshPauseKeyList();
}

void KeyPage::setUpdateBadge(const QString& version) {
    if (!updateBadge_) {
        return;
    }
    if (version.isEmpty()) {
        updateBadge_->hide();
        return;
    }
    updateBadge_->setText(QString::fromWCharArray(L"\u6709\u65b0\u7248\u672c V") + version);
    applyUpdateBadgeStyle();
    updateBadge_->show();
}

void KeyPage::applyUpdateBadgeStyle() {
    if (!updateBadge_) {
        return;
    }
    const auto palette = FactionTheme::instance().palette();
    updateBadge_->setStyleSheet(QString(
        "QLabel {"
        "  color: %1;"
        "  background: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  padding: 1px 8px;"
        "}")
        .arg(palette.accent.name(),
            palette.accentSoft.name(),
            palette.accentBorder.name()));
}

void KeyPage::refreshSequenceSwitchKey() {
    if (!sequenceSwitchKeyLayout_ || !sequenceSwitchKeyWidget_) return;

    sequenceSwitchClearBtn_ = nullptr;
    QLayoutItem* item = nullptr;
    while ((item = sequenceSwitchKeyLayout_->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (sequenceSwitchRecording_) {
        auto* recordingLabel = new QLabel(QString::fromWCharArray(L"\u5f55\u5236\u4e2d"), sequenceSwitchKeyWidget_);
        recordingLabel->setFont(Fluent::Typography::caption());
        QPalette palette = recordingLabel->palette();
        palette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
        recordingLabel->setPalette(palette);
        sequenceSwitchKeyLayout_->addWidget(recordingLabel);
    } else if (!sequenceSwitchKey_.has_value()) {
        auto* emptyLabel = new QLabel(QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"), sequenceSwitchKeyWidget_);
        emptyLabel->setFont(Fluent::Typography::caption());
        QPalette palette = emptyLabel->palette();
        palette.setColor(QPalette::WindowText, Fluent::Colors::textTertiary());
        emptyLabel->setPalette(palette);
        sequenceSwitchKeyLayout_->addWidget(emptyLabel);
    } else {
        auto* tag = new QWidget(sequenceSwitchKeyWidget_);
        tag->setStyleSheet(QString("background: %1; border-radius: 4px;")
            .arg(Fluent::Colors::hoverBg().name(QColor::HexArgb)));
        auto* tagLayout = new QHBoxLayout(tag);
        tagLayout->setContentsMargins(6, 2, 2, 2);
        tagLayout->setSpacing(2);

        const QString fullKeyName = QString::fromStdWString(GetKeyDisplayName(*sequenceSwitchKey_));
        const QFontMetrics metrics(Fluent::Typography::caption());
        const QString shownKeyName = metrics.elidedText(fullKeyName, Qt::ElideRight, kSequenceKeyLabelMaxWidth);
        auto* keyLabel = new QLabel(shownKeyName, tag);
        keyLabel->setFont(Fluent::Typography::caption());
        keyLabel->setStyleSheet("background: transparent;");
        keyLabel->setMaximumWidth(kSequenceKeyLabelMaxWidth);
        tagLayout->addWidget(keyLabel);

        sequenceSwitchClearBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE711)), tag);
        sequenceSwitchClearBtn_->setFixedSize(18, 18);
        sequenceSwitchClearBtn_->setFont(QFont("Segoe MDL2 Assets", 7));
        sequenceSwitchClearBtn_->setEnabled(!running_);
        tagLayout->addWidget(sequenceSwitchClearBtn_);
        sequenceSwitchKeyLayout_->addWidget(tag);

        connect(sequenceSwitchClearBtn_, &QPushButton::clicked, this, &KeyPage::sequenceSwitchKeyDeleteClicked);
    }
    if (sequenceSwitchAddBtn_) sequenceSwitchAddBtn_->setEnabled(!running_ && !sequenceSwitchRecording_);
}

void KeyPage::refreshPauseKeyList() {
    if (!pauseKeyListLayout_ || !pauseKeyListWidget_) return;

    // Clear existing items
    QLayoutItem* item = nullptr;
    while ((item = pauseKeyListLayout_->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (pauseKeys_.empty()) {
        auto* emptyLabel = new QLabel(QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"), pauseKeyListWidget_);
        emptyLabel->setFont(Fluent::Typography::caption());
        QPalette ePal = emptyLabel->palette();
        ePal.setColor(QPalette::WindowText, Fluent::Colors::textTertiary());
        emptyLabel->setPalette(ePal);
        pauseKeyListLayout_->addWidget(emptyLabel);
    } else {
        const QFont captionFont = Fluent::Typography::caption();
        const QFontMetrics metrics(captionFont);
        const int totalCount = static_cast<int>(pauseKeys_.size());
        const int availableWidth = pauseKeyListWidget_->width() > 0
            ? pauseKeyListWidget_->width()
            : 320;

        auto tagWidth = [this, &metrics](int index) {
            const QString keyName = QString::fromStdWString(GetKeyDisplayName(pauseKeys_[index]));
            return qMin(metrics.horizontalAdvance(keyName), kPauseKeyLabelMaxWidth) + kPauseTagChromeWidth;
        };
        auto requiredWidth = [&tagWidth, totalCount](int visibleCount) {
            int width = 0;
            for (int i = 0; i < visibleCount; ++i) width += tagWidth(i);
            const bool hasMore = visibleCount < totalCount;
            const int widgetCount = visibleCount + (hasMore ? 1 : 0);
            if (hasMore) width += kPauseMoreButtonWidth;
            if (widgetCount > 1) width += (widgetCount - 1) * kPauseListSpacing;
            return width;
        };

        int visibleCount = qMin(totalCount, kMaxVisiblePauseKeys);
        while (visibleCount > 0 && requiredWidth(visibleCount) > availableWidth) {
            --visibleCount;
        }

        for (int i = 0; i < visibleCount; ++i) {
            auto* tag = new QWidget(pauseKeyListWidget_);
            tag->setStyleSheet(QString("background: %1; border-radius: 4px;")
                .arg(Fluent::Colors::hoverBg().name(QColor::HexArgb)));
            auto* tagLayout = new QHBoxLayout(tag);
            tagLayout->setContentsMargins(6, 2, 2, 2);
            tagLayout->setSpacing(2);

            const QString fullKeyName = QString::fromStdWString(GetKeyDisplayName(pauseKeys_[i]));
            const QString shownKeyName = metrics.elidedText(fullKeyName, Qt::ElideRight, kPauseKeyLabelMaxWidth);
            auto* keyLabel = new QLabel(shownKeyName, tag);
            keyLabel->setFont(captionFont);
            keyLabel->setStyleSheet("background: transparent;");
            keyLabel->setMaximumWidth(kPauseKeyLabelMaxWidth);
            tagLayout->addWidget(keyLabel);

            auto* delBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE711)), tag);  // X icon
            delBtn->setFixedSize(18, 18);
            delBtn->setFont(QFont("Segoe MDL2 Assets", 7));
            tagLayout->addWidget(delBtn);

            pauseKeyListLayout_->addWidget(tag);

            connect(delBtn, &QPushButton::clicked, this, [this, i]() {
                emit pauseKeyDeleteClicked(i);
            });
        }

        const int hiddenCount = totalCount - visibleCount;
        if (hiddenCount > 0) {
            auto* moreBtn = new FluentButton(
                FluentButton::Secondary, QString("+%1").arg(hiddenCount), pauseKeyListWidget_);
            moreBtn->setFixedSize(kPauseMoreButtonWidth, 24);

            connect(moreBtn, &QPushButton::clicked, this, [this, moreBtn, visibleCount, totalCount]() {
                QStringList hiddenKeyNames;
                hiddenKeyNames.reserve(totalCount - visibleCount);
                for (int i = visibleCount; i < totalCount; ++i) {
                    hiddenKeyNames.push_back(QString::fromStdWString(GetKeyDisplayName(pauseKeys_[i])));
                }

                FluentPopupMenu::Options options;
                options.minWidth = 112;
                options.maxWidth = 180;
                options.itemHeight = 28;
                options.maxVisibleItems = 8;
                options.preferAbove = true;
                options.trailingText = QString(QChar(0x00D7));
                const QRect anchorRect(moreBtn->mapToGlobal(QPoint(0, 0)), moreBtn->size());
                const int selected = FluentPopupMenu::exec(this, anchorRect, hiddenKeyNames, options);
                if (selected >= 0) emit pauseKeyDeleteClicked(visibleCount + selected);
            });
            pauseKeyListLayout_->addWidget(moreBtn);
        }
    }
    pauseKeyListLayout_->addStretch();
}

void KeyPage::updateStartBtnText() {
    QString keyStr = (hotkeyName_.isEmpty() || hotkeyName_ == QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"))
        ? "" : QString(" (%1)").arg(hotkeyName_);
    startBtn_->setText((running_
        ? QString::fromWCharArray(L"\u25A0  \u505c\u6b62")
        : QString::fromWCharArray(L"\u25B6  \u542f\u52a8")) + keyStr);
}

void KeyPage::refreshTable() {
    table_->blockSignals(true);
    table_->setRowCount(0);

    for (int i = 0; i < (int)bindings_.size(); ++i) {
        const auto& b = bindings_[i];
        table_->insertRow(i);

        // Checkbox widget with proper blue+white-check style
        auto* checkWidget = new QWidget();
        checkWidget->setStyleSheet("background: transparent;");
        auto* checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        auto* checkBox = new FluentCheckBox(QString(), checkWidget);
        checkBox->setChecked(b.enabled);
        checkLayout->addWidget(checkBox);
        table_->setCellWidget(i, 0, checkWidget);

        auto* hiddenItem = new QTableWidgetItem();
        hiddenItem->setData(Qt::UserRole, static_cast<qulonglong>(b.id));
        table_->setItem(i, 0, hiddenItem);

        auto* keyItem = new QTableWidgetItem(QString::fromStdWString(GetKeyDisplayName(b.key)));
        keyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table_->setItem(i, 1, keyItem);
        auto* modeItem = new QTableWidgetItem(QString::fromStdWString(RepeatModeToText(b.mode)));
        modeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table_->setItem(i, 2, modeItem);
        auto mixedKind = PriorityIconWidget::Kind::None;
        if (b.mode == RepeatMode::Hold) {
            mixedKind = b.suppressMixedMode
                ? PriorityIconWidget::Kind::Disabled
                : PriorityIconWidget::Kind::Enabled;
        }
        auto* mixedWidget = new PriorityIconWidget(mixedKind);
        table_->setCellWidget(i, 3, mixedWidget);
        auto* delayItem = new QTableWidgetItem(QString::number(b.delayMs));
        delayItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table_->setItem(i, 4, delayItem);

        // Action icons (Segoe MDL2 Assets)
        auto* actionWidget = new QWidget();
        actionWidget->setStyleSheet("background: transparent;");
        auto* actionLayout = new QHBoxLayout(actionWidget);
        actionLayout->setContentsMargins(4, 0, 4, 0);
        actionLayout->setSpacing(4);
        auto* editBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE70F)), actionWidget);
        editBtn->setFixedSize(28, 28);
        editBtn->setFont(QFont("Segoe MDL2 Assets", 10));
        auto* delBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE74D)), actionWidget);
        delBtn->setFixedSize(28, 28);
        delBtn->setFont(QFont("Segoe MDL2 Assets", 10));
        actionLayout->addWidget(editBtn);
        actionLayout->addWidget(delBtn);
        table_->setCellWidget(i, 5, actionWidget);

        std::uint64_t id = b.id;
        connect(checkBox, &QCheckBox::toggled, this, [this, id](bool checked) { emit enableToggled(id, checked); });
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { emit editRequested(id); });
        connect(delBtn, &QPushButton::clicked, this, [this, id]() { emit deleteRequested(id); });
    }
    table_->blockSignals(false);
}

bool KeyPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == updateBadge_ && event->type() == QEvent::MouseButtonRelease) {
        emit updateBadgeClicked();
        return true;
    }
    if (watched == hotkeyEdit_ && event->type() == QEvent::MouseButtonPress) {
        if (running_) return true;  // Ignore clicks while running
        emit hotkeyEditClicked();
        return true;
    }
    if (watched == pauseKeyListWidget_ && event->type() == QEvent::Resize) {
        const int width = static_cast<QResizeEvent*>(event)->size().width();
        if (width != pauseListWidth_) {
            pauseListWidth_ = width;
            if (!pauseRefreshQueued_) {
                pauseRefreshQueued_ = true;
                QTimer::singleShot(0, this, [this]() {
                    pauseRefreshQueued_ = false;
                    refreshPauseKeyList();
                });
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void KeyPage::setProfiles(const std::vector<std::pair<std::uint64_t, QString>>& profiles, std::uint64_t activeId) {
    profileCombo_->blockSignals(true);
    profileCombo_->clear();
    int activeIndex = 0;
    for (int i = 0; i < static_cast<int>(profiles.size()); ++i) {
        profileCombo_->addItem(profiles[i].second, QVariant::fromValue(static_cast<qulonglong>(profiles[i].first)));
        if (profiles[i].first == activeId) activeIndex = i;
    }
    profileCombo_->setCurrentIndex(activeIndex);
    profileCombo_->blockSignals(false);

    profileDeleteBtn_->setEnabled(!running_);
}
