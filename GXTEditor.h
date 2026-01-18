#ifndef GXT_EDITOR_H
#define GXT_EDITOR_H

#include "GXTParser.h"
#include <string>
#include <vector>
#include <map>
#include <cctype>

// CKeyGen类 - 用于计算SA文件的hash
class CKeyGen {
private:
    // Precalculated table of 256 CRC32 hash keys computed according to the polynomial 0xEDB88320.
    static const unsigned int keyTable[256];

public:
    // Hash a string till the specified number of characters.
    static unsigned int GetKey(const char *str, int len);

    // Hash a string till a null-terminator is found.
    static unsigned int GetKey(const char *str);

    // Hash a string till a null-terminator is found by converting lowercase characters to uppercase.
    static unsigned int GetUppercaseKey(const char *str);

    // Append a string to the hash key of a previously hashed string.
    static unsigned int AppendStringToKey(unsigned int hash, const char *str);
};

struct GXTTableEntry {
    std::string key;
    std::string value;
};

struct GXTTable {
    std::string name;
    std::vector<GXTTableEntry> entries;
};

class GXTEditor {
private:
    std::vector<GXTTable> tables;
    std::string filePath;
    GXTVersion version;

public:
    // 构造函数
    GXTEditor();
    GXTEditor(const std::string& filePath);
    
    // 加载和保存文件
    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path = "");
    
    // 表操作
    size_t getTableCount() const;
    const GXTTable& getTable(size_t index) const;
    bool addTable(const std::string& tableName);
    bool removeTable(size_t index);
    bool renameTable(size_t index, const std::string& newName);
    
    // 条目操作
    size_t getEntryCount(size_t tableIndex) const;
    const GXTTableEntry& getEntry(size_t tableIndex, size_t entryIndex) const;
    bool addEntry(size_t tableIndex, const std::string& key, const std::string& value);
    bool removeEntry(size_t tableIndex, size_t entryIndex);
    bool updateEntry(size_t tableIndex, size_t entryIndex, const std::string& key, const std::string& value);
    bool updateEntryByKey(size_t tableIndex, const std::string& key, const std::string& value);
    
    // 查找功能
    int findTable(const std::string& tableName) const;
    int findEntry(size_t tableIndex, const std::string& key) const;
    
    // 获取版本信息
    GXTVersion getVersion() const { return version; }
    std::string getVersionString() const;
    
    // 设置版本信息
    void setVersion(GXTVersion ver) { version = ver; }
    
    // 保存为文本格式
    bool saveAsText(const std::string& path);                   // 文本格式
    
    // 工具函数
    bool isValidKey(const std::string& key) const;
    void clear();

    // 数据转换
    void convertData();

    // 保存方法
    bool saveAsGXT2Format(const std::string& path);             // GXT2格式
    bool saveAsGTA_III(const std::string& path);
    bool saveAsGTA_VC(const std::string& path);
    bool saveAsGTA_SA(const std::string& path);
    bool saveAsGTA_IV(const std::string& path);

private:
    // 辅助计算方法
    uint32_t calculateTDATSize(const GXTTable& table, int bitsPerChar) const;
    static uint32_t calculateCRC32(const std::string& str);
    uint32_t calculateJOAAT(const std::string& str) const;
    
public:
    // JAMCRC哈希计算（静态方法，用于GTA SA）
    static uint32_t calculateJAMCRC(const std::string& str);
    
    // GTA4哈希计算（静态方法，用于GTA IV）
    static uint32_t calculateGTA4Hash(const std::string& str);
    
    // 辅助函数：标准化键（去掉0x前缀）
    std::string normalizeKey(const std::string& key) const;
    
    // IVTKEY映射相关（用于GTA IV）
    static void LoadIVTKeyMap();
    static bool FindIVTKey(const std::string& key, uint32_t& outHash);
    
private:
    // IVTKEY映射（静态，全局共享）
    static std::unordered_map<std::string, uint32_t> s_ivtKeyMap;
    static bool s_ivtKeyMapLoaded;
};

#endif // GXT_EDITOR_H