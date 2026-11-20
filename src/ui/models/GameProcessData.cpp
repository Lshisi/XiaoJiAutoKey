#include "GameProcessData.h"

#include <QCoreApplication>

namespace {

QString text(const char* source) {
    return QCoreApplication::translate("GameProcessData", source);
}

bool matches(const QString& process, const QString& rule) {
    const QString pattern = rule.trimmed().toLower();
    return pattern.endsWith('*') && pattern.size() > 1
        ? process.startsWith(pattern.chopped(1), Qt::CaseInsensitive)
        : process.compare(pattern, Qt::CaseInsensitive) == 0;
}

bool contains(const QStringList& processes, const QString& name) {
    for (const auto& process : processes) {
        if (matches(name, process)) return true;
    }
    return false;
}

QVariantMap findWindow(const QVariantList& windows, const QString& pattern) {
    for (const auto& window : windows) {
        const auto entry = window.toMap();
        if (matches(entry.value("processName").toString(), pattern)) return entry;
    }
    return {};
}

void status(QVariantMap& data, const QString& message, bool error = false) {
    data.insert("status", message);
    data.insert("statusError", error);
}

}  // namespace

QVariantMap GameProcessData::create(const QStringList& processes, const QVariantList& windows) {
    QVariantMap data{{"processes", processes}, {"processName", QString()},
        {"status", QString()}, {"statusError", false}};
    refresh(data, windows);
    return data;
}

void GameProcessData::refresh(QVariantMap& data, const QVariantList& windows) {
    const QStringList processes = data.value("processes").toStringList();
    QVariantList available;
    for (const auto& window : windows) {
        auto entry = window.toMap();
        const QString process = entry.value("processName").toString();
        const bool builtIn = process.startsWith(QStringLiteral("jx3client"), Qt::CaseInsensitive);
        entry.insert("builtIn", builtIn);
        entry.insert("added", builtIn || contains(processes, process));
        available.append(entry);
    }
    data.insert("windows", available);

    const auto game = findWindow(available, QStringLiteral("jx3client*"));
    QVariantList configured{QVariantMap{{"title", text("剑网3")},
        {"detail", game.isEmpty() ? text("旗舰版、无界、缘起 · 自动识别") : game.value("title").toString()},
        {"processName", QStringLiteral("JX3Client*")}, {"icon", game.value("icon", QString())},
        {"builtIn", true}, {"running", !game.isEmpty()}}};
    for (const auto& process : processes) {
        const auto window = findWindow(available, process);
        configured.append(QVariantMap{
            {"title", window.isEmpty() ? process : window.value("title").toString()},
            {"detail", window.isEmpty() ? text("%1 · 未打开").arg(process) : process},
            {"processName", process}, {"icon", window.value("icon", QString())},
            {"builtIn", false}, {"running", !window.isEmpty()}});
    }
    data.insert("configured", configured);
}

void GameProcessData::add(QVariantMap& data, const QString& input) {
    const QString name = input.trimmed().toLower();
    bool invalid = name.isEmpty() || name.toUtf8().size() > 260 || name == "." || name == "..";
    for (const QChar character : name) {
        if (!character.isPrint() || QStringLiteral("\\/:?\"<>|").contains(character)) invalid = true;
    }
    const auto wildcard = name.indexOf('*');
    if (wildcard >= 0 && (wildcard == 0 || wildcard != name.size() - 1)) invalid = true;
    if (invalid) {
        status(data, text("请填写程序名，例如 game.exe；前缀匹配可写 mumu*，不要填写路径。"), true);
        return;
    }
    if (name.startsWith(QStringLiteral("jx3client"), Qt::CaseInsensitive)) {
        status(data, text("剑网3已内置识别，无需重复添加。"));
        return;
    }
    auto processes = data.value("processes").toStringList();
    if (contains(processes, name)) {
        status(data, text("该程序已在生效列表中。"));
        return;
    }
    processes.append(name);
    data.insert("processes", processes);
    data.insert("processName", QString());
    status(data, text("已添加 %1，保存后生效。").arg(name));
    refresh(data, data.value("windows").toList());
}

void GameProcessData::remove(QVariantMap& data, int index) {
    auto processes = data.value("processes").toStringList();
    // 首行始终为内置剑网3；删除只作用于额外规则。
    if (index <= 0 || index > processes.size()) return;
    processes.removeAt(index - 1);
    data.insert("processes", processes);
    status(data, QString());
    refresh(data, data.value("windows").toList());
}
