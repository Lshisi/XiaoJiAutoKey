#include "SettingsPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FluentCard.h"
#include "../components/FluentButton.h"
#include "../components/FluentLineEdit.h"
#include "../components/FluentComboBox.h"
#include "../../include/FactionTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QEvent>
#include <QScrollArea>

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet("SettingsPage { background: #F5F7FA; }");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    // 使用 QScrollArea 包裹内容，防止窗口缩小时内容被挤压
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(0,0,0,0.12); border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(0,0,0,0.2); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("background: transparent;");
    auto* outerLayout = new QVBoxLayout(scrollContent);
    outerLayout->setContentsMargins(16, 12, 16, 12);
    outerLayout->setSpacing(0);

    // Outer big card
    auto* outerCard = new FluentCard(this);

    // Page title
    auto* pageTitle = new QLabel(QString::fromWCharArray(L"\u5168\u5c40\u8bbe\u7f6e"), this);
    pageTitle->setFont(Fluent::Typography::pageTitle());
    outerCard->contentLayout()->addWidget(pageTitle);
    outerCard->contentLayout()->addSpacing(12);

    // ── Inner card 1: 门派主题（常用） ──
    auto* factionCard = new FluentCard(QString::fromWCharArray(L"\u95e8\u6d3e\u4e3b\u9898"), this);
    auto* factionRow = new QHBoxLayout();
    auto* factionLabel = new QLabel(QString::fromWCharArray(L"\u5f53\u524d\u95e8\u6d3e"), this);
    factionLabel->setFont(Fluent::Typography::body());
    factionRow->addWidget(factionLabel);
    factionRow->addStretch();
    factionCombo_ = new FluentComboBox(this);
    factionCombo_->setMinimumWidth(120);
    factionCombo_->setFixedHeight(28);
    const auto& factions = FactionTheme::instance().factions();
    for (const auto& f : factions) {
        factionCombo_->addItem(f.name, f.id);
    }
    factionRow->addWidget(factionCombo_);
    factionCard->contentLayout()->addLayout(factionRow);
    factionCard->contentLayout()->addSpacing(4);
    auto* factionDesc2 = new QLabel(QString::fromWCharArray(L"\u5207\u6362\u95e8\u6d3e\u540e\uff0c\u754c\u9762\u4e3b\u9898\u8272\u548c\u88c5\u9970\u5143\u7d20\u4f1a\u968f\u4e4b\u53d8\u5316"), this);
    factionDesc2->setFont(Fluent::Typography::caption());
    factionDesc2->setWordWrap(true);
    QPalette fdPal = factionDesc2->palette();
    fdPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    factionDesc2->setPalette(fdPal);
    factionCard->contentLayout()->addWidget(factionDesc2);
    outerCard->contentLayout()->addWidget(factionCard);
    outerCard->contentLayout()->addSpacing(12);

    // ── Inner card 3: 关闭窗口时 ──
    auto* closeCard = new FluentCard(QString::fromWCharArray(L"\u5173\u95ed\u7a97\u53e3\u65f6"), this);
    auto* radioAsk = new QRadioButton(QString::fromWCharArray(L"\u6bcf\u6b21\u8be2\u95ee"), this);
    auto* radioMin = new QRadioButton(QString::fromWCharArray(L"\u6700\u5c0f\u5316\u5230\u7cfb\u7edf\u6258\u76d8"), this);
    auto* radioExit = new QRadioButton(QString::fromWCharArray(L"\u76f4\u63a5\u9000\u51fa\u7a0b\u5e8f"), this);
    radioAsk->setFont(Fluent::Typography::body());
    radioMin->setFont(Fluent::Typography::body());
    radioExit->setFont(Fluent::Typography::body());
    radioAsk->setChecked(true);
    auto* btnGroup = new QButtonGroup(this);
    btnGroup->addButton(radioAsk, 0);
    btnGroup->addButton(radioMin, 1);
    btnGroup->addButton(radioExit, 2);
    closeActionGroup_ = btnGroup;
    closeCard->contentLayout()->addWidget(radioAsk);
    closeCard->contentLayout()->addWidget(radioMin);
    closeCard->contentLayout()->addWidget(radioExit);
    outerCard->contentLayout()->addWidget(closeCard);
    outerCard->contentLayout()->addSpacing(12);

    // ── Inner card 4: 配置文件 ──
    auto* configCard = new FluentCard(QString::fromWCharArray(L"\u914d\u7f6e\u6587\u4ef6"), this);
    configPathLabel_ = new QLabel(this);
    configPathLabel_->setFont(Fluent::Typography::caption());
    configPathLabel_->setWordWrap(true);
    QPalette cpPal = configPathLabel_->palette();
    cpPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    configPathLabel_->setPalette(cpPal);
    configCard->contentLayout()->addWidget(configPathLabel_);
    configCard->contentLayout()->addSpacing(6);
    auto* configBtnRow = new QHBoxLayout();
    auto* openBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u6253\u5f00\u6587\u4ef6\u5939"), this);
    openBtn->setFixedHeight(28);
    configBtnRow->addWidget(openBtn);
    configBtnRow->addStretch();
    configCard->contentLayout()->addLayout(configBtnRow);
    outerCard->contentLayout()->addWidget(configCard);

    outerCard->contentLayout()->addStretch();
    outerLayout->addWidget(outerCard, 1);

    scrollArea->setWidget(scrollContent);
    pageLayout->addWidget(scrollArea);

    // Connections
    connect(btnGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        CloseAction action = CloseAction::Ask;
        if (id == 1) action = CloseAction::Minimize;
        else if (id == 2) action = CloseAction::Exit;
        emit closeActionChanged(action);
    });
    connect(openBtn, &QPushButton::clicked, this, &SettingsPage::openConfigFolder);
    connect(factionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) return;
        QString factionId = factionCombo_->itemData(index).toString();
        emit factionChanged(factionId);
    });

}

void SettingsPage::setConfig(const AppConfig&) {}
void SettingsPage::setHotkeyText(const QString&) {}
void SettingsPage::setConfigPath(const QString& path) { configPathLabel_->setText(path); }

void SettingsPage::setCloseAction(CloseAction action) {
    if (!closeActionGroup_) return;
    int id = 0;
    if (action == CloseAction::Minimize) id = 1;
    else if (action == CloseAction::Exit) id = 2;
    closeActionGroup_->blockSignals(true);
    if (auto* btn = closeActionGroup_->button(id)) btn->setChecked(true);
    closeActionGroup_->blockSignals(false);
}

void SettingsPage::setCustomSoundPaths(const QString&, const QString&) {}

void SettingsPage::setFaction(const QString& factionId) {
    factionCombo_->blockSignals(true);
    for (int i = 0; i < factionCombo_->count(); ++i) {
        if (factionCombo_->itemData(i).toString() == factionId) {
            factionCombo_->setCurrentIndex(i);
            break;
        }
    }
    factionCombo_->blockSignals(false);
}

bool SettingsPage::eventFilter(QObject* watched, QEvent* event) {
    return QWidget::eventFilter(watched, event);
}
