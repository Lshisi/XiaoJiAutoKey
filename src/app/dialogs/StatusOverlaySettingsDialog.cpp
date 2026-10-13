#include "dialogs/StatusOverlaySettingsDialog.h"

#include "dialogs/DialogChrome.h"
#include "FactionTheme.h"
#include "ui/components/FluentButton.h"
#include "ui/components/FluentCheckBox.h"
#include "ui/styles/DesignTokens.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QSlider>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

StatusOverlaySettingsDialog::StatusOverlaySettingsDialog(
    const AppConfig& config, bool macroPage, QWidget* parent)
    : QDialog(parent),
      settings_{config.repeatOverlay, config.macroOverlay, config.statusOverlayOpacity} {
    setWindowTitle(QStringLiteral("\u60ac\u6d6e\u7a97\u8bbe\u7f6e"));
    DialogChrome::apply(this);
    setSizeGripEnabled(true);
    resize(400, 400);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(Fluent::Spacing::pageMargin, Fluent::Spacing::pageMargin,
        Fluent::Spacing::pageMargin, Fluent::Spacing::pageMargin);
    root->setSpacing(Fluent::Spacing::cardGap);
    auto* titleRow = new QHBoxLayout();
    auto* title = new QLabel(windowTitle(), this);
    title->setFont(Fluent::Typography::cardTitle());
    titleRow->addWidget(title, 1);
    titleRow->addWidget(DialogChrome::createCloseButton(this));
    root->addLayout(titleRow);

    tabs_ = new QTabWidget(this);
    tabs_->setFont(Fluent::Typography::body());
    tabs_->setUsesScrollButtons(true);
    root->addWidget(tabs_, 1);

    auto* repeat = addPage(QStringLiteral("\u6309\u952e\u8fde\u53d1"));
    addCheck(repeat, QStringLiteral("\u542f\u505c\u5feb\u6377\u952e"), &settings_.repeat.showHotkey);
    addCheck(repeat, QStringLiteral("\u72ec\u7acb\u6a21\u5f0f"), &settings_.repeat.showIndependent);
    addCheck(repeat, QStringLiteral("\u6309\u538b\u6a21\u5f0f"), &settings_.repeat.showHold);
    addCheck(repeat, QStringLiteral("\u5f00\u5173\u6a21\u5f0f"), &settings_.repeat.showToggle);
    addCheck(repeat, QStringLiteral("\u5207\u6362\u6a21\u5f0f"), &settings_.repeat.showSequence);
    addCheck(repeat, QStringLiteral("\u5207\u6362\u5feb\u6377\u952e"), &settings_.repeat.showSequenceHotkey);
    addCheck(repeat, QStringLiteral("\u6682\u505c\u5feb\u6377\u952e"), &settings_.repeat.showPauseKeys);
    repeat->addStretch();

    auto* macro = addPage(QStringLiteral("\u5faa\u73af\u8fde\u53d1"));
    addCheck(macro, QStringLiteral("\u89e6\u53d1\u5feb\u6377\u952e"), &settings_.macro.showHotkey);
    addCheck(macro, QStringLiteral("\u89e6\u53d1\u6a21\u5f0f"), &settings_.macro.showTriggerMode);
    addCheck(macro, QStringLiteral("\u5faa\u73af\u6b21\u6570"), &settings_.macro.showLoopCount);
    addCheck(macro, QStringLiteral("\u6682\u505c\u5feb\u6377\u952e"), &settings_.macro.showPauseKeys);
    macro->addStretch();

    auto* general = addPage(QStringLiteral("\u901a\u7528"));
    auto* opacityRow = new QHBoxLayout();
    opacityRow->setContentsMargins(0, 0, 0, 0);
    auto* opacityLabel = new QLabel(QStringLiteral("\u80cc\u666f\u900f\u660e\u5ea6"), general->parentWidget());
    opacityLabel->setFont(Fluent::Typography::body());
    opacityRow->addWidget(opacityLabel);
    opacityRow->addStretch();
    opacityValue_ = new QLabel(general->parentWidget());
    opacityValue_->setObjectName(QStringLiteral("overlayOpacityValue"));
    opacityValue_->setFont(Fluent::Typography::caption());
    opacityRow->addWidget(opacityValue_);
    general->addLayout(opacityRow);

    opacitySlider_ = new QSlider(Qt::Horizontal, general->parentWidget());
    opacitySlider_->setRange(30, 100);
    opacitySlider_->setSingleStep(5);
    opacitySlider_->setPageStep(10);
    opacitySlider_->setValue(settings_.opacity);
    opacityValue_->setText(QString::number(settings_.opacity) + "%");
    general->addWidget(opacitySlider_);

    auto* opacityDesc = new QLabel(
        QStringLiteral("\u6570\u503c\u8d8a\u4f4e\u8d8a\u900f\u660e\uff0c\u9501\u5b9a\u6309\u94ae\u5728\u6d6e\u7a97\u53f3\u4e0a\u89d2"), general->parentWidget());
    opacityDesc->setObjectName(QStringLiteral("overlayOpacityDesc"));
    opacityDesc->setFont(Fluent::Typography::caption());
    opacityDesc->setWordWrap(true);
    general->addWidget(opacityDesc);
    general->addStretch();

    connect(opacitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (opacityValue_) opacityValue_->setText(QString::number(value) + "%");
        settings_.opacity = value;
    });

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* cancel = new FluentButton(FluentButton::Secondary, QStringLiteral("\u53d6\u6d88"), this);
    auto* save = new FluentButton(FluentButton::Primary, QStringLiteral("\u4fdd\u5b58"), this);
    cancel->setAutoDefault(false);
    save->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &QDialog::accept);
    tabs_->setCurrentIndex(macroPage ? 1 : 0);
    refreshVisuals();
}

StatusOverlaySettings StatusOverlaySettingsDialog::settings() const {
    return settings_;
}

QVBoxLayout* StatusOverlaySettingsDialog::addPage(const QString& title) {
    auto* scroll = new QScrollArea(tabs_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(Fluent::Spacing::cardPadding, Fluent::Spacing::cardPadding,
        Fluent::Spacing::cardPadding, Fluent::Spacing::cardPadding);
    layout->setSpacing(Fluent::Spacing::cardGap);
    scroll->setWidget(content);
    tabs_->addTab(scroll, title);
    return layout;
}

void StatusOverlaySettingsDialog::addCheck(QVBoxLayout* layout, const QString& text, bool* value) {
    auto* check = new FluentCheckBox(text, layout->parentWidget());
    check->setChecked(*value);
    layout->addWidget(check);
    connect(check, &QCheckBox::toggled, this, [value](bool checked) { *value = checked; });
}

void StatusOverlaySettingsDialog::refreshVisuals() {
    const auto faction = FactionTheme::instance().palette();
    setPalette(Fluent::Theme::applicationPalette());
    setStyleSheet(QString(
        "QLabel { color: %1; background: transparent; }"
        "QLabel#overlayOpacityValue, QLabel#overlayOpacityDesc { color: %6; }"
        "QTabWidget::pane { border: 0; background: transparent; }"
        "QTabBar::tab { color: %1; background: transparent; padding: 8px 12px; border-bottom: 2px solid transparent; }"
        "QTabBar::tab:selected { border-bottom-color: %2; }"
        "QTabBar::tab:hover { background: %3; }"
        "QScrollArea { background: transparent; border: 0; }"
        "QScrollBar:vertical { background: transparent; border: none; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %4; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: %5; }"
        "QScrollBar::handle:vertical:pressed { background: %2; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; background: none; }"
        "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { width: 0; height: 0; background: none; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "QSlider::groove:horizontal { height: 4px; background: %7; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: %2; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %1; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }")
        .arg(Fluent::Colors::textPrimary().name(), faction.accent.name(),
            Fluent::Colors::hoverBg().name(QColor::HexArgb),
            Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.18)") : QStringLiteral("rgba(0,0,0,0.12)"),
            Fluent::Theme::isDarkMode() ? QStringLiteral("rgba(255,255,255,0.28)") : QStringLiteral("rgba(0,0,0,0.2)"),
            Fluent::Colors::textSecondary().name(),
            Fluent::Colors::border().name(QColor::HexArgb)));
    for (auto* scroll : findChildren<QScrollArea*>()) {
        scroll->viewport()->setAutoFillBackground(false);
        scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
        scroll->widget()->setAutoFillBackground(false);
        scroll->widget()->setStyleSheet(QStringLiteral("background: transparent;"));
    }
    int contentWidth = 0;
    for (auto* check : findChildren<QCheckBox*>()) {
        check->setFont(Fluent::Typography::body());
        check->setMinimumHeight(std::max(24, check->fontMetrics().height() + 8));
        contentWidth = std::max(contentWidth, check->sizeHint().width());
        check->update();
    }
    for (auto* button : findChildren<FluentButton*>()) button->refreshMetrics();
    int contentHeight = 0;
    for (auto* scroll : findChildren<QScrollArea*>()) {
        auto* pageLayout = scroll->widget()->layout();
        if (!pageLayout) continue;
        pageLayout->invalidate();
        pageLayout->activate();
        contentHeight = std::max(contentHeight, pageLayout->sizeHint().height());
    }
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }
    if (auto* currentScreen = screen()) {
        const QRect area = currentScreen->availableGeometry();
        const QSize limit = area.size() - QSize(2 * Fluent::Spacing::pageMargin, 2 * Fluent::Spacing::pageMargin);
        const int minimumWidth = contentWidth + 2 * Fluent::Spacing::pageMargin
            + 2 * Fluent::Spacing::cardPadding + style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        setMinimumSize(QSize(minimumWidth, 220).boundedTo(limit));
        setMaximumSize(limit);
        // Size against the tallest page so switching tabs does not resize the dialog.
        const int chromeHeight = std::max(0, layout()->sizeHint().height() - tabs_->sizeHint().height())
            + tabs_->tabBar()->sizeHint().height();
        const int preferredHeight = chromeHeight + contentHeight + Fluent::Spacing::cardGap / 2;
        resize(QSize(width(), preferredHeight).boundedTo(limit));
        if (isVisible()) {
            move(std::clamp(x(), area.left(), std::max(area.left(), area.right() - width() + 1)),
                std::clamp(y(), area.top(), std::max(area.top(), area.bottom() - height() + 1)));
        }
    }
    update();
}

void StatusOverlaySettingsDialog::paintEvent(QPaintEvent* event) {
    DialogChrome::paint(this, event);
}

void StatusOverlaySettingsDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    refreshVisuals();
}

bool StatusOverlaySettingsDialog::event(QEvent* event) {
    if (event->type() == QEvent::ScreenChangeInternal) {
        QTimer::singleShot(0, this, [this]() { refreshVisuals(); });
    }
    return QDialog::event(event);
}

void StatusOverlaySettingsDialog::mousePressEvent(QMouseEvent* event) {
    if (DialogChrome::mousePress(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
    QDialog::mousePressEvent(event);
}

void StatusOverlaySettingsDialog::mouseMoveEvent(QMouseEvent* event) {
    if (DialogChrome::mouseMove(this, event, dragging_, dragStartGlobal_, dragStartFrame_)) return;
    QDialog::mouseMoveEvent(event);
}

void StatusOverlaySettingsDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (DialogChrome::mouseRelease(event, dragging_)) return;
    QDialog::mouseReleaseEvent(event);
}
