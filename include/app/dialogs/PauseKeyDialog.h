#pragma once

#include "dialogs/KeyCaptureDialogBase.h"

struct PauseKeyEditResult {
    KeyDescriptor key;
};

class PauseKeyDialog : public KeyCaptureDialogBase {
    Q_OBJECT

public:
    explicit PauseKeyDialog(QWidget* parent = nullptr, std::optional<KeyDescriptor> existing = std::nullopt);

    [[nodiscard]] PauseKeyEditResult editResult() const;
};
