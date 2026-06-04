#include "FactionTheme.h"

FactionTheme::FactionTheme() {
    // Default (no faction) - uses standard blue
    factions_.push_back({"none", QString::fromWCharArray(L"\u65e0"),
        QColor(0x00, 0x78, 0xD4), QColor(0x00, 0x78, 0xD4, 40), QString(), QString()});

    // Shaolin
    factions_.push_back({"shaolin", QString::fromWCharArray(L"\u5c11\u6797"),
        QColor(0xB8, 0x86, 0x0B), QColor(0xB8, 0x86, 0x0B, 30),
        ":/assets/factions/shaolin.png", QString()});

    // Wanhua
    factions_.push_back({"wanhua", QString::fromWCharArray(L"\u4e07\u82b1"),
        QColor(0x2E, 0x8B, 0x57), QColor(0x2E, 0x8B, 0x57, 30),
        ":/assets/factions/wanhua.png", QString()});

    // Qixiu
    factions_.push_back({"qixiu", QString::fromWCharArray(L"\u4e03\u79c0"),
        QColor(0xDB, 0x7C, 0x93), QColor(0xDB, 0x7C, 0x93, 30),
        ":/assets/factions/qixiu.png", QString()});

    // Chunyang
    factions_.push_back({"chunyang", QString::fromWCharArray(L"\u7eaf\u9633"),
        QColor(0x4A, 0x90, 0xD9), QColor(0x4A, 0x90, 0xD9, 30),
        ":/assets/factions/chunyang.png", QString()});

    // Tiance
    factions_.push_back({"tiance", QString::fromWCharArray(L"\u5929\u7b56"),
        QColor(0xC0, 0x2C, 0x2C), QColor(0xC0, 0x2C, 0x2C, 30),
        ":/assets/factions/tiance.png", QString()});

    // Cangjian
    factions_.push_back({"cangjian", QString::fromWCharArray(L"\u85cf\u5251"),
        QColor(0xC8, 0x8A, 0x1A), QColor(0xC8, 0x8A, 0x1A, 30),
        ":/assets/factions/cangjian.png", QString()});

    // Wudu
    factions_.push_back({"wudu", QString::fromWCharArray(L"\u4e94\u6bd2"),
        QColor(0x8B, 0x3A, 0xB5), QColor(0x8B, 0x3A, 0xB5, 30),
        ":/assets/factions/wudu.png", QString()});

    // Tangmen
    factions_.push_back({"tangmen", QString::fromWCharArray(L"\u5510\u95e8"),
        QColor(0x3D, 0x4A, 0x7A), QColor(0x3D, 0x4A, 0x7A, 30),
        ":/assets/factions/tangmen.png", QString()});

    // Mingjiao
    factions_.push_back({"mingjiao", QString::fromWCharArray(L"\u660e\u6559"),
        QColor(0xD4, 0x4A, 0x0E), QColor(0xD4, 0x4A, 0x0E, 30),
        ":/assets/factions/mingjiao.png", QString()});

    // Gaibang
    factions_.push_back({"gaibang", QString::fromWCharArray(L"\u4e10\u5e2e"),
        QColor(0x6B, 0x8E, 0x23), QColor(0x6B, 0x8E, 0x23, 30),
        ":/assets/factions/gaibang.png", QString()});

    // Cangyun
    factions_.push_back({"cangyun", QString::fromWCharArray(L"\u82cd\u4e91"),
        QColor(0x5C, 0x3A, 0x3A), QColor(0x5C, 0x3A, 0x3A, 30),
        ":/assets/factions/cangyun.png", QString()});

    // Changge
    factions_.push_back({"changge", QString::fromWCharArray(L"\u957f\u6b4c"),
        QColor(0x5F, 0x9E, 0xA0), QColor(0x5F, 0x9E, 0xA0, 30),
        ":/assets/factions/changge.png", QString()});

    // 闇稿垁
    factions_.push_back({"badao", QString::fromWCharArray(L"\u9738\u5200"),
        QColor(0x70, 0x80, 0x90), QColor(0x70, 0x80, 0x90, 30),
        ":/assets/factions/badao.png", QString()});

    // Penglai
    factions_.push_back({"penglai", QString::fromWCharArray(L"\u84ec\u83b1"),
        QColor(0x00, 0xA5, 0xCF), QColor(0x00, 0xA5, 0xCF, 30),
        ":/assets/factions/penglai.png", QString()});

    // Lingxuege
    factions_.push_back({"lingxuege", QString::fromWCharArray(L"\u51cc\u96ea\u9601"),
        QColor(0x4A, 0x4A, 0x4A), QColor(0x4A, 0x4A, 0x4A, 30),
        ":/assets/factions/lingxuege.png", QString()});

    // Yantianzong
    factions_.push_back({"yantianzong", QString::fromWCharArray(L"\u884d\u5929\u5b97"),
        QColor(0x2C, 0x3E, 0x8C), QColor(0x2C, 0x3E, 0x8C, 30),
        ":/assets/factions/yantianzong.png", QString()});

    // Yaozong
    factions_.push_back({"yaozong", QString::fromWCharArray(L"\u836f\u5b97"),
        QColor(0x4C, 0xAF, 0x50), QColor(0x4C, 0xAF, 0x50, 30),
        ":/assets/factions/yaozong.png", QString()});

    // Daozong
    factions_.push_back({"daozong", QString::fromWCharArray(L"\u5200\u5b97"),
        QColor(0x6D, 0x6D, 0x6D), QColor(0x6D, 0x6D, 0x6D, 30),
        ":/assets/factions/daozong.png", QString()});

    // Wanling
    factions_.push_back({"wanling", QString::fromWCharArray(L"\u4e07\u7075"),
        QColor(0x22, 0x8B, 0x22), QColor(0x22, 0x8B, 0x22, 30),
        ":/assets/factions/wanling.png", QString()});
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
QString FactionTheme::iconPath() const { return current().iconPath; }
QString FactionTheme::patternPath() const { return current().patternPath; }
