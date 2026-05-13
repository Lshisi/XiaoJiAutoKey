#include "ProfileManager.h"
#include "JsonLite.h"
#include "AppTypes.h"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace {

std::optional<std::string> GetStr(const jsonlite::Object& obj, const std::string& key) {
    auto it = obj.find(key);
    if (it == obj.end()) return std::nullopt;
    if (auto* v = it->second.AsString()) return *v;
    return std::nullopt;
}

std::optional<double> GetNum(const jsonlite::Object& obj, const std::string& key) {
    auto it = obj.find(key);
    if (it == obj.end()) return std::nullopt;
    if (auto* v = it->second.AsNumber()) return *v;
    return std::nullopt;
}

}  // namespace

ProfileManager::ProfileManager(std::filesystem::path profilesDir)
    : profilesDir_(std::move(profilesDir)) {
    std::filesystem::create_directories(profilesDir_);
}

std::filesystem::path ProfileManager::profilesFilePath() const {
    return profilesDir_ / L"profiles.json";
}

void ProfileManager::Load() {
    profiles_.clear();
    activeId_ = 0;

    auto filePath = profilesFilePath();
    if (!std::filesystem::exists(filePath)) {
        // First run: create a default profile pointing to existing config.json if present
        ProfileEntry def;
        def.id = 1;
        def.name = L"\u9ed8\u8ba4\u914d\u7f6e";

        // Check if there's an existing config_path.txt pointer from old system
        auto pointerFile = profilesDir_ / L"config_path.txt";
        if (std::filesystem::exists(pointerFile)) {
            std::wifstream ifs(pointerFile);
            std::wstring customPath;
            if (std::getline(ifs, customPath) && !customPath.empty() && std::filesystem::exists(customPath)) {
                def.configPath = customPath;
            } else {
                def.configPath = profilesDir_ / L"config.json";
            }
        } else {
            def.configPath = profilesDir_ / L"config.json";
        }

        profiles_.push_back(def);
        activeId_ = 1;
        Save();
        return;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file) return;

    std::stringstream buf;
    buf << file.rdbuf();

    jsonlite::Value root;
    std::string error;
    if (!jsonlite::Parse(buf.str(), root, error)) return;

    auto* obj = root.AsObject();
    if (!obj) return;

    if (auto id = GetNum(*obj, "activeId")) {
        activeId_ = static_cast<std::uint64_t>(*id);
    }

    auto arrIt = obj->find("profiles");
    if (arrIt == obj->end()) return;
    auto* arr = arrIt->second.AsArray();
    if (!arr) return;

    for (auto& item : *arr) {
        auto* entry = item.AsObject();
        if (!entry) continue;

        ProfileEntry p;
        if (auto id = GetNum(*entry, "id")) p.id = static_cast<std::uint64_t>(*id);
        if (auto name = GetStr(*entry, "name")) p.name = ToWide(*name);
        if (auto path = GetStr(*entry, "configPath")) p.configPath = ToWide(*path);

        if (p.id > 0 && !p.name.empty()) {
            profiles_.push_back(std::move(p));
        }
    }

    if (profiles_.empty()) {
        ProfileEntry def;
        def.id = 1;
        def.name = L"\u9ed8\u8ba4\u914d\u7f6e";
        def.configPath = profilesDir_ / L"config.json";
        profiles_.push_back(def);
        activeId_ = 1;
        Save();
        return;
    }

    // Validate activeId
    if (!FindById(activeId_)) {
        activeId_ = profiles_.front().id;
    }
}

void ProfileManager::Save() const {
    jsonlite::Object root;
    root.emplace("activeId", static_cast<double>(activeId_));

    jsonlite::Array arr;
    for (auto& p : profiles_) {
        jsonlite::Object entry;
        entry.emplace("id", static_cast<double>(p.id));
        entry.emplace("name", ToUtf8(p.name));
        entry.emplace("configPath", ToUtf8(p.configPath.wstring()));
        arr.emplace_back(std::move(entry));
    }
    root.emplace("profiles", std::move(arr));

    std::string content = jsonlite::Stringify(root) + "\n";
    std::ofstream file(profilesFilePath(), std::ios::binary | std::ios::trunc);
    if (file) file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

const ProfileEntry* ProfileManager::ActiveProfile() const {
    return FindById(activeId_);
}

const ProfileEntry* ProfileManager::FindById(std::uint64_t id) const {
    for (auto& p : profiles_) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

ProfileEntry ProfileManager::AddProfile(const std::wstring& name) {
    ProfileEntry p;
    p.id = nextId();
    p.name = name;
    p.configPath = generateConfigPath(p.id);
    profiles_.push_back(p);
    activeId_ = p.id;
    Save();
    return p;
}

bool ProfileManager::RenameProfile(std::uint64_t id, const std::wstring& newName) {
    for (auto& p : profiles_) {
        if (p.id == id) {
            p.name = newName;
            Save();
            return true;
        }
    }
    return false;
}

bool ProfileManager::UpdateConfigPath(std::uint64_t id, const std::filesystem::path& newPath) {
    for (auto& p : profiles_) {
        if (p.id == id) {
            p.configPath = newPath;
            Save();
            return true;
        }
    }
    return false;
}

bool ProfileManager::DeleteProfile(std::uint64_t id) {
    if (profiles_.size() <= 1) return false;

    auto it = std::find_if(profiles_.begin(), profiles_.end(),
        [id](const ProfileEntry& p) { return p.id == id; });
    if (it == profiles_.end()) return false;

    // Remove the config file if it exists
    if (std::filesystem::exists(it->configPath)) {
        std::filesystem::remove(it->configPath);
    }

    profiles_.erase(it);

    // If we deleted the active profile, switch to first
    if (activeId_ == id) {
        activeId_ = profiles_.front().id;
    }
    Save();
    return true;
}

void ProfileManager::SetActive(std::uint64_t id) {
    if (FindById(id)) {
        activeId_ = id;
        Save();
    }
}

std::uint64_t ProfileManager::nextId() const {
    std::uint64_t maxId = 0;
    for (auto& p : profiles_) maxId = std::max(maxId, p.id);
    return maxId + 1;
}

std::filesystem::path ProfileManager::generateConfigPath(std::uint64_t id) const {
    std::wstring filename = L"profile_" + std::to_wstring(id) + L".json";
    return profilesDir_ / filename;
}
