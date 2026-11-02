#pragma once

#include "ShareCodec.h"

#include <QCoreApplication>
#include <QStringList>

inline QString ShareNamePreview(const SharePayload& payload) {
    QStringList sections;
    for (const auto& profile : payload.profiles) {
        QStringList names;
        for (const auto& binding : profile.bindings) {
            if (binding.name.empty()) continue;
            names << QCoreApplication::translate("ShareNamePreview", "%1：%2")
                .arg(QString::fromStdWString(GetKeyDisplayName(binding.key)),
                    QString::fromStdWString(binding.name));
        }
        if (!names.isEmpty()) {
            sections << QCoreApplication::translate("ShareNamePreview", "方案：%1\n%2")
                .arg(QString::fromStdWString(profile.name), names.join(QStringLiteral("\n")));
        }
    }
    for (const auto& macro : payload.macros) {
        QStringList names;
        for (const auto& step : macro.steps) {
            if (step.name.empty()) continue;
            names << QCoreApplication::translate("ShareNamePreview", "%1：%2")
                .arg(QString::fromStdWString(GetKeyDisplayName(step.key)),
                    QString::fromStdWString(step.name));
        }
        if (!names.isEmpty()) {
            sections << QCoreApplication::translate("ShareNamePreview", "循环：%1\n%2")
                .arg(QString::fromStdWString(macro.name), names.join(QStringLiteral("\n")));
        }
    }
    return sections.join(QStringLiteral("\n\n"));
}
