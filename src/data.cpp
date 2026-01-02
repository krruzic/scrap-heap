#include "data.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace ScrapHeap {

DataManager& DataManager::instance() {
    static DataManager manager;
    return manager;
}

// Binary I/O helpers
void DataManager::writeString(std::ostream& out, const std::string& str) {
    uint16_t len = static_cast<uint16_t>(str.length());
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) {
        out.write(str.data(), len);
    }
}

std::string DataManager::readString(std::istream& in) {
    uint16_t len = 0;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (len == 0 || !in.good()) return "";

    std::string str(len, '\0');
    in.read(&str[0], len);
    return str;
}

void DataManager::writePlayerStats(std::ostream& out, const PlayerStats& ps) {
    writeString(out, ps.tag);

    // Win/loss tracking
    out.write(reinterpret_cast<const char*>(&ps.wins), sizeof(ps.wins));
    out.write(reinterpret_cast<const char*>(&ps.losses), sizeof(ps.losses));
    out.write(reinterpret_cast<const char*>(&ps.draws), sizeof(ps.draws));

    // Combat stats
    out.write(reinterpret_cast<const char*>(&ps.totalKills), sizeof(ps.totalKills));
    out.write(reinterpret_cast<const char*>(&ps.totalDeaths), sizeof(ps.totalDeaths));
    out.write(reinterpret_cast<const char*>(&ps.totalDamageDealt), sizeof(ps.totalDamageDealt));
    out.write(reinterpret_cast<const char*>(&ps.totalDamageTaken), sizeof(ps.totalDamageTaken));

    // Match stats
    out.write(reinterpret_cast<const char*>(&ps.totalMatchesPlayed), sizeof(ps.totalMatchesPlayed));
    out.write(reinterpret_cast<const char*>(&ps.totalMatchTime), sizeof(ps.totalMatchTime));
    out.write(reinterpret_cast<const char*>(&ps.longestWinStreak), sizeof(ps.longestWinStreak));
    out.write(reinterpret_cast<const char*>(&ps.currentWinStreak), sizeof(ps.currentWinStreak));

    // Favorite components
    writeString(out, ps.favoriteFrame);
    writeString(out, ps.favoriteEngine);
    writeString(out, ps.favoriteWeapon);
    writeString(out, ps.favoriteSpecial);
    out.write(reinterpret_cast<const char*>(&ps.favoriteFrameCount), sizeof(ps.favoriteFrameCount));
    out.write(reinterpret_cast<const char*>(&ps.favoriteEngineCount), sizeof(ps.favoriteEngineCount));
    out.write(reinterpret_cast<const char*>(&ps.favoriteWeaponCount), sizeof(ps.favoriteWeaponCount));
    out.write(reinterpret_cast<const char*>(&ps.favoriteSpecialCount), sizeof(ps.favoriteSpecialCount));

    // Component usage maps
    auto writeUsageMap = [this, &out](const std::map<std::string, int>& usageMap) {
        uint16_t count = static_cast<uint16_t>(usageMap.size());
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& [name, usage] : usageMap) {
            writeString(out, name);
            out.write(reinterpret_cast<const char*>(&usage), sizeof(usage));
        }
    };

    writeUsageMap(ps.frameUsage);
    writeUsageMap(ps.engineUsage);
    writeUsageMap(ps.weaponUsage);
    writeUsageMap(ps.specialUsage);
}

PlayerStats DataManager::readPlayerStats(std::istream& in) {
    PlayerStats ps;

    ps.tag = readString(in);

    // Win/loss tracking
    in.read(reinterpret_cast<char*>(&ps.wins), sizeof(ps.wins));
    in.read(reinterpret_cast<char*>(&ps.losses), sizeof(ps.losses));
    in.read(reinterpret_cast<char*>(&ps.draws), sizeof(ps.draws));

    // Combat stats
    in.read(reinterpret_cast<char*>(&ps.totalKills), sizeof(ps.totalKills));
    in.read(reinterpret_cast<char*>(&ps.totalDeaths), sizeof(ps.totalDeaths));
    in.read(reinterpret_cast<char*>(&ps.totalDamageDealt), sizeof(ps.totalDamageDealt));
    in.read(reinterpret_cast<char*>(&ps.totalDamageTaken), sizeof(ps.totalDamageTaken));

    // Match stats
    in.read(reinterpret_cast<char*>(&ps.totalMatchesPlayed), sizeof(ps.totalMatchesPlayed));
    in.read(reinterpret_cast<char*>(&ps.totalMatchTime), sizeof(ps.totalMatchTime));
    in.read(reinterpret_cast<char*>(&ps.longestWinStreak), sizeof(ps.longestWinStreak));
    in.read(reinterpret_cast<char*>(&ps.currentWinStreak), sizeof(ps.currentWinStreak));

    // Favorite components
    ps.favoriteFrame = readString(in);
    ps.favoriteEngine = readString(in);
    ps.favoriteWeapon = readString(in);
    ps.favoriteSpecial = readString(in);
    in.read(reinterpret_cast<char*>(&ps.favoriteFrameCount), sizeof(ps.favoriteFrameCount));
    in.read(reinterpret_cast<char*>(&ps.favoriteEngineCount), sizeof(ps.favoriteEngineCount));
    in.read(reinterpret_cast<char*>(&ps.favoriteWeaponCount), sizeof(ps.favoriteWeaponCount));
    in.read(reinterpret_cast<char*>(&ps.favoriteSpecialCount), sizeof(ps.favoriteSpecialCount));

    // Component usage maps
    auto readUsageMap = [this, &in](std::map<std::string, int>& usageMap) {
        uint16_t count = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));
        for (uint16_t i = 0; i < count && in.good(); ++i) {
            std::string name = readString(in);
            int usage = 0;
            in.read(reinterpret_cast<char*>(&usage), sizeof(usage));
            usageMap[name] = usage;
        }
    };

    readUsageMap(ps.frameUsage);
    readUsageMap(ps.engineUsage);
    readUsageMap(ps.weaponUsage);
    readUsageMap(ps.specialUsage);

    return ps;
}

bool DataManager::load() {
    std::ifstream file(SAVE_FILE, std::ios::binary);
    if (!file.is_open()) {
        // Try loading legacy format
        return loadLegacy();
    }

    // Read and verify header
    uint32_t magic = 0;
    uint16_t version = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (magic != SAVE_MAGIC) {
        file.close();
        return loadLegacy();
    }

    if (version > SAVE_VERSION) {
        // Future version, try to load anyway
    }

    // Read global stats
    file.read(reinterpret_cast<char*>(&globalStats.totalMatches), sizeof(globalStats.totalMatches));
    file.read(reinterpret_cast<char*>(&globalStats.totalRounds), sizeof(globalStats.totalRounds));
    file.read(reinterpret_cast<char*>(&globalStats.totalPlayTime), sizeof(globalStats.totalPlayTime));
    globalStats.mostUsedFrame = readString(file);
    globalStats.mostUsedEngine = readString(file);
    globalStats.mostUsedWeapon = readString(file);
    globalStats.mostUsedSpecial = readString(file);

    // Read global usage maps
    auto readGlobalUsageMap = [this, &file](std::map<std::string, int>& usageMap) {
        uint16_t count = 0;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        for (uint16_t i = 0; i < count && file.good(); ++i) {
            std::string name = readString(file);
            int usage = 0;
            file.read(reinterpret_cast<char*>(&usage), sizeof(usage));
            usageMap[name] = usage;
        }
    };

    readGlobalUsageMap(globalFrameUsage);
    readGlobalUsageMap(globalEngineUsage);
    readGlobalUsageMap(globalWeaponUsage);
    readGlobalUsageMap(globalSpecialUsage);

    // Read tags
    tags.clear();
    uint16_t tagCount = 0;
    file.read(reinterpret_cast<char*>(&tagCount), sizeof(tagCount));
    for (uint16_t i = 0; i < tagCount && file.good(); ++i) {
        tags.push_back(readString(file));
    }

    // Read player stats
    stats.clear();
    uint16_t statsCount = 0;
    file.read(reinterpret_cast<char*>(&statsCount), sizeof(statsCount));
    for (uint16_t i = 0; i < statsCount && file.good(); ++i) {
        stats.push_back(readPlayerStats(file));
    }

    return file.good();
}

bool DataManager::save() {
    std::ofstream file(SAVE_FILE, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Write header
    file.write(reinterpret_cast<const char*>(&SAVE_MAGIC), sizeof(SAVE_MAGIC));
    file.write(reinterpret_cast<const char*>(&SAVE_VERSION), sizeof(SAVE_VERSION));

    // Write global stats
    file.write(reinterpret_cast<const char*>(&globalStats.totalMatches), sizeof(globalStats.totalMatches));
    file.write(reinterpret_cast<const char*>(&globalStats.totalRounds), sizeof(globalStats.totalRounds));
    file.write(reinterpret_cast<const char*>(&globalStats.totalPlayTime), sizeof(globalStats.totalPlayTime));
    writeString(file, globalStats.mostUsedFrame);
    writeString(file, globalStats.mostUsedEngine);
    writeString(file, globalStats.mostUsedWeapon);
    writeString(file, globalStats.mostUsedSpecial);

    // Write global usage maps
    auto writeGlobalUsageMap = [this, &file](const std::map<std::string, int>& usageMap) {
        uint16_t count = static_cast<uint16_t>(usageMap.size());
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& [name, usage] : usageMap) {
            writeString(file, name);
            file.write(reinterpret_cast<const char*>(&usage), sizeof(usage));
        }
    };

    writeGlobalUsageMap(globalFrameUsage);
    writeGlobalUsageMap(globalEngineUsage);
    writeGlobalUsageMap(globalWeaponUsage);
    writeGlobalUsageMap(globalSpecialUsage);

    // Write tags
    uint16_t tagCount = static_cast<uint16_t>(tags.size());
    file.write(reinterpret_cast<const char*>(&tagCount), sizeof(tagCount));
    for (const auto& tag : tags) {
        writeString(file, tag);
    }

    // Write player stats
    uint16_t statsCount = static_cast<uint16_t>(stats.size());
    file.write(reinterpret_cast<const char*>(&statsCount), sizeof(statsCount));
    for (const auto& ps : stats) {
        writePlayerStats(file, ps);
    }

    return file.good();
}

bool DataManager::loadLegacy() {
    bool loaded = false;

    // Try to load legacy tags
    std::ifstream tagsFile(LEGACY_TAGS_FILE);
    if (tagsFile.is_open()) {
        tags.clear();
        std::string line;
        while (std::getline(tagsFile, line)) {
            line = trim(line);
            if (!line.empty()) {
                tags.push_back(toUpperCase(line));
            }
        }
        loaded = true;
    }

    // Try to load legacy stats
    std::ifstream statsFile(LEGACY_STATS_FILE);
    if (statsFile.is_open()) {
        stats.clear();
        std::string line;
        while (std::getline(statsFile, line)) {
            std::istringstream iss(line);
            std::vector<std::string> words;
            std::string word;
            while (iss >> word) {
                words.push_back(word);
            }

            if (words.size() >= 3) {
                try {
                    int losses = std::stoi(words.back());
                    words.pop_back();
                    int wins = std::stoi(words.back());
                    words.pop_back();

                    std::string tag;
                    for (size_t i = 0; i < words.size(); ++i) {
                        if (i > 0) tag += " ";
                        tag += words[i];
                    }

                    PlayerStats ps;
                    ps.tag = tag;
                    ps.wins = wins;
                    ps.losses = losses;
                    ps.totalMatchesPlayed = wins + losses;
                    stats.push_back(ps);
                } catch (...) {
                    // Skip malformed lines
                }
            }
        }
        loaded = true;
    }

    // Try to load legacy global stats
    std::ifstream globalFile(LEGACY_GLOBAL_FILE);
    if (globalFile.is_open()) {
        std::string line;
        while (std::getline(globalFile, line)) {
            std::istringstream iss(line);
            std::string key;
            iss >> key;

            if (key == "total_matches") {
                iss >> globalStats.totalMatches;
            } else if (key == "total_rounds") {
                iss >> globalStats.totalRounds;
            }
        }
        loaded = true;
    }

    // Save in new format if we loaded any legacy data
    if (loaded) {
        save();
    }

    return loaded;
}

void DataManager::addTag(const std::string& tag) {
    std::string upperTag = toUpperCase(trim(tag));
    if (upperTag.empty()) return;
    if (upperTag.length() > MAX_TAG_LENGTH) {
        upperTag = upperTag.substr(0, MAX_TAG_LENGTH);
    }
    tags.push_back(upperTag);
    save();
}

void DataManager::removeTag(int index) {
    if (index < 0 || index >= static_cast<int>(tags.size())) return;
    tags.erase(tags.begin() + index);
    save();
}

bool DataManager::tagExists(const std::string& tag) const {
    std::string upperTag = toUpperCase(tag);
    for (const auto& t : tags) {
        if (t == upperTag) return true;
    }
    return false;
}

PlayerStats* DataManager::getStatsForTag(const std::string& tag) {
    for (auto& ps : stats) {
        if (ps.tag == tag) return &ps;
    }
    return nullptr;
}

PlayerStats& DataManager::getOrCreateStats(const std::string& tag) {
    PlayerStats* existing = getStatsForTag(tag);
    if (existing) return *existing;

    PlayerStats newStats;
    newStats.tag = tag;
    stats.push_back(newStats);
    return stats.back();
}

void DataManager::recordMatchResult(const std::string& tag, bool won, bool draw,
                                   int kills, int deaths, float damageDealt,
                                   float damageTaken, float matchTime) {
    PlayerStats& ps = getOrCreateStats(tag);

    if (draw) {
        ps.draws++;
        ps.currentWinStreak = 0;
    } else if (won) {
        ps.wins++;
        ps.currentWinStreak++;
        if (ps.currentWinStreak > ps.longestWinStreak) {
            ps.longestWinStreak = ps.currentWinStreak;
        }
    } else {
        ps.losses++;
        ps.currentWinStreak = 0;
    }

    ps.totalKills += kills;
    ps.totalDeaths += deaths;
    ps.totalDamageDealt += damageDealt;
    ps.totalDamageTaken += damageTaken;
    ps.totalMatchesPlayed++;
    ps.totalMatchTime += matchTime;

    // Sort stats by win ratio
    std::sort(stats.begin(), stats.end(),
              [](const PlayerStats& a, const PlayerStats& b) {
                  return a.getWinRatio() > b.getWinRatio();
              });

    save();
}

void DataManager::recordComponentUsage(const std::string& tag,
                                       const std::string& frame, const std::string& engine,
                                       const std::string& weapon, const std::string& special) {
    PlayerStats& ps = getOrCreateStats(tag);

    // Update player usage counts
    ps.frameUsage[frame]++;
    ps.engineUsage[engine]++;
    ps.weaponUsage[weapon]++;
    ps.specialUsage[special]++;

    // Update favorites
    if (ps.frameUsage[frame] > ps.favoriteFrameCount) {
        ps.favoriteFrame = frame;
        ps.favoriteFrameCount = ps.frameUsage[frame];
    }
    if (ps.engineUsage[engine] > ps.favoriteEngineCount) {
        ps.favoriteEngine = engine;
        ps.favoriteEngineCount = ps.engineUsage[engine];
    }
    if (ps.weaponUsage[weapon] > ps.favoriteWeaponCount) {
        ps.favoriteWeapon = weapon;
        ps.favoriteWeaponCount = ps.weaponUsage[weapon];
    }
    if (ps.specialUsage[special] > ps.favoriteSpecialCount) {
        ps.favoriteSpecial = special;
        ps.favoriteSpecialCount = ps.specialUsage[special];
    }

    // Update global usage
    globalFrameUsage[frame]++;
    globalEngineUsage[engine]++;
    globalWeaponUsage[weapon]++;
    globalSpecialUsage[special]++;

    updateMostUsed();
    save();
}

void DataManager::updateMostUsed() {
    int maxCount = 0;
    for (const auto& [name, count] : globalFrameUsage) {
        if (count > maxCount) {
            maxCount = count;
            globalStats.mostUsedFrame = name;
        }
    }

    maxCount = 0;
    for (const auto& [name, count] : globalEngineUsage) {
        if (count > maxCount) {
            maxCount = count;
            globalStats.mostUsedEngine = name;
        }
    }

    maxCount = 0;
    for (const auto& [name, count] : globalWeaponUsage) {
        if (count > maxCount) {
            maxCount = count;
            globalStats.mostUsedWeapon = name;
        }
    }

    maxCount = 0;
    for (const auto& [name, count] : globalSpecialUsage) {
        if (count > maxCount) {
            maxCount = count;
            globalStats.mostUsedSpecial = name;
        }
    }
}

void DataManager::incrementMatchCount() {
    globalStats.totalMatches++;
    save();
}

void DataManager::addPlayTime(float seconds) {
    globalStats.totalPlayTime += seconds;
    save();
}

} // namespace ScrapHeap
