#include "KeyPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FluentTable.h"
#include "../components/FluentButton.h"
#include "../components/FluentLineEdit.h"
#include "../components/FluentSwitch.h"
#include "../components/FluentCard.h"
#include "../components/FluentComboBox.h"
#include "../components/FluentCheckBox.h"
#include "FactionTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QEvent>
#include <QCheckBox>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kTriggerCardHeight = 162;

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
    profileAddBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE710)), this);  // + icon
    profileAddBtn_->setFixedSize(28, 28);
    profileAddBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileAddBtn_->setToolTip(QString::fromWCharArray(L"\u65b0\u5efa\u914d\u7f6e"));
    profileRow->addWidget(profileAddBtn_);
    profileRenameBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE8AC)), this);  // rename icon
    profileRenameBtn_->setFixedSize(28, 28);
    profileRenameBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileRenameBtn_->setToolTip(QString::fromWCharArray(L"\u91cd\u547d\u540d"));
    profileRow->addWidget(profileRenameBtn_);
    profileDeleteBtn_ = new FluentButton(FluentButton::Icon, QString(QChar(0xE74D)), this);  // delete icon
    profileDeleteBtn_->setFixedSize(28, 28);
    profileDeleteBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    profileDeleteBtn_->setToolTip(QString::fromWCharArray(L"\u5220\u9664\u914d\u7f6e"));
    profileRow->addWidget(profileDeleteBtn_);
    tableCard->contentLayout()->addLayout(profileRow);
    tableCard->contentLayout()->addSpacing(8);

    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(QString::fromWCharArray(L"\u6309\u952e\u5217\u8868"), this);
    title->setFont(Fluent::Typography::pageTitle());
    headerRow->addWidget(title);
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

    auto* pauseRow = new QHBoxLayout();
    pauseRow->setSpacing(6);
    auto* pauseLabel = new QLabel(QString::fromWCharArray(L"\u6682\u505c\u952e:"), this);
    pauseLabel->setFont(Fluent::Typography::caption());
    QPalette pauseLabelPal = pauseLabel->palette();
    pauseLabelPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    pauseLabel->setPalette(pauseLabelPal);
    pauseLabel->setToolTip(QString::fromWCharArray(L"\u6309\u4f4f\u6682\u505c\u952e\u65f6\uff0c\u6240\u6709\u5f00\u5173\u6a21\u5f0f\u8fde\u53d1\u6682\u505c\uff0c\u677e\u5f00\u540e\u6062\u590d"));
    pauseRow->addWidget(pauseLabel);

    pauseKeyListWidget_ = new QWidget(this);
    pauseKeyListLayout_ = new QHBoxLayout(pauseKeyListWidget_);
    pauseKeyListLayout_->setContentsMargins(0, 0, 0, 0);
    pauseKeyListLayout_->setSpacing(4);
    pauseRow->addWidget(pauseKeyListWidget_, 1);

    auto* pauseAddBtn = new FluentButton(FluentButton::Icon, QString(QChar(0xE710)), this);
    pauseAddBtn->setFixedSize(24, 24);
    pauseAddBtn->setFont(QFont("Segoe MDL2 Assets", 9));
    pauseAddBtn->setToolTip(QString::fromWCharArray(L"\u6dfb\u52a0\u6682\u505c\u952e"));
    pauseRow->addWidget(pauseAddBtn);
    tableCard->contentLayout()->addLayout(pauseRow);

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
    controlCard->contentLayout()->addSpacing(4);
    statusLabel_ = new QLabel(QString::fromWCharArray(L"\u72b6\u6001: \u5df2\u505c\u6b62"), this);
    statusLabel_->setFont(Fluent::Typography::caption());
    QPalette sPal = statusLabel_->palette();
    sPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    statusLabel_->setPalette(sPal);
    controlCard->contentLayout()->addWidget(statusLabel_);
    rightLayout->addWidget(controlCard);

    // -- Card 2: startup trigger --
    auto* triggerCard = new FluentCard(QString::fromWCharArray(L"\u542f\u52a8\u89e6\u53d1"), this);
    triggerCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    triggerCard->setFixedHeight(kTriggerCardHeight);
    triggerCard->layout()->setContentsMargins(12, 6, 12, 6);
    triggerCard->contentLayout()->setSpacing(4);
    auto* hotkeyLabel = new QLabel(QString::fromWCharArray(L"\u542f\u505c\u5feb\u6377\u952e"), this);
    hotkeyLabel->setFont(Fluent::Typography::caption());
    QPalette hPal = hotkeyLabel->palette();
    hPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    hotkeyLabel->setPalette(hPal);
    triggerCard->contentLayout()->addWidget(hotkeyLabel);
    hotkeyEdit_ = new FluentLineEdit(this);
    hotkeyEdit_->setReadOnly(true);
    hotkeyEdit_->setPlaceholderText(QString::fromWCharArray(L"\u70b9\u51fb\u5f55\u5236\u6309\u952e..."));
    auto* hotkeyRow = new QHBoxLayout();
    hotkeyRow->setSpacing(6);
    hotkeyRow->addWidget(hotkeyEdit_, 1);
    hotkeyClearBtn_ = new FluentButton(FluentButton::Secondary, QString(QChar(0xE74D)), this);
    hotkeyClearBtn_->setFixedSize(32, 32);
    hotkeyClearBtn_->setFont(QFont("Segoe MDL2 Assets", 10));
    hotkeyClearBtn_->setToolTip(QString::fromWCharArray(L"\u5220\u9664\u542f\u505c\u5feb\u6377\u952e"));
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
    soundSettingsBtn_->setToolTip(QString::fromWCharArray(L"\u97f3\u6548\u8bbe\u7f6e"));
    beepRow->addWidget(soundSettingsBtn_);
    triggerCard->contentLayout()->addLayout(beepRow);
    statusOverlayCheck_ = new FluentCheckBox(QString::fromWCharArray(L"\u663e\u793a\u5230\u6d6e\u7a97"), this);
    statusOverlayCheck_->setFont(Fluent::Typography::body());
    triggerCard->contentLayout()->addWidget(statusOverlayCheck_);
    rightLayout->addWidget(triggerCard);

    // -- Card 3: 连发优先 --
    auto* mixedCard = new FluentCard(QString::fromWCharArray(L"\u8bbe\u7f6e"), this);
    mixedModeCheck_ = new FluentCheckBox(QString::fromWCharArray(L"\u542f\u7528\u8fde\u53d1\u4f18\u5148"), this);
    mixedModeCheck_->setFont(Fluent::Typography::body());
    mixedModeCheck_->setToolTip(QString::fromWCharArray(L"\u5f00\u542f\u540e\uff0c\u6309\u4f4f\u6309\u538b\u6a21\u5f0f\u952e\u65f6\uff0c\u4f1a\u6682\u505c\u6240\u6709\u5f00\u5173\u6a21\u5f0f\u8fde\u53d1\uff0c\u677e\u5f00\u540e\u6062\u590d\u3002"));
    mixedCard->contentLayout()->addWidget(mixedModeCheck_);
    mixedCard->contentLayout()->addSpacing(2);
    auto* mixedDesc = new QLabel(QString::fromWCharArray(L"\u6309\u4f4f\u6309\u538b\u952e\u65f6\uff0c\u6682\u505c\u5f00\u5173\u8fde\u53d1\u3002"), this);
    mixedDesc->setFont(Fluent::Typography::caption());
    mixedDesc->setWordWrap(true);
    QPalette mPal = mixedDesc->palette();
    mPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    mixedDesc->setPalette(mPal);
    mixedCard->contentLayout()->addWidget(mixedDesc);
    mixedCard->contentLayout()->addSpacing(8);
    auto* wasdSeparator = new QWidget(this);
    wasdSeparator->setFixedHeight(1);
    wasdSeparator->setStyleSheet(QString("background: %1;").arg(Fluent::Colors::divider().name(QColor::HexArgb)));
    mixedCard->contentLayout()->addWidget(wasdSeparator);
    mixedCard->contentLayout()->addSpacing(8);
    wasdProtectionCheck_ = new FluentCheckBox(QString::fromWCharArray(L"WASD \u9632\u5361\u4f4d"), this);
    wasdProtectionCheck_->setFont(Fluent::Typography::body());
    wasdProtectionCheck_->setToolTip(QString::fromWCharArray(L"\u6309\u4f4f W/A/S/D \u65f6\uff0c\u4e34\u65f6\u964d\u4f4e\u5f00\u5173\u8fde\u53d1\u9891\u7387\uff0c\u677e\u5f00\u540e\u6062\u590d\u3002"));
    mixedCard->contentLayout()->addWidget(wasdProtectionCheck_);
    mixedCard->contentLayout()->addSpacing(2);
    auto* wasdDesc = new QLabel(QString::fromWCharArray(L"\u79fb\u52a8\u65f6\u4e34\u65f6\u964d\u901f\uff0c\u677e\u5f00\u6062\u590d\u3002"), this);
    wasdDesc->setFont(Fluent::Typography::caption());
    wasdDesc->setWordWrap(true);
    QPalette wPal = wasdDesc->palette();
    wPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    wasdDesc->setPalette(wPal);
    mixedCard->contentLayout()->addWidget(wasdDesc);
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
    startBtn_->setEnabled(true);
    startBtn_->setButtonStyle(running ? FluentButton::Danger : FluentButton::Primary);
    updateStartBtnText();
    statusLabel_->setText(running
        ? QString::fromWCharArray(L"\u72b6\u6001: \u8fd0\u884c\u4e2d")
        : QString::fromWCharArray(L"\u72b6\u6001: \u5df2\u505c\u6b62"));

    // Disable editing controls while running
    addBtn_->setEnabled(!running);
    hotkeyEdit_->setEnabled(!running);
    profileCombo_->setEnabled(!running);
    profileAddBtn_->setEnabled(!running);
    profileRenameBtn_->setEnabled(!running);
    profileDeleteBtn_->setEnabled(!running);
    table_->setEnabled(!running);
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

void KeyPage::updateThemeColors() {
    applyPageTheme();
    table_->refreshThemeStyle();
    profileCombo_->refreshThemeStyle();
    beepCheck_->update();
    if (statusOverlayCheck_) statusOverlayCheck_->update();
    mixedModeCheck_->update();
    if (wasdProtectionCheck_) wasdProtectionCheck_->update();
    // Table checkboxes will be updated on next refreshTable()
    refreshTable();
}

void KeyPage::applyPageTheme() {
    const auto palette = FactionTheme::instance().palette();
    setStyleSheet(QString("KeyPage { background: %1; }").arg(palette.surfaceTint.name()));
}

void KeyPage::setPauseKeys(const std::vector<KeyDescriptor>& keys) {
    pauseKeys_ = keys;
    refreshPauseKeyList();
}

void KeyPage::refreshPauseKeyList() {
    // Clear existing items
    QLayoutItem* item;
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
        QString mixedTip = QString::fromWCharArray(L"\u5f00\u5173\u6a21\u5f0f\u4e0d\u53c2\u4e0e\u8fde\u53d1\u4f18\u5148");
        if (b.mode == RepeatMode::Hold) {
            mixedKind = b.suppressMixedMode
                ? PriorityIconWidget::Kind::Disabled
                : PriorityIconWidget::Kind::Enabled;
            mixedTip = b.suppressMixedMode
                ? QString::fromWCharArray(L"\u4e0d\u53c2\u4e0e\uff1a\u6309\u4f4f\u8be5\u6309\u538b\u952e\u4e0d\u4f1a\u6682\u505c\u5f00\u5173\u8fde\u53d1")
                : QString::fromWCharArray(L"\u53c2\u4e0e\uff1a\u6309\u4f4f\u8be5\u6309\u538b\u952e\u4f1a\u6682\u505c\u5f00\u5173\u8fde\u53d1");
        }
        auto* mixedWidget = new PriorityIconWidget(mixedKind);
        mixedWidget->setToolTip(mixedTip);
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
    if (watched == hotkeyEdit_ && event->type() == QEvent::MouseButtonPress) {
        if (running_) return true;  // Ignore clicks while running
        emit hotkeyEditClicked();
        return true;
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
