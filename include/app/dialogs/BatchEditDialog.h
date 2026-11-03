#pragma once

#include "AppTypes.h"
#include "dialogs/QmlDialog.h"

#include <optional>

class BatchEditDialog final : public QmlDialog {
public:
    BatchEditDialog(bool macroMode, int count, QWidget* parent = nullptr);

    [[nodiscard]] std::optional<bool> enabled() const;
    [[nodiscard]] std::optional<RepeatMode> mode() const;
    [[nodiscard]] std::optional<int> delayMs() const;
    [[nodiscard]] std::optional<int> durationMs() const;
    [[nodiscard]] std::optional<bool> participatePriority() const;

protected:
    void handleAction(const QString& action, const QVariant& value) override;
    void editField(const QString& field, const QVariant& value) override;

private:
    [[nodiscard]] bool canAccept() const;
    void updateAcceptAction();

    bool macroMode_ = false;
};
