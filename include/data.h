#pragma once

#include <string>
#include <vector>
#include <map>

namespace ScrapHeap {

// Player stats record
struct PlayerStats {
    std::string tag;
    int wins = 0;
    int losses = 0;

    float getRatio() const {
        if (wins + losses == 0) return 0.0f;
        return static_cast<float>(wins) / static_cast<float>(wins + losses);
    }
};

// Global game stats
struct GlobalStats {
    int totalMatches = 0;
    int totalRounds = 0;
    std::string mostUsedFrame;
    std::string mostUsedWeapon;
    // Can add more global stats here
};

// Data manager for persistence
class DataManager {
public:
    static DataManager& instance();

    // Load all data from files
    bool loadAll();

    // Save all data to files
    bool saveAll();

    // Tags management
    const std::vector<std::string>& getTags() const { return tags; }
    void addTag(const std::string& tag);
    void removeTag(int index);
    bool tagExists(const std::string& tag) const;
    bool loadTags();
    bool saveTags();

    // Stats management
    const std::vector<PlayerStats>& getStats() const { return stats; }
    void recordWin(const std::string& tag);
    void recordLoss(const std::string& tag);
    PlayerStats* getStatsForTag(const std::string& tag);
    bool loadStats();
    bool saveStats();

    // Global stats
    const GlobalStats& getGlobalStats() const { return globalStats; }
    void incrementMatchCount();
    void recordComponentUsage(const std::string& frame, const std::string& weapon);
    bool loadGlobalStats();
    bool saveGlobalStats();

private:
    DataManager() = default;

    std::vector<std::string> tags;
    std::vector<PlayerStats> stats;
    GlobalStats globalStats;

    // Component usage tracking
    std::map<std::string, int> frameUsage;
    std::map<std::string, int> weaponUsage;

    static constexpr const char* TAGS_FILE = "tags.txt";
    static constexpr const char* STATS_FILE = "stats.txt";
    static constexpr const char* GLOBAL_STATS_FILE = "global_stats.txt";
};

} // namespace ScrapHeap
