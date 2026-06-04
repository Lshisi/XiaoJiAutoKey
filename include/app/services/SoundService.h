#pragma once

#include <filesystem>
#include <QString>
#include <string>
#include <utility>

class SoundService {
public:
    explicit SoundService(std::filesystem::path exeDir);

    void playFile(const std::filesystem::path& soundPath, int volume, const wchar_t* alias) const;
    void closeAlias(const wchar_t* alias) const;

    [[nodiscard]] std::filesystem::path resolveToggleSoundPath(
        bool running,
        bool macroMode,
        const std::wstring& customOn,
        const std::wstring& customOff) const;

    [[nodiscard]] bool isValidAudioFile(const QString& path) const;

private:
    [[nodiscard]] std::filesystem::path resolveAudioPath(const wchar_t* fileName) const;

    std::filesystem::path exeDir_;
};
