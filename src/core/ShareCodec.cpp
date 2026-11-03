#include "MacroActions.h"
#include "ShareCodec.h"

#include "JsonLite.h"
#include "miniz.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

// ── UTF-8 / 宽字符转换（Windows 用系统 API，其它平台用 codecvt 便于本地测试）──
std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
#ifdef _WIN32
    const int size = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    if (size > 0) {
        ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size);
    }
    return result;
#else
    // 仅用于非 Windows 的本地测试构建。
    std::wstring result;
    result.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const auto byte = static_cast<unsigned char>(text[i]);
        unsigned int codePoint = 0;
        int extra = 0;
        if (byte < 0x80) {
            codePoint = byte;
        } else if ((byte & 0xE0) == 0xC0) {
            codePoint = byte & 0x1F; extra = 1;
        } else if ((byte & 0xF0) == 0xE0) {
            codePoint = byte & 0x0F; extra = 2;
        } else if ((byte & 0xF8) == 0xF0) {
            codePoint = byte & 0x07; extra = 3;
        } else {
            ++i;
            continue;
        }
        bool valid = true;
        for (int k = 0; k < extra && valid; ++k) {
            if (i + 1 + k >= text.size()) { valid = false; break; }
            const auto cont = static_cast<unsigned char>(text[i + 1 + k]);
            if ((cont & 0xC0) != 0x80) valid = false;
            else codePoint = (codePoint << 6) | (cont & 0x3F);
        }
        if (!valid) { ++i; continue; }
        i += static_cast<std::size_t>(extra) + 1;
        if (codePoint <= 0xFFFF) {
            result.push_back(static_cast<wchar_t>(codePoint));
        } else {
            codePoint -= 0x10000;
            result.push_back(static_cast<wchar_t>(0xD800 + (codePoint >> 10)));
            result.push_back(static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)));
        }
    }
    return result;
#endif
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
#ifdef _WIN32
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    if (size > 0) {
        ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    }
    return result;
#else
    std::string result;
    result.reserve(text.size());
    for (const wchar_t wide : text) {
        unsigned int codePoint = static_cast<unsigned int>(wide);
        if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
            result.push_back('?');
            continue;
        }
        if (codePoint < 0x80) {
            result.push_back(static_cast<char>(codePoint));
        } else if (codePoint < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }
    return result;
#endif
}

// ── 按键编码（与 config.json 的 "K:<vk>" / "K:<vk>+Ctrl+Shift" 一致）──
std::string EncodeKey(const KeyDescriptor& key) {
    std::string prefix = key.device == InputDevice::Mouse ? "M:" : "K:";
    std::string result = prefix + std::to_string(key.code);
    if ((key.modifiers & kKeyModifierCtrl) != 0) result += "+Ctrl";
    if ((key.modifiers & kKeyModifierShift) != 0) result += "+Shift";
    if ((key.modifiers & kKeyModifierAlt) != 0) result += "+Alt";
    return result;
}

std::optional<KeyDescriptor> DecodeKey(const std::string& text) {
    if (text.size() < 3 || (text[0] != 'K' && text[0] != 'M') || text[1] != ':') {
        return std::nullopt;
    }
    const auto firstModifier = text.find('+', 2);
    const std::string codeText = text.substr(2, firstModifier == std::string::npos
        ? std::string::npos : firstModifier - 2);
    if (codeText.empty()
        || !std::all_of(codeText.begin(), codeText.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        })) {
        return std::nullopt;
    }
    unsigned long value = 0;
    try {
        value = std::stoul(codeText);
    } catch (...) {
        return std::nullopt;
    }
    if (value == 0 || value > 0xFFFFFFFFul) {
        return std::nullopt;
    }
    KeyDescriptor key;
    key.device = text[0] == 'M' ? InputDevice::Mouse : InputDevice::Keyboard;
    key.code = static_cast<std::uint32_t>(value);
    if (firstModifier != std::string::npos) {
        if (text.back() == '+') return std::nullopt;
        std::size_t start = firstModifier + 1;
        while (start <= text.size()) {
            const auto end = text.find('+', start);
            const auto part = text.substr(start, end == std::string::npos
                ? std::string::npos : end - start);
            if (part == "Ctrl") {
                if ((key.modifiers & kKeyModifierCtrl) != 0) return std::nullopt;
                key.modifiers |= kKeyModifierCtrl;
            } else if (part == "Shift") {
                if ((key.modifiers & kKeyModifierShift) != 0) return std::nullopt;
                key.modifiers |= kKeyModifierShift;
            } else if (part == "Alt") {
                if ((key.modifiers & kKeyModifierAlt) != 0) return std::nullopt;
                key.modifiers |= kKeyModifierAlt;
            }
            else return std::nullopt;
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    key = NormalizeKeyDescriptor(key);
    return key;
}

std::optional<KeyDescriptor> DecodeKeyField(const jsonlite::Object& object, const std::string& field) {
    const auto it = object.find(field);
    if (it == object.end()) return std::nullopt;
    const auto* text = it->second.AsString();
    if (!text) return std::nullopt;
    return DecodeKey(*text);
}

std::optional<std::string> StringField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) return std::nullopt;
    if (const auto* value = it->second.AsString()) return *value;
    return std::nullopt;
}

std::optional<bool> BoolField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) return std::nullopt;
    if (const auto* value = it->second.AsBool()) return *value;
    return std::nullopt;
}

std::optional<double> NumberField(const jsonlite::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) return std::nullopt;
    if (const auto* value = it->second.AsNumber()) return *value;
    return std::nullopt;
}

std::optional<int> IntField(const jsonlite::Object& object, const std::string& key) {
    const auto value = NumberField(object, key);
    if (!value.has_value() || !std::isfinite(*value)) return std::nullopt;
    return static_cast<int>(*value);
}

int ClampDelay(int value) {
    return std::clamp(value, 1, 3600000);
}

// ── JSON 载荷 ──
jsonlite::Value PayloadToJson(const SharePayload& payload) {
    jsonlite::Object root;
    root.emplace("v", payload.formatVersion);
    if (!payload.faction.empty()) {
        root.emplace("faction", payload.faction);
    }

    jsonlite::Array profiles;
    for (const auto& profile : payload.profiles) {
        jsonlite::Object entry;
        entry.emplace("name", WideToUtf8(profile.name));
        entry.emplace("enabled", profile.enabled);
        entry.emplace("defaultDelayMs", ClampDelay(profile.defaultDelayMs));
        entry.emplace("mixedMode", profile.mixedMode);
        entry.emplace("wasdProtection", profile.wasdProtection);
        if (profile.controlHotkey.has_value()) {
            entry.emplace("controlHotkey", EncodeKey(*profile.controlHotkey));
        }
        if (profile.sequenceSwitchKey.has_value()) {
            entry.emplace("sequenceSwitchKey", EncodeKey(*profile.sequenceSwitchKey));
        }
        if (!profile.pauseKeys.empty()) {
            jsonlite::Array pauseKeys;
            for (const auto& key : profile.pauseKeys) {
                pauseKeys.emplace_back(EncodeKey(key));
            }
            entry.emplace("pauseKeys", std::move(pauseKeys));
        }
        jsonlite::Array bindings;
        for (const auto& binding : profile.bindings) {
            jsonlite::Object bindingObj;
            bindingObj.emplace("enabled", binding.enabled);
            bindingObj.emplace("key", EncodeKey(binding.key));
            if (!binding.name.empty()) bindingObj.emplace("name", WideToUtf8(binding.name));
            const char* mode = "toggle";
            if (binding.mode == RepeatMode::Hold) mode = "hold";
            else if (binding.mode == RepeatMode::Independent) mode = "independent";
            else if (binding.mode == RepeatMode::Sequence) mode = "sequence";
            else if (binding.mode == RepeatMode::LongPress) mode = "longPress";
            bindingObj.emplace("mode", std::string(mode));
            bindingObj.emplace("delayMs", ClampDelay(binding.delayMs));
            bindingObj.emplace("suppressMixedMode", binding.suppressMixedMode);
            if (binding.key.device == InputDevice::Mouse && binding.clickPos.IsFixed()) {
                jsonlite::Object clickObj;
                clickObj.emplace("mode", std::string("fixed"));
                clickObj.emplace("x", std::clamp(binding.clickPos.anchorX, 0.0, 1.0));
                clickObj.emplace("y", std::clamp(binding.clickPos.anchorY, 0.0, 1.0));
                bindingObj.emplace("clickPos", std::move(clickObj));
            }
            bindings.emplace_back(std::move(bindingObj));
        }
        entry.emplace("bindings", std::move(bindings));
        profiles.emplace_back(std::move(entry));
    }
    root.emplace("profiles", std::move(profiles));

    jsonlite::Array macros;
    for (const auto& macro : payload.macros) {
        jsonlite::Object entry;
        entry.emplace("name", WideToUtf8(macro.name));
        entry.emplace("enabled", macro.enabled);
        entry.emplace("triggerMode", std::string(macro.triggerMode == MacroTriggerMode::Hold ? "hold" : "toggle"));
        if (macro.triggerKey.has_value()) {
            entry.emplace("triggerKey", EncodeKey(*macro.triggerKey));
        }
        entry.emplace("loopCount", std::clamp(macro.loopCount, 0, 1000000));
        if (!macro.pauseKeys.empty()) {
            jsonlite::Array pauseKeys;
            for (const auto& key : macro.pauseKeys) {
                pauseKeys.emplace_back(EncodeKey(key));
            }
            entry.emplace("pauseKeys", std::move(pauseKeys));
        }
        jsonlite::Array steps;
        for (const auto& step : macro.steps) {
            jsonlite::Object stepObj;
            stepObj.emplace("action", MacroActionStorage(step.action));
            stepObj.emplace("durationMs", step.durationMs);
            stepObj.emplace("wheelDelta", step.wheelDelta);
            if (MacroActionNeedsKey(step.action)) stepObj.emplace("key", EncodeKey(step.key));
            stepObj.emplace("enabled", step.enabled);
            if (!step.name.empty()) stepObj.emplace("name", WideToUtf8(step.name));
            stepObj.emplace("delayAfterMs", std::max(0, step.delayAfterMs));
            if (step.key.device == InputDevice::Mouse && step.clickPos.IsFixed()) {
                jsonlite::Object clickObj;
                clickObj.emplace("mode", std::string("fixed"));
                clickObj.emplace("x", std::clamp(step.clickPos.anchorX, 0.0, 1.0));
                clickObj.emplace("y", std::clamp(step.clickPos.anchorY, 0.0, 1.0));
                stepObj.emplace("clickPos", std::move(clickObj));
            }
            steps.emplace_back(std::move(stepObj));
        }
        entry.emplace("steps", std::move(steps));
        macros.emplace_back(std::move(entry));
    }
    root.emplace("macros", std::move(macros));
    return root;
}

std::optional<SharePayload> PayloadFromJson(const std::string& json) {
    jsonlite::Value root;
    std::string error;
    if (!jsonlite::Parse(json, root, error) || !root.IsObject()) {
        return std::nullopt;
    }
    const auto* object = root.AsObject();

    SharePayload payload;
    payload.formatVersion = IntField(*object, "v").value_or(1);
    if (payload.formatVersion < 1 || payload.formatVersion > 99) {
        return std::nullopt;
    }
    if (const auto faction = StringField(*object, "faction")) {
        if (faction->size() <= 64) {
            payload.faction = *faction;
        }
    }

    const auto profilesIt = object->find("profiles");
    if (profilesIt != object->end()) {
        const auto* profiles = profilesIt->second.AsArray();
        if (profiles && profiles->size() <= ShareCodec::kMaxProfiles) {
            for (const auto& item : *profiles) {
                const auto* entry = item.AsObject();
                if (!entry) continue;
                ProfileConfig profile;
                if (const auto name = StringField(*entry, "name")) {
                    profile.name = Utf8ToWide(*name);
                }
                if (profile.name.empty() || profile.name.size() > 64) continue;
                profile.enabled = BoolField(*entry, "enabled").value_or(true);
                profile.defaultDelayMs = ClampDelay(IntField(*entry, "defaultDelayMs").value_or(kDefaultDelayMs));
                profile.mixedMode = BoolField(*entry, "mixedMode").value_or(false);
                profile.wasdProtection = BoolField(*entry, "wasdProtection").value_or(false);
                profile.controlHotkey = DecodeKeyField(*entry, "controlHotkey");
                profile.sequenceSwitchKey = DecodeKeyField(*entry, "sequenceSwitchKey");
                if (const auto pauseIt = entry->find("pauseKeys"); pauseIt != entry->end()) {
                    if (const auto* pauseArr = pauseIt->second.AsArray()) {
                        for (const auto& pauseItem : *pauseArr) {
                            if (const auto* text = pauseItem.AsString()) {
                                if (auto key = DecodeKey(*text)) {
                                    profile.pauseKeys.push_back(*key);
                                }
                            }
                            if (profile.pauseKeys.size() >= ShareCodec::kMaxPauseKeys) break;
                        }
                    }
                }
                if (const auto bindingsIt = entry->find("bindings"); bindingsIt != entry->end()) {
                    if (const auto* bindings = bindingsIt->second.AsArray()) {
                        for (const auto& bindingItem : *bindings) {
                            const auto* bindingObj = bindingItem.AsObject();
                            if (!bindingObj) continue;
                            if (profile.bindings.size() >= ShareCodec::kMaxBindingsPerProfile) break;
                            auto key = DecodeKeyField(*bindingObj, "key");
                            if (!key.has_value()) continue;
                            KeyBinding binding;
                            binding.enabled = BoolField(*bindingObj, "enabled").value_or(true);
                            binding.key = *key;
                            if (auto name = StringField(*bindingObj, "name")) {
                                if (name->size() <= 64) binding.name = Utf8ToWide(*name);
                            }
                            const auto mode = StringField(*bindingObj, "mode").value_or("toggle");
                            if (mode == "hold") binding.mode = RepeatMode::Hold;
                            else if (mode == "independent") binding.mode = RepeatMode::Independent;
                            else if (mode == "sequence") binding.mode = RepeatMode::Sequence;
                            else if (mode == "longPress") binding.mode = RepeatMode::LongPress;
                            else binding.mode = RepeatMode::Toggle;
                            binding.delayMs = ClampDelay(IntField(*bindingObj, "delayMs").value_or(kDefaultDelayMs));
                            binding.suppressMixedMode = BoolField(*bindingObj, "suppressMixedMode").value_or(false);
                            if (key->device == InputDevice::Mouse) {
                                if (const auto clickIt = bindingObj->find("clickPos"); clickIt != bindingObj->end()) {
                                    if (const auto* clickObj = clickIt->second.AsObject()) {
                                        if (StringField(*clickObj, "mode").value_or("") == "fixed") {
                                            binding.clickPos.fixedPosition = true;
                                            binding.clickPos.anchorX = std::clamp(
                                                NumberField(*clickObj, "x").value_or(0.5), 0.0, 1.0);
                                            binding.clickPos.anchorY = std::clamp(
                                                NumberField(*clickObj, "y").value_or(0.5), 0.0, 1.0);
                                        }
                                    }
                                }
                            }
                            profile.bindings.push_back(binding);
                        }
                    }
                }
                payload.profiles.push_back(std::move(profile));
            }
        }
    }

    const auto macrosIt = object->find("macros");
    if (macrosIt != object->end()) {
        const auto* macros = macrosIt->second.AsArray();
        if (macros && macros->size() <= ShareCodec::kMaxMacros) {
            for (const auto& item : *macros) {
                const auto* entry = item.AsObject();
                if (!entry) continue;
                MacroConfig macro;
                if (const auto name = StringField(*entry, "name")) {
                    macro.name = Utf8ToWide(*name);
                }
                if (macro.name.empty() || macro.name.size() > 64) continue;
                macro.enabled = BoolField(*entry, "enabled").value_or(true);
                macro.triggerMode = StringField(*entry, "triggerMode").value_or("toggle") == "hold"
                    ? MacroTriggerMode::Hold
                    : MacroTriggerMode::Toggle;
                macro.triggerKey = DecodeKeyField(*entry, "triggerKey");
                macro.loopCount = std::clamp(IntField(*entry, "loopCount").value_or(0), 0, 1000000);
                if (const auto pauseIt = entry->find("pauseKeys"); pauseIt != entry->end()) {
                    if (const auto* pauseArr = pauseIt->second.AsArray()) {
                        for (const auto& pauseItem : *pauseArr) {
                            if (const auto* text = pauseItem.AsString()) {
                                if (auto key = DecodeKey(*text)) {
                                    macro.pauseKeys.push_back(*key);
                                }
                            }
                            if (macro.pauseKeys.size() >= ShareCodec::kMaxPauseKeys) break;
                        }
                    }
                }
                if (const auto stepsIt = entry->find("steps"); stepsIt != entry->end()) {
                    if (const auto* steps = stepsIt->second.AsArray()) {
                        if (steps->size() > ShareCodec::kMaxStepsPerMacro) return std::nullopt;
                        for (const auto& stepItem : *steps) {
                            const auto* stepObj = stepItem.AsObject();
                            if (!stepObj) continue;
                            const auto action = ParseMacroAction(StringField(*stepObj, "action").value_or("click"));
                            if (!action) return std::nullopt;
                            auto key = DecodeKeyField(*stepObj, "key");
                            if (MacroActionNeedsKey(*action) && !key) return std::nullopt;
                            MacroStep step;
                            step.action = *action;
                            step.durationMs = IntField(*stepObj, "durationMs").value_or(500);
                            step.wheelDelta = IntField(*stepObj, "wheelDelta").value_or(120);
                            step.enabled = BoolField(*stepObj, "enabled").value_or(true);
                            if (key) step.key = *key;
                            if (auto name = StringField(*stepObj, "name")) {
                                if (name->size() <= 64) step.name = Utf8ToWide(*name);
                            }
                            step.delayAfterMs = std::max(0, IntField(*stepObj, "delayAfterMs").value_or(kDefaultDelayMs));
                            if (step.key.device == InputDevice::Mouse) {
                                if (const auto clickIt = stepObj->find("clickPos"); clickIt != stepObj->end()) {
                                    if (const auto* clickObj = clickIt->second.AsObject()) {
                                        if (StringField(*clickObj, "mode").value_or("") == "fixed") {
                                            step.clickPos.fixedPosition = true;
                                            step.clickPos.anchorX = std::clamp(
                                                NumberField(*clickObj, "x").value_or(0.5), 0.0, 1.0);
                                            step.clickPos.anchorY = std::clamp(
                                                NumberField(*clickObj, "y").value_or(0.5), 0.0, 1.0);
                                        }
                                    }
                                }
                            }
                            if (step.durationMs < 1 || (step.action == MacroAction::Wheel
                                && (step.wheelDelta == 0 || step.wheelDelta % 120 != 0
                                    || step.wheelDelta < -12000 || step.wheelDelta > 12000))) return std::nullopt;
                            NormalizeMacroStep(step);
                            macro.steps.push_back(step);
                        }
                    }
                }
                payload.macros.push_back(std::move(macro));
            }
        }
    }

    if (payload.IsEmpty()) {
        return std::nullopt;
    }
    return payload;
}

// ── zlib 压缩 + base64url ──
const char kBase64UrlAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string Base64UrlEncode(const std::uint8_t* data, std::size_t size) {
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    std::size_t i = 0;
    while (i + 2 < size) {
        const std::uint32_t triple = (static_cast<std::uint32_t>(data[i]) << 16)
            | (static_cast<std::uint32_t>(data[i + 1]) << 8)
            | static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kBase64UrlAlphabet[(triple >> 18) & 0x3F]);
        out.push_back(kBase64UrlAlphabet[(triple >> 12) & 0x3F]);
        out.push_back(kBase64UrlAlphabet[(triple >> 6) & 0x3F]);
        out.push_back(kBase64UrlAlphabet[triple & 0x3F]);
        i += 3;
    }
    if (i + 1 == size) {
        const std::uint32_t pair = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kBase64UrlAlphabet[(pair >> 18) & 0x3F]);
        out.push_back(kBase64UrlAlphabet[(pair >> 12) & 0x3F]);
    } else if (i + 2 == size) {
        const std::uint32_t pair = (static_cast<std::uint32_t>(data[i]) << 16)
            | (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(kBase64UrlAlphabet[(pair >> 18) & 0x3F]);
        out.push_back(kBase64UrlAlphabet[(pair >> 12) & 0x3F]);
        out.push_back(kBase64UrlAlphabet[(pair >> 6) & 0x3F]);
    }
    return out;
}

int Base64DecodeChar(char ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '-' || ch == '+') return 62;
    if (ch == '_' || ch == '/') return 63;
    return -1;
}

std::vector<std::uint8_t> Base64UrlDecode(const std::string& text) {
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 4 * 3 + 3);
    std::uint32_t buffer = 0;
    int bits = 0;
    for (const char ch : text) {
        if (ch == '=' || ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') continue;
        const int value = Base64DecodeChar(ch);
        if (value < 0) {
            return {};
        }
        buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

bool DeflateZlib(const std::string& input, std::vector<std::uint8_t>& output) {
    size_t outLength = 0;
    // 低 12 位是哈希探测次数（最大压缩），加上 zlib 头部标志。
    void* compressed = tdefl_compress_mem_to_heap(
        input.data(), input.size(), &outLength,
        TDEFL_WRITE_ZLIB_HEADER | (512 & TDEFL_MAX_PROBES_MASK));
    if (compressed == nullptr) {
        return false;
    }
    output.assign(
        static_cast<const std::uint8_t*>(compressed),
        static_cast<const std::uint8_t*>(compressed) + outLength);
    mz_free(compressed);
    return !output.empty();
}

bool InflateZlib(const std::uint8_t* data, std::size_t size, std::string& output) {
    if (size == 0) {
        return false;
    }

    tinfl_decompressor decompressor;
    tinfl_init(&decompressor);

    output.clear();
    output.resize(4096);
    std::size_t inPosition = 0;
    std::size_t outPosition = 0;
    int guard = 0;

    while (true) {
        size_t inBytes = size - inPosition;
        size_t outBytes = output.size() - outPosition;
        const tinfl_status status = tinfl_decompress(
            &decompressor,
            reinterpret_cast<const mz_uint8*>(data + inPosition),
            &inBytes,
            reinterpret_cast<mz_uint8*>(output.data()),
            reinterpret_cast<mz_uint8*>(output.data() + outPosition),
            &outBytes,
            (inPosition < size ? TINFL_FLAG_HAS_MORE_INPUT : 0)
                | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF
                | TINFL_FLAG_PARSE_ZLIB_HEADER);
        inPosition += inBytes;
        outPosition += outBytes;

        if (status == TINFL_STATUS_DONE) {
            output.resize(outPosition);
            return !output.empty();
        }
        if (status < TINFL_STATUS_DONE) {
            // 负值为各种解压失败。
            return false;
        }
        if (status == TINFL_STATUS_NEEDS_MORE_INPUT && inPosition >= size) {
            return false;
        }
        if (outPosition == output.size()) {
            if (output.size() >= ShareCodec::kMaxInflatedBytes) {
                return false;
            }
            output.resize(std::min(output.size() * 2, ShareCodec::kMaxInflatedBytes + 1));
        }
        if (++guard > 1000000) {
            return false;
        }
    }
}

}  // namespace

std::string ShareCodec::EncodeShareCode(const SharePayload& payload) {
    const std::string json = jsonlite::Stringify(PayloadToJson(payload));
    std::vector<std::uint8_t> compressed;
    if (!DeflateZlib(json, compressed)) {
        return {};
    }
    return "XJK1:" + Base64UrlEncode(compressed.data(), compressed.size());
}

std::optional<SharePayload> ShareCodec::DecodeShareCode(const std::string& code) {
    if (code.size() < 8 || code.size() > kMaxShareCodeChars) {
        return std::nullopt;
    }
    const std::string prefix = "XJK1:";
    if (code.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> compressed = Base64UrlDecode(code.substr(prefix.size()));
    if (compressed.empty()) {
        return std::nullopt;
    }

    std::string json;
    if (!InflateZlib(compressed.data(), compressed.size(), json)) {
        return std::nullopt;
    }
    return PayloadFromJson(json);
}

// ── 文本配方 ──
namespace {

constexpr const char* kRecipeMagic = "XJK-RECIPE V1";
constexpr const char* kTokenFaction = "\u95e8\u6d3e";          // 门派
constexpr const char* kTokenProfile = "\u65b9\u6848";          // 方案
constexpr const char* kTokenProfileSettings = "\u65b9\u6848\u8bbe\u7f6e";  // 方案设置
constexpr const char* kTokenBinding = "\u8fde\u53d1";          // 连发
constexpr const char* kTokenBindingName = "\u8fde\u53d1\u540d\u79f0";    // 连发名称
constexpr const char* kTokenMacro = "\u5b8f";                  // 宏
constexpr const char* kTokenMacroSettings = "\u5b8f\u8bbe\u7f6e";          // 宏设置
constexpr const char* kTokenSteps = "\u6b65\u9aa4";            // 步骤
constexpr const char* kTokenStepName = "\u6b65\u9aa4\u540d\u79f0";        // 步骤名称
constexpr const char* kKeyEnabled = "\u542f\u7528";            // 启用
constexpr const char* kKeyMixedMode = "\u8fde\u53d1\u4f18\u5148";          // 连发优先
constexpr const char* kKeyWasd = "WASD\u9632\u5361";           // WASD防卡
constexpr const char* kKeyDelay = "\u95f4\u9694";              // 间隔
constexpr const char* kKeyControlHotkey = "\u542f\u505c\u952e";            // 启停键
constexpr const char* kKeySwitchKey = "\u5207\u6362\u952e";   // 切换键
constexpr const char* kKeyPauseKeys = "\u6682\u505c\u952e";   // 暂停键
constexpr const char* kKeyPoint = "\u70b9\u4f4d";              // 点位
constexpr const char* kKeyPriority = "\u53c2\u4e0e";           // 参与
constexpr const char* kKeyTrigger = "\u89e6\u53d1";            // 触发
constexpr const char* kKeyTriggerKey = "\u89e6\u53d1\u952e";   // 触发键
constexpr const char* kKeyLoop = "\u5faa\u73af";               // 循环
constexpr const char* kModeToggle = "\u5f00\u5173";            // 开关
constexpr const char* kModeHold = "\u6309\u538b";              // 按压
constexpr const char* kModeIndependent = "\u72ec\u7acb";       // 独立
constexpr const char* kModeSequence = "\u5207\u6362";          // 切换
constexpr const char* kModeLongPress = "\u957f\u6309";
constexpr const char* kValueYes = "\u662f";                    // 是
constexpr const char* kValueNo = "\u5426";                     // 否

std::vector<std::string> SplitTokens(const std::string& value) {
    std::vector<std::string> tokens;
    std::string current;
    for (const char ch : value) {
        if (ch == ' ') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

// "键=值" 形式的 token 解析；返回的 value 不含前后空格。
bool ParseKeyedToken(const std::string& token, std::string& key, std::string& value) {
    const auto pos = token.find('=');
    if (pos == std::string::npos || pos == 0 || pos + 1 >= token.size()) {
        return false;
    }
    key = token.substr(0, pos);
    value = token.substr(pos + 1);
    return true;
}

bool ParseIndexedName(const std::string& value, std::size_t& index, std::wstring& name) {
    const auto space = value.find(' ');
    if (space == std::string::npos || space == 0 || space + 1 >= value.size()) return false;
    index = static_cast<std::size_t>(std::atoi(value.substr(0, space).c_str()));
    jsonlite::Value parsed;
    std::string error;
    if (!jsonlite::Parse(value.substr(space + 1), parsed, error) || !parsed.IsString()) return false;
    const auto* text = parsed.AsString();
    if (!text || text->empty() || text->size() > 64) return false;
    index = static_cast<std::size_t>(std::atoi(value.substr(0, space).c_str()));
    name = Utf8ToWide(*text);
    return true;
}

bool StartsWithKey(const std::string& line, const char* keyword, std::string& value) {
    const std::size_t length = std::strlen(keyword);
    if (line.compare(0, length, keyword) != 0) {
        return false;
    }
    if (line.size() > length && line[length] == ':') {
        value = line.substr(length + 1);
        while (!value.empty() && (value.front() == ' ')) value.erase(value.begin());
        return true;
    }
    return false;
}

std::string Trimmed(std::string text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\r')) {
        text.erase(text.begin());
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

std::vector<KeyDescriptor> ParseKeyList(const std::string& value) {
    std::vector<KeyDescriptor> keys;
    // 兼容 "K:8,M:4" 与 "K:8, M:4" 两种写法：按逗号与空格共同分隔。
    std::string normalized;
    for (const char ch : value) {
        normalized.push_back(ch == ',' ? ' ' : ch);
    }
    for (const auto& token : SplitTokens(normalized)) {
        if (auto key = DecodeKey(token)) {
            keys.push_back(*key);
            if (keys.size() >= ShareCodec::kMaxPauseKeys) break;
        }
    }
    return keys;
}

std::string PercentValue(double ratio) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f%%", ratio * 100.0);
    return buffer;
}

}  // namespace

std::string ShareCodec::EncodeTextRecipe(const SharePayload& payload) {
    std::string out = kRecipeMagic;
    out += "\n";
    if (!payload.faction.empty()) {
        out += std::string(kTokenFaction) + ": " + payload.faction + "\n";
    }
    for (const auto& profile : payload.profiles) {
        out += std::string(kTokenProfile) + ": " + WideToUtf8(profile.name) + "\n";
        std::string settings;
        if (profile.enabled) settings += std::string(" ") + kKeyEnabled;
        settings += " " + std::string(kKeyDelay) + "=" + std::to_string(ClampDelay(profile.defaultDelayMs));
        if (profile.mixedMode) settings += " " + std::string(kKeyMixedMode);
        if (profile.wasdProtection) settings += " " + std::string(kKeyWasd);
        if (profile.controlHotkey.has_value()) {
            settings += " " + std::string(kKeyControlHotkey) + "=" + EncodeKey(*profile.controlHotkey);
        }
        if (profile.sequenceSwitchKey.has_value()) {
            settings += " " + std::string(kKeySwitchKey) + "=" + EncodeKey(*profile.sequenceSwitchKey);
        }
        if (!profile.pauseKeys.empty()) {
            settings += " " + std::string(kKeyPauseKeys) + "=";
            for (std::size_t i = 0; i < profile.pauseKeys.size(); ++i) {
                if (i > 0) settings += ",";
                settings += EncodeKey(profile.pauseKeys[i]);
            }
        }
        out += std::string(kTokenProfileSettings) + ":" + settings + "\n";
        for (std::size_t bindingIndex = 0; bindingIndex < profile.bindings.size(); ++bindingIndex) {
            const auto& binding = profile.bindings[bindingIndex];
            std::string line = std::string(kTokenBinding) + ": " + EncodeKey(binding.key);
            if (binding.mode == RepeatMode::Hold) line += std::string(" ") + kModeHold;
            else if (binding.mode == RepeatMode::Independent) line += std::string(" ") + kModeIndependent;
            else if (binding.mode == RepeatMode::Sequence) line += std::string(" ") + kModeSequence;
            else if (binding.mode == RepeatMode::LongPress) line += std::string(" ") + kModeLongPress;
            else line += std::string(" ") + kModeToggle;
            if (!binding.enabled) {
                // 未启用的绑定用 ! 前缀标记
                line += " !";
            }
            line += " " + std::string(kKeyDelay) + "=" + std::to_string(ClampDelay(binding.delayMs));
            if (binding.mode == RepeatMode::Hold && binding.suppressMixedMode) {
                line += " " + std::string(kKeyPriority) + "=" + kValueNo;
            }
            if (binding.key.device == InputDevice::Mouse && binding.clickPos.IsFixed()) {
                line += " " + std::string(kKeyPoint) + "="
                    + PercentValue(std::clamp(binding.clickPos.anchorX, 0.0, 1.0)) + ","
                    + PercentValue(std::clamp(binding.clickPos.anchorY, 0.0, 1.0));
            }
            out += line + "\n";
            if (!binding.name.empty()) {
                out += std::string(kTokenBindingName) + ": " + std::to_string(bindingIndex)
                    + " " + jsonlite::Stringify(WideToUtf8(binding.name)) + "\n";
            }
        }
    }
    for (const auto& macro : payload.macros) {
        out += std::string(kTokenMacro) + ": " + WideToUtf8(macro.name) + "\n";
        std::string settings;
        if (macro.enabled) settings += std::string(" ") + kKeyEnabled;
        settings += " " + std::string(kKeyTrigger) + "="
            + (macro.triggerMode == MacroTriggerMode::Hold ? kModeHold : kModeToggle);
        if (macro.triggerKey.has_value()) {
            settings += " " + std::string(kKeyTriggerKey) + "=" + EncodeKey(*macro.triggerKey);
        }
        settings += " " + std::string(kKeyLoop) + "=" + std::to_string(std::clamp(macro.loopCount, 0, 1000000));
        if (!macro.pauseKeys.empty()) {
            settings += " " + std::string(kKeyPauseKeys) + "=";
            for (std::size_t i = 0; i < macro.pauseKeys.size(); ++i) {
                if (i > 0) settings += ",";
                settings += EncodeKey(macro.pauseKeys[i]);
            }
        }
        out += std::string(kTokenMacroSettings) + ":" + settings + "\n";
        std::string steps;
        for (const auto& step : macro.steps) {
            steps += std::string(" ") + (step.enabled ? "" : "!") + MacroActionStorage(step.action)
                + "/" + (MacroActionNeedsKey(step.action) ? EncodeKey(step.key) : "-")
                + "/" + std::to_string(step.durationMs) + "/" + std::to_string(step.delayAfterMs)
                + "/" + std::to_string(step.wheelDelta);
            if (step.key.device == InputDevice::Mouse && step.clickPos.IsFixed()) {
                // 固定点位后缀 @x%,y%；旧版本解析到延迟即止，可安全忽略
                steps += "@" + PercentValue(std::clamp(step.clickPos.anchorX, 0.0, 1.0))
                    + "," + PercentValue(std::clamp(step.clickPos.anchorY, 0.0, 1.0));
            }
        }
        if (!steps.empty()) {
            out += std::string(kTokenSteps) + ":" + steps + "\n";
        }
        for (std::size_t index = 0; index < macro.steps.size(); ++index) {
            if (macro.steps[index].name.empty()) continue;
            out += std::string(kTokenStepName) + ": " + std::to_string(index)
                + " " + jsonlite::Stringify(WideToUtf8(macro.steps[index].name)) + "\n";
        }
    }
    return out;
}

std::optional<SharePayload> ShareCodec::DecodeTextRecipe(const std::string& text) {
    if (text.empty() || text.size() > kMaxRecipeChars) {
        return std::nullopt;
    }

    SharePayload payload;
    payload.profiles.reserve(4);
    payload.macros.reserve(4);
    ProfileConfig* currentProfile = nullptr;
    MacroConfig* currentMacro = nullptr;
    bool sawMagic = false;

    std::size_t lineStart = 0;
    while (lineStart <= text.size()) {
        std::size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = text.size();
        }
        const std::string line = Trimmed(text.substr(lineStart, lineEnd - lineStart));
        lineStart = lineEnd + 1;
        if (line.empty()) continue;

        if (!sawMagic) {
            if (line == kRecipeMagic) {
                sawMagic = true;
                continue;
            }
            return std::nullopt;
        }

        std::string value;
        if (StartsWithKey(line, kTokenProfileSettings, value) && currentProfile != nullptr) {
            for (const auto& token : SplitTokens(value)) {
                std::string tokenKey;
                std::string tokenValue;
                if (ParseKeyedToken(token, tokenKey, tokenValue)) {
                    if (tokenKey == kKeyDelay) {
                        currentProfile->defaultDelayMs = ClampDelay(std::atoi(tokenValue.c_str()));
                    } else if (tokenKey == kKeyControlHotkey) {
                        currentProfile->controlHotkey = DecodeKey(tokenValue);
                    } else if (tokenKey == kKeySwitchKey) {
                        currentProfile->sequenceSwitchKey = DecodeKey(tokenValue);
                    } else if (tokenKey == kKeyPauseKeys) {
                        currentProfile->pauseKeys = ParseKeyList(tokenValue);
                    }
                } else if (token == kKeyEnabled) {
                    currentProfile->enabled = true;
                } else if (token == kKeyMixedMode) {
                    currentProfile->mixedMode = true;
                } else if (token == kKeyWasd) {
                    currentProfile->wasdProtection = true;
                }
            }
            continue;
        }
        if (StartsWithKey(line, kTokenBinding, value) && currentProfile != nullptr) {
            if (currentProfile->bindings.size() >= kMaxBindingsPerProfile) {
                continue;
            }
            bool enabled = true;
            KeyBinding binding;
            bool haveKey = false;
            for (const auto& token : SplitTokens(value)) {
                if (token == "!") {
                    enabled = false;
                    continue;
                }
                if (!haveKey && (token.rfind("K:", 0) == 0 || token.rfind("M:", 0) == 0)) {
                    if (auto key = DecodeKey(token)) {
                        binding.key = *key;
                        haveKey = true;
                    }
                    continue;
                }
                if (token == kModeHold) binding.mode = RepeatMode::Hold;
                else if (token == kModeIndependent) binding.mode = RepeatMode::Independent;
                else if (token == kModeSequence) binding.mode = RepeatMode::Sequence;
                else if (token == kModeLongPress) binding.mode = RepeatMode::LongPress;
                else if (token == kModeToggle) binding.mode = RepeatMode::Toggle;

                std::string tokenKey;
                std::string tokenValue;
                if (ParseKeyedToken(token, tokenKey, tokenValue)) {
                    if (tokenKey == kKeyDelay) {
                        binding.delayMs = ClampDelay(std::atoi(tokenValue.c_str()));
                    } else if (tokenKey == kKeyPriority) {
                        binding.suppressMixedMode = (tokenValue == kValueNo);
                    } else if (tokenKey == kKeyPoint && binding.key.device == InputDevice::Mouse) {
                        const auto comma = tokenValue.find(',');
                        if (comma != std::string::npos) {
                            const double x = std::atof(tokenValue.substr(0, comma).c_str()) / 100.0;
                            const double y = std::atof(tokenValue.substr(comma + 1).c_str()) / 100.0;
                            if (x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0) {
                                binding.clickPos.fixedPosition = true;
                                binding.clickPos.anchorX = x;
                                binding.clickPos.anchorY = y;
                            }
                        }
                    }
                }
            }
            if (!haveKey) {
                continue;
            }
            binding.enabled = enabled;
            currentProfile->bindings.push_back(binding);
            continue;
        }
        if (StartsWithKey(line, kTokenBindingName, value) && currentProfile != nullptr) {
            std::size_t index = 0;
            std::wstring name;
            if (!ParseIndexedName(value, index, name)) return std::nullopt;
            if (index < currentProfile->bindings.size()) currentProfile->bindings[index].name = name;
            continue;
        }
        if (StartsWithKey(line, kTokenMacroSettings, value) && currentMacro != nullptr) {
            for (const auto& token : SplitTokens(value)) {
                std::string tokenKey;
                std::string tokenValue;
                if (ParseKeyedToken(token, tokenKey, tokenValue)) {
                    if (tokenKey == kKeyTrigger) {
                        currentMacro->triggerMode = tokenValue == kModeHold
                            ? MacroTriggerMode::Hold
                            : MacroTriggerMode::Toggle;
                    } else if (tokenKey == kKeyTriggerKey) {
                        currentMacro->triggerKey = DecodeKey(tokenValue);
                    } else if (tokenKey == kKeyLoop) {
                        currentMacro->loopCount = std::clamp(std::atoi(tokenValue.c_str()), 0, 1000000);
                    } else if (tokenKey == kKeyPauseKeys) {
                        currentMacro->pauseKeys = ParseKeyList(tokenValue);
                    }
                } else if (token == kKeyEnabled) {
                    currentMacro->enabled = true;
                }
            }
            continue;
        }
        if (StartsWithKey(line, kTokenSteps, value) && currentMacro != nullptr) {
            for (const auto& token : SplitTokens(value)) {
                if (currentMacro->steps.size() >= kMaxStepsPerMacro) return std::nullopt;
                MacroStep step;
                step.enabled = token.front() != '!';
                const std::string value = token.substr(step.enabled ? 0 : 1);
                const auto at = value.find('@');
                const auto body = value.substr(0, at);
                std::vector<std::string> parts;
                std::size_t start = 0;
                while (true) {
                    auto end = body.find('/', start);
                    parts.push_back(body.substr(start, end == std::string::npos ? end : end - start));
                    if (end == std::string::npos) break;
                    start = end + 1;
                }
                if (parts.size() != 5) return std::nullopt;
                const auto action = ParseMacroAction(parts[0]);
                if (!action) return std::nullopt;
                step.action = *action;
                if (MacroActionNeedsKey(step.action)) {
                    const auto key = DecodeKey(parts[1]);
                    if (!key) return std::nullopt;
                    step.key = *key;
                }
                try {
                    step.durationMs = std::stoi(parts[2]);
                    step.delayAfterMs = std::stoi(parts[3]);
                    step.wheelDelta = std::stoi(parts[4]);
                    if (at != std::string::npos && step.key.device == InputDevice::Mouse) {
                        auto point = value.substr(at + 1);
                        auto comma = point.find(',');
                        if (comma == std::string::npos) return std::nullopt;
                        step.clickPos = {true, std::clamp(std::stod(point.substr(0, comma)), 0.0, 1.0),
                            std::clamp(std::stod(point.substr(comma + 1)), 0.0, 1.0)};
                    }
                } catch (...) { return std::nullopt; }
                if (step.durationMs < 1 || step.delayAfterMs < 0 || (step.action == MacroAction::Wheel
                    && (step.wheelDelta == 0 || step.wheelDelta % 120 != 0
                        || step.wheelDelta < -12000 || step.wheelDelta > 12000))) return std::nullopt;
                NormalizeMacroStep(step);
                currentMacro->steps.push_back(step);
            }
            continue;
        }
        if (StartsWithKey(line, kTokenStepName, value) && currentMacro != nullptr) {
            std::size_t index = 0;
            std::wstring name;
            if (!ParseIndexedName(value, index, name)) return std::nullopt;
            if (index < currentMacro->steps.size()) currentMacro->steps[index].name = name;
            continue;
        }
        if (StartsWithKey(line, kTokenProfile, value)) {
            if (payload.profiles.size() >= kMaxProfiles) continue;
            const std::wstring name = Utf8ToWide(Trimmed(value));
            if (name.empty() || name.size() > 64) continue;
            payload.profiles.emplace_back();
            payload.profiles.back().name = name;
            currentProfile = &payload.profiles.back();
            currentMacro = nullptr;
            continue;
        }
        if (StartsWithKey(line, kTokenMacro, value)) {
            if (payload.macros.size() >= kMaxMacros) continue;
            const std::wstring name = Utf8ToWide(Trimmed(value));
            if (name.empty() || name.size() > 64) continue;
            payload.macros.emplace_back();
            payload.macros.back().name = name;
            currentMacro = &payload.macros.back();
            currentProfile = nullptr;
            continue;
        }
        if (StartsWithKey(line, kTokenFaction, value)) {
            const std::string faction = Trimmed(value);
            if (faction.size() <= 64) {
                payload.faction = faction;
            }
            continue;
        }
    }

    if (!sawMagic || payload.IsEmpty()) {
        return std::nullopt;
    }
    return payload;
}

bool ShareCodec::LooksLikeShareText(const std::string& text) {
    if (text.empty() || text.size() > kMaxRecipeChars) {
        return false;
    }
    static const std::string kSharePrefix = "XJK1:";
    std::string line = Trimmed(text);
    if (line.empty()) {
        return false;
    }
    if (line.compare(0, kSharePrefix.size(), kSharePrefix) == 0) {
        return line.size() >= 8 && line.size() <= kMaxShareCodeChars;
    }
    const std::size_t lineEnd = line.find('\n');
    if (lineEnd != std::string::npos) {
        line = Trimmed(line.substr(0, lineEnd));
    }
    return line == kRecipeMagic;
}
