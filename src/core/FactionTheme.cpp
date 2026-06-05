#include "FactionTheme.h"

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

QColor usableAccent(const QColor& color) {
    QColor accent = color;
    accent.setAlpha(255);
    if (relativeLuminance(accent) > 0.42) {
        accent = mixColor(accent, QColor(0x18, 0x18, 0x18), 0.10);
    }
    return accent;
}

}

FactionTheme::FactionTheme() {
    // Default (no faction) - uses standard blue
    factions_.push_back({"none", QString::fromWCharArray(L"\u65e0"),
        QColor(0x00, 0x78, 0xD4), QColor(0x00, 0x78, 0xD4, 40), QString(), QString()});

    // Shaolin
    factions_.push_back({"shaolin", QString::fromWCharArray(L"\u5c11\u6797"),
        QColor(0xD1, 0x83, 0x36), QColor(0xD1, 0x83, 0x36, 30),
        ":/assets/factions/shaolin.png", QString()});

    // Wanhua
    factions_.push_back({"wanhua", QString::fromWCharArray(L"\u4e07\u82b1"),
        QColor(0xC2, 0x97, 0xFD), QColor(0xC2, 0x97, 0xFD, 30),
        ":/assets/factions/wanhua.png", QString()});

    // Qixiu
    factions_.push_back({"qixiu", QString::fromWCharArray(L"\u4e03\u79c0"),
        QColor(0xFD, 0x80, 0xAF), QColor(0xFD, 0x80, 0xAF, 30),
        ":/assets/factions/qixiu.png", QString()});

    // Chunyang
    factions_.push_back({"chunyang", QString::fromWCharArray(L"\u7eaf\u9633"),
        QColor(0x0A, 0x9A, 0xA0), QColor(0x0A, 0x9A, 0xA0, 30),
        ":/assets/factions/chunyang.png", QString()});

    // Tiance
    factions_.push_back({"tiance", QString::fromWCharArray(L"\u5929\u7b56"),
        QColor(0xFD, 0x6E, 0x52), QColor(0xFD, 0x6E, 0x52, 30),
        ":/assets/factions/tiance.png", QString()});

    // Cangjian
    factions_.push_back({"cangjian", QString::fromWCharArray(L"\u85cf\u5251"),
        QColor(0xFF, 0xD2, 0x4A), QColor(0xFF, 0xD2, 0x4A, 30),
        ":/assets/factions/cangjian.png", QString()});

    // Wudu
    factions_.push_back({"wudu", QString::fromWCharArray(L"\u4e94\u6bd2"),
        QColor(0x37, 0x92, 0xFD), QColor(0x37, 0x92, 0xFD, 30),
        ":/assets/factions/wudu.png", QString()});

    // Tangmen
    factions_.push_back({"tangmen", QString::fromWCharArray(L"\u5510\u95e8"),
        QColor(0x78, 0xB6, 0x36), QColor(0x78, 0xB6, 0x36, 30),
        ":/assets/factions/tangmen.png", QString()});

    // Mingjiao
    factions_.push_back({"mingjiao", QString::fromWCharArray(L"\u660e\u6559"),
        QColor(0xEE, 0x45, 0x5F), QColor(0xEE, 0x45, 0x5F, 30),
        ":/assets/factions/mingjiao.png", QString()});

    // Gaibang
    factions_.push_back({"gaibang", QString::fromWCharArray(L"\u4e10\u5e2e"),
        QColor(0xCB, 0x84, 0x3F), QColor(0xCB, 0x84, 0x3F, 30),
        ":/assets/factions/gaibang.png", QString()});

    // Cangyun
    factions_.push_back({"cangyun", QString::fromWCharArray(L"\u82cd\u4e91"),
        QColor(0xB3, 0x3C, 0x00), QColor(0xB3, 0x3C, 0x00, 30),
        ":/assets/factions/cangyun.png", QString()});

    // Changge
    factions_.push_back({"changge", QString::fromWCharArray(L"\u957f\u6b4c"),
        QColor(0x20, 0xA8, 0x72), QColor(0x20, 0xA8, 0x72, 30),
        ":/assets/factions/changge.png", QString()});

    // 闇稿垁
    factions_.push_back({"badao", QString::fromWCharArray(L"\u9738\u5200"),
        QColor(0x69, 0x6B, 0xBC), QColor(0x69, 0x6B, 0xBC, 30),
        ":/assets/factions/badao.png", QString()});

    // Penglai
    factions_.push_back({"penglai", QString::fromWCharArray(L"\u84ec\u83b1"),
        QColor(0x3B, 0x9F, 0xC7), QColor(0x3B, 0x9F, 0xC7, 30),
        ":/assets/factions/penglai.png", QString()});

    // Lingxuege
    factions_.push_back({"lingxuege", QString::fromWCharArray(L"\u51cc\u96ea\u9601"),
        QColor(0xA0, 0x09, 0x22), QColor(0xA0, 0x09, 0x22, 30),
        ":/assets/factions/lingxuege.png", QString()});

    // Yantianzong
    factions_.push_back({"yantianzong", QString::fromWCharArray(L"\u884d\u5929\u5b97"),
        QColor(0x8B, 0x5C, 0xF6), QColor(0x8B, 0x5C, 0xF6, 30),
        ":/assets/factions/yantianzong.png", QString()});

    // Yaozong
    factions_.push_back({"yaozong", QString::fromWCharArray(L"\u836f\u5b97"),
        QColor(0x00, 0xAB, 0x98), QColor(0x00, 0xAB, 0x98, 30),
        ":/assets/factions/yaozong.png", QString()});

    // Daozong
    factions_.push_back({"daozong", QString::fromWCharArray(L"\u5200\u5b97"),
        QColor(0x70, 0x93, 0xB2), QColor(0x70, 0x93, 0xB2, 30),
        ":/assets/factions/daozong.png", QString()});

    // Wanling
    factions_.push_back({"wanling", QString::fromWCharArray(L"\u4e07\u7075"),
        QColor(0xB8, 0x7C, 0x00), QColor(0xB8, 0x7C, 0x00, 30),
        ":/assets/factions/wanling.png", QString()});

    // Duanshi
    factions_.push_back({"duanshi", QString::fromWCharArray(L"\u6bb5\u6c0f"),
        QColor(0x35, 0x9C, 0xC6), QColor(0x35, 0x9C, 0xC6, 30),
        QString(), QString()});

    // Wuxiang
    factions_.push_back({"wuxiang", QString::fromWCharArray(L"\u65e0\u76f8"),
        QColor(0xA6, 0x5E, 0xFD), QColor(0xA6, 0x5E, 0xFD, 30),
        QString(), QString()});
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
    for (int i = 0; i < static_cast<int>(factions_.size()); ++i) {
        if (factions_[i].id == factionId) {
            currentIndex_ = i;
            return;
        }
    }
    currentIndex_ = 0;
}

QColor FactionTheme::primaryColor() const { return current().primaryColor; }
QColor FactionTheme::secondaryColor() const { return current().secondaryColor; }
FactionPalette FactionTheme::palette() const {
    const QColor accent = usableAccent(current().primaryColor);
    const QColor white(0xFF, 0xFF, 0xFF);
    const QColor black(0x00, 0x00, 0x00);
    FactionPalette p;
    p.accent = accent;
    p.accentHover = mixColor(accent, black, 0.10);
    p.accentPressed = mixColor(accent, black, 0.18);
    p.accentSoft = mixColor(white, accent, 0.12, 255);
    p.accentSubtle = mixColor(white, accent, 0.06, 255);
    p.accentBorder = mixColor(white, accent, 0.34, 255);
    p.surfaceTint = mixColor(QColor(0xF7, 0xF8, 0xFA), accent, 0.025, 255);
    p.textOnAccent = relativeLuminance(accent) > 0.48 ? QColor(0x16, 0x16, 0x16) : white;
    return p;
}
QString FactionTheme::iconPath() const { return current().iconPath; }
QString FactionTheme::patternPath() const { return current().patternPath; }
