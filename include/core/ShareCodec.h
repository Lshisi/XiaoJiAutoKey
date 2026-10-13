#pragma once

#include "AppTypes.h"

#include <optional>
#include <string>
#include <vector>

// 可分享的配置载荷：方案 + 宏（不含悬浮窗位置、音效路径等本机信息）。
struct SharePayload {
    int formatVersion = 1;
    std::string faction;  // 可选的门派主题提示
    std::vector<ProfileConfig> profiles;
    std::vector<MacroConfig> macros;

    [[nodiscard]] bool IsEmpty() const noexcept { return profiles.empty() && macros.empty(); }
};

// XJK1 分享码 / 文本配方 的编码解码。
// 输入不可信：所有解码路径都做数量与长度上限校验，参数值在解码时钳制。
class ShareCodec {
public:
    // "XJK1:<base64url(zlib(json))>"
    static constexpr std::size_t kMaxShareCodeChars = 200000;
    static constexpr std::size_t kMaxInflatedBytes = 4 * 1024 * 1024;
    static constexpr std::size_t kMaxRecipeChars = 400000;

    static constexpr std::size_t kMaxProfiles = 32;
    static constexpr std::size_t kMaxBindingsPerProfile = 256;
    static constexpr std::size_t kMaxMacros = 64;
    static constexpr std::size_t kMaxStepsPerMacro = 512;
    static constexpr std::size_t kMaxPauseKeys = 32;

    [[nodiscard]] static std::string EncodeShareCode(const SharePayload& payload);
    [[nodiscard]] static std::optional<SharePayload> DecodeShareCode(const std::string& code);

    // 文本配方：人类可读格式，导入导出都支持。
    [[nodiscard]] static std::string EncodeTextRecipe(const SharePayload& payload);
    [[nodiscard]] static std::optional<SharePayload> DecodeTextRecipe(const std::string& text);

    // 轻量嗅探：文本是否像分享内容（XJK1: 分享码或 XJK-RECIPE 文本配方）。
    // 只做前缀与长度判断，不解码，用于剪贴板检测和拖放判断。
    [[nodiscard]] static bool LooksLikeShareText(const std::string& text);
};
