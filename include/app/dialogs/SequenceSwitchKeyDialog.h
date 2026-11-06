#pragma once

#include "dialogs/KeyCaptureDialogBase.h"

struct SequenceSwitchKeyEditResult {
    KeyDescriptor key;
};

class SequenceSwitchKeyDialog : public KeyCaptureDialogBase {
    Q_OBJECT

public:
    explicit SequenceSwitchKeyDialog(QWidget* parent = nullptr, std::optional<KeyDescriptor> existing = std::nullopt);

    [[nodiscard]] SequenceSwitchKeyEditResult editResult() const;
};
