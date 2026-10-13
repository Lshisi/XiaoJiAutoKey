#include "ClickPositionGroup.h"

#include "ScreenPointPicker.h"
#include "FactionTheme.h"
#include "ui/components/FluentButton.h"
#include "ui/styles/DesignTokens.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QRadioButton>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <vector>

namespace {

// 与 SettingsPage 的单选钮同款：选中态用门派色圆环+圆心，未选中态用卡片底色，
// 保证深浅色模式下都与弹窗背景对比清晰。
QString radioThemeStyle() {
    const QColor accent = FactionTheme::instance().palette().accent;
    return QString(
        "QRadioButton { background: transparent; color: %2; spacing: 8px; }"
        "QRadioButton:disabled { color: %5; }"
        "QRadioButton::indicator { width: 14px; height: 14px; border-radius: 7px; }"
        "QRadioButton::indicator:unchecked { background: %3; border: 1px solid %4; }"
        "QRadioButton::indicator:checked {"
        "  border: 1px solid %1;"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.55, fx:0.5, fy:0.5,"
        "    stop:0 %1, stop:0.42 %1, stop:0.46 %3, stop:1 %3);"
        "}"
    ).arg(accent.name(),
        Fluent::Colors::textPrimary().name(),
        Fluent::Colors::cardBg().name(),
        Fluent::Colors::border().name(QColor::HexArgb),
        Fluent::Colors::textTertiary().name());
}

// 点位按鼠标所在屏幕的比例记录，与运行时换算保持一致。
QString AnchorText(double x, double y) {
    return QString::fromWCharArray(L"\u5df2\u62fe\u53d6: \u5c4f\u5e55\u5185 (%1%, %2%)")
        .arg(QString::number(x * 100.0, 'f', 1), QString::number(y * 100.0, 'f', 1));
}

QRect PickReferenceRect(const QPoint& pos) {
    if (QScreen* screen = QGuiApplication::screenAt(pos)) {
        return screen->geometry();
    }
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        return screen->geometry();
    }
    return {};
}

bool IsStatusOverlayWindow(const QWidget* widget) {
    const char* name = widget && widget->metaObject() ? widget->metaObject()->className() : nullptr;
    return name != nullptr && qstrcmp(name, "StatusOverlayWindow") == 0;
}

// 拾取期间临时隐掉的窗口集合：所属弹窗、主窗口、状态悬浮窗（含锁定按钮小窗）。
// 全部用透明度实现，不动显示状态与 Z 序，结束后恢复原透明度。
std::vector<std::pair<QPointer<QWidget>, qreal>> FadeAppWindows(QWidget* host) {
    std::vector<std::pair<QPointer<QWidget>, qreal>> faded;
    auto fade = [&faded](QWidget* widget) {
        if (!widget || !widget->isVisible() || widget->windowOpacity() <= 0.0) {
            return;
        }
        faded.emplace_back(widget, widget->windowOpacity());
        widget->setWindowOpacity(0.0);
    };

    fade(host);
    if (host && host->parentWidget()) {
        fade(host->parentWidget()->window());
    }
    const auto topLevels = QApplication::topLevelWidgets();
    for (QWidget* top : topLevels) {
        if (IsStatusOverlayWindow(top)
            || (top->parentWidget() && IsStatusOverlayWindow(top->parentWidget()->window()))) {
            fade(top);
        }
    }
    return faded;
}

void RestoreWindowOpacity(std::vector<std::pair<QPointer<QWidget>, qreal>>& faded) {
    for (auto& [widget, opacity] : faded) {
        if (widget) {
            widget->setWindowOpacity(opacity);
        }
    }
    faded.clear();
}

}  // namespace

ClickPositionGroup::ClickPositionGroup(std::function<bool()> driverSupportsMouseMove, QWidget* parent)
    : QWidget(parent),
      driverSupportsMouseMove_(std::move(driverSupportsMouseMove)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* groupTitle = new QLabel(QString::fromWCharArray(L"\u70b9\u51fb\u4f4d\u7f6e"), this);
    groupTitle->setFont(Fluent::Typography::caption());
    QPalette groupTitlePalette = groupTitle->palette();
    groupTitlePalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    groupTitle->setPalette(groupTitlePalette);
    layout->addWidget(groupTitle);

    followRadio_ = new QRadioButton(QString::fromWCharArray(L"\u8ddf\u968f\u9f20\u6807\u5f53\u524d\u4f4d\u7f6e"), this);
    followRadio_->setFont(Fluent::Typography::body());
    followRadio_->setFixedHeight(22);
    followRadio_->setStyleSheet(radioThemeStyle());
    followRadio_->setChecked(true);
    layout->addWidget(followRadio_);

    auto* fixedRow = new QHBoxLayout();
    fixedRow->setContentsMargins(0, 0, 0, 0);
    fixedRow->setSpacing(8);
    fixedRadio_ = new QRadioButton(QString::fromWCharArray(L"\u56fa\u5b9a\u70b9\u4f4d"), this);
    fixedRadio_->setFont(Fluent::Typography::body());
    fixedRadio_->setFixedHeight(22);
    fixedRadio_->setStyleSheet(radioThemeStyle());
    fixedRow->addWidget(fixedRadio_);
    pickBtn_ = new FluentButton(FluentButton::Secondary,
        QString::fromWCharArray(L"\u62fe\u53d6\u5750\u6807"), this);
    pickBtn_->setFixedHeight(24);
    fixedRow->addWidget(pickBtn_);
    fixedRow->addStretch();
    layout->addLayout(fixedRow);

    pickedLabel_ = new QLabel(QString::fromWCharArray(L"\u672a\u62fe\u53d6"), this);
    pickedLabel_->setFont(Fluent::Typography::caption());
    QPalette pickPalette = pickedLabel_->palette();
    pickPalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    pickedLabel_->setPalette(pickPalette);
    layout->addWidget(pickedLabel_);

    hintLabel_ = new QLabel(this);
    hintLabel_->setFont(Fluent::Typography::caption());
    hintLabel_->setWordWrap(true);
    QPalette hintPalette = hintLabel_->palette();
    hintPalette.setColor(QPalette::WindowText, Fluent::Colors::textSecondary());
    hintLabel_->setPalette(hintPalette);
    layout->addWidget(hintLabel_);

    connect(pickBtn_, &QPushButton::clicked, this, &ClickPositionGroup::beginPick);
    connect(fixedRadio_, &QRadioButton::toggled, this, [this](bool checked) {
        refreshPickButtonsEnabled();
        if (!checked) {
            clickPos_.fixedPosition = false;
        }
    });
}

void ClickPositionGroup::setValue(const ClickPosition& pos) {
    clickPos_ = pos;
    if (clickPos_.IsFixed()) {
        fixedRadio_->setChecked(true);
        pickedLabel_->setText(AnchorText(clickPos_.anchorX, clickPos_.anchorY));
    } else {
        followRadio_->setChecked(true);
        pickedLabel_->setText(QString::fromWCharArray(L"\u672a\u62fe\u53d6"));
    }
}

ClickPosition ClickPositionGroup::value() const {
    if (clickPos_.IsFixed() && fixedRadio_->isChecked()) {
        return clickPos_;
    }
    return ClickPosition{};
}

void ClickPositionGroup::applyDriverSupport() {
    const bool supported = driverSupportsMouseMove_ ? driverSupportsMouseMove_() : false;
    fixedRadio_->setEnabled(supported);
    refreshPickButtonsEnabled();
    hintLabel_->setText(supported
        ? QString::fromWCharArray(
            L"\u56fa\u5b9a\u70b9\u4f4d\u6309\u5c4f\u5e55\u6bd4\u4f8b\u8bb0\u5f55\uff0c"
            L"\u5207\u6362\u5206\u8fa8\u7387\u540e\u81ea\u52a8\u6362\u7b97\u3002")
        : QString::fromWCharArray(
            L"\u5f53\u524d\u9a71\u52a8\u4e0d\u652f\u6301\u9f20\u6807\u79fb\u52a8\uff0c"
            L"\u65e0\u6cd5\u4f7f\u7528\u56fa\u5b9a\u70b9\u4f4d\u3002"));
}

void ClickPositionGroup::beginPick() {
    QWidget* host = window();

    auto faded = FadeAppWindows(host);

    ScreenPointPicker picker;
    const bool confirmed = picker.exec() == QDialog::Accepted;
    const QPoint pos = picker.pickedPos();

    RestoreWindowOpacity(faded);
    if (host) {
        host->activateWindow();
    }
    if (confirmed) {
        applyPick(pos);
    }
}

void ClickPositionGroup::applyPick(const QPoint& pos) {
    double x = 0.5;
    double y = 0.5;
    const QRect geometry = PickReferenceRect(pos);
    if (geometry.width() > 0 && geometry.height() > 0) {
        x = static_cast<double>(pos.x() - geometry.left()) / static_cast<double>(geometry.width());
        y = static_cast<double>(pos.y() - geometry.top()) / static_cast<double>(geometry.height());
    }

    clickPos_.fixedPosition = true;
    clickPos_.anchorX = std::clamp(x, 0.0, 1.0);
    clickPos_.anchorY = std::clamp(y, 0.0, 1.0);
    pickedLabel_->setText(AnchorText(clickPos_.anchorX, clickPos_.anchorY));
    refreshPickButtonsEnabled();
}

void ClickPositionGroup::refreshPickButtonsEnabled() {
    const bool supported = driverSupportsMouseMove_ && driverSupportsMouseMove_();
    const bool fixed = fixedRadio_ && fixedRadio_->isChecked();
    pickBtn_->setEnabled(supported && fixed);
}
