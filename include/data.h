#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace ScrapHeap {

// Save file magic number and version
constexpr uint32_t SAVE_MAGIC = 0x53485356;  // "SHSV" - Scrap Heap Save
constexpr uint16_t SAVE_VERSION = 1;

// Extended player stats record
struct PlayerStats {
    std::string tag;

    // Win/Loss tracking
    int wins = 0;
    int losses = 0;
    int draws = 0;

    // Combat stats
    int totalKills = 0;
    int totalDeaths = 0;
    float totalDamageDealt = 0.0f;
    float totalDamageTaken = 0.0f;

    // Match stats
    int totalMatchesPlayed = 0;
    float totalMatchTime = 0.0f;  // In seconds
    int longestWinStreak = 0;
    int currentWinStreak = 0;

    // Favorite components (by usage count)
    std::string favoriteFrame;
    std::string favoriteEngine;
    std::string favoriteWeapon;
    std::string favoriteSpecial;
    int favoriteFrameCount = 0;
    int favoriteEngineCount = 0;
    int favoriteWeaponCount = 0;
    int favoriteSpecialCount = 0;

    // Component usage maps (stored separately, not in binary per-player)
    std::map<std::string, int> frameUsage;
    std::map<std::string, int> engineUsage;
    std::map<std::string, int> weaponUsage;
    std::map<std::string, int> specialUsage;

    // Computed stats
    float getWinRatio() const {
        int total = wins + losses + draws;
        if (total == 0) return 0.0f;
        return static_cast<float>(wins) / static_cast<float>(total);
    }

    float getKDRatio() const {
        if (totalDeaths == 0) return static_cast<float>(totalKills);
        return static_cast<float>(totalKills) / static_cast<float>(totalDeaths);
    }

    float getAverageDamagePerMatch() const {
        if (totalMatchesPlayed == 0) return 0.0f;
        return totalDamageDealt / static_cast<float>(totalMatchesPlayed);
    }
};

// Global game stats
struct GlobalStats {
    int totalMatches = 0;
    int totalRounds = 0;
    float totalPlayTime = 0.0f;
    std::string mostUsedFrame;
    std::string mostUsedEngine;
    std::string mostUsedWeapon;
    std::string mostUsedSpecial;
};

// Data manager for persistence
class DataManager {
public:
    static DataManager& instance();

    // Load/save all data from single binary file
    bool load();
    bool save();

    // Legacy load (for migration from old format)
    bool loadLegacy();

    // Tags management
    const std::vector<std::string>& getTags() const { return tags; }
    void addTag(const std::string& tag);
    void removeTag(int index);
    bool tagExists(const std::string& tag) const;

    // Stats management
    const std::vector<PlayerStats>& getStats() const { return stats; }
    PlayerStats* getStatsForTag(const std::string& tag);
    PlayerStats& getOrCreateStats(const std::string& tag);

    // Record match results
    void recordMatchResult(const std::string& tag, bool won, bool draw,
                          int kills, int deaths, float damageDealt,
                          float damageTaken, float matchTime);
    void recordComponentUsage(const std::string& tag,
                             const std::string& frame, const std::string& engine,
                             const std::string& weapon, const std::string& special);

    // Global stats
    const GlobalStats& getGlobalStats() const { return globalStats; }
    void incrementMatchCount();
    void addPlayTime(float seconds);

private:
    DataManager() = default;

    // Binary I/O helpers
    void writeString(std::ostream& out, const std::string& str);
    std::string readString(std::istream& in);
    void writePlayerStats(std::ostream& out, const PlayerStats& ps);
    PlayerStats readPlayerStats(std::istream& in);

    std::vector<std::string> tags;
    std::vector<PlayerStats> stats;
    GlobalStats globalStats;

    // Global component usage tracking
    std::map<std::string, int> globalFrameUsage;
    std::map<std::string, int> globalEngineUsage;
    std::map<std::string, int> globalWeaponUsage;
    std::map<std::string, int> globalSpecialUsage;

    void updateMostUsed();

    static constexpr const char* SAVE_FILE = "scrapheap.sav";

    // Legacy files (for migration)
    static constexpr const char* LEGACY_TAGS_FILE = "tags.txt";
    static constexpr const char* LEGACY_STATS_FILE = "stats.txt";
    static constexpr const char* LEGACY_GLOBAL_FILE = "global_stats.txt";
};

} // namespace ScrapHeap
