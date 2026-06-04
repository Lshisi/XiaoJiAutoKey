#pragma once

#include <QColor>
#include <QString>
#include <vector>

struct FactionInfo {
    QString id;           // e.g. "cangjian"
    QString name;         // e.g. "藏剑"
    QColor primaryColor;  // main accent color
    QColor secondaryColor;// subtle/background accent
    QString iconPath;     // qrc path to faction icon
    QString patternPath;  // qrc path to decorative pattern (optional)
};

class FactionTheme {
public:
    static FactionTheme& instance();

    const std::vector<FactionInfo>& factions() const;
    const FactionInfo& current() const;
    const FactionInfo* find(const QString& id) const;

    void setCurrent(const QString& factionId);

    // Convenience accessors for current theme
    QColor primaryColor() const;
    QColor secondaryColor() const;
    QString iconPath() const;
    QString patternPath() const;

private:
    FactionTheme();
    std::vector<FactionInfo> factions_;
    int currentIndex_ = 0;
};
