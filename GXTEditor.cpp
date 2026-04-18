#include "GXTEditor.h"
#include <windows.h>
#include <fstream>
#include <codecvt>
#include <locale>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <QString>
#include <unordered_map>
#include <vector>
#include <QFile>
#include <QTextStream>
#include <QDir>

// IVTKEY映射静态成员定义
std::unordered_map<std::string, uint32_t> GXTEditor::s_ivtKeyMap;
bool GXTEditor::s_ivtKeyMapLoaded = false;

// 构造函数实现
GXTEditor::GXTEditor()
    : version(GXTVersion::UNKNOWN)
{
}

GXTEditor::GXTEditor(const std::string& filePath)
    : filePath(filePath)
    , version(GXTVersion::UNKNOWN)
{
    loadFromFile(filePath);
}

// 加载文件实现
bool GXTEditor::loadFromFile(const std::string& path)
{
    // 使用GXTParser加载文件
    try {
        GXTParser parser;
        bool result = parser.parse(path);
        if (result) {
            // 从GXTTabl转换为GXTTable
            const auto& parserTables = parser.getTables();
            tables.clear();
            tables.reserve(parserTables.size());
            
            for (const auto& parserTable : parserTables) {
                GXTTable table;
                table.name = parserTable.name;
                table.entries.reserve(parserTable.entries.size());
                
                for (const auto& entry : parserTable.entries) {
                    GXTTableEntry tableEntry;
                    tableEntry.key = entry.key;
                    tableEntry.value = entry.value;
                    tableEntry.originalKey = entry.originalKey;  // 复制originalKey字段
                    table.entries.push_back(tableEntry);
                }
                
                tables.push_back(table);
            }
            
            version = parser.getDetectedVersion();
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

// ===== UTF-8 到UTF-16LE 转换辅助函数 =====
// 返回UTF-16LE整数列表（包含null终止符）
std::vector<uint16_t> utf8_to_utf16le_vector(const std::string& utf8) {
    // 先将UTF-8转换为宽字符
    int wideCount = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::vector<uint16_t> result;
    if (wideCount == 0) {
        result.push_back(0); // null终止符
        return result;
    }
    
    std::wstring wideString(wideCount, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wideString[0], wideCount);
    
    // 将宽字符转换为UTF-16LE整数列表（包含null终止符）
    for (wchar_t c : wideString) {
        result.push_back(static_cast<uint16_t>(c));
    }
    
    return result;
}

// 兼容旧代码的字节串版本
std::string utf8_to_utf16le(const std::string& utf8) {
    auto vec = utf8_to_utf16le_vector(utf8);
    std::string result;
    result.reserve(vec.size() * 2);
    for (uint16_t code : vec) {
        result.push_back(code & 0xFF);
        result.push_back((code >> 8) & 0xFF);
    }
    return result;
}

// ===== 十六进制字符串到uint32转换 =====
uint32_t hexStringToUint32(const std::string& hexStr) {
    // 处理 "0x12345678" 或 "12345678" 格式
    std::string cleanHex = hexStr;
    if (cleanHex.substr(0, 2) == "0x" || cleanHex.substr(0, 2) == "0X") {
        cleanHex = cleanHex.substr(2);
    }
    
    // 转换为大写
    std::transform(cleanHex.begin(), cleanHex.end(), cleanHex.begin(), ::toupper);
    
    uint32_t result = 0;
    std::stringstream ss(cleanHex);
    ss >> std::hex >> result;
    
    return result;
}

// ===== 辅助哈希函数 =====
// CRC32校验和计算
static uint32_t calculateCRC32(const std::string& input) {
    // CRC32多项式表（预计算）
    static uint32_t crc32_table[256] = {0};
    static bool initialized = false;
    
    // 初始化CRC32表
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (uint32_t j = 0; j < 8; ++j) {
                crc = (crc >> 1) ^ (-(crc & 1) & 0xEDB88320);
            }
            crc32_table[i] = crc;
        }
        initialized = true;
    }
    
    uint32_t crc = 0xFFFFFFFF;
    for (char c : input) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ static_cast<unsigned char>(c)) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

// JOAAT哈希计算（Jenkins One-at-a-Time）
static uint32_t calculateJOAAT(const std::string& input) {
    uint32_t hash = 0;
    for (char c : input) {
        hash += static_cast<unsigned char>(c);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

// JAMCRC哈希计算（GXT SA使用）
static uint32_t calculateJAMCRC(const std::string& input) {
    // JAMCRC是CRC32的变体：对CRC32结果取反
    uint32_t crc = calculateCRC32(input);
    return 0xFFFFFFFF ^ crc;
}

// 标准化键（去掉x前缀或0x前缀）
std::string GXTEditor::normalizeKey(const std::string& key) const {
    std::string normalized = key;
    // 移除0x或0X前缀
    if (normalized.length() >= 2 && (normalized.substr(0, 2) == "0x" || normalized.substr(0, 2) == "0X")) {
        normalized = normalized.substr(2);
    }
    // 移除x前缀（兼容旧格式）
    else if (normalized.length() >= 1 && normalized[0] == 'x') {
        normalized = normalized.substr(1);
    }
    return normalized;
}

// 计算TDAT块的数据大小
uint32_t GXTEditor::calculateTDATSize(const GXTTable& table, int bitsPerChar) const {
    uint32_t size = 0;
    for (const auto& entry : table.entries) {
        if (bitsPerChar == 8) {
            size += static_cast<uint32_t>(entry.value.size() + 1); // UTF-8: 字符串 + null终止符
        } else if (bitsPerChar == 16) {
            // UTF-16LE: 每个字符2字节 + null终止符(2字节)
            size += static_cast<uint32_t>((entry.value.size() + 1) * 2);
        }
    }
    return size;
}

// 辅助函数：打开文件（处理UTF-8路径）
static std::ofstream openFileForWriting(const std::string& path) {
    std::ofstream file;
    
    // 转换UTF-8路径到宽字符以支持中文路径
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wideLen > 0) {
        std::vector<wchar_t> widePath(wideLen);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, widePath.data(), wideLen);
        file.open(widePath.data(), std::ios::binary);
    }
    
    if (!file.is_open()) {
        // 如果转换失败，尝试直接用UTF-8字符串（可能在某些系统上有效）
        file.open(path, std::ios::binary);
    }
    
    return file;
}

void GXTEditor::convertData() {
    // 转换数据格式
    for (auto& table : tables) {
        for (auto& entry : table.entries) {
            // 标准化键（去掉x前缀）
            std::string normalizedKey = normalizeKey(entry.key);
            
            // 重新计算键的哈希值
            uint32_t newHash = calculateJOAAT(normalizedKey);
            entry.key = "x" + QString::number(newHash, 16).toUpper().toStdString();
        }
    }
}

bool GXTEditor::saveToFile(const std::string& path) {
    std::string savePath = path.empty() ? filePath : path;
    // 根据不同版本采用不同的保存策略
    switch (version) {
    case GXTVersion::GTA_III:
        return saveAsGTA_III(savePath);  // III使用16位字符，无TABL块
    case GXTVersion::GTA_VC:
        return saveAsGTA_VC(savePath); // VC使用16位字符，有TABL块
    case GXTVersion::GTA_SA:
        return saveAsGTA_SA(savePath);  // SA使用8位字符
    case GXTVersion::GTA_IV:
        return saveAsGTA_IV(savePath); // IV使用16位字符
    case GXTVersion::GXT2:
        return saveAsGXT2Format(savePath); // GXT2格式
    default:
        return saveAsGTA_SA(savePath); // 默认使用SA格式
    }
}

// 保存GTA VC格式 (GXT1) - FILE*操作 + 两遍扫描优化
bool GXTEditor::saveAsGTA_VC(const std::string& path) {
    try {
        // 转换路径以支持中文
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wideLen == 0) {
            std::cerr << "Error: Invalid path encoding" << std::endl;
            return false;
        }

        std::vector<wchar_t> widePath(wideLen);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, widePath.data(), wideLen);

        FILE* outputFile = _wfopen(widePath.data(), L"wb");
        if (outputFile == nullptr) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        // 排序表 (MAIN优先，其他按字典序)
        auto sorted_tables = tables;
        if (!std::is_sorted(sorted_tables.begin(), sorted_tables.end(),
            [](const GXTTable& a, const GXTTable& b) {
                if (a.name == "MAIN") return true;
                if (b.name == "MAIN") return false;
                return a.name < b.name;
            })) {
            std::sort(sorted_tables.begin(), sorted_tables.end(),
                [](const GXTTable& a, const GXTTable& b) {
                    if (a.name == "MAIN") return true;
                    if (b.name == "MAIN") return false;
                    return a.name < b.name;
                });
        }

        const int SizeOfTABL = 12;  // 8字节表名 + 4字节偏移
        const int SizeOfTKEY = 12;  // 4字节偏移 + 8字节键

        // 写TABL块头
        long foTableBlock = 8;
        int32_t tableBlockSize = static_cast<int32_t>(sorted_tables.size() * SizeOfTABL);

        fwrite("TABL", 1, 4, outputFile);
        fwrite(&tableBlockSize, 4, 1, outputFile);

        // 跳过TABL条目空间，稍后回填
        fseek(outputFile, tableBlockSize, SEEK_CUR);

        // 处理每个表
        for (const auto& table : sorted_tables) {
            long foKeyBlock = ftell(outputFile);

            int32_t keyBlockSize = static_cast<int32_t>(table.entries.size() * SizeOfTKEY);

            // 计算TDAT数据大小
            int32_t dataBlockSize = 0;
            for (const auto& entry : table.entries) {
                dataBlockSize += static_cast<int32_t>(utf8_to_utf16le_vector(entry.value).size() * 2);
            }

            // 回填TABL条目
            fseek(outputFile, foTableBlock, SEEK_SET);
            char eightChars[8] = {0};
            table.name.copy(eightChars, 7);
            fwrite(eightChars, 1, 8, outputFile);
            fwrite(&foKeyBlock, 4, 1, outputFile);
            foTableBlock += SizeOfTABL;

            // 定位到TKEY块位置
            fseek(outputFile, foKeyBlock, SEEK_SET);

            // 非MAIN表先写8字节表名
            if (table.name != "MAIN") {
                fwrite(eightChars, 1, 8, outputFile);
            }

            // 写TKEY块头
            fwrite("TKEY", 1, 4, outputFile);
            fwrite(&keyBlockSize, 4, 1, outputFile);
            long foKeyData = ftell(outputFile);

            // 跳过TKEY条目空间，稍后回填
            fseek(outputFile, keyBlockSize, SEEK_CUR);

            // 写TDAT块头
            fwrite("TDAT", 1, 4, outputFile);
            fwrite(&dataBlockSize, 4, 1, outputFile);

            long firstTDATEntryOffset = ftell(outputFile);

            // 写入TDAT数据并回填TKEY条目
            for (const auto& entry : table.entries) {
                long foDataBlock = ftell(outputFile);

                // 转换为UTF-16LE
                auto utf16Data = utf8_to_utf16le_vector(entry.value);

                // 回填TKEY条目
                int32_t TDATOffset = static_cast<int32_t>(foDataBlock - firstTDATEntryOffset);

                fseek(outputFile, foKeyData, SEEK_SET);
                fwrite(&TDATOffset, 4, 1, outputFile);

                // 写入键（8字节，填充）
                char keyChars[8] = {0};
                // GTA VC: 直接使用entry.key（已经是原始字符串）
                std::string keyToWrite = entry.key;
                if (keyToWrite.size() > 8) keyToWrite.resize(8);
                keyToWrite.resize(8, '\0');
                fwrite(keyToWrite.c_str(), 1, 8, outputFile);
                foKeyData += SizeOfTKEY;

                // 写入TDAT数据
                fseek(outputFile, foDataBlock, SEEK_SET);
                for (uint16_t code : utf16Data) {
                    fwrite(&code, 2, 1, outputFile);
                }
            }
        }

        fclose(outputFile);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in saveAsGTA_VC: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in saveAsGTA_VC" << std::endl;
        return false;
    }
}

// 保存GTA SA格式 - FILE*操作 + 两遍扫描优化
bool GXTEditor::saveAsGTA_SA(const std::string& path) {
    try {
        // 转换路径以支持中文
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wideLen == 0) {
            std::cerr << "Error: Invalid path encoding" << std::endl;
            return false;
        }

        std::vector<wchar_t> widePath(wideLen);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, widePath.data(), wideLen);

        FILE* outputFile = _wfopen(widePath.data(), L"wb");
        if (outputFile == nullptr) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        const int SizeOfTABL = 12;  // 8字节表名 + 4字节偏移
        const int SizeOfTKEY = 8;   // 4字节偏移 + 4字节哈希

        // 排序表（MAIN优先，其他按字典序）
        auto sorted_tables = tables;
        if (!std::is_sorted(sorted_tables.begin(), sorted_tables.end(),
            [](const GXTTable& a, const GXTTable& b) {
                if (a.name == "MAIN") return true;
                if (b.name == "MAIN") return false;
                return a.name < b.name;
            })) {
            std::sort(sorted_tables.begin(), sorted_tables.end(),
                [](const GXTTable& a, const GXTTable& b) {
                    if (a.name == "MAIN") return true;
                    if (b.name == "MAIN") return false;
                    return a.name < b.name;
                });
        }

        fwrite("\x04\x00\x08\x00", 4, 1, outputFile);

        // 写TABL块头
        long foTableBlock = 12;
        int32_t tableBlockSize = static_cast<int32_t>(sorted_tables.size() * SizeOfTABL);

        fwrite("TABL", 4, 1, outputFile);
        fwrite(&tableBlockSize, 4, 1, outputFile);

        long foKeyBlock = tableBlockSize + 12;
        int32_t keyBlockOffset = foKeyBlock;

        // 处理每个表
        for (const auto& table : sorted_tables) {
            // 计算TKEY和TDAT块大小
            int32_t keyBlockSize = static_cast<int32_t>(table.entries.size() * SizeOfTKEY);
            int32_t dataBlockSize = 0;
            for (const auto& entry : table.entries) {
                dataBlockSize += static_cast<int32_t>(entry.value.length() + 1);
            }

            // 回填TABL条目
            fseek(outputFile, foTableBlock, SEEK_SET);
            char eightChars[8] = {0};
            table.name.copy(eightChars, 7);
            fwrite(eightChars, 8, 1, outputFile);
            fwrite(&keyBlockOffset, 4, 1, outputFile);
            foTableBlock += SizeOfTABL;

            // 定位到TKEY块
            fseek(outputFile, foKeyBlock, SEEK_SET);

            // 非MAIN表先写8字节表名
            if (table.name != "MAIN") {
                fwrite(eightChars, 8, 1, outputFile);
            }

            // 写TKEY块头
            fwrite("TKEY", 4, 1, outputFile);
            fwrite(&keyBlockSize, 4, 1, outputFile);

            long foKeyData = ftell(outputFile);

            // 跳过TKEY条目空间，稍后回填
            fseek(outputFile, keyBlockSize, SEEK_CUR);

            // 写TDAT块头
            long TDATOffset = ftell(outputFile);
            fwrite("TDAT", 4, 1, outputFile);
            fwrite(&dataBlockSize, 4, 1, outputFile);

            long foDataBlock = ftell(outputFile);

            // 处理每个条目并写入TDAT数据
            for (const auto& entry : table.entries) {
                // 计算数据偏移
                int32_t dataOffset = static_cast<int32_t>(foDataBlock - TDATOffset - 8);

                // 回填TKEY条目
                fseek(outputFile, foKeyData, SEEK_SET);
                fwrite(&dataOffset, 4, 1, outputFile);

                // 计算hash值
                uint32_t hash;
                try {
                    hash = std::stoul(entry.key, nullptr, 16);
                } catch (const std::exception&) {
                    std::string key_str = normalizeKey(entry.key);
                    std::string upperKey = key_str;
                    std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::toupper);
                    hash = CKeyGen::GetUppercaseKey(upperKey.c_str());
                }
                fwrite(&hash, 4, 1, outputFile);

                foKeyData += SizeOfTKEY;

                // 写入TDAT数据（UTF-8 + null终止符）
                fseek(outputFile, foDataBlock, SEEK_SET);
                fwrite(entry.value.c_str(), entry.value.length() + 1, 1, outputFile);

                foDataBlock = ftell(outputFile);
            }

            foKeyBlock = ftell(outputFile);
            keyBlockOffset = foKeyBlock;
        }

        fclose(outputFile);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in saveAsGTA_SA: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in saveAsGTA_SA" << std::endl;
        return false;
    }
}

// 保存GXT2格式 - 高速直接指针写入
bool GXTEditor::saveAsGXT2Format(const std::string& path) {
    try {
        // Step 1: 统计总条目数
        size_t totalEntries = 0;
        for (const auto& table : tables) {
            totalEntries += table.entries.size();
        }
        
        if (totalEntries == 0) {
            std::cerr << "Error: No entries to save" << std::endl;
            return false;
        }
        
        // Step 2: 按MAIN表优先排序收集条目
        std::vector<std::pair<const std::string*, const std::string*>> entries;
        entries.reserve(totalEntries);
        
        // 先收集GXT2表（GXT2格式的标准表名）
        for (const auto& table : tables) {
            if (table.name == "GXT2") {
                for (const auto& entry : table.entries) {
                    entries.emplace_back(&entry.key, &entry.value);
                }
                break;
            }
        }
        // 再收集MAIN表（兼容旧格式）
        for (const auto& table : tables) {
            if (table.name == "MAIN") {
                for (const auto& entry : table.entries) {
                    entries.emplace_back(&entry.key, &entry.value);
                }
                break;
            }
        }
        // 再收集其他表
        for (const auto& table : tables) {
            if (table.name != "GXT2" && table.name != "MAIN") {
                for (const auto& entry : table.entries) {
                    entries.emplace_back(&entry.key, &entry.value);
                }
            }
        }
        
        // Step 3: 计算字符串区大小和偏移
        std::vector<uint32_t> relativeOffsets;
        relativeOffsets.reserve(entries.size());
        uint32_t stringDataSize = 0;
        
        for (const auto& [key, value] : entries) {
            relativeOffsets.push_back(stringDataSize);
            stringDataSize += static_cast<uint32_t>(value->size() + 1); // +1 for null terminator
        }
        
        // Step 4: 计算文件布局
        const uint32_t numEntries = static_cast<uint32_t>(entries.size());
        const uint32_t header1Size = 8;
        const uint32_t entryTableSize = numEntries * 8;
        const uint32_t header2Size = 8;
        const uint32_t alignStrings = 4;
        const uint32_t preStringLength = header1Size + entryTableSize + header2Size;
        
        uint32_t padding = 0;
        if (alignStrings > 1) {
            uint32_t rem = preStringLength % alignStrings;
            if (rem != 0) padding = alignStrings - rem;
        }
        
        const uint32_t stringStart = preStringLength + padding;
        const uint32_t totalSize = stringStart + stringDataSize;
        const uint32_t secondEndValue = totalSize;
        
        // Step 5: 预分配完整缓冲区
        std::vector<uint8_t> buffer;
        buffer.resize(totalSize); // 直接resize到最终大小
        uint8_t* ptr = buffer.data();
        
        // Step 6: 写入第一个头部 (8字节)
        *ptr++ = '2'; *ptr++ = 'T'; *ptr++ = 'X'; *ptr++ = 'G';
        memcpy(ptr, &numEntries, 4);
        ptr += 4;
        
        // Step 7: 写入条目表 (每个条目8字节: hash + offset)
        for (size_t i = 0; i < entries.size(); ++i) {
            const std::string& keyStr = *entries[i].first;
            
            // 计算hash
            uint32_t hash;
            if (keyStr.length() >= 2 && keyStr[0] == '0' && (keyStr[1] == 'x' || keyStr[1] == 'X')) {
                try {
                    hash = static_cast<uint32_t>(std::stoul(keyStr.substr(2), nullptr, 16));
                } catch (...) {
                    hash = calculateJOAAT(keyStr);
                }
            } else {
                bool allHex = !keyStr.empty() && std::all_of(keyStr.begin(), keyStr.end(), 
                    [](char c) { return std::isxdigit(c); });
                if (allHex) {
                    try {
                        hash = static_cast<uint32_t>(std::stoul(keyStr, nullptr, 16));
                    } catch (...) {
                        hash = calculateJOAAT(keyStr);
                    }
                } else {
                    hash = calculateJOAAT(keyStr);
                }
            }
            
            uint32_t absOffset = stringStart + relativeOffsets[i];
            memcpy(ptr, &hash, 4);
            ptr += 4;
            memcpy(ptr, &absOffset, 4);
            ptr += 4;
        }
        
        // Step 8: 写入第二个头部 (8字节)
        *ptr++ = '2'; *ptr++ = 'T'; *ptr++ = 'X'; *ptr++ = 'G';
        memcpy(ptr, &secondEndValue, 4);
        ptr += 4;
        
        // Step 9: 填充对齐
        for (uint32_t i = 0; i < padding; ++i) {
            *ptr++ = 0;
        }
        
        // Step 10: 写入字符串数据
        for (const auto& [key, value] : entries) {
            const std::string& val = *value;
            memcpy(ptr, val.c_str(), val.size() + 1); // 包含null终止符
            ptr += val.size() + 1;
        }
        
        // Step 11: 一次性写入文件
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        file.close();
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in saveAsGXT2Format: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in saveAsGXT2Format" << std::endl;
        return false;
    }
}

// 保存为文本格式 - 内存缓冲 + 一次性写入
bool GXTEditor::saveAsText(const std::string& path) {
    try {
        // 构建字符串缓冲区
        std::string buffer;
        buffer.reserve(1024 * 1024); // 预分配1MB
        
        // 写入文件内容（纯文本，不添加 BOM 或额外注释）

        // 按照MAIN表优先的顺序写入表
        std::vector<size_t> tableOrder;

        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name == "MAIN") {
                tableOrder.push_back(i);
                break;
            }
        }

        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name != "MAIN") {
                tableOrder.push_back(i);
            }
        }

        // 构建每个表的内容
        for (size_t tableIdx : tableOrder) {
            const auto& tbl = tables[tableIdx];
            buffer += "[" + tbl.name + "]\n";

            for (const auto& entry : tbl.entries) {
                std::string outputKey = entry.originalKey;
                if (outputKey.empty()) {
                    outputKey = entry.key;
                }
                // 如果是8位十六进制hash，自动加0x前缀
                if (outputKey.length() == 8) {
                    bool allHex = true;
                    for (char c : outputKey) {
                        if (!isxdigit(static_cast<unsigned char>(c))) {
                            allHex = false;
                            break;
                        }
                    }
                    if (allHex) {
                        outputKey = "0x" + outputKey;
                    }
                }
                buffer += outputKey + "=" + entry.value + "\n";
            }

            buffer += "\n";
        }

        // 一次性写入文件
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }
        file.write(buffer.data(), buffer.size());
        file.close();
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in saveAsText: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in saveAsText" << std::endl;
        return false;
    }
}

// 简化的CRC32计算（实际应用中应使用标准CRC32算法）
uint32_t GXTEditor::calculateCRC32(const std::string& str) {
    // 这是一个简化的实现，实际应使用标准的CRC32算法
    uint32_t crc = 0xFFFFFFFF;
    for (char c : str) {
        crc ^= static_cast<uint32_t>(c);
        for (int i = 0; i < 8; i++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

// JOAAT哈希算法（用于GXT2）根据Python实现
uint32_t GXTEditor::calculateJOAAT(const std::string& str) const {
    uint32_t hash = 0;
    for (char c : str) {
        hash += static_cast<uint8_t>(c);
        hash &= 0xFFFFFFFF;
        hash += (hash << 10);
        hash &= 0xFFFFFFFF;
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash &= 0xFFFFFFFF;
    hash ^= (hash >> 11);
    hash += (hash << 15);
    hash &= 0xFFFFFFFF;
    return hash;
}

// JAMCRC哈希算法（用于GTA SA）
uint32_t GXTEditor::calculateJAMCRC(const std::string& str) {
    // JAMCRC是CRC32的变体：对CRC32结果取反
    uint32_t crc = calculateCRC32(str);
    return 0xFFFFFFFF ^ crc;
}

// GTA4哈希计算（用于GTA IV）- 完全匹配Python实现
uint32_t GXTEditor::calculateGTA4Hash(const std::string& str) {
    uint32_t retHash = 0;

    for (char c : str) {
        // A-Z 转小写
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + 32);
        }
        // 反斜杠替换
        else if (c == '\\') {
            c = '/';
        }

        uint32_t cVal = static_cast<uint32_t>(static_cast<unsigned char>(c)) & 0xFF;
        uint32_t tmp = (retHash + cVal) & 0xFFFFFFFF;
        uint32_t mult = (1025 * tmp) & 0xFFFFFFFF;
        retHash = ((mult >> 6) ^ mult) & 0xFFFFFFFF;
    }

    uint32_t a = (9 * retHash) & 0xFFFFFFFF;
    uint32_t aXor = (a ^ (a >> 11)) & 0xFFFFFFFF;
    retHash = (32769 * aXor) & 0xFFFFFFFF;

    return retHash;
}

size_t GXTEditor::getTableCount() const {
    return tables.size();
}

const GXTTable& GXTEditor::getTable(size_t index) const {
    if (index >= tables.size()) {
        throw std::out_of_range("Table index out of range");
    }
    return tables[index];
}

bool GXTEditor::addTable(const std::string& tableName) {
    // 检查表名是否已存在
    if (findTable(tableName) != -1) {
        return false;
    }
    
    // 验证表名
    if (tableName.empty() || tableName.size() > 8) {
        return false;
    }
    
    GXTTable table;
    table.name = tableName;
    tables.push_back(table);
    return true;
}

bool GXTEditor::removeTable(size_t index) {
    if (index >= tables.size()) {
        return false;
    }
    
    tables.erase(tables.begin() + index);
    return true;
}

bool GXTEditor::renameTable(size_t index, const std::string& newName) {
    if (index >= tables.size()) {
        return false;
    }
    
    // 检查新名称是否已存在（除非是当前表）
    int existingIndex = findTable(newName);
    if (existingIndex != -1 && static_cast<size_t>(existingIndex) != index) {
        return false;
    }
    
    // 验证表名
    if (newName.empty() || newName.size() > 8) {
        return false;
    }
    
    tables[index].name = newName;
    return true;
}

size_t GXTEditor::getEntryCount(size_t tableIndex) const {
    if (tableIndex >= tables.size()) {
        throw std::out_of_range("Table index out of range");
    }
    return tables[tableIndex].entries.size();
}

const GXTTableEntry& GXTEditor::getEntry(size_t tableIndex, size_t entryIndex) const {
    if (tableIndex >= tables.size()) {
        throw std::out_of_range("Table index out of range");
    }
    if (entryIndex >= tables[tableIndex].entries.size()) {
        throw std::out_of_range("Entry index out of range");
    }
    return tables[tableIndex].entries[entryIndex];
}

bool GXTEditor::addEntry(size_t tableIndex, const std::string& key, const std::string& value) {
    if (tableIndex >= tables.size()) {
        return false;
    }
    
    // 标准化键（去掉x前缀）
    std::string normalizedKey = normalizeKey(key);
    
    // 检查键是否已存在
    if (findEntry(tableIndex, normalizedKey) != -1) {
        return false;
    }
    
    // 验证键
    if (!isValidKey(normalizedKey)) {
        return false;
    }
    
    GXTTableEntry entry;
    entry.key = normalizedKey;
    entry.value = value;
    tables[tableIndex].entries.push_back(entry);
    return true;
}

bool GXTEditor::removeEntry(size_t tableIndex, size_t entryIndex) {
    if (tableIndex >= tables.size()) {
        return false;
    }
    if (entryIndex >= tables[tableIndex].entries.size()) {
        return false;
    }
    
    tables[tableIndex].entries.erase(tables[tableIndex].entries.begin() + entryIndex);
    return true;
}

bool GXTEditor::updateEntry(size_t tableIndex, size_t entryIndex, const std::string& key, const std::string& value) {
    if (tableIndex >= tables.size()) {
        return false;
    }
    if (entryIndex >= tables[tableIndex].entries.size()) {
        return false;
    }
    
    // 标准化键（去掉x前缀）
    std::string normalizedKey = normalizeKey(key);
    
    // 如果键改变，检查是否与现有键冲突
    if (normalizedKey != tables[tableIndex].entries[entryIndex].key) {
        if (!isValidKey(normalizedKey)) {
            return false;
        }
        if (findEntry(tableIndex, normalizedKey) != -1) {
            return false;
        }
        tables[tableIndex].entries[entryIndex].key = normalizedKey;
    }
    
    tables[tableIndex].entries[entryIndex].value = value;
    return true;
}

bool GXTEditor::updateEntryByKey(size_t tableIndex, const std::string& key, const std::string& value) {
    // 标准化键（去掉x前缀）
    std::string normalizedKey = normalizeKey(key);
    int entryIndex = findEntry(tableIndex, normalizedKey);
    if (entryIndex == -1) {
        return false;
    }
    
    tables[tableIndex].entries[entryIndex].value = value;
    return true;
}

// 高性能批量添加条目（跳过重复检查，用于保存流程）
void GXTEditor::addEntriesBatch(size_t tableIndex, const std::vector<std::pair<std::string, std::string>>& entries) {
    if (tableIndex >= tables.size()) {
        return;
    }
    
    auto& tableEntries = tables[tableIndex].entries;
    tableEntries.reserve(tableEntries.size() + entries.size());
    
    for (const auto& [key, value] : entries) {
        GXTTableEntry entry;
        entry.key = normalizeKey(key);
        entry.value = value;
        tableEntries.push_back(std::move(entry));
    }
}

int GXTEditor::findTable(const std::string& tableName) const {
    for (size_t i = 0; i < tables.size(); i++) {
        if (tables[i].name == tableName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int GXTEditor::findEntry(size_t tableIndex, const std::string& key) const {
    if (tableIndex >= tables.size()) {
        return -1;
    }
    
    for (size_t i = 0; i < tables[tableIndex].entries.size(); i++) {
        if (tables[tableIndex].entries[i].key == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string GXTEditor::getVersionString() const {
    switch (version) {
        case GXTVersion::GTA_III: return "GTA III";
        case GXTVersion::GTA_VC: return "GTA Vice City";
        case GXTVersion::GTA_SA: return "GTA San Andreas";
        case GXTVersion::GTA_IV: return "GTA IV";
        case GXTVersion::GXT2: return "GXT2";
        default: return "Unknown";
    }
}

bool GXTEditor::isValidKey(const std::string& key) const {
    if (key.empty() || key.size() > 8) {
        return false;
    }
    
    // 检查键是否包含非法字符
    for (char c : key) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    return true;
}

void GXTEditor::clear() {
    tables.clear();
    filePath.clear();
    version = GXTVersion::UNKNOWN;
}

// 保存GTA III格式 - FILE*操作 + 两遍扫描优化
bool GXTEditor::saveAsGTA_III(const std::string& path) {
    try {
        // 转换路径以支持中文
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wideLen == 0) {
            std::cerr << "Error: Invalid path encoding" << std::endl;
            return false;
        }

        std::vector<wchar_t> widePath(wideLen);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, widePath.data(), wideLen);

        FILE* outputFile = _wfopen(widePath.data(), L"wb");
        if (outputFile == nullptr) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        const int SizeOfTKEY = 12;  // 4字节偏移 + 8字节键

        // 收集所有条目并按键排序
        std::vector<std::pair<std::string, std::string>> all_entries;
        for (const auto& table : tables) {
            for (const auto& entry : table.entries) {
                all_entries.push_back({entry.key, entry.value});
            }
        }

        if (!std::is_sorted(all_entries.begin(), all_entries.end(),
            [](const auto& a, const auto& b) {
                return a.first < b.first;
            })) {
            std::sort(all_entries.begin(), all_entries.end(),
                [](const auto& a, const auto& b) {
                    return a.first < b.first;
                });
        }

        if (all_entries.empty()) {
            fclose(outputFile);
            return true;
        }

        // 计算块大小
        uint32_t keyBlockSize = static_cast<uint32_t>(all_entries.size() * SizeOfTKEY);
        uint32_t dataBlockSize = 0;
        for (const auto& [key, value] : all_entries) {
            dataBlockSize += static_cast<uint32_t>(utf8_to_utf16le_vector(value).size() * 2);
        }

        // 写TKEY块头
        long fpKeyBlock = 8;
        long fpDataBlock = keyBlockSize + 8;

        fwrite("TKEY", 4, 1, outputFile);
        fwrite(&keyBlockSize, 4, 1, outputFile);

        // 定位到TDAT块头
        fseek(outputFile, fpDataBlock, SEEK_SET);
        fwrite("TDAT", 4, 1, outputFile);
        fwrite(&dataBlockSize, 4, 1, outputFile);
        fpDataBlock += 8;

        // 处理每个条目
        for (const auto& [key, value] : all_entries) {
            // 回填TKEY条目
            fseek(outputFile, fpKeyBlock, SEEK_SET);
            uint32_t offset = static_cast<uint32_t>(fpDataBlock - keyBlockSize - 16);
            fwrite(&offset, 4, 1, outputFile);

            char keyName[8] = {0};
            strncpy(keyName, key.c_str(), 7);
            fwrite(keyName, 8, 1, outputFile);
            fpKeyBlock += SizeOfTKEY;

            // 写入TDAT数据
            fseek(outputFile, fpDataBlock, SEEK_SET);
            auto utf16Data = utf8_to_utf16le_vector(value);
            fwrite(utf16Data.data(), 2, utf16Data.size(), outputFile);
            fpDataBlock += static_cast<long>(utf16Data.size() * 2);
        }

        fclose(outputFile);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in saveAsGTA_III: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in saveAsGTA_III" << std::endl;
        return false;
    }
}

// 保存无表文件格式 (如GTA III) - 直接保存 entries 到 TKEY/TDAT 块
bool GXTEditor::saveAsNoTable(const std::string& path, const std::vector<GXTEntry>& entries) {
    try {
        // 转换路径以支持中文
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wideLen == 0) {
            std::cerr << "Error: Invalid path encoding" << std::endl;
            return false;
        }

        std::vector<wchar_t> widePath(wideLen);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, widePath.data(), wideLen);

        FILE* outputFile = _wfopen(widePath.data(), L"wb");
        if (outputFile == nullptr) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        const int SizeOfTKEY = 12;  // 4字节偏移 + 8字节键

        // 收集所有条目并按键排序
        std::vector<std::pair<std::string, std::string>> all_entries;
        for (const auto& entry : entries) {
            all_entries.push_back({entry.key, entry.value});
        }

        if (!std::is_sorted(all_entries.begin(), all_entries.end(),
            [](const auto& a, const auto& b) {
                return a.first < b.first;
            })) {
            std::sort(all_entries.begin(), all_entries.end(),
                [](const auto& a, const auto& b) {
                    return a.first < b.first;
                });
        }

        if (all_entries.empty()) {
            fclose(outputFile);
            return true;
        }

        // 计算块大小
        uint32_t keyBlockSize = static_cast<uint32_t>(all_entries.size() * SizeOfTKEY);
        uint32_t dataBlockSize = 0;
        for (const auto& [key, value] : all_entries) {
            dataBlockSize += static_cast<uint32_t>(utf8_to_utf16le_vector(value).size() * 2);
        }

        // 写TKEY块头
        long fpKeyBlock = 8;
        long fpDataBlock = keyBlockSize + 8;

        fwrite("TKEY", 4, 1, outputFile);
        fwrite(&keyBlockSize, 4, 1, outputFile);

        // 定位到TDAT块头
        fseek(outputFile, fpDataBlock, SEEK_SET);
        fwrite("TDAT", 4, 1, outputFile);
        fwrite(&dataBlockSize, 4, 1, outputFile);
        fpDataBlock += 8;

        // 处理每个条目
        for (const auto& [key, value] : all_entries) {
            // 回填TKEY条目
            fseek(outputFile, fpKeyBlock, SEEK_SET);
            uint32_t offset = static_cast<uint32_t>(fpDataBlock - keyBlockSize - 16);
            fwrite(&offset, 4, 1, outputFile);

            char keyName[8] = {0};
            strncpy(keyName, key.c_str(), 7);
            fwrite(keyName, 8, 1, outputFile);
            fpKeyBlock += SizeOfTKEY;

            // 写入TDAT数据
            fseek(outputFile, fpDataBlock, SEEK_SET);
            auto utf16Data = utf8_to_utf16le_vector(value);
            fwrite(utf16Data.data(), 2, utf16Data.size(), outputFile);
            fpDataBlock += static_cast<long>(utf16Data.size() * 2);
        }

        fclose(outputFile);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in saveAsNoTable: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in saveAsNoTable" << std::endl;
        return false;
    }
}

// 保存GTA IV无表格式 - 有文件头，TKEY条目为8字节（偏移+CRC32）
bool GXTEditor::saveAsNoTableIV(const std::string& path, const std::vector<GXTEntry>& entries) {
    try {
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wideLen == 0) {
            std::cerr << "Error: Invalid path encoding" << std::endl;
            return false;
        }

        std::vector<wchar_t> widePath(wideLen);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, widePath.data(), wideLen);

        FILE* outputFile = _wfopen(widePath.data(), L"wb");
        if (outputFile == nullptr) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        // 写入文件头：版本号4，字符位数16（UTF-16）
        uint16_t version = 4;
        uint16_t bitsPerChar = 16;
        fwrite(&version, 2, 1, outputFile);
        fwrite(&bitsPerChar, 2, 1, outputFile);

        if (entries.empty()) {
            fclose(outputFile);
            return true;
        }

        // 收集所有条目并按CRC32排序
        std::vector<std::pair<uint32_t, std::string>> all_entries;
        for (const auto& entry : entries) {
            std::string keyStr = entry.key;
            if (keyStr.size() > 2 && (keyStr.substr(0, 2) == "0x" || keyStr.substr(0, 2) == "0X")) {
                keyStr = keyStr.substr(2);
            }
            uint32_t hash = static_cast<uint32_t>(std::stoul(keyStr, nullptr, 16));
            all_entries.push_back({hash, entry.value});
        }

        if (!std::is_sorted(all_entries.begin(), all_entries.end(),
            [](const auto& a, const auto& b) {
                return a.first < b.first;
            })) {
            std::sort(all_entries.begin(), all_entries.end(),
                [](const auto& a, const auto& b) {
                    return a.first < b.first;
                });
        }

        // 计算块大小
        uint32_t keyBlockSize = static_cast<uint32_t>(all_entries.size() * 8);  // 每条目8字节
        uint32_t dataBlockSize = 0;
        for (const auto& [hash, value] : all_entries) {
            dataBlockSize += static_cast<uint32_t>(utf8_to_utf16le_vector(value).size() * 2 + 2);  // +2 for null terminator
        }

        // 写TKEY块头
        fwrite("TKEY", 4, 1, outputFile);
        fwrite(&keyBlockSize, 4, 1, outputFile);

        // 计算TDAT块位置
        long tdatStart = 4 + 8 + keyBlockSize;  // 文件头(4) + TKEY头(8) + TKEY数据(keyBlockSize)
        
        // 写TDAT块头
        fseek(outputFile, tdatStart, SEEK_SET);
        fwrite("TDAT", 4, 1, outputFile);
        fwrite(&dataBlockSize, 4, 1, outputFile);

        // 写入TKEY条目和TDAT数据
        uint32_t dataOffset = 0;  // 相对于TDAT数据起始位置的偏移
        long tkeyDataPos = 12;  // TKEY数据起始位置（文件头4 + TKEY头8）
        long tdatDataPos = tdatStart + 8;  // TDAT数据起始位置

        for (const auto& [hash, value] : all_entries) {
            // 写TKEY条目
            fseek(outputFile, tkeyDataPos, SEEK_SET);
            fwrite(&dataOffset, 4, 1, outputFile);
            fwrite(&hash, 4, 1, outputFile);
            tkeyDataPos += 8;

            // 写TDAT数据
            fseek(outputFile, tdatDataPos, SEEK_SET);
            auto utf16Data = utf8_to_utf16le_vector(value);
            fwrite(utf16Data.data(), 2, utf16Data.size(), outputFile);
            // 写入null终止符
            uint16_t nullTerm = 0;
            fwrite(&nullTerm, 2, 1, outputFile);
            
            // 更新偏移
            dataOffset += static_cast<uint32_t>(utf16Data.size() * 2 + 2);
            tdatDataPos += static_cast<long>(utf16Data.size() * 2 + 2);
        }

        fclose(outputFile);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in saveAsNoTableIV: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in saveAsNoTableIV" << std::endl;
        return false;
    }
}

// 保存GTA IV格式 - 文件seek + 两遍扫描优化
bool GXTEditor::saveAsGTA_IV(const std::string& path) {
    try {
        // 打开文件
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        // MAIN表优先排序
        std::vector<size_t> tableOrder;
        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name == "MAIN") {
                tableOrder.push_back(i);
                break;
            }
        }
        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name != "MAIN") {
                tableOrder.push_back(i);
            }
        }

        // 表信息结构
        struct TableInfo {
            std::string name;
            uint32_t offset;
            uint32_t tkeySize;
            uint32_t tdatSize;
            uint32_t numEntries;
        };
        std::vector<TableInfo> tableInfos;

        // 第一遍：计算所有表的偏移量和大小
        uint32_t currentOffset = 4; // 版本头4字节
        currentOffset += 8; // TABL块头8字节
        uint32_t tablEntriesSize = static_cast<uint32_t>(tableOrder.size()) * 12;
        currentOffset += tablEntriesSize; // TABL条目

        for (size_t tableIdx : tableOrder) {
            const auto& table = tables[tableIdx];
            TableInfo info;
            info.name = table.name;
            info.offset = currentOffset;
            info.numEntries = static_cast<uint32_t>(table.entries.size());
            info.tkeySize = info.numEntries * 8;

            // 非MAIN表先写表名
            if (table.name != "MAIN") {
                currentOffset += 8;
            }

            currentOffset += 8; // TKEY块头
            currentOffset += info.tkeySize; // TKEY数据

            // 预计算TDAT数据大小
            uint32_t totalDataSize = 0;
            for (const auto& entry : table.entries) {
                totalDataSize += static_cast<uint32_t>(utf8_to_utf16le(entry.value).size() + 2);
            }
            info.tdatSize = totalDataSize;

            currentOffset += 8; // TDAT块头
            currentOffset += info.tdatSize; // TDAT数据

            tableInfos.push_back(info);
        }

        // 写入版本信息
        uint16_t version = 4;
        uint16_t bits = 16;
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&bits), sizeof(bits));

        // 写入TABL块头
        file.write("TABL", 4);
        file.write(reinterpret_cast<const char*>(&tablEntriesSize), sizeof(tablEntriesSize));

        // 预留TABL条目空间
        uint32_t tablEntriesPos = static_cast<uint32_t>(file.tellp());
        for (size_t i = 0; i < tableOrder.size(); ++i) {
            uint8_t zero[12] = {0};
            file.write(reinterpret_cast<const char*>(zero), 12);
        }

        // 第二遍：写入每个表的数据
        for (size_t i = 0; i < tableInfos.size(); ++i) {
            const TableInfo& info = tableInfos[i];
            const GXTTable& table = tables[tableOrder[i]];

            // 非MAIN表先写表名
            if (info.name != "MAIN") {
                std::string paddedName = info.name;
                if (paddedName.size() > 7) paddedName.resize(7);
                paddedName.resize(8, '\0');
                file.write(paddedName.c_str(), 8);
            }

            // 写入TKEY块头
            file.write("TKEY", 4);
            file.write(reinterpret_cast<const char*>(&info.tkeySize), sizeof(info.tkeySize));

            // 写入TKEY条目
            uint32_t currentDataOffset = 0;
            for (const auto& entry : table.entries) {
                // 计算hash值
                uint32_t hashVal;
                try {
                    hashVal = std::stoul(entry.key, nullptr, 16);
                } catch (const std::exception&) {
                    std::string normalizedKey = normalizeKey(entry.key);
                    if (!FindIVTKey(normalizedKey, hashVal)) {
                        hashVal = calculateGTA4Hash(normalizedKey);
                    }
                }

                file.write(reinterpret_cast<const char*>(&currentDataOffset), sizeof(currentDataOffset));
                file.write(reinterpret_cast<const char*>(&hashVal), sizeof(hashVal));

                // 更新偏移
                currentDataOffset += static_cast<uint32_t>(utf8_to_utf16le(entry.value).size() + 2);
            }

            // 写入TDAT块头
            file.write("TDAT", 4);
            file.write(reinterpret_cast<const char*>(&info.tdatSize), sizeof(info.tdatSize));

            // 写入TDAT数据（UTF-16字符串）
            for (const auto& entry : table.entries) {
                std::string utf16Data = utf8_to_utf16le(entry.value);
                file.write(utf16Data.c_str(), utf16Data.size());
                file.write("\0\0", 2); // UTF-16 null终止符
            }
        }

        // 回填TABL表项
        file.seekp(tablEntriesPos);
        for (size_t i = 0; i < tableInfos.size(); ++i) {
            const TableInfo& info = tableInfos[i];
            std::string paddedName = info.name;
            if (paddedName.size() > 7) paddedName.resize(7);
            paddedName.resize(8, '\0');
            file.write(paddedName.c_str(), 8);
            file.write(reinterpret_cast<const char*>(&info.offset), sizeof(info.offset));
        }

        file.close();
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in saveAsGTA_IV: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in saveAsGTA_IV" << std::endl;
        return false;
    }
}

// CKeyGen实现 - 用于SA文件的hash计算
const unsigned int CKeyGen::keyTable[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

unsigned int CKeyGen::GetKey(const char *str, int len) {
    unsigned int hash = 0xFFFFFFFF;
    for(int i = 0; i < len; i++)
        hash = keyTable[(unsigned char)hash ^ *str++] ^ (hash >> 8);
    return hash;
}

unsigned int CKeyGen::GetKey(const char *str) {
    unsigned int hash = 0xFFFFFFFF;
    while(*str)
        hash = keyTable[(unsigned char)hash ^ *str++] ^ (hash >> 8);
    return hash;
}

unsigned int CKeyGen::GetUppercaseKey(const char *str) {
    unsigned int hash = 0xFFFFFFFF;
    while(*str)
        hash = keyTable[(unsigned char)hash ^ std::toupper(*str++)] ^ (hash >> 8);
    return hash;
}

unsigned int CKeyGen::AppendStringToKey(unsigned int hash, const char *str) {
    while(*str)
        hash = keyTable[(unsigned char)hash ^ *str++] ^ (hash >> 8);
    return hash;
}

// 加载IVTKEY映射（静态方法，全局只加载一次）
void GXTEditor::LoadIVTKeyMap() {
    // 如果已经加载过，直接返回
    if (s_ivtKeyMapLoaded) {
        return;
    }
    
    // 构建文件路径
    QString keylistPath = QDir::current().filePath("keylist/IVTKEY.lst");
    
    // 检查文件是否存在
    if (!QFile::exists(keylistPath)) {
        std::cerr << "IVTKEY映射文件不存在: " << keylistPath.toStdString() << std::endl;
        s_ivtKeyMapLoaded = true; // 标记为已加载，避免重复尝试
        return;
    }
    
    // 打开文件
    QFile file(keylistPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "无法打开IVTKEY映射文件: " << keylistPath.toStdString() << std::endl;
        s_ivtKeyMapLoaded = true; // 标记为已加载
        return;
    }
    
    QTextStream stream(&file);
    int loadedCount = 0;
    
    // 逐行读取文件
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        
        // 跳过空行和注释行
        if (line.isEmpty() || line.startsWith("#") || line.startsWith("//")) {
            continue;
        }
        
        // 解析格式: hash key (例如: 0x12345678 SOME_KEY)
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            QString hashStr = parts[0].trimmed();
            QString keyStr = parts[1].trimmed();
            
            // 转换hash字符串为uint32
            bool ok;
            uint32_t hash = hashStr.toUInt(&ok, 16);
            
            if (ok) {
                // 添加到映射表（key -> hash）
                s_ivtKeyMap[keyStr.toStdString()] = hash;
                loadedCount++;
            }
        }
    }
    
    file.close();
    s_ivtKeyMapLoaded = true;
    
    std::cout << "成功加载 " << loadedCount << " 条IVTKEY映射" << std::endl;
}

// 在IVTKEY映射中查找key对应的hash
bool GXTEditor::FindIVTKey(const std::string& key, uint32_t& outHash) {
    if (!s_ivtKeyMapLoaded) {
        LoadIVTKeyMap();
    }
    
    auto it = s_ivtKeyMap.find(key);
    if (it != s_ivtKeyMap.end()) {
        outHash = it->second;
        return true;
    }
    return false;
}

// 保存WHM文件 - 内存缓冲 + 一次性写入
bool GXTEditor::saveAsWHM(const std::string& path, const std::vector<WHMEntry>& entries) {
    try {
        // 构建内存缓冲区
        std::vector<uint8_t> buffer;
        buffer.reserve(1024 * 1024); // 预分配1MB
        
        // 计算文本数据块
        std::vector<uint32_t> offsets;
        std::string blob;
        blob.reserve(entries.size() * 64); // 预估算
        
        for (const auto& entry : entries) {
            offsets.push_back(static_cast<uint32_t>(blob.size()));
            blob.append(entry.text);
            blob.push_back('\0');
        }
        
        // 写入条目数
        uint32_t count = static_cast<uint32_t>(entries.size());
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&count), 
                      reinterpret_cast<uint8_t*>(&count) + sizeof(count));
        
        // 写入条目表（hash + offset）
        for (size_t i = 0; i < entries.size(); ++i) {
            uint32_t hash = entries[i].hash;
            uint32_t offset = offsets[i];
            buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&hash), 
                          reinterpret_cast<uint8_t*>(&hash) + sizeof(hash));
            buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&offset), 
                          reinterpret_cast<uint8_t*>(&offset) + sizeof(offset));
        }
        
        // 写入blob大小
        uint32_t blobSize = static_cast<uint32_t>(blob.size());
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&blobSize), 
                      reinterpret_cast<uint8_t*>(&blobSize) + sizeof(blobSize));
        
        // 写入blob数据
        buffer.insert(buffer.end(), blob.begin(), blob.end());
        
        // 一次性写入文件
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in saveAsWHM: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception in saveAsWHM" << std::endl;
        return false;
    }
}

// 保存DAT文件 - 内存缓冲 + 一次性写入
bool GXTEditor::saveAsDAT(const std::string& path, const std::vector<DATEntry>& entries) {
    try {
        // 构建内存缓冲区
        std::vector<uint8_t> buffer;
        buffer.reserve(1024 * 1024); // 预分配1MB
        
        // 计算文本数据块
        std::vector<uint32_t> offsets;
        std::string blob;
        blob.reserve(entries.size() * 64);
        
        for (const auto& entry : entries) {
            offsets.push_back(static_cast<uint32_t>(blob.size()));
            blob.append(entry.text);
            blob.push_back('\0');
        }
        
        // 写入条目数
        uint32_t count = static_cast<uint32_t>(entries.size());
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&count), 
                      reinterpret_cast<uint8_t*>(&count) + sizeof(count));
        
        // 写入条目表（hash + offset）
        for (size_t i = 0; i < entries.size(); ++i) {
            uint32_t hash = entries[i].hash;
            uint32_t offset = offsets[i];
            buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&hash), 
                          reinterpret_cast<uint8_t*>(&hash) + sizeof(hash));
            buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&offset), 
                          reinterpret_cast<uint8_t*>(&offset) + sizeof(offset));
        }
        
        // 写入blob大小
        uint32_t blobSize = static_cast<uint32_t>(blob.size());
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&blobSize), 
                      reinterpret_cast<uint8_t*>(&blobSize) + sizeof(blobSize));
        
        // 写入blob数据
        buffer.insert(buffer.end(), blob.begin(), blob.end());
        
        // 一次性写入文件
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in saveAsDAT: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception in saveAsDAT" << std::endl;
        return false;
    }
}