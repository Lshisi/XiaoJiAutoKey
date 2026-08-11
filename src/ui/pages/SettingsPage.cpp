#include "SettingsPage.h"
#include "../styles/DesignTokens.h"
#include "../components/FactionIcon.h"
#include "../components/FactionMenu.h"
#include "../components/FluentButton.h"
#include "../components/FluentCard.h"
#include "FactionTheme.h"

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

namespace {

constexpr int kSettingsActionButtonWidth = 120;
constexpr int kSettingsActionButtonHeight = 28;

QLabel* makeSectionTitle(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setFont(Fluent::Typography::cardTitle());
    return label;
}

QWidget* makeDivider(QWidget* parent) {
    auto* divider = new QWidget(parent);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QString("background: %1;").arg(Fluent::Colors::divider().name(QColor::HexArgb)));
    return divider;
}

class FactionSelectButton : public QPushButton {
public:
    explicit FactionSelectButton(QWidget* parent = nullptr)
        : QPushButton(parent) {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(kSettingsActionButtonWidth, kSettingsActionButtonHeight);
        setFont(Fluent::Typography::body());
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void enterEvent(QEvent* event) override {
        hovered_ = true;
        QPushButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent* event) override {
        hovered_ = false;
        QPushButton::leaveEvent(event);
        update();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const auto palette = FactionTheme::instance().palette();
        const QColor bg = hovered_ ? palette.accentSubtle : Fluent::Colors::cardBg();
        const QColor border = hasFocus() ? palette.accentBorder
            : (hovered_ ? palette.accentBorder : Fluent::Colors::border());
        const qreal borderWidth = Fluent::Painting::alignedStrokeWidth(hasFocus() ? 2.0 : 1.0, devicePixelRatioF());
        const QRectF r = Fluent::Painting::alignedStrokeRect(rect(), borderWidth);
        QPainterPath path;
        path.addRoundedRect(r, 8, 8);
        p.fillPath(path, bg);
        p.setPen(QPen(border, borderWidth));
        p.drawPath(path);

        p.setFont(font());
        p.setPen(Fluent::Colors::textPrimary());
        p.drawText(rect().adjusted(12, 0, -34, 0), Qt::AlignVCenter | Qt::AlignLeft, text());

        p.setPen(Qt::NoPen);
        p.setBrush(Fluent::Colors::textSecondary());
        const QPointF c(width() - 17.0, height() / 2.0 + 1.0);
        QPainterPath arrow;
        arrow.moveTo(c.x() - 4.0, c.y() - 2.5);
        arrow.lineTo(c.x() + 4.0, c.y() - 2.5);
        arrow.lineTo(c.x(), c.y() + 2.5);
        arrow.closeSubpath();
        p.drawPath(arrow);
    }

private:
    bool hovered_ = false;
};

QPalette mutedPalette(QWidget* widget) {
    QPalette pal = widget->palette();
    pal.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    return pal;
}

void showFallbackFactionBadge(QLabel* label, const QString& name, const FactionPalette& palette) {
    label->setPixmap(QPixmap());
    label->setText(name.left(1));
    label->setFont(Fluent::Typography::bodyStrong());
    label->setStyleSheet(QString(
        "background: %1;"
        "border: 1px solid %2;"
        "border-radius: 18px;"
        "color: %3;")
            .arg(palette.accentSubtle.name(QColor::HexArgb),
                palette.accentBorder.name(QColor::HexArgb),
                Fluent::Colors::textPrimary().name()));
    label->show();
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
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        QString("QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: %2; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
            .arg(Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.18)") : QStringLiteral("rgba(0,0,0,0.12)"),
                Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.28)") : QStringLiteral("rgba(0,0,0,0.2)")));

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    scrollContent->setStyleSheet("background: transparent;");
    auto* outerLayout = new QVBoxLayout(scrollContent);
    outerLayout->setContentsMargins(Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap,
        Fluent::Spacing::pageMargin, Fluent::Spacing::cardGap);
    outerLayout->setSpacing(0);

    auto* panel = new FluentCard(scrollContent);
    auto* panelLayout = panel->contentLayout();
    panelLayout->setSpacing(10);

    auto* pageTitle = new QLabel(QString::fromWCharArray(L"\u5168\u5c40\u8bbe\u7f6e"), panel);
    pageTitle->setFont(Fluent::Typography::pageTitle());
    panelLayout->addWidget(pageTitle);

    auto* factionPreview = new QWidget(panel);
    factionPreview->setObjectName("factionPreview");
    factionPreview->setAttribute(Qt::WA_StyledBackground, true);
    auto* factionLayout = new QHBoxLayout(factionPreview);
    factionLayout->setContentsMargins(12, 6, 12, 6);
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

    factionButton_ = new FactionSelectButton(factionPreview);
    factionLayout->addWidget(factionButton_);
    panelLayout->addWidget(factionPreview);

    panelLayout->addWidget(makeDivider(panel));

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

    panelLayout->addWidget(makeSectionTitle(QString::fromWCharArray(L"\u542f\u52a8\u65f6"), panel));
    radioStartupTray_ = new QRadioButton(QString::fromWCharArray(L"\u8fdb\u5165\u7cfb\u7edf\u6258\u76d8"), panel);
    radioStartupWindow_ = new QRadioButton(QString::fromWCharArray(L"\u663e\u793a\u4e3b\u7a97\u53e3"), panel);
    radioStartupTray_->setFont(Fluent::Typography::body());
    radioStartupWindow_->setFont(Fluent::Typography::body());
    radioStartupTray_->setFixedHeight(22);
    radioStartupWindow_->setFixedHeight(22);
    radioStartupWindow_->setChecked(true);
    auto* startupGroup = new QButtonGroup(this);
    startupGroup->addButton(radioStartupTray_, 0);
    startupGroup->addButton(radioStartupWindow_, 1);
    startupActionGroup_ = startupGroup;
    panelLayout->addWidget(radioStartupTray_);
    panelLayout->addWidget(radioStartupWindow_);
    panelLayout->addWidget(makeDivider(panel));

    panelLayout->addWidget(makeSectionTitle(QString::fromWCharArray(L"\u5173\u95ed\u7a97\u53e3\u65f6"), panel));
    radioAsk_ = new QRadioButton(QString::fromWCharArray(L"\u6bcf\u6b21\u8be2\u95ee"), panel);
    radioMin_ = new QRadioButton(QString::fromWCharArray(L"\u6700\u5c0f\u5316\u5230\u7cfb\u7edf\u6258\u76d8"), panel);
    radioExit_ = new QRadioButton(QString::fromWCharArray(L"\u76f4\u63a5\u9000\u51fa\u7a0b\u5e8f"), panel);
    radioAsk_->setFont(Fluent::Typography::body());
    radioMin_->setFont(Fluent::Typography::body());
    radioExit_->setFont(Fluent::Typography::body());
    radioAsk_->setFixedHeight(22);
    radioMin_->setFixedHeight(22);
    radioExit_->setFixedHeight(22);
    radioAsk_->setChecked(true);
    auto* btnGroup = new QButtonGroup(this);
    btnGroup->addButton(radioAsk_, 0);
    btnGroup->addButton(radioMin_, 1);
    btnGroup->addButton(radioExit_, 2);
    closeActionGroup_ = btnGroup;
    panelLayout->addWidget(radioAsk_);
    panelLayout->addWidget(radioMin_);
    panelLayout->addWidget(radioExit_);

    outerLayout->addWidget(panel);
    outerLayout->addSpacing(0);
    scrollArea->setWidget(scrollContent);
    if (auto* viewport = scrollArea->viewport()) {
        viewport->setStyleSheet(QStringLiteral("background: transparent;"));
    }
    pageLayout->addWidget(scrollArea);

    connect(startupGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        emit startupActionChanged(id == 1 ? StartupAction::Window : StartupAction::Tray);
    });
    connect(btnGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        CloseAction action = CloseAction::Ask;
        if (id == 1) action = CloseAction::Minimize;
        else if (id == 2) action = CloseAction::Exit;
        emit closeActionChanged(action);
    });
    connect(statusOverlayOpacitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (statusOverlayOpacityValue_) statusOverlayOpacityValue_->setText(QString::number(value) + "%");
        emit statusOverlayOpacityChanged(value);
    });
    connect(factionButton_, &QPushButton::clicked, this, &SettingsPage::showFactionMenu);

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

void SettingsPage::setStartupAction(StartupAction action) {
    if (!startupActionGroup_) return;
    const int id = action == StartupAction::Window ? 1 : 0;
    startupActionGroup_->blockSignals(true);
    if (auto* btn = startupActionGroup_->button(id)) btn->setChecked(true);
    startupActionGroup_->blockSignals(false);
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
    Q_UNUSED(factionId);
    applyPageTheme();
}

void SettingsPage::applyPageTheme() {
    const auto palette = FactionTheme::instance().palette();
    setStyleSheet(QString(
        "SettingsPage { background: %1; }"
        "QWidget#factionPreview { background-color: %2; border-radius: 8px; }"
    ).arg(palette.surfaceTint.name(), Fluent::Colors::cardBg().name()));

    const QString radioStyle = QString(
        "QRadioButton { background: transparent; color: %2; spacing: 8px; }"
        "QRadioButton::indicator { width: 14px; height: 14px; border-radius: 7px; }"
        "QRadioButton::indicator:unchecked { background: %3; border: 1px solid %4; }"
        "QRadioButton::indicator:checked {"
        "  border: 1px solid %1;"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.55, fx:0.5, fy:0.5,"
        "    stop:0 %1, stop:0.42 %1, stop:0.46 %3, stop:1 %3);"
        "}"
    ).arg(palette.accent.name(),
        Fluent::Colors::textPrimary().name(),
        Fluent::Colors::cardBg().name(),
        Fluent::Colors::border().name(QColor::HexArgb));
    if (radioAsk_) radioAsk_->setStyleSheet(radioStyle);
    if (radioMin_) radioMin_->setStyleSheet(radioStyle);
    if (radioExit_) radioExit_->setStyleSheet(radioStyle);
    if (radioStartupTray_) radioStartupTray_->setStyleSheet(radioStyle);
    if (radioStartupWindow_) radioStartupWindow_->setStyleSheet(radioStyle);

    updateFactionButtonText();
    if (factionPreviewName_) factionPreviewName_->setText(FactionTheme::instance().current().name);
    if (factionPreviewBar_) {
        factionPreviewBar_->setStyleSheet(QString("background: %1; border-radius: 1px;").arg(palette.accent.name()));
    }
    if (factionPreviewIcon_) {
        const auto& faction = FactionTheme::instance().current();
        const QString iconPath = FactionTheme::instance().iconPath();
        if (!iconPath.isEmpty()) {
            QPixmap pix = FactionIcon::render(iconPath, QSize(34, 34), factionPreviewIcon_,
                Fluent::Theme::isDarkMode() ? Fluent::Colors::textPrimary() : QColor());
            if (!pix.isNull()) {
                factionPreviewIcon_->setStyleSheet("background: transparent;");
                factionPreviewIcon_->setText(QString());
                factionPreviewIcon_->setPixmap(pix);
                factionPreviewIcon_->show();
            } else {
                showFallbackFactionBadge(factionPreviewIcon_, faction.name, palette);
            }
        } else {
            showFallbackFactionBadge(factionPreviewIcon_, faction.name, palette);
        }
    }
    if (statusOverlayOpacitySlider_) {
        statusOverlayOpacitySlider_->setStyleSheet(QString(
            "QSlider::groove:horizontal { height: 4px; background: %2; border-radius: 2px; }"
            "QSlider::sub-page:horizontal { background: %1; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: %3; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }"
        ).arg(palette.accent.name(),
            Fluent::Colors::border().name(QColor::HexArgb),
            Fluent::Colors::textPrimary().name()));
    }
}

bool SettingsPage::eventFilter(QObject* watched, QEvent* event) {
    return QWidget::eventFilter(watched, event);
}

void SettingsPage::showFactionMenu() {
    if (!factionButton_) return;
    const QPoint popupPos = factionButton_->mapToGlobal(QPoint(0, factionButton_->height() - 2));
    FactionMenu::Options options;
    options.minWidth = factionButton_->width();
    options.itemHeight = 24;
    options.drawScrollBar = true;
    options.bottomMargin = 16;
    const QString factionId = FactionMenu::exec(this, popupPos, options);
    if (!factionId.isEmpty() && factionId != FactionTheme::instance().current().id) {
        emit factionChanged(factionId);
    }
}

void SettingsPage::updateFactionButtonText() {
    if (!factionButton_) return;

    factionButton_->setText(FactionTheme::instance().current().name);
    factionButton_->update();
}
