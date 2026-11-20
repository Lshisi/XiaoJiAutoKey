#pragma once

#include "SoundSettings.h"
#include "audio/SoundPlayback.h"
#include <array>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <QString>
#include <string>
#include <thread>
#include <utility>

class SoundService {
public:
    explicit SoundService(std::filesystem::path exeDir);
    ~SoundService();

    void playEvent(SoundEvent event, const SoundEventSettings& settings, bool preview = false) const;
    void closeAlias(const wchar_t* alias) const;
    void closeAllAliases() const;
    void resumePlayback() const;
    void warmUpSounds(const SoundSettings& settings) const;
    [[nodiscard]] std::filesystem::path resolveSoundPath(SoundEvent event, const SoundEventSettings& settings) const;

    [[nodiscard]] bool isValidAudioFile(const QString& path) const;

private:
    [[nodiscard]] std::filesystem::path resolveAudioPath(const wchar_t* fileName) const;
    void workerLoop();
    void playFileSync(SoundPlaybackRequest request) const;
    bool ensureAliasOpen(const SoundPlaybackRequest& request) const;
    static void closeAliasSync(const std::wstring& alias);

    std::filesystem::path exeDir_;
    mutable std::map<std::wstring, std::filesystem::path> openAliases_;
    mutable std::array<std::wstring, static_cast<std::size_t>(SoundChannel::Count)> playingAliases_;
    mutable std::mutex playbackMutex_;
    mutable std::mutex queueMutex_;
    mutable std::condition_variable queueCv_;
    mutable std::deque<SoundPlaybackRequest> queue_;
    mutable bool stopping_ = false;
    mutable bool quiesced_ = false;
    mutable std::thread worker_;
};
