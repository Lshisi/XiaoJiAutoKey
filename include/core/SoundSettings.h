#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

enum class SoundGroup { Repeat, Macro, Recording };

enum class SoundEvent {
    RepeatOn, RepeatOff,
    IndependentOn, IndependentOff, IndependentSwitch, SequenceSwitch,
    RepeatPause, RepeatResume,
    MacroOn, MacroOff, MacroPause, MacroResume,
    RecordingOn, RecordingOff,
    Count,
};

struct SoundEventDefinition {
    SoundEvent event;
    const char* id;
    const char* title;
    const char* description;
    const wchar_t* fileName;
    SoundGroup group;
    int defaultVolume;
};

struct SoundEventSettings {
    bool enabled = true;
    std::wstring customPath;
    int volume = 80;
};

constexpr std::size_t kSoundEventCount = static_cast<std::size_t>(SoundEvent::Count);
using SoundSettings = std::array<SoundEventSettings, kSoundEventCount>;

constexpr std::size_t SoundEventIndex(SoundEvent event) { return static_cast<std::size_t>(event); }

const std::array<SoundEventDefinition, kSoundEventCount>& SoundEventDefinitions();
const SoundEventDefinition& SoundDefinition(SoundEvent event);
const SoundEventDefinition* FindSoundDefinition(std::string_view id);
SoundSettings DefaultSoundSettings();
