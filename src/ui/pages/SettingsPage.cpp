#include "SettingsPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FluentButton.h"
#include "../components/FluentCard.h"
#include "../components/FluentComboBox.h"
#include "FactionTheme.h"

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

namespace {

QLabel* makeSectionTitle(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setFont(Fluent::Typography::cardTitle());
    return label;
}

QWidget* makeDivider(QWidget* parent) {
    auto* divider = new QWidget(parent);
    divider->setFixedHeight(1);
    divider->setStyleSheet("background: rgba(0,0,0,0.06);");
    return divider;
}

QPalette mutedPalette(QWidget* widget) {
    QPalette pal = widget->palette();
    pal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    return pal;
}

}

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

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

    auto* panel = new FluentCard(scrollContent);
    auto* panelLayout = panel->contentLayout();
    panelLayout->setSpacing(14);

    auto* pageTitle = new QLabel(QString::fromWCharArray(L"\u5168\u5c40\u8bbe\u7f6e"), panel);
    pageTitle->setFont(Fluent::Typography::pageTitle());
    panelLayout->addWidget(pageTitle);

    auto* factionPreview = new QWidget(panel);
    factionPreview->setObjectName("factionPreview");
    factionPreview->setAttribute(Qt::WA_StyledBackground, true);
    auto* factionLayout = new QHBoxLayout(factionPreview);
    factionLayout->setContentsMargins(12, 10, 12, 10);
    factionLayout->setSpacing(10);

    factionPreviewIcon_ = new QLabel(factionPreview);
    factionPreviewIcon_->setFixedSize(36, 36);
    factionPreviewIcon_->setAlignment(Qt::AlignCenter);
    factionLayout->addWidget(factionPreviewIcon_);

    auto* factionTextLayout = new QVBoxLayout();
    factionTextLayout->setContentsMargins(0, 0, 0, 0);
    factionTextLayout->setSpacing(3);
    auto* factionCaption = new QLabel(QString::fromWCharArray(L"\u95e8\u6d3e\u4e3b\u9898"), factionPreview);
    factionCaption->setFont(Fluent::Typography::caption());
    factionCaption->setPalette(mutedPalette(factionCaption));
    factionTextLayout->addWidget(factionCaption);
    factionPreviewName_ = new QLabel(factionPreview);
    factionPreviewName_->setFont(Fluent::Typography::bodyStrong());
    factionTextLayout->addWidget(factionPreviewName_);
    factionPreviewBar_ = new QWidget(factionPreview);
    factionPreviewBar_->setFixedHeight(3);
    factionTextLayout->addWidget(factionPreviewBar_);
    factionLayout->addLayout(factionTextLayout, 1);

    factionCombo_ = new FluentComboBox(factionPreview);
    factionCombo_->setMinimumWidth(120);
    factionCombo_->setFixedHeight(28);
    const auto& factions = FactionTheme::instance().factions();
    for (const auto& f : factions) factionCombo_->addItem(f.name, f.id);
    factionLayout->addWidget(factionCombo_);
    panelLayout->addWidget(factionPreview);

    panelLayout->addWidget(makeSectionTitle(QString::fromWCharArray(L"\u72b6\u6001\u6d6e\u7a97"), panel));
    auto* opacityRow = new QHBoxLayout();
    auto* opacityLabel = new QLabel(QString::fromWCharArray(L"\u80cc\u666f\u900f\u660e\u5ea6"), panel);
    opacityLabel->setFont(Fluent::Typography::body());
    opacityRow->addWidget(opacityLabel);
    opacityRow->addStretch();
    statusOverlayOpacityValue_ = new QLabel(panel);
    statusOverlayOpacityValue_->setFont(Fluent::Typography::caption());
    statusOverlayOpacityValue_->setPalette(mutedPalette(statusOverlayOpacityValue_));
    opacityRow->addWidget(statusOverlayOpacityValue_);
    panelLayout->addLayout(opacityRow);

    statusOverlayOpacitySlider_ = new QSlider(Qt::Horizontal, panel);
    statusOverlayOpacitySlider_->setRange(30, 100);
    statusOverlayOpacitySlider_->setSingleStep(5);
    statusOverlayOpacitySlider_->setPageStep(10);
    panelLayout->addWidget(statusOverlayOpacitySlider_);

    auto* opacityDesc = new QLabel(QString::fromWCharArray(L"\u6570\u503c\u8d8a\u4f4e\u8d8a\u900f\u660e\uff0c\u9501\u5b9a\u6309\u94ae\u5728\u6d6e\u7a97\u53f3\u4e0a\u89d2"), panel);
    opacityDesc->setFont(Fluent::Typography::caption());
    opacityDesc->setWordWrap(true);
    opacityDesc->setPalette(mutedPalette(opacityDesc));
    panelLayout->addWidget(opacityDesc);
    panelLayout->addWidget(makeDivider(panel));

    panelLayout->addWidget(makeSectionTitle(QString::fromWCharArray(L"\u5173\u95ed\u7a97\u53e3\u65f6"), panel));
    radioAsk_ = new QRadioButton(QString::fromWCharArray(L"\u6bcf\u6b21\u8be2\u95ee"), panel);
    radioMin_ = new QRadioButton(QString::fromWCharArray(L"\u6700\u5c0f\u5316\u5230\u7cfb\u7edf\u6258\u76d8"), panel);
    radioExit_ = new QRadioButton(QString::fromWCharArray(L"\u76f4\u63a5\u9000\u51fa\u7a0b\u5e8f"), panel);
    radioAsk_->setFont(Fluent::Typography::body());
    radioMin_->setFont(Fluent::Typography::body());
    radioExit_->setFont(Fluent::Typography::body());
    radioAsk_->setChecked(true);
    auto* btnGroup = new QButtonGroup(this);
    btnGroup->addButton(radioAsk_, 0);
    btnGroup->addButton(radioMin_, 1);
    btnGroup->addButton(radioExit_, 2);
    closeActionGroup_ = btnGroup;
    panelLayout->addWidget(radioAsk_);
    panelLayout->addWidget(radioMin_);
    panelLayout->addWidget(radioExit_);
    panelLayout->addWidget(makeDivider(panel));

    panelLayout->addWidget(makeSectionTitle(QString::fromWCharArray(L"\u914d\u7f6e\u6587\u4ef6"), panel));
    configPathLabel_ = new QLabel(panel);
    configPathLabel_->setFont(Fluent::Typography::caption());
    configPathLabel_->setWordWrap(true);
    configPathLabel_->setPalette(mutedPalette(configPathLabel_));
    panelLayout->addWidget(configPathLabel_);
    auto* configBtnRow = new QHBoxLayout();
    auto* openBtn = new FluentButton(FluentButton::Secondary, QString::fromWCharArray(L"\u6253\u5f00\u6587\u4ef6\u5939"), panel);
    openBtn->setFixedHeight(28);
    configBtnRow->addWidget(openBtn);
    configBtnRow->addStretch();
    panelLayout->addLayout(configBtnRow);
    panelLayout->addStretch();

    outerLayout->addWidget(panel);
    outerLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    pageLayout->addWidget(scrollArea);

    connect(btnGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        CloseAction action = CloseAction::Ask;
        if (id == 1) action = CloseAction::Minimize;
        else if (id == 2) action = CloseAction::Exit;
        emit closeActionChanged(action);
    });
    connect(openBtn, &QPushButton::clicked, this, &SettingsPage::openConfigFolder);
    connect(statusOverlayOpacitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (statusOverlayOpacityValue_) statusOverlayOpacityValue_->setText(QString::number(value) + "%");
        emit statusOverlayOpacityChanged(value);
    });
    connect(factionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) return;
        emit factionChanged(factionCombo_->itemData(index).toString());
    });

    applyPageTheme();
}

void SettingsPage::setConfig(const AppConfig& config) {
    if (!statusOverlayOpacitySlider_) return;
    statusOverlayOpacitySlider_->blockSignals(true);
    statusOverlayOpacitySlider_->setValue(config.statusOverlayOpacity);
    statusOverlayOpacitySlider_->blockSignals(false);
    if (statusOverlayOpacityValue_) statusOverlayOpacityValue_->setText(QString::number(config.statusOverlayOpacity) + "%");
}

void SettingsPage::setHotkeyText(const QString&) {}

void SettingsPage::setConfigPath(const QString& path) {
    configPathLabel_->setText(path);
}

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
    applyPageTheme();
}

void SettingsPage::applyPageTheme() {
    const auto palette = FactionTheme::instance().palette();
    setStyleSheet(QString(
        "SettingsPage { background: %1; }"
        "QWidget#factionPreview { background-color: #FFFFFF; border-radius: 8px; }"
    ).arg(palette.surfaceTint.name()));

    const QString radioStyle = QString(
        "QRadioButton { background: transparent; color: #1F1F1F; spacing: 8px; }"
        "QRadioButton::indicator { width: 14px; height: 14px; border-radius: 7px; }"
        "QRadioButton::indicator:unchecked { background: #FFFFFF; border: 1px solid rgba(0,0,0,0.35); }"
        "QRadioButton::indicator:checked {"
        "  border: 1px solid %1;"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.55, fx:0.5, fy:0.5,"
        "    stop:0 %1, stop:0.42 %1, stop:0.46 #FFFFFF, stop:1 #FFFFFF);"
        "}"
    ).arg(palette.accent.name());
    if (radioAsk_) radioAsk_->setStyleSheet(radioStyle);
    if (radioMin_) radioMin_->setStyleSheet(radioStyle);
    if (radioExit_) radioExit_->setStyleSheet(radioStyle);

    if (factionCombo_) factionCombo_->refreshThemeStyle();
    if (factionPreviewName_) factionPreviewName_->setText(FactionTheme::instance().current().name);
    if (factionPreviewBar_) {
        factionPreviewBar_->setStyleSheet(QString("background: %1; border-radius: 1px;").arg(palette.accent.name()));
    }
    if (factionPreviewIcon_) {
        const QString iconPath = FactionTheme::instance().iconPath();
        if (!iconPath.isEmpty()) {
            QPixmap pix(iconPath);
            if (!pix.isNull()) {
                factionPreviewIcon_->setStyleSheet("background: transparent;");
                factionPreviewIcon_->setPixmap(pix.scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        } else {
            factionPreviewIcon_->setPixmap(QPixmap());
            factionPreviewIcon_->setText(QString::fromWCharArray(L"\u5c0f"));
            factionPreviewIcon_->setFont(Fluent::Typography::bodyStrong());
            factionPreviewIcon_->setStyleSheet(QString("background: %1; color: %2; border-radius: 18px;")
                .arg(palette.accent.name(), palette.textOnAccent.name()));
        }
    }
    if (statusOverlayOpacitySlider_) {
        statusOverlayOpacitySlider_->setStyleSheet(QString(
            "QSlider::groove:horizontal { height: 4px; background: rgba(0,0,0,0.12); border-radius: 2px; }"
            "QSlider::sub-page:horizontal { background: %1; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: #202020; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }"
        ).arg(palette.accent.name()));
    }
}

bool SettingsPage::eventFilter(QObject* watched, QEvent* event) {
    return QWidget::eventFilter(watched, event);
}
