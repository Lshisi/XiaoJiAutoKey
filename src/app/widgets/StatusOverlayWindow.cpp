#include "widgets/StatusOverlayWindow.h"

#include "ui/styles/DesignTokens.h"

#include <algorithm>
#include <Windows.h>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRect>
#include <QScreen>
#include <QShowEvent>
#include <QStringList>
#include <QVBoxLayout>

namespace {

QString TextOrDash(const std::optional<KeyDescriptor>& key) {
    if (!key.has_value()) return QString::fromWCharArray(L"[-]");
    return QString::fromStdWString(GetKeyDisplayName(*key));
}

QString LoopText(const MacroConfig* macro) {
    if (!macro) return QString::fromWCharArray(L"\u65e0\u5b8f");
    if (macro->loopCount <= 0) return QString::fromWCharArray(L"\u65e0\u9650\u5faa\u73af");
    return QString::number(macro->loopCount) + QString::fromWCharArray(L"\u6b21\u5faa\u73af");
}

}  // namespace

StatusOverlayWindow::StatusOverlayWindow(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
    setCursor(Qt::SizeAllCursor);
    setFixedWidth(178);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 9);
    root->setSpacing(6);

    repeatIndicator_ = new QLabel(this);
    repeatIndicator_->setFixedSize(14, 14);
    repeatIndicator_->setStyleSheet("border-radius: 7px; background: #FF9F1A;");
    macroIndicator_ = new QLabel(this);
    macroIndicator_->setFixedSize(14, 14);
    macroIndicator_->setStyleSheet("border-radius: 7px; background: #FF9F1A;");

    auto makeBlock = [this](QLabel* indicator, QLabel*& stateLabel, QLabel*& hotkeyLabel) {
        auto* block = new QWidget(this);
        auto* outer = new QHBoxLayout(block);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(8);
        if (indicator) {
            outer->addWidget(indicator, 0, Qt::AlignTop);
        } else {
            outer->addSpacing(22);
        }
        auto* layout = new QVBoxLayout();
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(1);
        stateLabel = new QLabel(block);
        stateLabel->setFont(Fluent::Typography::caption());
        stateLabel->setStyleSheet("color: rgba(255, 255, 255, 0.95);");
        hotkeyLabel = new QLabel(block);
        hotkeyLabel->setFont(Fluent::Typography::caption());
        hotkeyLabel->setStyleSheet("color: rgba(255, 255, 255, 0.9);");
        hotkeyLabel->setWordWrap(true);
        layout->addWidget(stateLabel);
        layout->addWidget(hotkeyLabel);
        outer->addLayout(layout, 1);
        return block;
    };
    repeatBlock_ = makeBlock(repeatIndicator_, repeatTitleLabel_, repeatHotkeyLabel_);
    macroBlock_ = makeBlock(macroIndicator_, macroTitleLabel_, macroHotkeyLabel_);
    root->addWidget(repeatBlock_);
    root->addWidget(macroBlock_);

    lockButtonWindow_ = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    lockButtonWindow_->setAttribute(Qt::WA_TranslucentBackground, true);
    lockButtonWindow_->setAttribute(Qt::WA_ShowWithoutActivating, true);
    lockButtonWindow_->setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
    lockButtonWindow_->setFixedSize(24, 24);
    auto* lockLayout = new QHBoxLayout(lockButtonWindow_);
    lockLayout->setContentsMargins(0, 0, 0, 0);
    lockButton_ = new QPushButton(lockButtonWindow_);
    lockButton_->setFixedSize(22, 22);
    lockButton_->setCursor(Qt::PointingHandCursor);
    lockButton_->setFont(QFont("Segoe MDL2 Assets", 10));
    lockButton_->setText(QString(QChar(0xE785)));
    lockButton_->setToolTip(QString::fromWCharArray(L"\u9501\u5b9a\u4f4d\u7f6e"));
    lockButton_->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.08); color: rgba(255,255,255,0.85);"
        " border: 1px solid rgba(255,255,255,0.14); border-radius: 4px; padding: 0; }"
        "QPushButton:hover { background: rgba(255,255,255,0.16); }"
    );
    lockLayout->addWidget(lockButton_);
    connect(lockButton_, &QPushButton::clicked, this, [this]() {
        setLocked(!locked_);
        emit lockChanged(locked_);
    });

    setStyleSheet("StatusOverlayWindow { background: transparent; }");
}

void StatusOverlayWindow::setOverlayEnabled(bool enabled) {
    if (overlayEnabled_ == enabled) return;
    overlayEnabled_ = enabled;
    applyVisibleState();
}

void StatusOverlayWindow::updateStatus(const AppConfig& config,
    const ProfileConfig* profile,
    const MacroConfig* macro,
    bool repeatRunning,
    bool macroRunning) {
    const bool showRepeat = config.statusOverlayShowRepeat;
    const bool showMacro = config.statusOverlayShowMacro;
    setOverlayEnabled(showRepeat || showMacro);
    if (!overlayEnabled_) return;

    setLocked(config.statusOverlayLocked);
    setOverlayOpacity(config.statusOverlayOpacity);
    moveToConfiguredPosition(config);

    repeatIndicator_->setVisible(showRepeat);
    repeatIndicator_->setStyleSheet(QString("border-radius: 7px; background: %1;")
        .arg(repeatRunning ? "#24C26A" : "#FF9F1A"));
    macroIndicator_->setVisible(showMacro);
    macroIndicator_->setStyleSheet(QString("border-radius: 7px; background: %1;")
        .arg(macroRunning ? "#24C26A" : "#FF9F1A"));

    repeatBlock_->setVisible(showRepeat);
    macroBlock_->setVisible(showMacro);
    if (showRepeat) {
        repeatTitleLabel_->setText(QString::fromWCharArray(repeatRunning ? L"\u8fde\u53d1\uff1a\u5df2\u5f00\u542f" : L"\u8fde\u53d1\uff1a\u5df2\u5173\u95ed"));
        repeatHotkeyLabel_->setText(QString::fromWCharArray(L"\u542f\u505c ") + TextOrDash(profile ? profile->controlHotkey : std::nullopt));
    }
    if (showMacro) {
        macroTitleLabel_->setText(QString::fromWCharArray(macroRunning ? L"\u5b8f\uff1a\u5df2\u5f00\u542f" : L"\u5b8f\uff1a\u5df2\u5173\u95ed"));
        macroHotkeyLabel_->setText(QString::fromWCharArray(L"\u542f\u505c ") + TextOrDash(macro ? macro->triggerKey : std::nullopt) +
            QString::fromWCharArray(L"  ") + LoopText(macro));
    }

    adjustSize();
    move(boundedPosition(pos()));
    syncLockButton();
}

void StatusOverlayWindow::mousePressEvent(QMouseEvent* event) {
    if (locked_) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    dragging_ = true;
    dragStartGlobal_ = event->globalPos();
    dragStartWindow_ = pos();
    event->accept();
}

void StatusOverlayWindow::mouseMoveEvent(QMouseEvent* event) {
    if (locked_) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (!dragging_) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    move(boundedPosition(dragStartWindow_ + event->globalPos() - dragStartGlobal_));
    syncLockButton();
    event->accept();
}

void StatusOverlayWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (locked_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (!dragging_ || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    dragging_ = false;
    positionInitialized_ = true;
    syncLockButton();
    emit positionChanged(pos());
    event->accept();
}

void StatusOverlayWindow::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF rect = this->rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(rect, 6, 6);
    const int alpha = std::clamp(opacity_, 30, 100) * 255 / 100;
    painter.fillPath(path, QColor(0x34, 0x34, 0x34, alpha));
    painter.setPen(QColor(0x70, 0x70, 0x70, std::min(220, alpha + 35)));
    painter.drawPath(path);
}

void StatusOverlayWindow::setLocked(bool locked) {
    if (locked_ == locked) return;
    locked_ = locked;
    dragging_ = false;
    applyClickThrough();
    setCursor(locked_ ? Qt::ArrowCursor : Qt::SizeAllCursor);
    if (lockButton_) {
        lockButton_->setText(QString(QChar(locked_ ? 0xE72E : 0xE785)));
        lockButton_->setToolTip(QString::fromWCharArray(locked_ ? L"\u89e3\u9501\u4f4d\u7f6e" : L"\u9501\u5b9a\u4f4d\u7f6e"));
    }
    if (isVisible()) {
        syncLockButton();
    }
}

void StatusOverlayWindow::setOverlayOpacity(int opacity) {
    opacity = std::clamp(opacity, 30, 100);
    if (opacity_ == opacity) return;
    opacity_ = opacity;
    update();
}

void StatusOverlayWindow::applyClickThrough() {
    const auto hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) return;
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (locked_) {
        exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
        exStyle |= WS_EX_LAYERED;
    }
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
}

void StatusOverlayWindow::syncLockButton() {
    if (!lockButtonWindow_) return;
    if (!isVisible()) {
        lockButtonWindow_->hide();
        return;
    }
    lockButtonWindow_->move(frameGeometry().topRight() - QPoint(lockButtonWindow_->width() + 4, -4));
    lockButtonWindow_->show();
    lockButtonWindow_->raise();
}

void StatusOverlayWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    applyClickThrough();
    move(boundedPosition(pos()));
    syncLockButton();
}

QPoint StatusOverlayWindow::defaultPosition() const {
    const auto* screen = QGuiApplication::primaryScreen();
    if (!screen) return QPoint(6, 6);
    const QRect area = screen->availableGeometry();
    return area.topLeft() + QPoint(6, 6);
}

QPoint StatusOverlayWindow::positionFromConfig(const AppConfig& config) const {
    if (!config.statusOverlayPositionSaved) return defaultPosition();
    return QPoint(config.statusOverlayX, config.statusOverlayY);
}

QPoint StatusOverlayWindow::boundedPosition(const QPoint& position) const {
    const auto* screen = QGuiApplication::primaryScreen();
    if (!screen) return position;

    const QRect area = screen->availableGeometry();
    const int x = std::clamp(position.x(), area.left(), std::max(area.left(), area.right() - width() + 1));
    const int y = std::clamp(position.y(), area.top(), std::max(area.top(), area.bottom() - height() + 1));
    return QPoint(x, y);
}

void StatusOverlayWindow::moveToConfiguredPosition(const AppConfig& config) {
    if (positionInitialized_ || dragging_) return;
    move(boundedPosition(positionFromConfig(config)));
    positionInitialized_ = true;
}

void StatusOverlayWindow::applyVisibleState() {
    if (overlayEnabled_) {
        if (!positionInitialized_) move(defaultPosition());
        show();
        raise();
        syncLockButton();
    } else {
        hide();
        if (lockButtonWindow_) lockButtonWindow_->hide();
        positionInitialized_ = false;
    }
}
