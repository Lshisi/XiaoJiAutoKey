#pragma once

#include <QString>
#include <QColor>
#include <QFont>
#include <QMargins>

namespace Fluent {

// ── Colors ──────────────────────────────────────
namespace Colors {
    // Primary
    inline QColor primary()        { return QColor(0x00, 0x78, 0xD4); }
    inline QColor primaryHover()   { return QColor(0x10, 0x6E, 0xBE); }
    inline QColor primaryPressed() { return QColor(0x00, 0x5A, 0x9E); }

    // Backgrounds - content area is WHITE, sidebar is light gray
    inline QColor pageBg()         { return QColor(0xFF, 0xFF, 0xFF); }  // content = white
    inline QColor cardBg()         { return QColor(0xFF, 0xFF, 0xFF); }
    inline QColor sidebarBg()      { return QColor(0xF5, 0xF7, 0xFA); }  // sidebar = light gray
    inline QColor titleBarBg()     { return QColor(0xFF, 0xFF, 0xFF); }

    // Text
    inline QColor textPrimary()    { return QColor(0x1F, 0x1F, 0x1F); }
    inline QColor textSecondary()  { return QColor(0x60, 0x60, 0x60); }
    inline QColor textTertiary()   { return QColor(0x90, 0x90, 0x90); }

    // Semantic
    inline QColor danger()         { return QColor(0xD1, 0x34, 0x38); }
    inline QColor success()        { return QColor(0x10, 0x7C, 0x10); }

    // Borders & Dividers
    inline QColor border()         { return QColor(0, 0, 0, 20); }
    inline QColor divider()        { return QColor(0, 0, 0, 15); }
    inline QColor cardBorder()     { return QColor(0, 0, 0, 15); }       // slightly more visible
    inline QColor sidebarBorder()  { return QColor(0, 0, 0, 13); }

    // Hover / Selection
    inline QColor hoverBg()        { return QColor(0, 0, 0, 10); }
    inline QColor selectedBg()     { return QColor(0, 120, 212, 20); }
    inline QColor activeNavBg()    { return QColor(0, 120, 212, 26); }
    inline QColor rowHover()       { return QColor(0, 0, 0, 6); }

    // Close button
    inline QColor closeHover()     { return QColor(0xE8, 0x11, 0x23); }
}

// ── Typography (smaller sizes) ──────────────────
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

// ── Spacing & Sizing ────────────────────────────
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

// ── Animation ───────────────────────────────────
namespace Animation {
    constexpr int hoverDuration = 120;
    constexpr int fadeDuration  = 160;
    constexpr int slideDuration = 200;
}

} // namespace Fluent
