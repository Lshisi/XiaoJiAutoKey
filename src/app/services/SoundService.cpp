#include "services/SoundService.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <array>
#include <fstream>
#include <queue>
#include <vector>
#include <Windows.h>
#include <mmsystem.h>
#include <utility>

#pragma comment(lib, "winmm.lib")

namespace {

const QStringList kSupportedAudioExtensions = {
    "mp3", "wav", "wma", "mid", "midi"
};

std::wstring defaultAliasForPath(const std::filesystem::path& soundPath) {
    const auto fileName = soundPath.filename().wstring();
    if (fileName == L"\u6309\u952e\u5f00\u542f.wav") return L"autokey_repeat_on_sound";
    if (fileName == L"\u6309\u952e\u5173\u95ed.wav") return L"autokey_repeat_off_sound";
    if (fileName == L"\u5faa\u73af\u5f00\u542f.wav") return L"autokey_macro_on_sound";
    if (fileName == L"\u5faa\u73af\u5173\u95ed.wav") return L"autokey_macro_off_sound";
    return {};
}

}  // namespace

SoundService::SoundService(std::filesystem::path exeDir) : exeDir_(std::move(exeDir)) {
    worker_ = std::thread(&SoundService::workerLoop, this);
}

SoundService::~SoundService() {
    {
        std::lock_guard lock(queueMutex_);
        stopping_ = true;
        while (!queue_.empty()) {
            queue_.pop();
        }
    }
    queueCv_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
    closeAliasSync(L"autokey_toggle_sound");
    closeAliasSync(L"autokey_macro_toggle_sound");
    closeAliasSync(L"autokey_repeat_on_sound");
    closeAliasSync(L"autokey_repeat_off_sound");
    closeAliasSync(L"autokey_macro_on_sound");
    closeAliasSync(L"autokey_macro_off_sound");
    closeAliasSync(L"autokey_preview_sound");
    openAliases_.clear();
}

void SoundService::playFile(const std::filesystem::path& soundPath, int volume, const wchar_t* alias) const {
    if (soundPath.empty()) return;

    {
        std::lock_guard lock(queueMutex_);
        if (stopping_) return;
        const auto defaultAlias = defaultAliasForPath(soundPath);
        const std::wstring aliasName = defaultAlias.empty() ? std::wstring(alias) : defaultAlias;
        std::queue<PlayRequest> retained;
        while (!queue_.empty()) {
            auto request = std::move(queue_.front());
            queue_.pop();
            if (request.mode == RequestMode::Play && request.alias != aliasName) {
                retained.push(std::move(request));
            }
        }
        queue_ = std::move(retained);
        queue_.push(PlayRequest{soundPath, volume, aliasName, RequestMode::Play});
    }
    queueCv_.notify_one();
}

void SoundService::closeAlias(const wchar_t* alias) const {
    const std::wstring aliasName(alias);
    {
        std::lock_guard lock(queueMutex_);
        std::queue<PlayRequest> retained;
        while (!queue_.empty()) {
            auto request = std::move(queue_.front());
            queue_.pop();
            if (request.alias != aliasName) {
                retained.push(std::move(request));
            }
        }
        queue_ = std::move(retained);
        openAliases_.erase(aliasName);
    }
    closeAliasSync(aliasName);
}

void SoundService::closeAllAliases() const {
    // 锁内完成：ensureAliasOpen 的 close+open+insert 是原子段，若在锁外关闭，
    // 工作线程可能立刻重新 open 再次占住文件；quiesced_ 让随后到达的播放
    // 请求直接失效，直到 resumePlayback()。
    std::lock_guard lock(queueMutex_);
    quiesced_ = true;
    std::queue<PlayRequest> empty;
    queue_.swap(empty);
    // 兜底把固定别名也关一遍：openAliases_ 理论上覆盖所有常驻设备，但只要
    // 漏掉一个，更新覆盖 audio/ 时 CopyFileW 就会稳定失败。
    std::vector<std::wstring> names = {
        L"autokey_toggle_sound",
        L"autokey_macro_toggle_sound",
        L"autokey_repeat_on_sound",
        L"autokey_repeat_off_sound",
        L"autokey_macro_on_sound",
        L"autokey_macro_off_sound",
        L"autokey_preview_sound",
    };
    for (const auto& entry : openAliases_) {
        names.push_back(entry.first);
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    {
        std::ofstream log(exeDir_ / L"update-debug.log", std::ios::app);
        if (log) {
            log << "closeAllAliases: map=" << openAliases_.size()
                << " closing=" << names.size() << "\n";
        }
    }
    {
        const MCIERROR rc = mciSendStringW(L"close all", nullptr, 0, nullptr);
        std::ofstream log(exeDir_ / L"update-debug.log", std::ios::app);
        if (log) {
            log << "  close all rc=" << rc << "\n";
        }
    }
    for (const auto& name : names) {
        const MCIERROR rc = mciSendStringW((L"close " + name).c_str(), nullptr, 0, nullptr);
        if (rc != 0) {
            std::ofstream log(exeDir_ / L"update-debug.log", std::ios::app);
            if (log) {
                log << "  close " << std::string(name.begin(), name.end())
                    << " rc=" << rc << "\n";
            }
        }
    }
    openAliases_.clear();
}

void SoundService::resumePlayback() const {
    std::lock_guard lock(queueMutex_);
    quiesced_ = false;
}

void SoundService::warmUpDefaultSounds() const {
    warmUpToggleSounds({}, {}, {}, {});
}

void SoundService::warmUpToggleSounds(
    const std::wstring& repeatOn,
    const std::wstring& repeatOff,
    const std::wstring& macroOn,
    const std::wstring& macroOff) const {
    std::array requests{
        PlayRequest{resolveToggleSoundPath(true, false, repeatOn, repeatOff), 0, L"autokey_repeat_on_sound", RequestMode::SilentPrime},
        PlayRequest{resolveToggleSoundPath(false, false, repeatOn, repeatOff), 0, L"autokey_repeat_off_sound", RequestMode::SilentPrime},
        PlayRequest{resolveToggleSoundPath(true, true, macroOn, macroOff), 0, L"autokey_macro_on_sound", RequestMode::SilentPrime},
        PlayRequest{resolveToggleSoundPath(false, true, macroOn, macroOff), 0, L"autokey_macro_off_sound", RequestMode::SilentPrime},
    };

    {
        std::lock_guard lock(queueMutex_);
        if (stopping_) return;
        for (auto& request : requests) {
            if (!request.soundPath.empty()) {
                queue_.push(std::move(request));
            }
        }
    }
    queueCv_.notify_one();
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
            soundPath = resolveAudioPath(running ? L"\u5faa\u73af\u5f00\u542f.wav" : L"\u5faa\u73af\u5173\u95ed.wav");
        } else {
            soundPath = resolveAudioPath(running ? L"\u6309\u952e\u5f00\u542f.wav" : L"\u6309\u952e\u5173\u95ed.wav");
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

void SoundService::workerLoop() {
    while (true) {
        PlayRequest request;
        {
            std::unique_lock lock(queueMutex_);
            queueCv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) {
                return;
            }
            request = std::move(queue_.front());
            queue_.pop();
        }
        try {
            playFileSync(request);
        } catch (...) {
        }
    }
}

void SoundService::playFileSync(const PlayRequest& request) const {
    if (request.soundPath.empty() || request.alias.empty()) return;

    if (!ensureAliasOpen(request)) return;
    if (request.mode == RequestMode::OpenOnly) return;

    const int mciVolume = std::clamp(request.volume, 0, 100) * 10;
    const std::wstring volumeCommand = L"setaudio " + request.alias + L" volume to " + std::to_wstring(mciVolume);
    mciSendStringW(volumeCommand.c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"seek " + request.alias + L" to start").c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"play " + request.alias + L" from 0").c_str(), nullptr, 0, nullptr);

    if (request.mode == RequestMode::SilentPrime) {
        mciSendStringW((L"stop " + request.alias).c_str(), nullptr, 0, nullptr);
        mciSendStringW((L"seek " + request.alias + L" to start").c_str(), nullptr, 0, nullptr);
    }
}

bool SoundService::ensureAliasOpen(const PlayRequest& request) const {
    std::lock_guard lock(queueMutex_);
    if (quiesced_) {
        return false;
    }
    const auto it = openAliases_.find(request.alias);
    if (it != openAliases_.end() && it->second == request.soundPath) {
        return true;
    }
    openAliases_.erase(request.alias);

    closeAliasSync(request.alias);
    // MCI's waveaudio driver does not reliably support the per-alias
    // "setaudio ... volume" command. Open WAV files through mpegvideo as
    // well so the configured preview/repeat/macro volume is actually used.
    const std::wstring openCommand = L"open \"" + request.soundPath.wstring() + L"\" type mpegvideo alias " + request.alias;
    if (mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr) != 0) {
        return false;
    }
    openAliases_[request.alias] = request.soundPath;
    return true;
}

void SoundService::closeAliasSync(const std::wstring& alias) {
    if (alias.empty()) return;
    mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);
}
