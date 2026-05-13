#include "KeyPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FluentTable.h"
#include "../components/FluentButton.h"
#include "../components/FluentLineEdit.h"
#include "../components/FluentSwitch.h"
#include "../components/FluentCard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QEvent>
#include <QCheckBox>

// Common checkbox style with white checkmark on blue background
static const char* kCheckBoxStyle =
    "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px; }"
    "QCheckBox::indicator:checked {"
    "  background: #0078D4; border: 1px solid #0078D4;"
    "  image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxMiIgaGVpZ2h0PSIxMiIgdmlld0JveD0iMCAwIDEyIDEyIj48cGF0aCBkPSJNMTAgM0w1IDhMMiA1IiBmaWxsPSJub25lIiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjIiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIgc3Ryb2tlLWxpbmVqb2luPSJyb3VuZCIvPjwvc3ZnPg==);"
    "}"
    "QCheckBox::indicator:unchecked {"
    "  background: #FFFFFF; border: 1.5px solid rgba(0,0,0,0.3); }"
    ;

KeyPage::KeyPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet("KeyPage { background: #F5F7FA; }");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    // ═══ Left: Table Card ═══
    auto* tableCard = new FluentCard(this);

    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(QString::fromWCharArray(L"\u70ed\u952e\u5217\u8868"), this);
    title->setFont(Fluent::Typography::pageTitle());
    headerRow->addWidget(title);
    headerRow->addStretch();
    auto* addBtn = new FluentButton(FluentButton::Primary, QString::fromWCharArray(L"+  \u6dfb\u52a0\u6309\u952e"), this);
    addBtn->setFixedHeight(30);
    addBtn->setMinimumWidth(100);
    headerRow->addWidget(addBtn);
    tableCard->contentLayout()->addLayout(headerRow);

    table_ = new FluentTable(0, 5, this);
    table_->setHorizontalHeaderLabels({
        QString::fromWCharArray(L"\u72b6\u6001"),
        QString::fromWCharArray(L"\u6309\u952e"),
        QString::fromWCharArray(L"\u6a21\u5f0f"),
        QString::fromWCharArray(L"\u5ef6\u8fdf (ms)"),
        QString::fromWCharArray(L"\u64cd\u4f5c")
    });
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    table_->setColumnWidth(0, 50);
    table_->setColumnWidth(2, 80);
    table_->setColumnWidth(3, 72);
    table_->setColumnWidth(4, 80);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setStyleSheet(table_->styleSheet() +
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(0,0,0,0.12); border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(0,0,0,0.2); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );
    tableCard->contentLayout()->addWidget(table_, 1);
    layout->addWidget(tableCard, 5);

    // ═══ Right panel ═══
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
    stopBtn_->setEnabled(false);  // Initially disabled (not running)
    controlCard->contentLayout()->addWidget(stopBtn_);
    controlCard->contentLayout()->addSpacing(4);
    statusLabel_ = new QLabel(QString::fromWCharArray(L"\u72b6\u6001: \u5df2\u505c\u6b62"), this);
    statusLabel_->setFont(Fluent::Typography::caption());
    QPalette sPal = statusLabel_->palette();
    sPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    statusLabel_->setPalette(sPal);
    controlCard->contentLayout()->addWidget(statusLabel_);
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
    beepCheck_ = new QCheckBox(QString::fromWCharArray(L"\u5f00\u542f\u63d0\u793a\u97f3"), this);
    beepCheck_->setFont(Fluent::Typography::body());
    beepCheck_->setStyleSheet(kCheckBoxStyle);
    triggerCard->contentLayout()->addWidget(beepCheck_);
    rightLayout->addWidget(triggerCard);

    rightLayout->addStretch();
    layout->addLayout(rightLayout, 3);

    // ═══ Connections ═══
    connect(addBtn, &QPushButton::clicked, this, &KeyPage::addRequested);
    connect(startBtn_, &QPushButton::clicked, this, &KeyPage::startStopClicked);
    connect(stopBtn_, &QPushButton::clicked, this, &KeyPage::startStopClicked);
    connect(beepCheck_, &QCheckBox::toggled, this, &KeyPage::beepToggled);
    hotkeyEdit_->installEventFilter(this);

    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item->column() == 0) {
            auto id = item->data(Qt::UserRole).toULongLong();
            emit enableToggled(id, item->checkState() == Qt::Checked);
        }
    });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < (int)bindings_.size()) emit editRequested(bindings_[row].id);
    });
}

void KeyPage::setBindings(const std::vector<KeyBinding>& bindings) {
    bindings_ = bindings;
    refreshTable();
}

void KeyPage::setRunning(bool running) {
    running_ = running;
    // Enable/disable buttons based on state
    startBtn_->setEnabled(!running);
    stopBtn_->setEnabled(running);
    statusLabel_->setText(running
        ? QString::fromWCharArray(L"\u72b6\u6001: \u8fd0\u884c\u4e2d")
        : QString::fromWCharArray(L"\u72b6\u6001: \u5df2\u505c\u6b62"));
}

void KeyPage::setHotkeyText(const QString& text) {
    hotkeyEdit_->setText(text);
    hotkeyName_ = text;
    updateStartBtnText();
}

void KeyPage::setBeepEnabled(bool enabled) {
    beepCheck_->blockSignals(true);
    beepCheck_->setChecked(enabled);
    beepCheck_->blockSignals(false);
}

void KeyPage::updateStartBtnText() {
    QString keyStr = (hotkeyName_.isEmpty() || hotkeyName_ == QString::fromWCharArray(L"\u672a\u8bbe\u7f6e"))
        ? "" : QString(" (%1)").arg(hotkeyName_);
    startBtn_->setText(QString::fromWCharArray(L"\u25B6  \u542f\u52a8") + keyStr);
    stopBtn_->setText(QString::fromWCharArray(L"\u25A0  \u505c\u6b62") + keyStr);
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
        auto* checkBox = new QCheckBox(checkWidget);
        checkBox->setChecked(b.enabled);
        checkBox->setStyleSheet(kCheckBoxStyle);
        checkLayout->addWidget(checkBox);
        table_->setCellWidget(i, 0, checkWidget);

        auto* hiddenItem = new QTableWidgetItem();
        hiddenItem->setData(Qt::UserRole, static_cast<qulonglong>(b.id));
        table_->setItem(i, 0, hiddenItem);

        table_->setItem(i, 1, new QTableWidgetItem(QString::fromStdWString(GetKeyDisplayName(b.key))));
        table_->setItem(i, 2, new QTableWidgetItem(QString::fromStdWString(RepeatModeToText(b.mode))));
        table_->setItem(i, 3, new QTableWidgetItem(QString::number(b.delayMs)));

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
        table_->setCellWidget(i, 4, actionWidget);

        std::uint64_t id = b.id;
        connect(checkBox, &QCheckBox::toggled, this, [this, id](bool checked) { emit enableToggled(id, checked); });
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { emit editRequested(id); });
        connect(delBtn, &QPushButton::clicked, this, [this, id]() { emit deleteRequested(id); });
    }
    table_->blockSignals(false);
}

bool KeyPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == hotkeyEdit_ && event->type() == QEvent::MouseButtonPress) {
        emit hotkeyEditClicked();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
