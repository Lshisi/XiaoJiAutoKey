#pragma once

#include "SoundSettings.h"
#include <filesystem>
#include <string>

enum class SoundChannel { Repeat, Macro, Recording, Preview, Count };
enum class SoundRequestMode { Play, SilentPrime, Stop };

struct SoundPlaybackRequest {
    std::filesystem::path path;
    int volume = 80;
    std::wstring alias;
    SoundChannel channel = SoundChannel::Repeat;
    SoundRequestMode mode = SoundRequestMode::Play;
    SoundEvent event = SoundEvent::RepeatOn;
};
