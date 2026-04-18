#pragma once

#include <QString>
#include <QStringList>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

struct GXTTabl;
struct GXTEntry;
struct WHMEntry;
struct DATEntry;

class InvertedIndex {
public:
    struct MatchLocation {
        int tableIndex;
        int entryIndex;
        int position;
        
        MatchLocation(int t, int e, int p) : tableIndex(t), entryIndex(e), position(p) {}
    };

private:
    std::unordered_map<std::string, std::vector<MatchLocation>> m_wordIndex;
    std::unordered_map<std::string, std::vector<MatchLocation>> m_exactIndex;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_isBuilding{false};
    std::atomic<bool> m_isValid{false};
    size_t m_totalEntries{0};
    
    static constexpr size_t MAX_INDEX_SIZE = 1000000;
    static constexpr int MIN_WORD_LENGTH = 2;

public:
    static InvertedIndex& instance() {
        static InvertedIndex index;
        return index;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_wordIndex.clear();
        m_exactIndex.clear();
        m_totalEntries = 0;
        m_isValid = false;
    }
    
    bool isValid() const { return m_isValid.load(); }
    size_t totalEntries() const { return m_totalEntries; }
    size_t indexSize() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_wordIndex.size() + m_exactIndex.size();
    }
    
    void buildIndex(const std::vector<GXTTabl>& tables);
    void buildIndexForWHM(const std::vector<WHMEntry>& entries);
    void buildIndexForDAT(const std::vector<DATEntry>& entries);
    
    std::vector<MatchLocation> search(const QString& query, bool caseSensitive = false);
    std::vector<MatchLocation> searchExact(const QString& query, bool caseSensitive = false);
    
    void addEntry(int tableIndex, int entryIndex, const std::string& key, const std::string& value);
    void removeEntry(int tableIndex, int entryIndex);
    void updateEntry(int tableIndex, int entryIndex, const std::string& oldKey, const std::string& oldValue, 
                     const std::string& newKey, const std::string& newValue);

private:
    InvertedIndex() = default;
    InvertedIndex(const InvertedIndex&) = delete;
    InvertedIndex& operator=(const InvertedIndex&) = delete;
    
    std::vector<std::string> tokenize(const std::string& text);
    std::vector<std::string> tokenizeChinese(const std::string& text);
    void addToIndex(const std::string& word, int tableIndex, int entryIndex, int position);
    void removeFromIndex(int tableIndex, int entryIndex);
};
