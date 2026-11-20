#include "services/SoundService.h"

#include <QFileInfo>
#include <QStringList>
#include <algorithm>
#include <array>
#include <vector>
#include <Windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace {
const QStringList kSupportedAudioExtensions = {"mp3", "wav", "wma", "mid", "midi"};

std::wstring EventAlias(SoundEvent event) {
    return L"autokey_sound_" + QString::fromLatin1(SoundDefinition(event).id).toStdWString();
}

SoundChannel EventChannel(SoundEvent event) {
    return static_cast<SoundChannel>(SoundDefinition(event).group);
}
}

SoundService::SoundService(std::filesystem::path exeDir) : exeDir_(std::move(exeDir)) {
    worker_ = std::thread(&SoundService::workerLoop, this);
}

SoundService::~SoundService() {
    {
        std::lock_guard lock(queueMutex_);
        stopping_ = true;
        queue_.clear();
    }
    queueCv_.notify_one();
    if (worker_.joinable()) worker_.join();
    closeAllAliases();
}

void SoundService::playEvent(SoundEvent event, const SoundEventSettings& settings, bool preview) const {
    SoundPlaybackRequest request{settings.customPath, settings.volume,
        preview ? L"autokey_preview_sound" : EventAlias(event),
        preview ? SoundChannel::Preview : EventChannel(event),
        (preview || settings.enabled) && settings.volume > 0
            ? SoundRequestMode::Play : SoundRequestMode::Stop, event};
    {
        std::lock_guard lock(queueMutex_);
        if (stopping_ || quiesced_) return;
        std::erase_if(queue_, [&request](const auto& queued) { return queued.channel == request.channel; });
        const auto priming = std::find_if(queue_.begin(), queue_.end(), [](const auto& queued) {
            return queued.mode == SoundRequestMode::SilentPrime;
        });
        queue_.insert(priming, std::move(request));
    }
    queueCv_.notify_one();
}

void SoundService::closeAlias(const wchar_t* alias) const {
    std::scoped_lock lock(playbackMutex_, queueMutex_);
    const std::wstring name(alias);
    std::erase_if(queue_, [&name](const auto& request) { return request.alias == name; });
    closeAliasSync(name);
    openAliases_.erase(name);
    for (auto& playing : playingAliases_) {
        if (playing == name) playing.clear();
    }
}

void SoundService::closeAllAliases() const {
    // 更新覆盖 audio/ 前，持锁暂停播放并释放全部文件句柄。
    std::scoped_lock lock(playbackMutex_, queueMutex_);
    quiesced_ = true;
    queue_.clear();
    std::vector<std::wstring> names{
        L"autokey_toggle_sound", L"autokey_macro_toggle_sound",
        L"autokey_repeat_on_sound", L"autokey_repeat_off_sound",
        L"autokey_macro_on_sound", L"autokey_macro_off_sound", L"autokey_preview_sound",
    };
    for (const auto& definition : SoundEventDefinitions()) names.push_back(EventAlias(definition.event));
    for (const auto& [alias, path] : openAliases_) names.push_back(alias);
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    mciSendStringW(L"close all", nullptr, 0, nullptr);
    for (const auto& name : names) closeAliasSync(name);
    openAliases_.clear();
    playingAliases_.fill(std::wstring{});
}

void SoundService::resumePlayback() const {
    std::lock_guard lock(queueMutex_);
    quiesced_ = false;
}

void SoundService::warmUpSounds(const SoundSettings& settings) const {
    std::lock_guard lock(queueMutex_);
    if (stopping_ || quiesced_) return;
    for (const auto& definition : SoundEventDefinitions()) {
        const auto& option = settings[SoundEventIndex(definition.event)];
        if (!option.enabled || option.volume == 0) continue;
        queue_.push_back({option.customPath, 0, EventAlias(definition.event),
            EventChannel(definition.event), SoundRequestMode::SilentPrime, definition.event});
    }
    queueCv_.notify_one();
}

std::filesystem::path SoundService::resolveSoundPath(SoundEvent event, const SoundEventSettings& settings) const {
    if (!settings.customPath.empty()) {
        const std::filesystem::path path(settings.customPath);
        std::error_code error;
        return std::filesystem::is_regular_file(path, error) ? path : std::filesystem::path{};
    }
    return resolveAudioPath(SoundDefinition(event).fileName);
}

bool SoundService::isValidAudioFile(const QString& path) const {
    const QFileInfo info(path);
    return info.isFile() && kSupportedAudioExtensions.contains(info.suffix().toLower());
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
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;
    }
    return {};
}

void SoundService::workerLoop() {
    while (true) {
        {
            std::unique_lock lock(queueMutex_);
            queueCv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_) return;
        }
        // 先锁播放，再取请求；关闭试听或更新时不会漏掉已取出、尚未播放的请求。
        std::lock_guard playbackLock(playbackMutex_);
        SoundPlaybackRequest request;
        {
            std::lock_guard lock(queueMutex_);
            if (stopping_) return;
            if (queue_.empty()) continue;
            request = std::move(queue_.front());
            queue_.pop_front();
        }
        try {
            playFileSync(std::move(request));
        } catch (...) {
        }
    }
}

void SoundService::playFileSync(SoundPlaybackRequest request) const {
    auto& playing = playingAliases_[static_cast<std::size_t>(request.channel)];
    if (request.mode != SoundRequestMode::SilentPrime && !playing.empty()) {
        mciSendStringW((L"stop " + playing).c_str(), nullptr, 0, nullptr);
        playing.clear();
    }
    // 预热不能重置同一设备上正在播放的提示。
    if (request.mode == SoundRequestMode::SilentPrime && playing == request.alias) return;
    if (request.mode == SoundRequestMode::Stop) return;
    if (request.path.empty()) request.path = resolveAudioPath(SoundDefinition(request.event).fileName);
    std::error_code error;
    if (!std::filesystem::is_regular_file(request.path, error) || !ensureAliasOpen(request)) return;
    const int volume = std::clamp(request.volume, 0, 100) * 10;
    mciSendStringW((L"setaudio " + request.alias + L" volume to " + std::to_wstring(volume)).c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"play " + request.alias + L" from 0").c_str(), nullptr, 0, nullptr);
    if (request.mode == SoundRequestMode::SilentPrime) {
        mciSendStringW((L"stop " + request.alias).c_str(), nullptr, 0, nullptr);
        mciSendStringW((L"seek " + request.alias + L" to start").c_str(), nullptr, 0, nullptr);
    } else playing = request.alias;
}

bool SoundService::ensureAliasOpen(const SoundPlaybackRequest& request) const {
    const auto it = openAliases_.find(request.alias);
    if (it != openAliases_.end() && it->second == request.path) return true;
    closeAliasSync(request.alias);
    openAliases_.erase(request.alias);
    // WAV 也使用 mpegvideo，确保每个设备的音量设置能生效。
    const auto command = L"open \"" + request.path.wstring() + L"\" type mpegvideo alias " + request.alias;
    if (mciSendStringW(command.c_str(), nullptr, 0, nullptr) != 0) return false;
    openAliases_[request.alias] = request.path;
    return true;
}

void SoundService::closeAliasSync(const std::wstring& alias) {
    if (!alias.empty()) mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);
}
