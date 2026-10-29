#pragma once

#include "FactionTheme.h"
#include "DesignTokens.h"
#include <QVariantMap>

namespace Fluent {
inline QVariantMap qmlTheme() {
    const auto& faction = FactionTheme::instance().current();
    const auto colors = FactionTheme::instance().palette();
    return {
        {"dark", Fluent::Theme::isDarkMode()},
        {"background", Fluent::Colors::workspaceBg()}, {"surface", Fluent::Colors::workspacePanel()},
        {"surfaceTint", colors.surfaceTint},
        {"field", Fluent::Colors::workspaceField()}, {"border", Fluent::Colors::workspaceLine()},
        {"text", Fluent::Colors::workspaceText()}, {"muted", Fluent::Colors::workspaceMuted()},
        {"hover", Fluent::Colors::hoverBg()}, {"divider", Fluent::Colors::divider()},
        {"accent", colors.accent}, {"accentHover", colors.accentHover},
        {"accentPressed", colors.accentPressed}, {"accentSoft", colors.accentSoft},
        {"accentSubtle", colors.accentSubtle}, {"accentBorder", colors.accentBorder},
        {"accentForeground", colors.textOnAccent}, {"danger", Fluent::Colors::danger()},
        {"warning", Fluent::Colors::warning()}, {"success", Fluent::Colors::success()},
        {"scrollHandle", Fluent::Styles::scrollBarHandle()},
        {"scrollHandleHover", Fluent::Styles::scrollBarHandleHover()},
        {"factionId", faction.id}, {"factionName", faction.name},
        {"factionIcon", QString(faction.iconPath).replace(QStringLiteral(":/"), QStringLiteral("qrc:/"))},
        {"pageMargin", Fluent::Spacing::pageMargin}, {"cardGap", Fluent::Spacing::cardGap},
        {"cardPadding", Fluent::Spacing::cardPadding}, {"cardRadius", Fluent::Spacing::cardRadius},
        {"dialogRadius", Fluent::Spacing::dialogRadius},
        {"titleBarHeight", Fluent::Spacing::titleBarHeight},
        {"sidebarWidth", Fluent::Spacing::sidebarWidth},
        {"tableHeaderHeight", Fluent::Spacing::tableHeaderH}, {"tableRowHeight", Fluent::Spacing::tableRowH},
        {"buttonRadius", Fluent::Spacing::buttonRadius}, {"inputRadius", Fluent::Spacing::inputRadius}
    };
}
}
