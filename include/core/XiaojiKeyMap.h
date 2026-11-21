#pragma once

#include "AppTypes.h"

namespace XiaojiKeys {
struct Output {
    std::uint8_t modifier = 0;
    std::uint8_t keyboardUsage = 0;
    std::uint16_t consumerUsage = 0;
    std::uint8_t mouseButton = 0;
    [[nodiscard]] bool IsValid() const {
        return modifier || keyboardUsage || consumerUsage || mouseButton;
    }
};

[[nodiscard]] Output Resolve(const KeyDescriptor& key);
}
