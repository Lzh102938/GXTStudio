#include "InvertedIndex.h"
#include "GXTStudio.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <set>

void InvertedIndex::buildIndex(const std::vector<GXTTabl>& tables) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isBuilding = true;
    
    m_wordIndex.clear();
    m_exactIndex.clear();
    m_totalEntries = 0;
    
    for (size_t tableIdx = 0; tableIdx < tables.size(); ++tableIdx) {
        const auto& table = tables[tableIdx];
        for (size_t entryIdx = 0; entryIdx < table.entries.size(); ++entryIdx) {
            const auto& entry = table.entries[entryIdx];
            
            addToIndex(entry.key, static_cast<int>(tableIdx), static_cast<int>(entryIdx), 0);
            
            auto words = tokenize(entry.value);
            for (size_t wordPos = 0; wordPos < words.size(); ++wordPos) {
                addToIndex(words[wordPos], static_cast<int>(tableIdx), static_cast<int>(entryIdx), 
                          static_cast<int>(wordPos + 1));
            }
            
            m_exactIndex[entry.value].push_back(
                MatchLocation(static_cast<int>(tableIdx), static_cast<int>(entryIdx), 0));
            
            ++m_totalEntries;
            
            if (m_totalEntries > MAX_INDEX_SIZE) {
                m_isBuilding = false;
                m_isValid = true;
                return;
            }
        }
    }
    
    m_isBuilding = false;
    m_isValid = true;
}

void InvertedIndex::buildIndexForWHM(const std::vector<WHMEntry>& entries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isBuilding = true;
    
    m_wordIndex.clear();
    m_exactIndex.clear();
    m_totalEntries = 0;
    
    for (size_t entryIdx = 0; entryIdx < entries.size(); ++entryIdx) {
        const auto& entry = entries[entryIdx];
        
        char hexBuf[16];
        snprintf(hexBuf, sizeof(hexBuf), "0x%08X", entry.hash);
        std::string hashStr(hexBuf);
        
        addToIndex(hashStr, 0, static_cast<int>(entryIdx), 0);
        
        auto words = tokenize(entry.text);
        for (size_t wordPos = 0; wordPos < words.size(); ++wordPos) {
            addToIndex(words[wordPos], 0, static_cast<int>(entryIdx), static_cast<int>(wordPos + 1));
        }
        
        m_exactIndex[entry.text].push_back(MatchLocation(0, static_cast<int>(entryIdx), 0));
        
        ++m_totalEntries;
        
        if (m_totalEntries > MAX_INDEX_SIZE) {
            m_isBuilding = false;
            m_isValid = true;
            return;
        }
    }
    
    m_isBuilding = false;
    m_isValid = true;
}

void InvertedIndex::buildIndexForDAT(const std::vector<DATEntry>& entries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isBuilding = true;
    
    m_wordIndex.clear();
    m_exactIndex.clear();
    m_totalEntries = 0;
    
    for (size_t entryIdx = 0; entryIdx < entries.size(); ++entryIdx) {
        const auto& entry = entries[entryIdx];
        
        char hexBuf[16];
        snprintf(hexBuf, sizeof(hexBuf), "0x%08X", entry.hash);
        std::string hashStr(hexBuf);
        
        addToIndex(hashStr, 0, static_cast<int>(entryIdx), 0);
        
        auto words = tokenize(entry.text);
        for (size_t wordPos = 0; wordPos < words.size(); ++wordPos) {
            addToIndex(words[wordPos], 0, static_cast<int>(entryIdx), static_cast<int>(wordPos + 1));
        }
        
        m_exactIndex[entry.text].push_back(MatchLocation(0, static_cast<int>(entryIdx), 0));
        
        ++m_totalEntries;
        
        if (m_totalEntries > MAX_INDEX_SIZE) {
            m_isBuilding = false;
            m_isValid = true;
            return;
        }
    }
    
    m_isBuilding = false;
    m_isValid = true;
}

std::vector<InvertedIndex::MatchLocation> InvertedIndex::search(const QString& query, bool caseSensitive) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_isValid || query.isEmpty()) {
        return {};
    }
    
    std::string queryStr = query.toStdString();
    std::vector<MatchLocation> results;
    
    auto exactIt = m_exactIndex.find(queryStr);
    if (exactIt != m_exactIndex.end()) {
        results = exactIt->second;
        return results;
    }
    
    bool hasChinese = false;
    for (char c : queryStr) {
        if (static_cast<unsigned char>(c) > 127) {
            hasChinese = true;
            break;
        }
    }
    
    if (hasChinese || queryStr.length() <= 3) {
        return {};
    }
    
    auto words = tokenize(queryStr);
    if (words.empty()) {
        return {};
    }
    
    std::map<std::pair<int, int>, int> matchCount;
    
    for (const auto& word : words) {
        auto it = m_wordIndex.find(word);
        if (it != m_wordIndex.end()) {
            std::set<std::pair<int, int>> entriesForWord;
            for (const auto& loc : it->second) {
                entriesForWord.insert({loc.tableIndex, loc.entryIndex});
            }
            
            for (const auto& entry : entriesForWord) {
                matchCount[entry]++;
            }
        }
    }
    
    size_t requiredMatches = words.size();
    for (const auto& [entry, count] : matchCount) {
        if (count >= static_cast<int>(requiredMatches)) {
            results.push_back(MatchLocation(entry.first, entry.second, 0));
        }
    }
    
    std::sort(results.begin(), results.end(), [](const MatchLocation& a, const MatchLocation& b) {
        if (a.tableIndex != b.tableIndex) return a.tableIndex < b.tableIndex;
        if (a.entryIndex != b.entryIndex) return a.entryIndex < b.entryIndex;
        return a.position < b.position;
    });
    
    return results;
}

std::vector<InvertedIndex::MatchLocation> InvertedIndex::searchExact(const QString& query, bool caseSensitive) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_isValid || query.isEmpty()) {
        return {};
    }
    
    std::string queryStr = query.toStdString();
    auto it = m_exactIndex.find(queryStr);
    
    if (it != m_exactIndex.end()) {
        return it->second;
    }
    
    return {};
}

void InvertedIndex::addEntry(int tableIndex, int entryIndex, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    addToIndex(key, tableIndex, entryIndex, 0);
    
    auto words = tokenize(value);
    for (size_t wordPos = 0; wordPos < words.size(); ++wordPos) {
        addToIndex(words[wordPos], tableIndex, entryIndex, static_cast<int>(wordPos + 1));
    }
    
    m_exactIndex[value].push_back(MatchLocation(tableIndex, entryIndex, 0));
    
    ++m_totalEntries;
}

void InvertedIndex::removeEntry(int tableIndex, int entryIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    removeFromIndex(tableIndex, entryIndex);
    if (m_totalEntries > 0) {
        --m_totalEntries;
    }
}

void InvertedIndex::updateEntry(int tableIndex, int entryIndex, 
                                 const std::string& oldKey, const std::string& oldValue,
                                 const std::string& newKey, const std::string& newValue) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    removeFromIndex(tableIndex, entryIndex);
    
    addToIndex(newKey, tableIndex, entryIndex, 0);
    
    auto words = tokenize(newValue);
    for (size_t wordPos = 0; wordPos < words.size(); ++wordPos) {
        addToIndex(words[wordPos], tableIndex, entryIndex, static_cast<int>(wordPos + 1));
    }
    
    m_exactIndex[newValue].push_back(MatchLocation(tableIndex, entryIndex, 0));
}

std::vector<std::string> InvertedIndex::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    
    if (text.empty()) {
        return tokens;
    }
    
    std::string currentWord;
    currentWord.reserve(32);
    
    bool hasChinese = false;
    for (char c : text) {
        if (static_cast<unsigned char>(c) > 127) {
            hasChinese = true;
            break;
        }
    }
    
    if (hasChinese) {
        return tokenizeChinese(text);
    }
    
    for (char c : text) {
        if (std::isalnum(c) || c == '_') {
            currentWord += std::tolower(c);
        } else {
            if (currentWord.length() >= MIN_WORD_LENGTH) {
                tokens.push_back(currentWord);
            }
            currentWord.clear();
        }
    }
    
    if (currentWord.length() >= MIN_WORD_LENGTH) {
        tokens.push_back(currentWord);
    }
    
    return tokens;
}

std::vector<std::string> InvertedIndex::tokenizeChinese(const std::string& text) {
    std::vector<std::string> tokens;
    tokens.reserve(text.size() / 2);
    
    for (size_t i = 0; i < text.size(); ) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        
        if (c < 128) {
            std::string word;
            while (i < text.size() && static_cast<unsigned char>(text[i]) < 128) {
                if (std::isalnum(text[i]) || text[i] == '_') {
                    word += std::tolower(text[i]);
                } else {
                    if (word.length() >= MIN_WORD_LENGTH) {
                        tokens.push_back(word);
                    }
                    word.clear();
                }
                ++i;
            }
            if (word.length() >= MIN_WORD_LENGTH) {
                tokens.push_back(word);
            }
        } else {
            size_t charLen = 1;
            if ((c & 0xE0) == 0xC0) charLen = 2;
            else if ((c & 0xF0) == 0xE0) charLen = 3;
            else if ((c & 0xF8) == 0xF0) charLen = 4;
            
            if (i + charLen <= text.size()) {
                tokens.push_back(text.substr(i, charLen));
            }
            i += charLen;
        }
    }
    
    return tokens;
}

void InvertedIndex::addToIndex(const std::string& word, int tableIndex, int entryIndex, int position) {
    if (word.empty() || word.length() < MIN_WORD_LENGTH) {
        return;
    }
    
    m_wordIndex[word].push_back(MatchLocation(tableIndex, entryIndex, position));
}

void InvertedIndex::removeFromIndex(int tableIndex, int entryIndex) {
    for (auto it = m_wordIndex.begin(); it != m_wordIndex.end(); ) {
        auto& locations = it->second;
        locations.erase(
            std::remove_if(locations.begin(), locations.end(),
                [tableIndex, entryIndex](const MatchLocation& loc) {
                    return loc.tableIndex == tableIndex && loc.entryIndex == entryIndex;
                }),
            locations.end()
        );
        
        if (locations.empty()) {
            it = m_wordIndex.erase(it);
        } else {
            ++it;
        }
    }
    
    for (auto it = m_exactIndex.begin(); it != m_exactIndex.end(); ) {
        auto& locations = it->second;
        locations.erase(
            std::remove_if(locations.begin(), locations.end(),
                [tableIndex, entryIndex](const MatchLocation& loc) {
                    return loc.tableIndex == tableIndex && loc.entryIndex == entryIndex;
                }),
            locations.end()
        );
        
        if (locations.empty()) {
            it = m_exactIndex.erase(it);
        } else {
            ++it;
        }
    }
}
