#pragma once

#include <QString>

#include <utility>

struct ControllerResult {
    bool ok = false;
    QString message;

    [[nodiscard]] static ControllerResult success() {
        return {true, {}};
    }

    [[nodiscard]] static ControllerResult failure(QString message) {
        return {false, std::move(message)};
    }
};
