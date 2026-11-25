#pragma once

#include "FactionTheme.h"
#include "DesignTokens.h"
#include <QVariantMap>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace Fluent {
inline bool qmlMotionEnabled() {
#ifdef Q_OS_WIN
    BOOL enabled = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
        return enabled != FALSE;
#endif
    return true;
}

inline QVariantMap qmlTheme() {
    const auto& factionTheme = FactionTheme::instance();
    const auto& faction = factionTheme.current();
    const auto colors = factionTheme.palette();
    const bool dark = Fluent::Theme::isDarkMode();
    const bool crispy = factionTheme.crispyThemeEnabled();
    const QColor crispyBrand = FactionTheme::crispyAccent(false);
    const QColor crispyCream("#fff6e8");
    return {
        {"dark", dark},
        {"background", Fluent::Colors::workspaceBg()}, {"surface", Fluent::Colors::workspacePanel()},
        {"surfaceTint", colors.surfaceTint},
        {"sidebarBackground", crispy ? (dark ? QColor("#2b2626") : QColor("#fffaf1")) : colors.surfaceTint},
        {"titleBarBackground", colors.titleBarBackground},
        {"field", Fluent::Colors::workspaceField()},
        {"border", crispy ? (dark ? QColor("#4d403d") : QColor("#e8e0d5")) : Fluent::Colors::workspaceLine()},
        {"text", crispy ? (dark ? QColor("#f1e8df") : QColor("#282322")) : Fluent::Colors::workspaceText()},
        {"muted", crispy ? (dark ? QColor("#bfb4ac") : QColor("#7b746f")) : Fluent::Colors::workspaceMuted()},
        {"bodyText", crispy ? (dark ? QColor("#e1d6cf") : QColor("#3f3831")) : Fluent::Colors::workspaceBodyText()},
        {"keyOverviewFill", Fluent::Colors::keyOverviewFill()},
        {"keyOverviewHover", Fluent::Colors::keyOverviewHover()},
        {"keyOverviewBorder", Fluent::Colors::keyOverviewBorder()},
        {"keyOverviewBorderHover", Fluent::Colors::keyOverviewBorderHover()},
        {"hover", Fluent::Colors::hoverBg()}, {"divider", Fluent::Colors::divider()},
        {"accent", colors.accent}, {"accentHover", colors.accentHover},
        {"accentPressed", colors.accentPressed}, {"accentSoft", colors.accentSoft},
        {"accentSubtle", colors.accentSubtle}, {"accentBorder", colors.accentBorder},
        {"accentForeground", colors.textOnAccent}, {"danger", Fluent::Colors::danger()},
        {"primaryFill", crispy ? crispyBrand : colors.accent},
        {"primaryHover", crispy ? crispyBrand.darker(110) : colors.accentHover},
        {"primaryPressed", crispy ? crispyBrand.darker(120) : colors.accentPressed},
        {"primaryForeground", crispy ? crispyCream : colors.textOnAccent},
        {"warning", Fluent::Colors::warning()}, {"success", Fluent::Colors::success()},
        {"info", Fluent::Theme::isDarkMode() ? QColor("#8ab8ed") : Fluent::Colors::primary()},
        {"scrollHandle", Fluent::Styles::scrollBarHandle()},
        {"scrollHandleHover", Fluent::Styles::scrollBarHandleHover()},
        {"factionId", faction.id}, {"factionName", faction.name},
        {"themeName", factionTheme.themeName()}, {"crispyThemeEnabled", factionTheme.crispyThemeEnabled()},
        {"factionIcon", QString(factionTheme.iconPath()).replace(QStringLiteral(":/"), QStringLiteral("qrc:/"))},
        {"crispyAccent", FactionTheme::crispyAccent(dark)},
        {"crispyAccentForeground", crispyCream},
        {"crispyBrand", crispyBrand}, {"crispyCream", crispyCream},
        {"crispyPaper", dark ? QColor("#302c27") : QColor("#fffdf7")},
        {"crispyText", dark ? QColor("#f3e8d4") : QColor("#3e332a")},
        {"crispyMuted", dark ? QColor("#c0af99") : QColor("#827365")},
        {"crispyGold", dark ? QColor("#d5ba7b") : QColor("#92733d")},
        {"pageMargin", Fluent::Spacing::pageMargin}, {"cardGap", Fluent::Spacing::cardGap},
        {"cardPadding", Fluent::Spacing::cardPadding}, {"cardRadius", Fluent::Spacing::cardRadius},
        {"dialogRadius", Fluent::Spacing::dialogRadius},
        {"titleBarHeight", Fluent::Spacing::titleBarHeight},
        {"sidebarWidth", Fluent::Spacing::sidebarWidth},
        {"controlPanelWidth", Fluent::Spacing::controlPanelWidth},
        {"tableHeaderHeight", Fluent::Spacing::tableHeaderH}, {"tableRowHeight", Fluent::Spacing::tableRowH},
        {"motionEnabled", qmlMotionEnabled()},
        {"hoverDuration", Fluent::Animation::hoverDuration}, {"fadeDuration", Fluent::Animation::fadeDuration},
        {"slideDuration", Fluent::Animation::slideDuration},
        {"buttonRadius", Fluent::Spacing::buttonRadius}, {"inputRadius", Fluent::Spacing::inputRadius}
    };
}
}
