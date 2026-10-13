#pragma once

#include <QColor>
#include <QFont>
#include <QMargins>
#include <QPalette>
#include <QRectF>
#include <QSettings>
#include <QString>
#include <QtGlobal>

namespace Fluent {

namespace Painting {
    inline qreal alignedStrokeWidth(qreal logicalWidth, qreal devicePixelRatio) {
        const qreal ratio = qMax<qreal>(1.0, devicePixelRatio);
        return qMax(1, qRound(logicalWidth * ratio)) / ratio;
    }

    inline QRectF alignedStrokeRect(const QRect& rect, qreal strokeWidth) {
        const qreal inset = strokeWidth / 2.0;
        return QRectF(rect).adjusted(inset, inset, -inset, -inset);
    }
}

namespace Theme {
    inline bool readSystemDarkMode() {
#ifdef Q_OS_WIN
        QSettings settings(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
            QSettings::NativeFormat);
        settings.sync();
        return settings.value(QStringLiteral("AppsUseLightTheme"), 1).toInt() == 0;
#else
        return false;
#endif
    }

    inline bool& darkModeCache() {
        static bool value = readSystemDarkMode();
        return value;
    }

    inline bool refreshDarkModeCache() {
        bool& value = darkModeCache();
        const bool previous = value;
        value = readSystemDarkMode();
        return value != previous;
    }

    inline bool isDarkMode() {
        return darkModeCache();
    }

    inline QPalette applicationPalette() {
        QPalette palette;
        const bool dark = isDarkMode();
        const QColor window = dark ? QColor(0x20, 0x20, 0x20) : QColor(0xFF, 0xFF, 0xFF);
        const QColor base = dark ? QColor(0x2B, 0x2B, 0x2B) : QColor(0xFF, 0xFF, 0xFF);
        const QColor text = dark ? QColor(0xF3, 0xF3, 0xF3) : QColor(0x1F, 0x1F, 0x1F);
        const QColor muted = dark ? QColor(0xC8, 0xC8, 0xC8) : QColor(0x60, 0x60, 0x60);
        const QColor disabled = dark ? QColor(0x9A, 0x9A, 0x9A) : QColor(0x90, 0x90, 0x90);
        const QColor accent(0x00, 0x78, 0xD4);

        palette.setColor(QPalette::Window, window);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::AlternateBase, window);
        palette.setColor(QPalette::ToolTipBase, base);
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Button, base);
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, Qt::white);
        palette.setColor(QPalette::Highlight, accent);
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::PlaceholderText, muted);
        palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
        return palette;
    }
}

namespace Colors {
    inline QColor primary()        { return QColor(0x00, 0x78, 0xD4); }
    inline QColor primaryHover()   { return QColor(0x10, 0x6E, 0xBE); }
    inline QColor primaryPressed() { return QColor(0x00, 0x5A, 0x9E); }

    inline QColor pageBg()         { return Theme::isDarkMode() ? QColor(0x20, 0x20, 0x20) : QColor(0xFF, 0xFF, 0xFF); }
    inline QColor cardBg()         { return Theme::isDarkMode() ? QColor(0x2B, 0x2B, 0x2B) : QColor(0xFF, 0xFF, 0xFF); }
    inline QColor sidebarBg()      { return Theme::isDarkMode() ? QColor(0x24, 0x24, 0x24) : QColor(0xF5, 0xF7, 0xFA); }
    inline QColor titleBarBg()     { return Theme::isDarkMode() ? QColor(0x1F, 0x1F, 0x1F) : QColor(0xF3, 0xF5, 0xF8); }

    inline QColor textPrimary()    { return Theme::isDarkMode() ? QColor(0xF3, 0xF3, 0xF3) : QColor(0x1F, 0x1F, 0x1F); }
    inline QColor textSecondary()  { return Theme::isDarkMode() ? QColor(0xC8, 0xC8, 0xC8) : QColor(0x60, 0x60, 0x60); }
    inline QColor textTertiary()   { return Theme::isDarkMode() ? QColor(0x9A, 0x9A, 0x9A) : QColor(0x90, 0x90, 0x90); }

    inline QColor danger()         { return Theme::isDarkMode() ? QColor(0xFF, 0x99, 0x9A) : QColor(0xD1, 0x34, 0x38); }
    inline QColor success()        { return Theme::isDarkMode() ? QColor(0x6C, 0xD7, 0x6C) : QColor(0x10, 0x7C, 0x10); }

    inline QColor border()         { return Theme::isDarkMode() ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 20); }
    inline QColor divider()        { return Theme::isDarkMode() ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 15); }
    inline QColor cardBorder()     { return Theme::isDarkMode() ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 15); }
    inline QColor sidebarBorder()  { return Theme::isDarkMode() ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 13); }

    inline QColor hoverBg()        { return Theme::isDarkMode() ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 10); }
    inline QColor selectedBg()     { return Theme::isDarkMode() ? QColor(0, 120, 212, 46) : QColor(0, 120, 212, 20); }
    inline QColor activeNavBg()    { return Theme::isDarkMode() ? QColor(0, 120, 212, 54) : QColor(0, 120, 212, 26); }
    inline QColor rowHover()       { return Theme::isDarkMode() ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 6); }

    inline QColor closeHover()     { return QColor(0xE8, 0x11, 0x23); }
}

namespace Typography {
    inline QString fontFamily() { return QStringLiteral("Microsoft YaHei UI"); }

    inline QFont makeFont(int pointSize, QFont::Weight weight = QFont::Normal) {
        QFont f(fontFamily(), pointSize);
        f.setWeight(weight);
        f.setStyleStrategy(QFont::PreferAntialias);
        f.setHintingPreference(QFont::PreferFullHinting);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0);
        return f;
    }

    inline QFont pageTitle() {
        return makeFont(14, QFont::Bold);
    }
    inline QFont cardTitle() {
        return makeFont(11, QFont::Bold);
    }
    inline QFont body() {
        return makeFont(9);
    }
    inline QFont bodyStrong() {
        return makeFont(9, QFont::Bold);
    }
    inline QFont caption() {
        return makeFont(9);
    }
    inline QFont smallText() {
        return makeFont(8);
    }
    inline QFont titleBarTitle() {
        return makeFont(10, QFont::Bold);
    }
}

namespace Spacing {
    constexpr int pageMargin    = 16;
    constexpr int cardGap       = 12;
    constexpr int cardPadding   = 14;
    constexpr int cardRadius    = 8;
    constexpr int buttonRadius  = 6;
    constexpr int inputRadius   = 6;
    constexpr int dialogRadius  = 12;
    constexpr int sidebarWidth  = 130;
    constexpr int titleBarHeight = 34;
    constexpr int buttonHeight  = 32;
    constexpr int inputHeight   = 32;
    constexpr int navItemSize   = 56;
    constexpr int navIconSize   = 20;
    constexpr int navRadius     = 14;
    constexpr int tableHeaderH  = 36;
    constexpr int tableRowH     = 40;
}

namespace Animation {
    constexpr int hoverDuration = 120;
    constexpr int fadeDuration  = 160;
    constexpr int slideDuration = 200;
}

namespace Styles {
    // 全局统一的滚动条配色：深色用白色半透明、浅色用黑色半透明。
    inline QColor scrollBarHandle() {
        return Theme::isDarkMode() ? QColor(255, 255, 255, 46) : QColor(0, 0, 0, 31);
    }

    inline QColor scrollBarHandleHover() {
        return Theme::isDarkMode() ? QColor(255, 255, 255, 71) : QColor(0, 0, 0, 51);
    }

    // 全局统一的滚动条样式：6px 圆角细条、透明轨道、无原生箭头。
    // 所有滚动容器（页面、弹窗、文本框、表格、列表）都必须套用，禁止露出原生滚动条。
    inline QString scrollBarQss(const QColor& handle, const QColor& hover,
        const QColor& pressed = QColor(), int minLength = 30) {
        const auto rgba = [](const QColor& c) {
            return QStringLiteral("rgba(%1,%2,%3,%4)")
                .arg(c.red()).arg(c.green()).arg(c.blue())
                .arg(c.alphaF(), 0, 'f', 3);
        };
        QString qss = QStringLiteral(
            "QScrollBar:vertical { background: transparent; border: none; width: 6px; margin: 0; }"
            "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: %2px; }"
            "QScrollBar::handle:vertical:hover { background: %3; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; background: none; }"
            "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { width: 0; height: 0; background: none; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
            .arg(rgba(handle)).arg(minLength).arg(rgba(hover));
        if (pressed.isValid()) {
            qss += QStringLiteral("QScrollBar::handle:vertical:pressed { background: %1; }")
                .arg(rgba(pressed));
        }
        return qss;
    }
}

} // namespace Fluent
