#include "MacroPage.h"
#include "../styles/DesignTokens.h"
#include "../components/DraggableTable.h"
#include "../components/FluentButton.h"
#include "../components/FluentLineEdit.h"
#include "../components/FluentComboBox.h"
#include "../components/FluentCard.h"
#include "FactionTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QEvent>
#include <QIntValidator>

static QString checkBoxStyle() {
    QString color = FactionTheme::instance().primaryColor().name();
    return QString(
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px; }"
        "QCheckBox::indicator:checked {"
        "  background: %1; border: 1px solid %1;"
        "  image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxMiIgaGVpZ2h0PSIxMiIgdmlld0JveD0iMCAwIDEyIDEyIj48cGF0aCBkPSJNMTAgM0w1IDhMMiA1IiBmaWxsPSJub25lIiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjIiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIgc3Ryb2tlLWxpbmVqb2luPSJyb3VuZCIvPjwvc3ZnPg==);"
        "}"
        "QCheckBox::indicator:unchecked {"
        "  background: #FFFFFF; border: 1.5px solid rgba(0,0,0,0.3); }"
    ).arg(color);
}

MacroPage::MacroPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet("MacroPage { background: #F5F7FA; }");
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::ClickFocus);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    // ── Left: Table Card ──
    auto* tableCard = new FluentCard(this);

    // -- Macro profile selector row --
    auto* profileRow = new QHBoxLayout();
    profileRow->setSpacing(6);
    auto* profileLabel = new QLabel(QString::fromWCharArray(L"\u5b8f\u914d\u7f6e:"), this);
    profileLabel->setFont(Fluent::Typography::caption());
    QPalette pLabelPal = profileLabel->palette();
    pLabelPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    profileLabel->setPalette(pLabelPal);
    profileRow->addWidget(profileLabel);
    macroCombo_ = new FluentComboBox(this);
    macroCombo_->setMinimumWidth(120);
    macroCombo_->setFixedHeight(28);
    profileRow->addWidget(macroCombo_, 1);
    macroAddBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE710)), this);
    macroAddBtn_->setFixedSize(28, 28);
    macroAddBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    macroAddBtn_->setToolTip(QString::fromWCharArray(L"\u65b0\u5efa\u5b8f"));
    profileRow->addWidget(macroAddBtn_);
    macroRenameBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE8AC)), this);
    macroRenameBtn_->setFixedSize(28, 28);
    macroRenameBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    macroRenameBtn_->setToolTip(QString::fromWCharArray(L"\u91cd\u547d\u540d"));
    profileRow->addWidget(macroRenameBtn_);
    macroDeleteBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE74D)), this);
    macroDeleteBtn_->setFixedSize(28, 28);
    macroDeleteBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    macroDeleteBtn_->setToolTip(QString::fromWCharArray(L"\u5220\u9664\u5b8f"));
    profileRow->addWidget(macroDeleteBtn_);
    tableCard->contentLayout()->addLayout(profileRow);
    tableCard->contentLayout()->addSpacing(8);

    // Header row
    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(QString::fromWCharArray(L"\u6309\u952e\u5e8f\u5217"), this);
    title->setFont(Fluent::Typography::pageTitle());
    headerRow->addWidget(title);
    headerRow->addStretch();
    addBtn_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"+  \u6dfb\u52a0\u6b65\u9aa4"), this);
    addBtn_->setFixedHeight(30);
    addBtn_->setMinimumWidth(100);
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
    table_->setStyleSheet(table_->styleSheet() +
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(0,0,0,0.12); border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(0,0,0,0.2); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );
    tableCard->contentLayout()->addWidget(table_, 1);
    layout->addWidget(tableCard, 5);

    // ── Right panel ──
    auto* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(16);

    // -- Card 1: 快捷控制 --
    auto* controlCard = new FluentCard(QString::fromWCharArray(L"\u5feb\u6377\u63a7\u5236"), this);
    startBtn_ = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"\u25B6  \u542f\u52a8"), this);
    startBtn_->setFixedHeight(36);
    controlCard->contentLayout()->addWidget(startBtn_);
    controlCard->contentLayout()->addSpacing(4);
    stopBtn_ = new FluentButton(FluentButton::Danger, QString::fromWCharArray(L"\u25A0  \u505c\u6b62"), this);
    stopBtn_->setFixedHeight(36);
    stopBtn_->setEnabled(false);
    controlCard->contentLayout()->addWidget(stopBtn_);
    rightLayout->addWidget(controlCard);

    // -- Card 2: 启动触发 --
    auto* triggerCard = new FluentCard(QString::fromWCharArray(L"\u542f\u52a8\u89e6\u53d1"), this);
    auto* hotkeyLabel = new QLabel(QString::fromWCharArray(L"\u542f\u505c\u5feb\u6377\u952e"), this);
    hotkeyLabel->setFont(Fluent::Typography::caption());
    QPalette hPal = hotkeyLabel->palette();
    hPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    hotkeyLabel->setPalette(hPal);
    triggerCard->contentLayout()->addWidget(hotkeyLabel);
    hotkeyEdit_ = new FluentLineEdit(this);
    hotkeyEdit_->setReadOnly(true);
    hotkeyEdit_->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
    triggerCard->contentLayout()->addWidget(hotkeyEdit_);
    triggerCard->contentLayout()->addSpacing(8);
    auto* beepRow = new QHBoxLayout();
    beepCheck_ = new QCheckBox(QString::fromWCharArray(L"\u5f00\u542f\u63d0\u793a\u97f3"), this);
    beepCheck_->setFont(Fluent::Typography::body());
    beepCheck_->setStyleSheet(checkBoxStyle());
    beepRow->addWidget(beepCheck_);
    beepRow->addStretch();
    soundSettingsBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE189)), this);
    soundSettingsBtn_->setFixedSize(32, 28);
    soundSettingsBtn_->setFont(QFont("Segoe MDL2 Assets", 12));
    soundSettingsBtn_->setToolTip(QString::fromWCharArray(L"\u97f3\u6548\u8bbe\u7f6e"));
    beepRow->addWidget(soundSettingsBtn_);
    triggerCard->contentLayout()->addLayout(beepRow);
    statusOverlayCheck_ = new QCheckBox(QString::fromWCharArray(L"\u663e\u793a\u5230\u6d6e\u7a97"), this);
    statusOverlayCheck_->setFont(Fluent::Typography::body());
    statusOverlayCheck_->setStyleSheet(checkBoxStyle());
    triggerCard->contentLayout()->addWidget(statusOverlayCheck_);
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
    loopEdit_->setPlaceholderText("0");
    loopEdit_->setValidator(new QIntValidator(0, 99999, this));
    modeCard->contentLayout()->addWidget(loopEdit_);
    rightLayout->addWidget(modeCard);

    rightLayout->addStretch();
    layout->addLayout(rightLayout, 3);

    // ── Connections ──
    connect(addBtn_, &QPushButton::clicked, this, &MacroPage::addStepRequested);
    connect(startBtn_, &QPushButton::clicked, this, &MacroPage::startStopClicked);
    connect(stopBtn_, &QPushButton::clicked, this, &MacroPage::startStopClicked);
    connect(beepCheck_, &QCheckBox::toggled, this, &MacroPage::beepToggled);
    connect(statusOverlayCheck_, &QCheckBox::toggled, this, &MacroPage::statusOverlayToggled);
    connect(soundSettingsBtn_, &QPushButton::clicked, this, &MacroPage::soundSettingsClicked);
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
        loopEdit_->clearFocus();
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
    startBtn_->setEnabled(!running);
    stopBtn_->setEnabled(running);
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
}

void MacroPage::setHotkeyText(const QString& text) {
    hotkeyEdit_->setText(text);
    hotkeyName_ = text;
    // Update button text with hotkey hint
    QString keyStr = (hotkeyName_.isEmpty() || hotkeyName_ == QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"))
        ? "" : QString(" (%1)").arg(hotkeyName_);
    startBtn_->setText(QString::fromWCharArray(L"\u25B6  \u542f\u52a8") + keyStr);
    stopBtn_->setText(QString::fromWCharArray(L"\u25A0  \u505c\u6b62") + keyStr);
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
    beepCheck_->setStyleSheet(checkBoxStyle());
    if (statusOverlayCheck_) statusOverlayCheck_->setStyleSheet(checkBoxStyle());
    refreshTable();
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
    macroDeleteBtn_->setEnabled(macros.size() > 1);
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
        handleItem->setToolTip(QString::fromWCharArray(L"\u62D6\u52A8\u6392\u5E8F"));
        table_->setItem(i, 0, handleItem);

        // Key name
        auto* keyItem = new QTableWidgetItem(QString::fromStdWString(GetKeyDisplayName(step.key)));
        keyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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
