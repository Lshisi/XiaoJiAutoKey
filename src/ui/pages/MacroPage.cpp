#include "MacroPage.h"
#include "../styles/DesignTokens.h"
#include "../components/DraggableTable.h"
#include "../components/FluentButton.h"
#include "../components/FluentLineEdit.h"
#include "../components/FluentComboBox.h"
#include "../components/FluentCard.h"
#include "../components/FluentCheckBox.h"
#include "../components/FluentSwitch.h"
#include "FactionTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QEvent>
#include <QIntValidator>

namespace {
class LoopCountValidator final : public QIntValidator {
public:
    explicit LoopCountValidator(QObject* parent) : QIntValidator(0, 99999, parent) {}

    void fixup(QString& input) const override {
        // Empty input must become acceptable before QLineEdit emits editingFinished.
        if (input.trimmed().isEmpty()) input = QStringLiteral("0");
    }
};
}

MacroPage::MacroPage(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::ClickFocus);
    applyPageTheme();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap,
        Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap);
    layout->setSpacing(Fluent::Spacing::cardGap);

    // ── Left: Table Card ──
    auto* tableCard = new FluentCard(this);
    tableCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // -- Macro profile selector row --
    auto* profileRow = new QHBoxLayout();
    profileRow->setSpacing(6);
    auto* profileLabel = new QLabel(QString::fromWCharArray(L"\u5faa\u73af\u914d\u7f6e:"), this);
    profileLabel->setFont(Fluent::Typography::caption());
    QPalette pLabelPal = profileLabel->palette();
    pLabelPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    profileLabel->setPalette(pLabelPal);
    profileRow->addWidget(profileLabel);
    macroCombo_ = new FluentComboBox(this);
    macroCombo_->setMinimumWidth(120);
    macroCombo_->setFixedHeight(28);
    profileRow->addWidget(macroCombo_, 1);
    macroEnabledLabel_ = new QLabel(QString::fromWCharArray(L"\u542f\u7528\u914d\u7f6e"), this);
    macroEnabledLabel_->setFont(Fluent::Typography::caption());
    QPalette enabledLabelPal = macroEnabledLabel_->palette();
    enabledLabelPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    macroEnabledLabel_->setPalette(enabledLabelPal);
    profileRow->addWidget(macroEnabledLabel_);
    macroEnabledSwitch_ = new FluentSwitch(this);
    profileRow->addWidget(macroEnabledSwitch_);
    macroAddBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE710)), this);
    macroAddBtn_->setFixedSize(28, 28);
    macroAddBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileRow->addWidget(macroAddBtn_);
    macroRenameBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE8AC)), this);
    macroRenameBtn_->setFixedSize(28, 28);
    macroRenameBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileRow->addWidget(macroRenameBtn_);
    macroDeleteBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE74D)), this);
    macroDeleteBtn_->setFixedSize(28, 28);
    macroDeleteBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileRow->addWidget(macroDeleteBtn_);
    macroShareBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE72D)), this);  // share icon
    macroShareBtn_->setFixedSize(28, 28);
    macroShareBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    macroShareBtn_->setToolTip(QString::fromWCharArray(L"\u5bfc\u51fa\u5f53\u524d\u5b8f"));
    profileRow->addWidget(macroShareBtn_);
    tableCard->contentLayout()->addLayout(profileRow);
    tableCard->contentLayout()->addSpacing(8);

    // Header row
    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(QString::fromWCharArray(L"\u5faa\u73af\u5217\u8868"), this);
    title->setFont(Fluent::Typography::pageTitle());
    headerRow->addWidget(title);
    headerRow->addStretch();
    addBtn_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"+  \u6dfb\u52a0\u6309\u952e"), this);
    addBtn_->setFixedHeight(28);
    addBtn_->setMinimumWidth(92);
    headerRow->addWidget(addBtn_);
    tableCard->contentLayout()->addLayout(headerRow);

    // Steps table
    table_ = new DraggableTable(0, 4, this);
    table_->setHorizontalHeaderLabels({
        QString(),
        QString::fromWCharArray(L"\u6309\u952e"),
        QString::fromWCharArray(L"\u95f4\u9694 (ms)"),
        QString::fromWCharArray(L"\u64cd\u4f5c")
    });
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table_->setColumnWidth(0, 32);
    table_->setColumnWidth(3, 80);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->verticalHeader()->setVisible(false);
    table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tableCard->contentLayout()->addWidget(table_, 1);

    auto* pauseSeparator = new QWidget(this);
    pauseSeparator->setFixedHeight(1);
    pauseSeparator->setStyleSheet(QString("background: %1;").arg(Fluent::Colors::divider().name(QColor::HexArgb)));
    tableCard->contentLayout()->addWidget(pauseSeparator);
    tableCard->contentLayout()->addSpacing(8);

    auto* pauseRow = new QHBoxLayout();
    pauseRow->setSpacing(6);
    auto* pauseLabel = new QLabel(QString::fromWCharArray(L"\u6682\u505c\u952e:"), this);
    pauseLabel->setFont(Fluent::Typography::caption());
    QPalette pauseLabelPal = pauseLabel->palette();
    pauseLabelPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    pauseLabel->setPalette(pauseLabelPal);
    pauseRow->addWidget(pauseLabel);

    pauseKeyListWidget_ = new QWidget(this);
    pauseKeyListLayout_ = new QHBoxLayout(pauseKeyListWidget_);
    pauseKeyListLayout_->setContentsMargins(0, 0, 0, 0);
    pauseKeyListLayout_->setSpacing(4);
    pauseRow->addWidget(pauseKeyListWidget_, 1);

    auto* pauseAddBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE710)), this);
    pauseAddBtn->setFixedSize(24, 24);
    pauseAddBtn->setFont(QFont("Segoe MDL2 Assets", 9));
    pauseRow->addWidget(pauseAddBtn);
    tableCard->contentLayout()->addLayout(pauseRow);
    layout->addWidget(tableCard, 5);

    // ── Right panel ──
    auto* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(Fluent::Spacing::cardGap);

    // -- Card 1: 快捷控制 --
    auto* controlCard = new FluentCard(QString::fromWCharArray(L"\u5feb\u6377\u63a7\u5236"), this);
    controlCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    startBtn_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u25B6  \u542f\u52a8"), this);
    startBtn_->setFixedHeight(32);
    controlCard->contentLayout()->addWidget(startBtn_);
    rightLayout->addWidget(controlCard);

    // -- Card 2: 启动触发 --
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
    soundSettingsBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE189)), this);
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
    connect(overlaySettingsBtn, &QPushButton::clicked, this, &MacroPage::statusOverlaySettingsClicked);
    rightLayout->addWidget(triggerCard);

    // -- Card 3: 模式设置 --
    auto* modeCard = new FluentCard(QString::fromWCharArray(L"\u6a21\u5f0f\u8bbe\u7f6e"), this);
    auto* modeLabel = new QLabel(QString::fromWCharArray(L"\u89e6\u53d1\u6a21\u5f0f"), this);
    modeLabel->setFont(Fluent::Typography::caption());
    QPalette mPal = modeLabel->palette();
    mPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    modeLabel->setPalette(mPal);
    modeCard->contentLayout()->addWidget(modeLabel);
    modeCombo_ = new FluentComboBox(this);
    modeCombo_->addItem(QString::fromWCharArray(L"\u5f00\u5173\u6a21\u5f0f"));
    modeCombo_->addItem(QString::fromWCharArray(L"\u6309\u538b\u6a21\u5f0f"));
    modeCard->contentLayout()->addWidget(modeCombo_);
    modeCard->contentLayout()->addSpacing(8);
    loopLabel_ = new QLabel(QString::fromWCharArray(L"\u5faa\u73af\u6b21\u6570 (0=\u65e0\u9650)"), this);
    loopLabel_->setFont(Fluent::Typography::caption());
    loopLabel_->setPalette(mPal);
    modeCard->contentLayout()->addWidget(loopLabel_);
    loopEdit_ = new FluentLineEdit(this);
    loopEdit_->setText("0");
    loopEdit_->setValidator(new LoopCountValidator(loopEdit_));
    modeCard->contentLayout()->addWidget(loopEdit_);
    modeCard->contentLayout()->addStretch();
    modeCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    rightLayout->addWidget(modeCard, 1);
    rightLayout->setStretch(0, 0);
    rightLayout->setStretch(1, 0);
    rightLayout->setStretch(2, 1);
    layout->addLayout(rightLayout, 3);

    // ── Connections ──
    connect(addBtn_, &QPushButton::clicked, this, &MacroPage::addStepRequested);
    connect(startBtn_, &QPushButton::clicked, this, &MacroPage::startStopClicked);
    connect(beepCheck_, &QCheckBox::toggled, this, &MacroPage::beepToggled);
    connect(statusOverlayCheck_, &QCheckBox::toggled, this, &MacroPage::statusOverlayToggled);
    connect(pauseAddBtn, &QPushButton::clicked, this, &MacroPage::pauseKeyAddClicked);
    connect(soundSettingsBtn_, &QPushButton::clicked, this, &MacroPage::soundSettingsClicked);
    connect(hotkeyClearBtn_, &QPushButton::clicked, this, &MacroPage::hotkeyClearClicked);
    connect(hotkeyEdit_, &QLineEdit::cursorPositionChanged, this, [](){ /* dummy */ });
    hotkeyEdit_->installEventFilter(this);

    // 拖拽排序
    connect(table_, &DraggableTable::rowMoveRequested, this, &MacroPage::stepMoveRequested);

    connect(macroCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) return;
        auto id = static_cast<quint64>(macroCombo_->itemData(index).toULongLong());
        emit macroSwitched(id);
    });
    connect(macroAddBtn_, &QPushButton::clicked, this, &MacroPage::macroAddRequested);
    connect(macroRenameBtn_, &QPushButton::clicked, this, &MacroPage::macroRenameRequested);
    connect(macroDeleteBtn_, &QPushButton::clicked, this, &MacroPage::macroDeleteRequested);
    connect(macroShareBtn_, &QPushButton::clicked, this, &MacroPage::shareRequested);
    connect(macroEnabledSwitch_, &FluentSwitch::toggled, this, &MacroPage::macroEnabledToggled);

    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        // Hide loop count for Hold mode (按压模式)
        bool isHold = (index == 1);
        loopLabel_->setVisible(!isHold);
        loopEdit_->setVisible(!isHold);
        emit modeChanged(index);
    });
    connect(loopEdit_, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        int count = loopEdit_->text().toInt(&ok);
        if (!ok || count < 0) count = 0;
        loopEdit_->setText(QString::number(count));
        emit loopCountChanged(count);
    });

    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < static_cast<int>(steps_.size())) emit editStepRequested(row);
    });
}

void MacroPage::setMacros(const std::vector<MacroConfig>& /*macros*/) {
    // Refresh handled by refreshMacroPage in MainWindow
}

void MacroPage::setActiveMacro(std::uint64_t /*macroId*/) {
    // Handled by combo selection
}

void MacroPage::setSteps(const std::vector<MacroStep>& steps) {
    steps_ = steps;
    refreshTable();
}

void MacroPage::setMode(MacroTriggerMode mode) {
    modeCombo_->blockSignals(true);
    modeCombo_->setCurrentIndex(mode == MacroTriggerMode::Hold ? 1 : 0);
    modeCombo_->blockSignals(false);
    // Hide loop count for Hold mode
    bool isHold = (mode == MacroTriggerMode::Hold);
    loopLabel_->setVisible(!isHold);
    loopEdit_->setVisible(!isHold);
}

void MacroPage::setLoopCount(int count) {
    loopEdit_->blockSignals(true);
    loopEdit_->setText(QString::number(count));
    loopEdit_->blockSignals(false);
}

void MacroPage::setRunning(bool running) {
    running_ = running;
    startBtn_->setEnabled(running || macroEnabled_);
    startBtn_->setButtonStyle(running ? FluentButton::Danger : FluentButton::Primary);
    updateStartBtnText();
    addBtn_->setEnabled(!running);
    table_->setEnabled(!running);
    macroCombo_->setEnabled(!running);
    macroAddBtn_->setEnabled(!running);
    macroRenameBtn_->setEnabled(!running);
    macroDeleteBtn_->setEnabled(!running);
    modeCombo_->setEnabled(!running);
    loopEdit_->setEnabled(!running);
    hotkeyEdit_->setEnabled(!running);
    beepCheck_->setEnabled(!running);
    if (statusOverlayCheck_) statusOverlayCheck_->setEnabled(!running);
    if (soundSettingsBtn_) soundSettingsBtn_->setEnabled(!running);
    if (hotkeyClearBtn_) {
        hotkeyClearBtn_->setEnabled(!running && !hotkeyName_.isEmpty()
            && hotkeyName_ != QString::fromWCharArray(L"\u672a\u8bbe\u7f6e")
            && hotkeyName_ != QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e..."));
    }
}

void MacroPage::setMacroEnabled(bool enabled) {
    macroEnabled_ = enabled;
    macroEnabledSwitch_->blockSignals(true);
    macroEnabledSwitch_->setChecked(enabled);
    macroEnabledSwitch_->blockSignals(false);
    startBtn_->setEnabled(running_ || macroEnabled_);
}

void MacroPage::setHotkeyText(const QString& text) {
    hotkeyEdit_->setText(text);
    hotkeyName_ = text;
    if (hotkeyClearBtn_) {
        hotkeyClearBtn_->setEnabled(!running_ && !text.isEmpty()
            && text != QString::fromWCharArray(L"\u672a\u8bbe\u7f6e")
            && text != QString::fromWCharArray(L"\u8bf7\u6309\u4e0b\u4e00\u4e2a\u952e..."));
    }
    updateStartBtnText();
}

void MacroPage::updateStartBtnText() {
    QString keyStr = (hotkeyName_.isEmpty() || hotkeyName_ == QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"))
        ? "" : QString(" (%1)").arg(hotkeyName_);
    startBtn_->setText((running_
        ? QString::fromWCharArray(L"\u25A0  \u505c\u6b62")
        : QString::fromWCharArray(L"\u25B6  \u542f\u52a8")) + keyStr);
}

void MacroPage::setBeepEnabled(bool enabled) {
    beepCheck_->blockSignals(true);
    beepCheck_->setChecked(enabled);
    beepCheck_->blockSignals(false);
}

void MacroPage::setStatusOverlayEnabled(bool enabled) {
    if (!statusOverlayCheck_) return;
    statusOverlayCheck_->blockSignals(true);
    statusOverlayCheck_->setChecked(enabled);
    statusOverlayCheck_->blockSignals(false);
}

void MacroPage::updateThemeColors() {
    applyPageTheme();
    table_->refreshThemeStyle();
    macroCombo_->refreshThemeStyle();
    if (macroEnabledLabel_) {
        QPalette labelPalette = macroEnabledLabel_->palette();
        labelPalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
        macroEnabledLabel_->setPalette(labelPalette);
    }
    if (macroEnabledSwitch_) macroEnabledSwitch_->update();
    modeCombo_->refreshThemeStyle();
    beepCheck_->update();
    if (statusOverlayCheck_) statusOverlayCheck_->update();
    refreshPauseKeyList();
    refreshTable();
}

void MacroPage::applyPageTheme() {
    const auto palette = FactionTheme::instance().palette();
    setStyleSheet(QString("MacroPage { background: %1; }").arg(palette.surfaceTint.name()));
}

void MacroPage::setMacroProfiles(const std::vector<std::pair<std::uint64_t, QString>>& macros, std::uint64_t activeId) {
    macroCombo_->blockSignals(true);
    macroCombo_->clear();
    int activeIndex = 0;
    for (int i = 0; i < static_cast<int>(macros.size()); ++i) {
        macroCombo_->addItem(macros[i].second, QVariant::fromValue(static_cast<qulonglong>(macros[i].first)));
        if (macros[i].first == activeId) activeIndex = i;
    }
    macroCombo_->setCurrentIndex(activeIndex);
    macroCombo_->blockSignals(false);
    macroDeleteBtn_->setEnabled(!running_);
}

void MacroPage::setPauseKeys(const std::vector<KeyDescriptor>& keys) {
    pauseKeys_ = keys;
    refreshPauseKeyList();
}

void MacroPage::refreshPauseKeyList() {
    if (!pauseKeyListLayout_ || !pauseKeyListWidget_) return;

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
        for (int i = 0; i < static_cast<int>(pauseKeys_.size()); ++i) {
            auto* tag = new QWidget(pauseKeyListWidget_);
            tag->setStyleSheet(QString("background: %1; border-radius: 4px;")
                .arg(Fluent::Colors::hoverBg().name(QColor::HexArgb)));
            auto* tagLayout = new QHBoxLayout(tag);
            tagLayout->setContentsMargins(6, 2, 2, 2);
            tagLayout->setSpacing(2);

            auto* keyLabel = new QLabel(QString::fromStdWString(GetKeyDisplayName(pauseKeys_[i])), tag);
            keyLabel->setFont(Fluent::Typography::caption());
            keyLabel->setStyleSheet("background: transparent;");
            tagLayout->addWidget(keyLabel);

            auto* delBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE711)), tag);
            delBtn->setFixedSize(18, 18);
            delBtn->setFont(QFont("Segoe MDL2 Assets", 7));
            tagLayout->addWidget(delBtn);

            pauseKeyListLayout_->addWidget(tag);

            connect(delBtn, &QPushButton::clicked, this, [this, i]() {
                emit pauseKeyDeleteClicked(i);
            });
        }
    }

    pauseKeyListLayout_->addStretch();
}

void MacroPage::refreshTable() {
    table_->blockSignals(true);
    table_->setRowCount(0);

    for (int i = 0; i < static_cast<int>(steps_.size()); ++i) {
        const auto& step = steps_[i];
        table_->insertRow(i);

        // Drag handle (first column)
        auto* handleItem = new QTableWidgetItem(QString::fromWCharArray(L"\u2261"));  // ≡
        handleItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(i, 0, handleItem);

        // Key name
        auto* keyItem = new QTableWidgetItem(QString::fromStdWString(GetKeyDisplayName(step.key)));
        keyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        if (step.key.device == InputDevice::Mouse && step.clickPos.IsFixed()) {
            keyItem->setText(keyItem->text() + QString::fromWCharArray(L" \u25c9"));
            keyItem->setToolTip(QString::fromWCharArray(
                L"\u56fa\u5b9a\u70b9\u4f4d: \u5c4f\u5e55\u5185 (%1%, %2%)")
                .arg(QString::number(step.clickPos.anchorX * 100.0, 'f', 1),
                    QString::number(step.clickPos.anchorY * 100.0, 'f', 1)));
        }
        table_->setItem(i, 1, keyItem);

        // Delay
        auto* delayItem = new QTableWidgetItem(QString::number(step.delayAfterMs));
        delayItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table_->setItem(i, 2, delayItem);

        // Action buttons
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
        table_->setCellWidget(i, 3, actionWidget);

        int idx = i;
        connect(editBtn, &QPushButton::clicked, this, [this, idx]() { emit editStepRequested(idx); });
        connect(delBtn, &QPushButton::clicked, this, [this, idx]() { emit deleteStepRequested(idx); });
    }
    table_->blockSignals(false);
}

bool MacroPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == hotkeyEdit_ && event->type() == QEvent::MouseButtonPress) {
        if (running_) return true;
        emit hotkeyEditClicked();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
