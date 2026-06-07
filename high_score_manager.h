#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>

struct ScoreEntry {
    std::string name;
    int score;
    int level;
};

class HighScoreManager {
public:
    HighScoreManager(const std::string& custom_filename = "") {
        if (custom_filename.empty() || custom_filename == "high.score") {
            const char* home = std::getenv("HOME");
            std::string dir = home ? std::string(home) + "/.gcg" : ".gcg";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            filename = dir + "/high.score";
        } else {
            filename = custom_filename;
        }
        load();
    }

    void load() {
        entries.clear();
        std::ifstream f(filename);
        if (!f.is_open()) {
            for(int i=0; i<10; ++i) entries.push_back({"AAA", 1000 - i*100, 1});
            return;
        }

        std::string line;
        while (std::getline(f, line)) {
            std::stringstream ss(line);
            std::string name, s_score, s_level;
            if (std::getline(ss, name, ',') && 
                std::getline(ss, s_score, ',') && 
                std::getline(ss, s_level, ',')) {
                try {
                    entries.push_back({name, std::stoi(s_score), std::stoi(s_level)});
                } catch (...) {}
            }
        }
        sortAndLimit();
    }

    void save() {
        std::ofstream f(filename);
        if (!f.is_open()) {
            for(int i=0; i<10; ++i) entries.push_back({"AAA", 1000 - i*100, 1});
            return;
        }
        for (const auto& e : entries) {
            f << e.name << "," << e.score << "," << e.level << "\n";
        }
    }

    bool isHigher(int score) {
        if (entries.size() < 10) return true;
        return score > entries.back().score;
    }

    void addScore(const std::string& name, int score, int level) {
        entries.push_back({name, score, level});
        sortAndLimit();
        save();
    }

    const std::vector<ScoreEntry>& getEntries() const { return entries; }

private:
    std::string filename;
    std::vector<ScoreEntry> entries;

    void sortAndLimit() {
        std::sort(entries.begin(), entries.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
            return a.score > b.score;
        });
        if (entries.size() > 10) {
            entries.resize(10);
        }
    }
};
