#pragma once

#include <condition_variable>
#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <QString>
#include <string>
#include <thread>
#include <utility>

class SoundService {
public:
    explicit SoundService(std::filesystem::path exeDir);
    ~SoundService();

    void playFile(const std::filesystem::path& soundPath, int volume, const wchar_t* alias) const;
    void closeAlias(const wchar_t* alias) const;
    void warmUpDefaultSounds() const;
    void warmUpToggleSounds(
        const std::wstring& repeatOn,
        const std::wstring& repeatOff,
        const std::wstring& macroOn,
        const std::wstring& macroOff) const;

    [[nodiscard]] std::filesystem::path resolveToggleSoundPath(
        bool running,
        bool macroMode,
        const std::wstring& customOn,
        const std::wstring& customOff) const;

    [[nodiscard]] bool isValidAudioFile(const QString& path) const;

private:
    enum class RequestMode {
        Play,
        OpenOnly,
        SilentPrime,
    };

    struct PlayRequest {
        std::filesystem::path soundPath;
        int volume = 80;
        std::wstring alias;
        RequestMode mode = RequestMode::Play;
        std::shared_ptr<std::promise<void>> completion;
    };

    [[nodiscard]] std::filesystem::path resolveAudioPath(const wchar_t* fileName) const;
    void workerLoop();
    void playFileSync(const PlayRequest& request) const;
    bool ensureAliasOpen(const PlayRequest& request) const;
    static void closeAliasSync(const std::wstring& alias);

    std::filesystem::path exeDir_;
    mutable std::map<std::wstring, std::filesystem::path> openAliases_;
    mutable std::mutex queueMutex_;
    mutable std::condition_variable queueCv_;
    mutable std::queue<PlayRequest> queue_;
    mutable bool stopping_ = false;
    mutable std::thread worker_;
};
