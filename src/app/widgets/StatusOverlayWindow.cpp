#include "widgets/StatusOverlayWindow.h"

#include "FactionTheme.h"
#include "ui/styles/DesignTokens.h"

#include <algorithm>
#include <limits>
#include <Windows.h>
#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
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
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace {

constexpr int kBaseDpi = 96;

QColor mixColor(const QColor& a, const QColor& b, double t) {
    const double clamped = std::clamp(t, 0.0, 1.0);
    QColor out(
        static_cast<int>(a.red() + (b.red() - a.red()) * clamped),
        static_cast<int>(a.green() + (b.green() - a.green()) * clamped),
        static_cast<int>(a.blue() + (b.blue() - a.blue()) * clamped)
    );
    out.setAlpha(static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * clamped));
    return out;
}

QColor withAlpha(QColor color, int alpha) {
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}

QString TextOrDash(const std::optional<KeyDescriptor>& key) {
    if (!key.has_value()) return QString::fromWCharArray(L"[-]");
    return QString::fromStdWString(GetKeyDisplayName(*key));
}

QString LoopText(const MacroConfig* macro) {
    if (!macro) return QString::fromWCharArray(L"\u65e0\u5b8f");
    if (macro->loopCount <= 0) return QString::fromWCharArray(L"\u65e0\u9650\u5faa\u73af");
    return QString::number(macro->loopCount) + QString::fromWCharArray(L"\u6b21\u5faa\u73af");
}

const QScreen* nearestScreenForPoint(const QPoint& point) {
    if (const auto* screen = QGuiApplication::screenAt(point)) {
        return screen;
    }

    const auto screens = QGuiApplication::screens();
    const QScreen* nearest = nullptr;
    qint64 nearestDistance = std::numeric_limits<qint64>::max();
    for (const auto* screen : screens) {
        const QRect area = screen->availableGeometry();
        const int dx = point.x() < area.left()
            ? area.left() - point.x()
            : (point.x() > area.right() ? point.x() - area.right() : 0);
        const int dy = point.y() < area.top()
            ? area.top() - point.y()
            : (point.y() > area.bottom() ? point.y() - area.bottom() : 0);
        const qint64 distance = static_cast<qint64>(dx) * dx + static_cast<qint64>(dy) * dy;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = screen;
        }
    }

    return nearest ? nearest : QGuiApplication::primaryScreen();
}

const QScreen* defaultOverlayScreen() {
    if (const auto* screen = QGuiApplication::screenAt(QCursor::pos())) {
        return screen;
    }
    return QGuiApplication::primaryScreen();
}

int dpiFromWParam(WPARAM value) {
    const int dpiX = LOWORD(value);
    const int dpiY = HIWORD(value);
    return std::max(kBaseDpi, dpiX > 0 ? dpiX : dpiY);
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

    lockButtonWindow_ = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
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
    refreshThemeStyle(false, false);
}

void StatusOverlayWindow::setOverlayEnabled(bool enabled) {
    if (overlayEnabled_ == enabled) return;
    overlayEnabled_ = enabled;
    applyVisibleState();
}

void StatusOverlayWindow::refreshForDpiChange(int nativeDpi) {
    const int dpi = nativeDpi > 0 ? nativeDpi : currentScreenDpi();
    if (dpi > 0) {
        observedDpi_ = dpi;
    }

    setFont(Fluent::Typography::caption());
    if (repeatTitleLabel_) repeatTitleLabel_->setFont(Fluent::Typography::caption());
    if (repeatHotkeyLabel_) repeatHotkeyLabel_->setFont(Fluent::Typography::caption());
    if (macroTitleLabel_) macroTitleLabel_->setFont(Fluent::Typography::caption());
    if (macroHotkeyLabel_) macroHotkeyLabel_->setFont(Fluent::Typography::caption());
    if (lockButton_) lockButton_->setFont(QFont("Segoe MDL2 Assets", 10));

    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }
    adjustSize();
    move(boundedPosition(pos()));
    syncLockButton();
    update();
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

    refreshThemeStyle(repeatRunning, macroRunning);

    repeatIndicator_->setVisible(showRepeat);
    macroIndicator_->setVisible(showMacro);

    repeatBlock_->setVisible(showRepeat);
    macroBlock_->setVisible(showMacro);
    if (showRepeat) {
        repeatTitleLabel_->setText(QString::fromWCharArray(repeatRunning ? L"\u8fde\u53d1\uff1a\u5df2\u5f00\u542f" : L"\u8fde\u53d1\uff1a\u5df2\u5173\u95ed"));
        repeatHotkeyLabel_->setText(QString::fromWCharArray(L"\u542f\u505c ") + TextOrDash(profile ? profile->controlHotkey : std::nullopt));
    }
    if (showMacro) {
        macroTitleLabel_->setText(QString::fromWCharArray(macroRunning ? L"\u5faa\u73af\uff1a\u5df2\u5f00\u542f" : L"\u5faa\u73af\uff1a\u5df2\u5173\u95ed"));
        macroHotkeyLabel_->setText(QString::fromWCharArray(L"\u542f\u505c ") + TextOrDash(macro ? macro->triggerKey : std::nullopt) +
            QString::fromWCharArray(L"  ") + LoopText(macro));
    }

    adjustSize();
    move(boundedPosition(pos()));
    syncLockButton();
}

void StatusOverlayWindow::refreshThemeStyle(bool repeatRunning, bool macroRunning) {
    const auto palette = FactionTheme::instance().palette();
    const QColor titleColor = Fluent::Theme::isDarkMode()
        ? QColor(255, 255, 255, 242)
        : QColor(38, 38, 38, 235);
    const QColor bodyColor = Fluent::Theme::isDarkMode()
        ? QColor(255, 255, 255, 220)
        : QColor(60, 60, 60, 220);

    repeatTitleLabel_->setStyleSheet(QString("color: %1;").arg(titleColor.name(QColor::HexArgb)));
    macroTitleLabel_->setStyleSheet(QString("color: %1;").arg(titleColor.name(QColor::HexArgb)));
    repeatHotkeyLabel_->setStyleSheet(QString("color: %1;").arg(bodyColor.name(QColor::HexArgb)));
    macroHotkeyLabel_->setStyleSheet(QString("color: %1;").arg(bodyColor.name(QColor::HexArgb)));

    const QColor idleDot = Fluent::Theme::isDarkMode()
        ? withAlpha(palette.accentBorder, 210)
        : withAlpha(palette.accentBorder, 255);
    const QColor activeDot = Fluent::Theme::isDarkMode()
        ? mixColor(palette.accent, QColor(0xFF, 0xFF, 0xFF), 0.10)
        : palette.accent;

    repeatIndicator_->setStyleSheet(QString("border-radius: 7px; background: %1;")
        .arg((repeatRunning ? activeDot : idleDot).name(QColor::HexArgb)));
    macroIndicator_->setStyleSheet(QString("border-radius: 7px; background: %1;")
        .arg((macroRunning ? activeDot : idleDot).name(QColor::HexArgb)));

    const QColor buttonBg = Fluent::Theme::isDarkMode()
        ? withAlpha(palette.accentSubtle, 120)
        : withAlpha(palette.accentSubtle, 210);
    const QColor buttonHover = Fluent::Theme::isDarkMode()
        ? withAlpha(palette.accentSoft, 148)
        : withAlpha(palette.accentSoft, 235);
    const QColor buttonBorder = Fluent::Theme::isDarkMode()
        ? withAlpha(palette.accentBorder, 185)
        : withAlpha(palette.accentBorder, 215);
    const QColor buttonText = Fluent::Theme::isDarkMode()
        ? QColor(255, 255, 255, 230)
        : QColor(42, 42, 42, 230);
    lockButton_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2;"
        " border: 1px solid %3; border-radius: 4px; padding: 0; }"
        "QPushButton:hover { background: %4; }")
        .arg(buttonBg.name(QColor::HexArgb),
            buttonText.name(QColor::HexArgb),
            buttonBorder.name(QColor::HexArgb),
            buttonHover.name(QColor::HexArgb)));

    lockButtonWindow_->update();
    update();
}

void StatusOverlayWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (locked_) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    dragging_ = false;
    emit keyConfigRequested();
    event->accept();
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
    const auto palette = FactionTheme::instance().palette();
    const QColor fillBase = Fluent::Theme::isDarkMode()
        ? mixColor(QColor(0x2B, 0x2B, 0x2B, 255), palette.accent, 0.18)
        : mixColor(QColor(0xFF, 0xFF, 0xFF, 255), palette.accentSubtle, 0.78);
    const QColor borderBase = Fluent::Theme::isDarkMode()
        ? mixColor(palette.accentBorder, QColor(0xFF, 0xFF, 0xFF, 255), 0.08)
        : mixColor(palette.accentBorder, palette.accent, 0.24);
    painter.fillPath(path, withAlpha(fillBase, alpha));
    painter.setPen(withAlpha(borderBase, std::min(220, alpha + 28)));
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
    syncTopMostOrder();
}

void StatusOverlayWindow::syncTopMostOrder() {
    const HWND overlayHwnd = reinterpret_cast<HWND>(winId());
    const HWND lockHwnd = lockButtonWindow_ ? reinterpret_cast<HWND>(lockButtonWindow_->winId()) : nullptr;
    constexpr UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW;

    if (overlayHwnd) {
        SetWindowPos(overlayHwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
    }
    if (lockHwnd) {
        SetWindowPos(lockHwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
    }
}

void StatusOverlayWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    bindDpiRefreshSignals();
    observedDpi_ = currentScreenDpi();
    applyClickThrough();
    refreshForDpiChange(observedDpi_);
    move(boundedPosition(pos()));
    syncLockButton();
}

bool StatusOverlayWindow::event(QEvent* event) {
    if (event->type() == QEvent::ScreenChangeInternal) {
        refreshForDpiChange();
    }
    return QWidget::event(event);
}

bool StatusOverlayWindow::nativeEvent(const QByteArray& eventType, void* message, long* result) {
    Q_UNUSED(eventType)
    const auto* msg = static_cast<MSG*>(message);
    if (msg && msg->message == WM_DPICHANGED) {
        const int dpi = dpiFromWParam(msg->wParam);
        refreshForDpiChange(dpi);
        for (const int delayMs : {0, 120, 320}) {
            QTimer::singleShot(delayMs, this, [this, dpi]() {
                refreshForDpiChange(dpi);
            });
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}

QPoint StatusOverlayWindow::defaultPosition() const {
    const auto* screen = defaultOverlayScreen();
    if (!screen) return QPoint(6, 6);
    const QRect area = screen->availableGeometry();
    return area.topLeft() + QPoint(6, 6);
}

QPoint StatusOverlayWindow::positionFromConfig(const AppConfig& config) const {
    if (!config.statusOverlayPositionSaved) return defaultPosition();
    return QPoint(config.statusOverlayX, config.statusOverlayY);
}

QPoint StatusOverlayWindow::boundedPosition(const QPoint& position) const {
    const QPoint anchor = dragging_
        ? QCursor::pos()
        : position + QPoint(width() / 2, height() / 2);
    const auto* screen = nearestScreenForPoint(anchor);
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
        bindDpiRefreshSignals();
        refreshForDpiChange();
        syncLockButton();
    } else {
        hide();
        if (lockButtonWindow_) lockButtonWindow_->hide();
        positionInitialized_ = false;
    }
}

void StatusOverlayWindow::bindDpiRefreshSignals() {
    auto* handle = windowHandle();
    if (!handle) {
        return;
    }

    if (boundWindowHandle_ != handle) {
        boundWindowHandle_ = handle;
        connect(handle, &QWindow::screenChanged, this, [this](QScreen*) {
            boundScreen_ = nullptr;
            bindDpiRefreshSignals();
            refreshForDpiChange();
        }, Qt::UniqueConnection);
    }

    auto* screen = handle->screen();
    if (!screen || boundScreen_ == screen) {
        return;
    }

    boundScreen_ = screen;
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this](qreal dpi) {
        refreshForDpiChange(std::max(kBaseDpi, qRound(dpi)));
    }, Qt::UniqueConnection);
    connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this](qreal) {
        refreshForDpiChange();
    }, Qt::UniqueConnection);
}

int StatusOverlayWindow::currentScreenDpi() const {
    QScreen* screen = nullptr;
    if (auto* handle = windowHandle()) {
        screen = handle->screen();
    }
    if (!screen) {
        screen = QGuiApplication::screenAt(pos() + QPoint(width() / 2, height() / 2));
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    int dpi = screen ? std::max(kBaseDpi, qRound(screen->logicalDotsPerInch())) : kBaseDpi;
    if (screen) {
        dpi = std::max(dpi, qRound(screen->devicePixelRatio() * kBaseDpi));
    }
    if (auto* handle = windowHandle()) {
        dpi = std::max(dpi, qRound(handle->devicePixelRatio() * kBaseDpi));
    }
    dpi = std::max(dpi, qRound(devicePixelRatioF() * kBaseDpi));
    return dpi;
}
