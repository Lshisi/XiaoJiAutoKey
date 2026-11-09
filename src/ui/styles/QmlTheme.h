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
    const auto& faction = FactionTheme::instance().current();
    const auto colors = FactionTheme::instance().palette();
    return {
        {"dark", Fluent::Theme::isDarkMode()},
        {"background", Fluent::Colors::workspaceBg()}, {"surface", Fluent::Colors::workspacePanel()},
        {"surfaceTint", colors.surfaceTint},
        {"titleBarBackground", colors.titleBarBackground},
        {"field", Fluent::Colors::workspaceField()}, {"border", Fluent::Colors::workspaceLine()},
        {"text", Fluent::Colors::workspaceText()}, {"muted", Fluent::Colors::workspaceMuted()},
        {"bodyText", Fluent::Colors::workspaceBodyText()},
        {"keyOverviewFill", Fluent::Colors::keyOverviewFill()},
        {"keyOverviewHover", Fluent::Colors::keyOverviewHover()},
        {"keyOverviewBorder", Fluent::Colors::keyOverviewBorder()},
        {"keyOverviewBorderHover", Fluent::Colors::keyOverviewBorderHover()},
        {"hover", Fluent::Colors::hoverBg()}, {"divider", Fluent::Colors::divider()},
        {"accent", colors.accent}, {"accentHover", colors.accentHover},
        {"accentPressed", colors.accentPressed}, {"accentSoft", colors.accentSoft},
        {"accentSubtle", colors.accentSubtle}, {"accentBorder", colors.accentBorder},
        {"accentForeground", colors.textOnAccent}, {"danger", Fluent::Colors::danger()},
        {"warning", Fluent::Colors::warning()}, {"success", Fluent::Colors::success()},
        {"info", Fluent::Theme::isDarkMode() ? QColor("#8ab8ed") : Fluent::Colors::primary()},
        {"scrollHandle", Fluent::Styles::scrollBarHandle()},
        {"scrollHandleHover", Fluent::Styles::scrollBarHandleHover()},
        {"factionId", faction.id}, {"factionName", faction.name},
        {"factionIcon", QString(faction.iconPath).replace(QStringLiteral(":/"), QStringLiteral("qrc:/"))},
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
