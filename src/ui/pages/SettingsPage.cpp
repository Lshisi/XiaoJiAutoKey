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
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

// 支持的音频格式列表（MCI可播放的格式）
static const QStringList kSupportedAudioExtensions = {
    "mp3", "wav", "wma", "mid", "midi"
};

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

    // ── Inner card 1: 提示音设置（常用） ──
    auto* soundCard = new FluentCard(QString::fromWCharArray(L"\u63d0\u793a\u97f3\u8bbe\u7f6e"), this);

    auto* soundDesc = new QLabel(QString::fromWCharArray(L"\u81ea\u5b9a\u4e49\u5f00\u542f/\u5173\u95ed\u63d0\u793a\u97f3\uff0c\u652f\u6301 mp3\u3001wav\u3001wma \u683c\u5f0f"), this);
    soundDesc->setFont(Fluent::Typography::caption());
    soundDesc->setWordWrap(true);
    QPalette sdPal = soundDesc->palette();
    sdPal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    soundDesc->setPalette(sdPal);
    soundCard->contentLayout()->addWidget(soundDesc);
    soundCard->contentLayout()->addSpacing(8);

    // 开启提示音
    auto* soundOnLabel = new QLabel(QString::fromWCharArray(L"\u5f00\u542f\u63d0\u793a\u97f3:"), this);
    soundOnLabel->setFont(Fluent::Typography::body());
    soundCard->contentLayout()->addWidget(soundOnLabel);
    auto* soundOnRow = new QHBoxLayout();
    soundOnPathEdit_ = new FluentLineEdit(this);
    soundOnPathEdit_->setReadOnly(true);
    soundOnPathEdit_->setPlaceholderText(QString::fromWCharArray(L"\u9ed8\u8ba4\u5185\u7f6e\u97f3\u6548"));
    soundOnRow->addWidget(soundOnPathEdit_, 1);
    auto* soundOnBrowseBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u6d4f\u89c8"), this);
    soundOnBrowseBtn->setFixedHeight(28);
    soundOnBrowseBtn->setFixedWidth(60);
    soundOnRow->addWidget(soundOnBrowseBtn);
    auto* soundOnResetBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u91cd\u7f6e"), this);
    soundOnResetBtn->setFixedHeight(28);
    soundOnResetBtn->setFixedWidth(60);
    soundOnRow->addWidget(soundOnResetBtn);
    soundCard->contentLayout()->addLayout(soundOnRow);
    soundCard->contentLayout()->addSpacing(8);

    // 关闭提示音
    auto* soundOffLabel = new QLabel(QString::fromWCharArray(L"\u5173\u95ed\u63d0\u793a\u97f3:"), this);
    soundOffLabel->setFont(Fluent::Typography::body());
    soundCard->contentLayout()->addWidget(soundOffLabel);
    auto* soundOffRow = new QHBoxLayout();
    soundOffPathEdit_ = new FluentLineEdit(this);
    soundOffPathEdit_->setReadOnly(true);
    soundOffPathEdit_->setPlaceholderText(QString::fromWCharArray(L"\u9ed8\u8ba4\u5185\u7f6e\u97f3\u6548"));
    soundOffRow->addWidget(soundOffPathEdit_, 1);
    auto* soundOffBrowseBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u6d4f\u89c8"), this);
    soundOffBrowseBtn->setFixedHeight(28);
    soundOffBrowseBtn->setFixedWidth(60);
    soundOffRow->addWidget(soundOffBrowseBtn);
    auto* soundOffResetBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u91cd\u7f6e"), this);
    soundOffResetBtn->setFixedHeight(28);
    soundOffResetBtn->setFixedWidth(60);
    soundOffRow->addWidget(soundOffResetBtn);
    soundCard->contentLayout()->addLayout(soundOffRow);

    outerCard->contentLayout()->addWidget(soundCard);
    outerCard->contentLayout()->addSpacing(12);

    // ── Inner card 2: 门派主题（常用） ──
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
    auto* changeBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u81ea\u5b9a\u4e49\u8def\u5f84"), this);
    changeBtn->setFixedHeight(28);
    auto* resetBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u6062\u590d\u9ed8\u8ba4"), this);
    resetBtn->setFixedHeight(28);
    configBtnRow->addWidget(openBtn);
    configBtnRow->addWidget(changeBtn);
    configBtnRow->addWidget(resetBtn);
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
    connect(changeBtn, &QPushButton::clicked, this, &SettingsPage::changeConfigPath);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsPage::resetConfigPath);
    connect(factionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) return;
        QString factionId = factionCombo_->itemData(index).toString();
        emit factionChanged(factionId);
    });

    // Sound browse/reset connections
    connect(soundOnBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this,
            QString::fromWCharArray(L"\u9009\u62e9\u5f00\u542f\u63d0\u793a\u97f3"),
            QString(),
            QString::fromWCharArray(L"\u97f3\u9891\u6587\u4ef6 (*.mp3 *.wav *.wma *.mid *.midi)"));
        if (filePath.isEmpty()) return;
        if (!isValidAudioFile(filePath)) {
            QMessageBox::warning(this, QString::fromWCharArray(L"\u683c\u5f0f\u4e0d\u652f\u6301"),
                QString::fromWCharArray(L"\u8bf7\u9009\u62e9\u652f\u6301\u7684\u97f3\u9891\u683c\u5f0f\uff1amp3\u3001wav\u3001wma\u3001mid"));
            return;
        }
        soundOnPathEdit_->setText(filePath);
        emit customSoundOnChanged(filePath);
    });
    connect(soundOffBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this,
            QString::fromWCharArray(L"\u9009\u62e9\u5173\u95ed\u63d0\u793a\u97f3"),
            QString(),
            QString::fromWCharArray(L"\u97f3\u9891\u6587\u4ef6 (*.mp3 *.wav *.wma *.mid *.midi)"));
        if (filePath.isEmpty()) return;
        if (!isValidAudioFile(filePath)) {
            QMessageBox::warning(this, QString::fromWCharArray(L"\u683c\u5f0f\u4e0d\u652f\u6301"),
                QString::fromWCharArray(L"\u8bf7\u9009\u62e9\u652f\u6301\u7684\u97f3\u9891\u683c\u5f0f\uff1amp3\u3001wav\u3001wma\u3001mid"));
            return;
        }
        soundOffPathEdit_->setText(filePath);
        emit customSoundOffChanged(filePath);
    });
    connect(soundOnResetBtn, &QPushButton::clicked, this, [this]() {
        soundOnPathEdit_->clear();
        emit customSoundOnReset();
    });
    connect(soundOffResetBtn, &QPushButton::clicked, this, [this]() {
        soundOffPathEdit_->clear();
        emit customSoundOffReset();
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

void SettingsPage::setCustomSoundPaths(const QString& onPath, const QString& offPath) {
    soundOnPathEdit_->setText(onPath);
    soundOffPathEdit_->setText(offPath);
}

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

bool SettingsPage::isValidAudioFile(const QString& path) {
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) return false;
    QString ext = info.suffix().toLower();
    return kSupportedAudioExtensions.contains(ext);
}

bool SettingsPage::eventFilter(QObject* watched, QEvent* event) {
    return QWidget::eventFilter(watched, event);
}
