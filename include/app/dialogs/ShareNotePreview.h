#pragma once

#include "ShareCodec.h"

#include <QCoreApplication>
#include <QStringList>

inline QString ShareNotePreview(const SharePayload& payload) {
    QStringList notes;
    QStringList names;
    for (const auto& profile : payload.profiles) {
        if (!profile.note.empty()) {
            notes << QCoreApplication::translate("ShareNotePreview", "方案：%1\n%2")
                .arg(QString::fromStdWString(profile.name), QString::fromStdWString(profile.note));
        }
        for (const auto& binding : profile.bindings) {
            if (binding.name.empty()) continue;
            names << QCoreApplication::translate("ShareNotePreview", "%1：%2")
                .arg(QString::fromStdWString(GetKeyDisplayName(binding.key)),
                    QString::fromStdWString(binding.name));
            if (names.size() >= 8) break;
        }
        if (names.size() >= 8) break;
    }
    if (!names.isEmpty()) {
        notes << QCoreApplication::translate("ShareNotePreview", "按键名称：%1").arg(names.join(QStringLiteral("、")));
    }
    names.clear();
    for (const auto& macro : payload.macros) {
        if (!macro.note.empty()) {
            notes << QCoreApplication::translate("ShareNotePreview", "循环：%1\n%2")
                .arg(QString::fromStdWString(macro.name), QString::fromStdWString(macro.note));
        }
        for (const auto& step : macro.steps) {
            if (step.name.empty()) continue;
            names << QCoreApplication::translate("ShareNotePreview", "%1：%2")
                .arg(QString::fromStdWString(GetKeyDisplayName(step.key)),
                    QString::fromStdWString(step.name));
            if (names.size() >= 8) break;
        }
        if (names.size() >= 8) break;
    }
    if (!names.isEmpty()) {
        notes << QCoreApplication::translate("ShareNotePreview", "步骤名称：%1").arg(names.join(QStringLiteral("、")));
    }
    return notes.join(QStringLiteral("\n\n"));
}
