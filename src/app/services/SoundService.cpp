#include "services/SoundService.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <array>
#include <Windows.h>
#include <mmsystem.h>
#include <utility>

#pragma comment(lib, "winmm.lib")

namespace {

const QStringList kSupportedAudioExtensions = {
    "mp3", "wav", "wma", "mid", "midi"
};

}  // namespace

SoundService::SoundService(std::filesystem::path exeDir) : exeDir_(std::move(exeDir)) {}

void SoundService::playFile(const std::filesystem::path& soundPath, int volume, const wchar_t* alias) const {
    if (soundPath.empty()) return;

    const std::wstring aliasName(alias);
    closeAlias(alias);
    const std::wstring openCommand = L"open \"" + soundPath.wstring() + L"\" type mpegvideo alias " + aliasName;
    if (mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr) != 0) return;

    const int mciVolume = std::clamp(volume, 0, 100) * 10;
    const std::wstring volumeCommand = L"setaudio " + aliasName + L" volume to " + std::to_wstring(mciVolume);
    mciSendStringW(volumeCommand.c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"play " + aliasName + L" from 0").c_str(), nullptr, 0, nullptr);
}

void SoundService::closeAlias(const wchar_t* alias) const {
    const std::wstring aliasName(alias);
    mciSendStringW((L"close " + aliasName).c_str(), nullptr, 0, nullptr);
}

std::filesystem::path SoundService::resolveToggleSoundPath(
    bool running,
    bool macroMode,
    const std::wstring& customOn,
    const std::wstring& customOff) const {
    std::filesystem::path soundPath;
    if (running && !customOn.empty()) {
        soundPath = customOn;
        if (!std::filesystem::exists(soundPath)) soundPath.clear();
    } else if (!running && !customOff.empty()) {
        soundPath = customOff;
        if (!std::filesystem::exists(soundPath)) soundPath.clear();
    }

    if (soundPath.empty()) {
        if (macroMode) {
            soundPath = resolveAudioPath(running ? L"\u5faa\u73af\u5f00\u542f.mp3" : L"\u5faa\u73af\u5173\u95ed.mp3");
        } else {
            soundPath = resolveAudioPath(running ? L"\u6309\u952e\u5f00\u542f.mp3" : L"\u6309\u952e\u5173\u95ed.mp3");
        }
    }
    return soundPath;
}

bool SoundService::isValidAudioFile(const QString& path) const {
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) return false;
    return kSupportedAudioExtensions.contains(info.suffix().toLower());
}

std::filesystem::path SoundService::resolveAudioPath(const wchar_t* fileName) const {
    const auto parent1 = exeDir_.parent_path();
    const auto parent2 = parent1.parent_path();
    const std::array candidates{
        exeDir_ / L"audio" / fileName,
        exeDir_ / L"assets" / L"audio" / fileName,
        parent1 / L"assets" / L"audio" / fileName,
        parent2 / L"assets" / L"audio" / fileName,
    };
    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) return candidate;
    }
    return {};
}
