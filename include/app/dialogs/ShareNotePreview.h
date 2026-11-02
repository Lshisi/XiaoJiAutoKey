#pragma once

#include "ShareCodec.h"

#include <QCoreApplication>
#include <QStringList>

inline QString ShareNotePreview(const SharePayload& payload) {
    QStringList notes;
    for (const auto& profile : payload.profiles) {
        if (!profile.note.empty()) {
            notes << QCoreApplication::translate("ShareNotePreview", "方案：%1\n%2")
                .arg(QString::fromStdWString(profile.name), QString::fromStdWString(profile.note));
        }
    }
    for (const auto& macro : payload.macros) {
        if (!macro.note.empty()) {
            notes << QCoreApplication::translate("ShareNotePreview", "循环：%1\n%2")
                .arg(QString::fromStdWString(macro.name), QString::fromStdWString(macro.note));
        }
    }
    return notes.join(QStringLiteral("\n\n"));
}
