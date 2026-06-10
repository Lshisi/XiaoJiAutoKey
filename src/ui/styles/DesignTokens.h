#pragma once

#include <QColor>
#include <QFont>
#include <QMargins>
#include <QPalette>
#include <QSettings>
#include <QString>
#include <QtGlobal>

namespace Fluent {

namespace Theme {
    inline bool readSystemDarkMode() {
#ifdef Q_OS_WIN
        QSettings settings(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
            QSettings::NativeFormat);
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
    inline QColor titleBarBg()     { return Theme::isDarkMode() ? QColor(0x1F, 0x1F, 0x1F) : QColor(0xFF, 0xFF, 0xFF); }

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
    inline QString fontFamily() { return QStringLiteral("Segoe UI, Microsoft YaHei UI"); }

    inline QFont pageTitle() {
        QFont f(fontFamily(), 14);
        f.setWeight(QFont::DemiBold);
        return f;
    }
    inline QFont cardTitle() {
        QFont f(fontFamily(), 11);
        f.setWeight(QFont::DemiBold);
        return f;
    }
    inline QFont body() {
        QFont f(fontFamily(), 9);
        f.setWeight(QFont::Normal);
        return f;
    }
    inline QFont bodyStrong() {
        QFont f(fontFamily(), 9);
        f.setWeight(QFont::DemiBold);
        return f;
    }
    inline QFont caption() {
        QFont f(fontFamily(), 8);
        f.setWeight(QFont::Normal);
        return f;
    }
    inline QFont smallText() {
        QFont f(fontFamily(), 8);
        f.setWeight(QFont::Normal);
        return f;
    }
    inline QFont titleBarTitle() {
        QFont f(fontFamily(), 10);
        f.setWeight(QFont::DemiBold);
        return f;
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
    constexpr int titleBarHeight = 40;
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

} // namespace Fluent
