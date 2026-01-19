#ifndef GXT_PARSER_H
#define GXT_PARSER_H

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

// 添加GXT版本枚举
enum class GXTVersion {
    UNKNOWN,
    GTA_III,
    GTA_VC,
    GTA_SA,
    GTA_IV,
    GXT2
};

struct GXTEntry {
    std::string key;
    std::string value;
    std::string originalKey;  // 原始键值（用于显示，如DUMMY_l11）
};

struct GXTTabl {
    std::string name;
    std::vector<GXTEntry> entries;
};

struct WHMEntry {
    uint32_t hash;
    uint32_t offset;
    std::string text;
};

// DAT文件条目结构体（与WHMEntry分开，不混淆）
struct DATEntry {
    uint32_t hash;
    uint32_t offset;
    std::string text;
};

class GXTParser {
private:
    std::vector<GXTTabl> tables;
    bool originalHasTABL = true;
    int encodingType = 0; // 0=auto, 1=UTF-8, 2=CP1252, 3=GBK, 4=UTF-16LE
    GXTVersion detectedVersion = GXTVersion::UNKNOWN; // 添加版本检测字段

    uint16_t read_u16(FILE* f) const;
    uint32_t read_u32(FILE* f) const;
    std::wstring read_wstring(FILE* f) const;
    std::string cp1252_to_utf8(const std::string& s) const;
    std::string gbk_to_utf8(const std::string& s) const;
    std::string utf16le_to_utf8(const std::string& s) const;
    std::string guess_decode(const std::string& raw) const;
    std::string decode_with_encoding(const std::string& raw, int encoding) const;
    void makeDir(const std::string& path) const;
    bool parseGXT2(FILE* f, std::vector<GXTTabl>& outTables);

public:
    bool parse(const std::string& filePath);
    void exportToTxt(const std::string& outDir) const;
    bool parseWHM(const std::string& filePath, std::vector<WHMEntry>& outEntries) const;
    void exportWHMToTxt(const std::string& whmPath, const std::string& txtPath) const;
    // DAT文件处理方法（完全独立于WHM）
    bool parseDAT(const std::string& filePath, std::vector<DATEntry>& outEntries) const;
    void exportDATToTxt(const std::string& datPath, const std::string& txtPath) const;
    const std::vector<GXTTabl>& getTables() const { return tables; }
    void setLogCallback(std::function<void(const std::string&)> callback);
    void setEncoding(int encoding) { encodingType = encoding; }
    GXTVersion getDetectedVersion() const { return detectedVersion; } // 添加获取检测版本的方法

};

#endif // GXT_PARSER_H