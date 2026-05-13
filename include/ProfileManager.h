#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ProfileEntry {
    std::uint64_t id = 0;
    std::wstring name;
    std::filesystem::path configPath;  // absolute path to the profile's config.json
};

class ProfileManager {
public:
    explicit ProfileManager(std::filesystem::path profilesDir);

    void Load();
    void Save() const;

    [[nodiscard]] const std::vector<ProfileEntry>& Profiles() const noexcept { return profiles_; }
    [[nodiscard]] std::uint64_t ActiveId() const noexcept { return activeId_; }
    [[nodiscard]] const ProfileEntry* ActiveProfile() const;
    [[nodiscard]] const ProfileEntry* FindById(std::uint64_t id) const;

    // Returns the new profile
    ProfileEntry AddProfile(const std::wstring& name);
    bool RenameProfile(std::uint64_t id, const std::wstring& newName);
    bool UpdateConfigPath(std::uint64_t id, const std::filesystem::path& newPath);
    bool DeleteProfile(std::uint64_t id);  // cannot delete last profile
    void SetActive(std::uint64_t id);

private:
    [[nodiscard]] std::filesystem::path profilesFilePath() const;
    [[nodiscard]] std::filesystem::path generateConfigPath(std::uint64_t id) const;
    std::uint64_t nextId() const;

    std::filesystem::path profilesDir_;
    std::vector<ProfileEntry> profiles_;
    std::uint64_t activeId_ = 0;
};
