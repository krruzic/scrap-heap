#include "data.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace ScrapHeap {

DataManager& DataManager::instance() {
    static DataManager manager;
    return manager;
}

bool DataManager::loadAll() {
    bool success = true;
    success &= loadTags();
    success &= loadStats();
    success &= loadGlobalStats();
    return success;
}

bool DataManager::saveAll() {
    bool success = true;
    success &= saveTags();
    success &= saveStats();
    success &= saveGlobalStats();
    return success;
}

bool DataManager::loadTags() {
    tags.clear();

    std::ifstream file(TAGS_FILE);
    if (!file.is_open()) {
        return true;  // File not existing is OK
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (!line.empty()) {
            tags.push_back(toUpperCase(line));
        }
    }

    return true;
}

bool DataManager::saveTags() {
    std::ofstream file(TAGS_FILE);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& tag : tags) {
        file << tag << "\n";
    }

    return true;
}

void DataManager::addTag(const std::string& tag) {
    std::string upperTag = toUpperCase(trim(tag));
    if (upperTag.empty()) return;
    if (upperTag.length() > MAX_TAG_LENGTH) {
        upperTag = upperTag.substr(0, MAX_TAG_LENGTH);
    }
    tags.push_back(upperTag);
    saveTags();
}

void DataManager::removeTag(int index) {
    if (index < 0 || index >= static_cast<int>(tags.size())) return;
    tags.erase(tags.begin() + index);
    saveTags();
}

bool DataManager::tagExists(const std::string& tag) const {
    std::string upperTag = toUpperCase(tag);
    for (const auto& t : tags) {
        if (t == upperTag) return true;
    }
    return false;
}

bool DataManager::loadStats() {
    stats.clear();

    std::ifstream file(STATS_FILE);
    if (!file.is_open()) {
        return true;  // File not existing is OK
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string tag;
        int wins, losses;

        // Read tag (may contain spaces, so read until first number)
        std::string word;
        std::vector<std::string> words;
        while (iss >> word) {
            words.push_back(word);
        }

        if (words.size() >= 3) {
            // Last two are wins and losses
            try {
                losses = std::stoi(words.back());
                words.pop_back();
                wins = std::stoi(words.back());
                words.pop_back();

                // Rest is the tag
                tag.clear();
                for (size_t i = 0; i < words.size(); ++i) {
                    if (i > 0) tag += " ";
                    tag += words[i];
                }

                PlayerStats ps;
                ps.tag = tag;
                ps.wins = wins;
                ps.losses = losses;
                stats.push_back(ps);
            } catch (...) {
                // Skip malformed lines
            }
        }
    }

    // Sort by win ratio
    std::sort(stats.begin(), stats.end(),
              [](const PlayerStats& a, const PlayerStats& b) {
                  return a.getRatio() > b.getRatio();
              });

    return true;
}

bool DataManager::saveStats() {
    std::ofstream file(STATS_FILE);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& ps : stats) {
        file << ps.tag << " " << ps.wins << " " << ps.losses << "\n";
    }

    return true;
}

void DataManager::recordWin(const std::string& tag) {
    PlayerStats* ps = getStatsForTag(tag);
    if (ps) {
        ps->wins++;
    } else {
        PlayerStats newStats;
        newStats.tag = tag;
        newStats.wins = 1;
        newStats.losses = 0;
        stats.push_back(newStats);
    }
    saveStats();
}

void DataManager::recordLoss(const std::string& tag) {
    PlayerStats* ps = getStatsForTag(tag);
    if (ps) {
        ps->losses++;
    } else {
        PlayerStats newStats;
        newStats.tag = tag;
        newStats.wins = 0;
        newStats.losses = 1;
        stats.push_back(newStats);
    }
    saveStats();
}

PlayerStats* DataManager::getStatsForTag(const std::string& tag) {
    for (auto& ps : stats) {
        if (ps.tag == tag) return &ps;
    }
    return nullptr;
}

bool DataManager::loadGlobalStats() {
    std::ifstream file(GLOBAL_STATS_FILE);
    if (!file.is_open()) {
        return true;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "total_matches") {
            iss >> globalStats.totalMatches;
        } else if (key == "total_rounds") {
            iss >> globalStats.totalRounds;
        } else if (key == "most_used_frame") {
            std::getline(iss, globalStats.mostUsedFrame);
            globalStats.mostUsedFrame = trim(globalStats.mostUsedFrame);
        } else if (key == "most_used_weapon") {
            std::getline(iss, globalStats.mostUsedWeapon);
            globalStats.mostUsedWeapon = trim(globalStats.mostUsedWeapon);
        } else if (key == "frame_usage") {
            std::string name;
            int count;
            iss >> name >> count;
            frameUsage[name] = count;
        } else if (key == "weapon_usage") {
            std::string name;
            int count;
            iss >> name >> count;
            weaponUsage[name] = count;
        }
    }

    return true;
}

bool DataManager::saveGlobalStats() {
    std::ofstream file(GLOBAL_STATS_FILE);
    if (!file.is_open()) {
        return false;
    }

    file << "total_matches " << globalStats.totalMatches << "\n";
    file << "total_rounds " << globalStats.totalRounds << "\n";
    file << "most_used_frame " << globalStats.mostUsedFrame << "\n";
    file << "most_used_weapon " << globalStats.mostUsedWeapon << "\n";

    for (const auto& [name, count] : frameUsage) {
        file << "frame_usage " << name << " " << count << "\n";
    }
    for (const auto& [name, count] : weaponUsage) {
        file << "weapon_usage " << name << " " << count << "\n";
    }

    return true;
}

void DataManager::incrementMatchCount() {
    globalStats.totalMatches++;
    saveGlobalStats();
}

void DataManager::recordComponentUsage(const std::string& frame, const std::string& weapon) {
    frameUsage[frame]++;
    weaponUsage[weapon]++;

    // Update most used
    int maxFrameCount = 0;
    int maxWeaponCount = 0;

    for (const auto& [name, count] : frameUsage) {
        if (count > maxFrameCount) {
            maxFrameCount = count;
            globalStats.mostUsedFrame = name;
        }
    }

    for (const auto& [name, count] : weaponUsage) {
        if (count > maxWeaponCount) {
            maxWeaponCount = count;
            globalStats.mostUsedWeapon = name;
        }
    }

    saveGlobalStats();
}

} // namespace ScrapHeap
