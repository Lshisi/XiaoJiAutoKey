#include "FactionTheme.h"
#include "DesignTokens.h"

#include <QCoreApplication>
#include <algorithm>
#include <cmath>

namespace {

QColor mixColor(const QColor& a, const QColor& b, double t, int alpha = -1) {
    const double clamped = std::max(0.0, std::min(1.0, t));
    QColor out(
        static_cast<int>(a.red() + (b.red() - a.red()) * clamped),
        static_cast<int>(a.green() + (b.green() - a.green()) * clamped),
        static_cast<int>(a.blue() + (b.blue() - a.blue()) * clamped)
    );
    out.setAlpha(alpha >= 0 ? alpha : static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * clamped));
    return out;
}

double relativeLuminance(const QColor& c) {
    auto channel = [](int value) {
        double v = value / 255.0;
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.red()) + 0.7152 * channel(c.green()) + 0.0722 * channel(c.blue());
}

QColor usableAccent(const QColor& color, bool darkMode) {
    QColor accent = color;
    accent.setAlpha(255);
    const double luminance = relativeLuminance(accent);
    if (darkMode && luminance < 0.34) {
        accent = mixColor(accent, QColor(0xFF, 0xFF, 0xFF), 0.22);
    } else if (!darkMode && luminance < 0.24) {
        const double lift = std::max(0.14, std::min(0.28, 0.12 + (0.24 - luminance) * 0.55));
        accent = mixColor(accent, QColor(0xFF, 0xFF, 0xFF), lift);
    } else if (!darkMode && luminance > 0.42) {
        accent = mixColor(accent, QColor(0x18, 0x18, 0x18), 0.10);
    }
    return accent;
}

FactionInfo makeFaction(const char* id, const wchar_t* name, const QColor& color, const QString& iconPath = QString()) {
    QColor secondary = color;
    secondary.setAlpha(30);
    return {QString::fromLatin1(id), QString::fromWCharArray(name), color, secondary, iconPath, QString()};
}

}

FactionTheme::FactionTheme() {
    factions_.push_back(makeFaction("shaolin", L"\u5c11\u6797", QColor(0x79, 0x6D, 0x0B), ":/assets/factions/shaolin.svg"));
    factions_.push_back(makeFaction("wanhua", L"\u4e07\u82b1", QColor(0x30, 0x10, 0x45), ":/assets/factions/wanhua.svg"));
    factions_.push_back(makeFaction("qixiu", L"\u4e03\u79c0", QColor(0x9F, 0x4B, 0x7F), ":/assets/factions/qixiu.svg"));
    factions_.push_back(makeFaction("chunyang", L"\u7eaf\u9633", QColor(0x09, 0x59, 0x6F), ":/assets/factions/chunyang.svg"));
    factions_.push_back(makeFaction("tiance", L"\u5929\u7b56", QColor(0x69, 0x11, 0x10), ":/assets/factions/tiance.svg"));
    factions_.push_back(makeFaction("cangjian", L"\u85cf\u5251", QColor(0x92, 0x95, 0x1C), ":/assets/factions/cangjian.svg"));
    factions_.push_back(makeFaction("wudu", L"\u6bd2\u7ecf", QColor(0x08, 0x51, 0x96), ":/assets/factions/wudu.svg"));
    factions_.push_back(makeFaction("tangmen", L"\u5510\u95e8", QColor(0x4B, 0x6F, 0x29), ":/assets/factions/tangmen.svg"));
    factions_.push_back(makeFaction("mingjiao", L"\u660e\u6559", QColor(0x8F, 0x50, 0x13), ":/assets/factions/mingjiao.svg"));
    factions_.push_back(makeFaction("gaibang", L"\u4e10\u5e2e", QColor(0x9A, 0x64, 0x26), ":/assets/factions/gaibang.svg"));
    factions_.push_back(makeFaction("cangyun", L"\u82cd\u4e91", QColor(0x99, 0x2F, 0x03), ":/assets/factions/cangyun.svg"));
    factions_.push_back(makeFaction("changge", L"\u957f\u6b4c", QColor(0x1F, 0x75, 0x64), ":/assets/factions/changge.svg"));
    factions_.push_back(makeFaction("badao", L"\u9738\u5200", QColor(0x30, 0x27, 0x6B), ":/assets/factions/badao.svg"));
    factions_.push_back(makeFaction("penglai", L"\u84ec\u83b1", QColor(0x5D, 0x61, 0x7E), ":/assets/factions/penglai.svg"));
    factions_.push_back(makeFaction("lingxuege", L"\u51cc\u96ea\u9601", QColor(0xA1, 0x09, 0x22), ":/assets/factions/lingxuege.svg"));
    factions_.push_back(makeFaction("yantianzong", L"\u884d\u5929\u5b97", QColor(0x5D, 0x2D, 0x90), ":/assets/factions/yantianzong.svg"));
    factions_.push_back(makeFaction("yaozong", L"\u836f\u5b97", QColor(0x0A, 0x51, 0x57), ":/assets/factions/yaozong.svg"));
    factions_.push_back(makeFaction("daozong", L"\u5200\u5b97", QColor(0x3B, 0x5E, 0x9F), ":/assets/factions/daozong.svg"));
    factions_.push_back(makeFaction("wanling", L"\u4e07\u7075", QColor(0xA0, 0x87, 0x4B), ":/assets/factions/wanling.svg"));
    factions_.push_back(makeFaction("duanshi", L"\u6bb5\u6c0f", QColor(0x5F, 0x9B, 0xB4), ":/assets/factions/duanshi.svg"));
    factions_.push_back(makeFaction("wuxiang", L"\u65e0\u76f8\u697c", QColor(0x69, 0x5F, 0xB9), ":/assets/factions/wuxiang.svg"));

    setCurrent(QStringLiteral("cangjian"));
}

FactionTheme& FactionTheme::instance() {
    static FactionTheme inst;
    return inst;
}

const std::vector<FactionInfo>& FactionTheme::factions() const {
    return factions_;
}

const FactionInfo& FactionTheme::current() const {
    return factions_[currentIndex_];
}

const FactionInfo* FactionTheme::find(const QString& id) const {
    for (const auto& f : factions_) {
        if (f.id == id) return &f;
    }
    return nullptr;
}

void FactionTheme::setCurrent(const QString& factionId) {
    crispyThemeEnabled_ = false;
    for (int i = 0; i < static_cast<int>(factions_.size()); ++i) {
        if (factions_[i].id == factionId) {
            currentIndex_ = i;
            return;
        }
    }
    for (int i = 0; i < static_cast<int>(factions_.size()); ++i) {
        if (factions_[i].id == QStringLiteral("cangjian")) {
            currentIndex_ = i;
            return;
        }
    }
    currentIndex_ = 0;
}

void FactionTheme::setCrispyThemeEnabled(bool enabled) {
    crispyThemeEnabled_ = enabled && current().id == QStringLiteral("cangjian");
}

bool FactionTheme::crispyThemeEnabled() const { return crispyThemeEnabled_; }

QString FactionTheme::themeName() const {
    return crispyThemeEnabled_ ? QCoreApplication::translate("FactionTheme", "脆皮山庄") : current().name;
}

QColor FactionTheme::crispyAccent(bool darkMode) {
    return darkMode ? QColor("#ff8793") : QColor("#c41230");
}

QColor FactionTheme::primaryColor() const { return crispyThemeEnabled_ ? crispyAccent(false) : current().primaryColor; }
QColor FactionTheme::secondaryColor() const {
    if (!crispyThemeEnabled_) return current().secondaryColor;
    auto color = primaryColor();
    color.setAlpha(30);
    return color;
}
FactionPalette FactionTheme::palette() const {
    const bool darkMode = Fluent::Theme::isDarkMode();
    const QColor accent = crispyThemeEnabled_ ? crispyAccent(darkMode) : usableAccent(current().primaryColor, darkMode);
    const QColor white(0xFF, 0xFF, 0xFF);
    const QColor black(0x00, 0x00, 0x00);
    const QColor surfaceBase = darkMode ? QColor(0x20, 0x20, 0x20) : QColor(0xF7, 0xF8, 0xFA);
    const QColor cardBase = darkMode ? QColor(0x2B, 0x2B, 0x2B) : white;
    FactionPalette p;
    p.accent = accent;
    p.accentHover = darkMode ? mixColor(accent, white, 0.12) : mixColor(accent, black, 0.10);
    p.accentPressed = darkMode ? mixColor(accent, black, 0.16) : mixColor(accent, black, 0.18);
    p.accentSoft = mixColor(cardBase, accent, darkMode ? 0.30 : 0.12, 255);
    p.accentSubtle = mixColor(cardBase, accent, darkMode ? 0.18 : 0.06, 255);
    p.accentBorder = mixColor(cardBase, accent, darkMode ? 0.55 : 0.34, 255);
    p.surfaceTint = mixColor(surfaceBase, accent, darkMode ? 0.045 : 0.025, 255);
    p.titleBarBackground = mixColor(darkMode ? Fluent::Colors::titleBarBg() : surfaceBase,
        accent, darkMode ? 0.09 : 0.14, 255);
    if (crispyThemeEnabled_) {
        p.accentSoft = darkMode ? QColor("#42332e") : QColor("#f7ecd9");
        p.accentSubtle = darkMode ? QColor("#342b28") : QColor("#fcf7ee");
        p.accentBorder = darkMode ? QColor("#a66d65") : QColor("#dcb8a6");
        p.surfaceTint = darkMode ? QColor("#242122") : QColor("#f7f5f1");
        p.titleBarBackground = darkMode ? QColor("#292426") : QColor("#fffdf8");
    }
    p.textOnAccent = crispyThemeEnabled_ && darkMode ? QColor(0x25, 0x24, 0x1E)
        : relativeLuminance(accent) > 0.48 ? QColor(0x16, 0x16, 0x16) : white;
    return p;
}
QString FactionTheme::iconPath() const {
    return crispyThemeEnabled_ ? QStringLiteral(":/assets/factions/cangjian-crispy.svg") : current().iconPath;
}
QString FactionTheme::patternPath() const { return current().patternPath; }
