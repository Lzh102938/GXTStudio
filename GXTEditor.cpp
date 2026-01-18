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

// 保存GTA VC格式 (GXT1) - 完全符合Python版本规范
bool GXTEditor::saveAsGTA_VC(const std::string& path) {
    try {
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        // 步骤1: 排序表 (MAIN优先，其他按字典序)
        auto sorted_tables = tables;
        std::sort(sorted_tables.begin(), sorted_tables.end(),
            [](const GXTTable& a, const GXTTable& b) {
                if (a.name == "MAIN") return true;
                if (b.name == "MAIN") return false;
                return a.name < b.name;
            });

        // 步骤2: 写入TABL头部
        file.write("TABL", 4);
        uint32_t table_block_size = static_cast<uint32_t>(sorted_tables.size() * 12);
        file.write(reinterpret_cast<const char*>(&table_block_size), sizeof(table_block_size));

        // 步骤3: 预留TABL块空间
        std::map<std::string, uint32_t> table_offsets;
        file.seekp(8 + table_block_size, std::ios::beg);

        // 步骤4: 处理每个表
        for (const auto& table : sorted_tables) {
            // 记录当前表位置
            table_offsets[table.name] = static_cast<uint32_t>(file.tellp());

            // 如果不是MAIN表，先写8字节表名
            if (table.name != "MAIN") {
                std::string name_padded = table.name;
                if (name_padded.size() > 8) name_padded.resize(8);
                name_padded.resize(8, '\0');
                file.write(name_padded.c_str(), 8);
            }

            // 写TKEY头部
            file.write("TKEY", 4);
            uint32_t key_block_size = static_cast<uint32_t>(table.entries.size() * 12);  // offset(4) + key(8) = 12
            file.write(reinterpret_cast<const char*>(&key_block_size), sizeof(key_block_size));

            // 预留TKEY条目空间（记录起始位置，然后相对跳过）
            uint32_t key_entries_pos = static_cast<uint32_t>(file.tellp());
            file.seekp(key_block_size, std::ios::cur);  // 相对跳过，就像Python的 f.seek(key_block_size, 1)

            // 写TDAT头部
            file.write("TDAT", 4);
            
            // 预计算TDAT块大小
            uint32_t data_block_size = 0;
            std::vector<std::vector<uint16_t>> converted_values;
            for (const auto& entry : table.entries) {
                auto utf16Data = utf8_to_utf16le_vector(entry.value);
                converted_values.push_back(utf16Data);
                data_block_size += static_cast<uint32_t>(utf16Data.size() * 2);
            }
            
            file.write(reinterpret_cast<const char*>(&data_block_size), sizeof(data_block_size));
            uint32_t tdat_start = static_cast<uint32_t>(file.tellp());

            // 写入字符串数据并记录偏移
            std::vector<std::pair<std::string, uint32_t>> key_offsets;
            for (size_t i = 0; i < table.entries.size(); ++i) {
                const auto& entry = table.entries[i];
                const auto& utf16Data = converted_values[i];

                // 计算偏移量（相对于TDAT数据开始）
                uint32_t offset = static_cast<uint32_t>(file.tellp()) - tdat_start;
                key_offsets.push_back({entry.key, offset});

                // 写入UTF-16LE数据
                for (uint16_t code : utf16Data) {
                    file.write(reinterpret_cast<const char*>(&code), sizeof(uint16_t));
                }
            }

            // 回填TKEY条目
            file.seekp(key_entries_pos, std::ios::beg);
            for (const auto& [key, offset] : key_offsets) {
                // 写入offset（4字节）
                file.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
                // 写入key（8字节，填充）
                std::string key_padded = key;
                if (key_padded.size() > 8) key_padded.resize(8);
                key_padded.resize(8, '\0');
                file.write(key_padded.c_str(), 8);
            }

            // 回到文件末尾
            file.seekp(0, std::ios::end);
        }

        // 步骤5: 回填TABL条目
        file.seekp(8, std::ios::beg);
        for (const auto& table : sorted_tables) {
            // 写入表名（8字节，填充）
            std::string name_padded = table.name;
            if (name_padded.size() > 8) name_padded.resize(8);
            name_padded.resize(8, '\0');
            file.write(name_padded.c_str(), 8);
            // 写入偏移量（4字节）
            uint32_t offset = table_offsets[table.name];
            file.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
        }

        file.close();
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

// 保存GTA SA格式 - 完全按SAGXT.py逻辑复刻
bool GXTEditor::saveAsGTA_SA(const std::string& path) {
    try {
        std::ofstream f = openFileForWriting(path);
        if (!f.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        // Step 1: 写版本头 (4字节)
        f.write("\x04\x00\x08\x00", 4);

        // Step 2: 排序表（MAIN优先，其他按字典序）
        auto sorted_tables = tables;
        std::sort(sorted_tables.begin(), sorted_tables.end(),
            [](const GXTTable& a, const GXTTable& b) {
                if (a.name == "MAIN") return true;
                if (b.name == "MAIN") return false;
                return a.name < b.name;
            });

        // Step 3: 预计算TABL块大小
        uint32_t table_block_size = static_cast<uint32_t>(sorted_tables.size() * 12);

        // Step 4: 写TABL块头
        f.write("TABL", 4);
        f.write(reinterpret_cast<const char*>(&table_block_size), 4);

        // Step 5: 预先计算所有表的信息和位置
        struct TableInfo {
            std::string table_name;
            std::vector<GXTTableEntry> entries;
            uint32_t key_block_size;   // TKEY条目数组大小 (entry_count * 8)
            uint32_t data_block_size;  // TDAT数据大小
        };
        std::vector<TableInfo> table_info;

        for (const auto& table : sorted_tables) {
            uint32_t key_size = static_cast<uint32_t>(table.entries.size() * 8);
            uint32_t data_size = 0;
            for (const auto& entry : table.entries) {
                // UTF-8编码：每个字符1字节 + null终止符1字节
                data_size += static_cast<uint32_t>(entry.value.size() + 1);
            }
            table_info.push_back({table.name, table.entries, key_size, data_size});
        }

        // Step 6: 初始化位置跟踪
        uint32_t fo_table_block = 12;  // TABL条目写入位置 (从12开始)
        uint32_t fo_key_block = 12 + table_block_size;  // TKEY块数据起始位置
        uint32_t key_block_offset = fo_key_block;  // 当前表的TKEY块偏移（记录在TABL中）

        // 调试信息
        std::cerr << "Saving GXT file with " << sorted_tables.size() << " tables" << std::endl;

        // Step 7: 处理每个表
        for (const auto& info : table_info) {
            // 7.1: 写TABL条目 (8字节表名 + 4字节偏移)
            f.seekp(fo_table_block);
            std::string table_name_padded = info.table_name;
            if (table_name_padded.size() > 7) table_name_padded.resize(7);
            table_name_padded.resize(8, '\0');
            f.write(table_name_padded.c_str(), 8);
            f.write(reinterpret_cast<const char*>(&key_block_offset), 4);
            fo_table_block += 12;

            // 7.2: 写TKEY块头 (表名前缀 + TKEY标记 + 大小)
            f.seekp(fo_key_block);

            // 对于非MAIN表，先写8字节表名
            if (info.table_name != "MAIN") {
                f.write(table_name_padded.c_str(), 8);
            }

            // 写TKEY标签和大小
            f.write("TKEY", 4);
            f.write(reinterpret_cast<const char*>(&info.key_block_size), 4);

            // 记录TKEY数据写入位置 (紧随TKEY头之后)
            uint32_t fo_key_data = static_cast<uint32_t>(f.tellp());

            // 相对跳过TKEY条目空间
            f.seekp(info.key_block_size, std::ios::cur);

            // 7.3: 写TDAT块头
            f.write("TDAT", 4);
            f.write(reinterpret_cast<const char*>(&info.data_block_size), 4);

            // 记录TDAT数据写入位置 (紧随TDAT头之后)
            uint32_t tdat_offset = static_cast<uint32_t>(f.tellp());

            // 7.4: 按照C++参考代码的方式逐条写入数据
            // 先创建条目与哈希的映射，便于排序
            struct KeyEntry {
                uint32_t hash;
                const GXTTableEntry* entry;
            };
            std::vector<KeyEntry> sorted_entries;

            std::cerr << "  Processing table '" << info.table_name << "' with " << info.entries.size() << " entries" << std::endl;

            for (const auto& entry : info.entries) {
                // 使用normalizeKey来移除所有可能的前缀（x、0x、0X）
                std::string key_str = normalizeKey(entry.key);

                uint32_t hash_key;
                try {
                    // 解析十六进制键
                    hash_key = std::stoul(key_str, nullptr, 16);
                } catch (const std::exception& e) {
                    // 【修改】如果解析失败（不是hex格式），使用CKeyGen计算hash
                    std::cerr << "Info: Key '" << entry.key << "' is not hex format, calculating hash using CKeyGen" << std::endl;
                    
                    // 使用CKeyGen计算hash（转换为大写）
                    std::string upperKey = key_str;
                    std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::toupper);
                    hash_key = CKeyGen::GetUppercaseKey(upperKey.c_str());
                    
                    std::cerr << "  Calculated hash for '" << key_str << "': 0x" << std::hex << hash_key << std::dec << std::endl;
                }
                sorted_entries.push_back({hash_key, &entry});
            }

            // 按哈希值排序(GTA SA规范要求)
            std::sort(sorted_entries.begin(), sorted_entries.end(),
                [](const KeyEntry& a, const KeyEntry& b) {
                    return a.hash < b.hash;
                });

            std::cerr << "  Writing " << sorted_entries.size() << " entries..." << std::endl;

            // 按照Python参考代码批量写入TKEY和TDAT数据
            std::vector<uint8_t> key_data;  // TKEY数据缓冲区
            std::vector<std::vector<uint8_t>> data_blocks;  // TDAT数据块列表
            uint32_t total_data_size = 0;  // 累计TDAT数据大小

            for (const auto& key_entry : sorted_entries) {
                // 计算当前条目在TDAT中的偏移（从0开始，累加前面所有数据块大小）
                uint32_t data_offset = total_data_size;

                // 构建TKEY条目（4字节偏移 + 4字节哈希）
                uint8_t* offset_ptr = reinterpret_cast<uint8_t*>(&data_offset);
                uint32_t hash_key = key_entry.hash;  // 复制到非const变量
                uint8_t* hash_ptr = reinterpret_cast<uint8_t*>(&hash_key);
                for (int i = 0; i < 4; ++i) {
                    key_data.push_back(offset_ptr[i]);
                }
                for (int i = 0; i < 4; ++i) {
                    key_data.push_back(hash_ptr[i]);
                }

                std::cerr << "    Key: " << key_entry.entry->key << " -> Hash: 0x" << std::hex << key_entry.hash << std::dec
                          << ", Offset: " << data_offset << std::endl;

                // 构建TDAT数据块（UTF-8编码 + null终止符）
                const std::string& value = key_entry.entry->value;
                std::vector<uint8_t> data_block;
                data_block.reserve(value.size() + 1);
                for (char c : value) {
                    data_block.push_back(static_cast<uint8_t>(c));
                }
                data_block.push_back('\0');  // 添加null终止符
                data_blocks.push_back(data_block);

                // 累加数据大小
                total_data_size += static_cast<uint32_t>(data_block.size());
            }

            // 批量写入TKEY数据
            f.seekp(fo_key_data);
            if (!key_data.empty()) {
                f.write(reinterpret_cast<const char*>(key_data.data()), key_data.size());
            }

            // 批量写入TDAT数据
            f.seekp(tdat_offset);
            for (const auto& data_block : data_blocks) {
                f.write(reinterpret_cast<const char*>(data_block.data()), data_block.size());
            }

            // 7.5: 更新下一个表的位置
            fo_key_block = static_cast<uint32_t>(f.tellp());
            key_block_offset = fo_key_block;
        }

        f.close();
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

// 保存GXT2格式 - 完全符合Python版本规范
bool GXTEditor::saveAsGXT2Format(const std::string& path) {
    try {
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }
        
        // Step 1: 收集所有条目并排序 - MAIN表优先
        std::vector<std::pair<size_t, size_t>> entryOrder; // (table_index, entry_index)
        
        // 首先收集MAIN表的条目
        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name == "MAIN") {
                for (size_t j = 0; j < tables[i].entries.size(); ++j) {
                    entryOrder.push_back({i, j});
                }
                break;
            }
        }
        
        // 然后收集其他表的条目
        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name != "MAIN") {
                for (size_t j = 0; j < tables[i].entries.size(); ++j) {
                    entryOrder.push_back({i, j});
                }
            }
        }
        
        if (entryOrder.empty()) {
            std::cerr << "Error: No entries to save" << std::endl;
            return false;
        }
        
        // Step 2: 处理键值 - 键保持字符串形式，无需额外计算
        struct StringEntry {
            std::string key;
            size_t tableIndex;
            size_t entryIndex;
            std::string value;
        };
        
        std::vector<StringEntry> entries;
        for (const auto& [tableIdx, entryIdx] : entryOrder) {
            const auto& entry = tables[tableIdx].entries[entryIdx];
            entries.push_back({entry.key, tableIdx, entryIdx, entry.value});
        }
        
        // 保持原有顺序（MAIN表优先，然后其他表）
        
        // Step 3: 构建字符串数据区
        std::vector<uint8_t> stringData;
        std::vector<uint32_t> relativeOffsets;
        uint32_t currentRel = 0;
        
        for (const auto& entry : entries) {
            // 记录相对偏移
            relativeOffsets.push_back(currentRel);
            
            // 编码字符串为UTF-8并添加null终止符
            const std::string& val = entry.value;
            for (char c : val) {
                stringData.push_back(static_cast<uint8_t>(c));
            }
            stringData.push_back('\0'); // null terminator

            // 更新当前位置
            currentRel += static_cast<uint32_t>(val.size() + 1);
        }
        
        // Step 4: 计算文件布局
        uint32_t numEntries = static_cast<uint32_t>(entries.size());
        
        // 第一个头部：2TXG + 条目数量
        // 条目表：每个条目8字节 (哈希值 + 偏移量)
        uint32_t header1Size = 8;
        uint32_t entryTableSize = numEntries * 8;
        uint32_t header2Size = 8; // 第二个2TXG头部
        
        // 计算对齐填充
        uint32_t alignStrings = 4;
        uint32_t preStringLength = header1Size + entryTableSize + header2Size;
        uint32_t padding = 0;
        
        if (alignStrings > 1) {
            uint32_t rem = preStringLength % alignStrings;
            if (rem != 0) {
                padding = alignStrings - rem;
            }
        }
        
        uint32_t stringStart = preStringLength + padding;
        uint32_t totalSize = stringStart + static_cast<uint32_t>(stringData.size());
        uint32_t secondEndValue = totalSize; // 不使用size-1模式

        // Step 5: 写入第一个头部
        file.write("2TXG", 4);
        file.write(reinterpret_cast<const char*>(&numEntries), sizeof(numEntries));
        
        // Step 6: 写入条目表（使用绝对偏移量）
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& ent = entries[i];
            uint32_t absOffset = stringStart + relativeOffsets[i];

            // 直接使用键字符串计算哈希值（GXT2格式要求）
            uint32_t hash = calculateJOAAT(ent.key);

            // 写入哈希值（4字节）
            file.write(reinterpret_cast<const char*>(&hash), sizeof(hash));
            // 写入绝对偏移量（4字节）
            file.write(reinterpret_cast<const char*>(&absOffset), sizeof(absOffset));
        }

        // Step 7: 写入第二个头部
        file.write("2TXG", 4);
        file.write(reinterpret_cast<const char*>(&secondEndValue), sizeof(secondEndValue));
        
        // Step 8: 写入对齐填充
        if (padding > 0) {
            for (uint32_t i = 0; i < padding; ++i) {
                file.put('\0');
            }
        }
        
        // Step 9: 写入字符串数据区
        for (uint8_t byte : stringData) {
            file.write(reinterpret_cast<const char*>(&byte), 1);
        }

        file.close();
        
        std::cerr << "Info: GXT2 file saved successfully" << std::endl;
        std::cerr << " - Entry count: " << numEntries << std::endl;
        std::cerr << " - File size: " << totalSize << " bytes" << std::endl;
        std::cerr << " - String alignment padding: " << padding << " bytes" << std::endl;
        std::cerr << " - Second header end value: " << secondEndValue << std::endl;
        
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

// 保存为文本格式 - 完全符合Python版本规范
bool GXTEditor::saveAsText(const std::string& path) {
    try {
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }
        
        // 写入文件头部 - 使用UTF-8 BOM
        file << "\xEF\xBB\xBF";
        file << "# GXT Text Format\n";
        file << "# Version: " << getVersionString() << "\n\n";

        // 按照MAIN表优先的顺序写入表
        std::vector<size_t> tableOrder;

        // 首先写入MAIN表
        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name == "MAIN") {
                tableOrder.push_back(i);
                break;
            }
        }

        // 然后写入其他表
        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name != "MAIN") {
                tableOrder.push_back(i);
            }
        }

        // 写入每个表
        for (size_t tableIdx : tableOrder) {
            const auto& tbl = tables[tableIdx];

            file << "[" << tbl.name << "]\n";

            // 写入每个条目
            for (const auto& entry : tbl.entries) {
                file << entry.key << "=" << entry.value << "\n";
            }

            file << "\n";
        }

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

// GTA4哈希计算（用于GTA IV）- 修正算法
uint32_t GXTEditor::calculateGTA4Hash(const std::string& str) {
    uint32_t retHash = 0;
    
    for (char c : str) {
        // 遇到引号结束
        if (c == '"') {
            break;
        }

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

// 保存GTA III格式 - 完全按照LCGXT.py逻辑复刻
bool GXTEditor::saveAsGTA_III(const std::string& path) {
    try {
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        // GTA III没有版本头，直接从TKEY块开始（像Python版本一样）

        // 收集所有条目并按键排序（GTA III使用字典序）
        std::vector<std::pair<std::string, std::string>> all_entries;
        for (const auto& table : tables) {
            for (const auto& entry : table.entries) {
                all_entries.push_back({entry.key, entry.value});
            }
        }

        // 按键名字典序排序
        std::sort(all_entries.begin(), all_entries.end(),
            [](const auto& a, const auto& b) {
                return a.first < b.first;
            });

        // 计算块大小
        uint32_t entry_count = static_cast<uint32_t>(all_entries.size());
        uint32_t key_block_size = entry_count * 12; // 每个条目12字节：偏移(4) + 键(7) + null(1)

        // 预计算TDAT数据大小
        std::vector<std::vector<uint16_t>> convertedValues;
        uint32_t data_block_size = 0;
        for (const auto& [key, value] : all_entries) {
            auto utf16Data = utf8_to_utf16le_vector(value);
            convertedValues.push_back(utf16Data);
            data_block_size += static_cast<uint32_t>(utf16Data.size() * 2);
        }

        // 写入TKEY块头（没有版本头，直接开始）
        file.write("TKEY", 4);
        file.write(reinterpret_cast<const char*>(&key_block_size), sizeof(key_block_size));

        // 预留TKEY条目空间，跳到TDAT块头位置
        file.seekp(8 + key_block_size);
        file.write("TDAT", 4);
        file.write(reinterpret_cast<const char*>(&data_block_size), sizeof(data_block_size));

        // 计算数据起始位置
        uint32_t data_start_pos = 8 + key_block_size + 8; // TKEY头+TKEY数据+TDAT头
        uint32_t current_data_pos = data_start_pos;

        // 写入键值数据区
        for (size_t i = 0; i < all_entries.size(); ++i) {
            const auto& [k, v] = all_entries[i];
            const auto& utf16Data = convertedValues[i];

            // 计算偏移量（按照Python版本公式：current_data_pos - (key_block_size + 16)）
            uint32_t offset = current_data_pos - (key_block_size + 16);

            // 回写TKEY条目
            uint32_t tkey_entry_pos = 8 + (i * 12); // TKEY头后的位置
            file.seekp(tkey_entry_pos);
            file.write(reinterpret_cast<const char*>(&offset), sizeof(offset));

            // 写入键名字（7字节ASCII + 1字节null）- 完全按照Python的ljust(7, '\x00')方式
            std::string key_padded;
            key_padded.reserve(7);
            key_padded.append(k);
            // 如果长度不足7，用null字符左填充
            while (key_padded.size() < 7) {
                key_padded.push_back('\0');
            }
            // 如果长度超过7，截断到7
            if (key_padded.size() > 7) {
                key_padded.resize(7);
            }
            file.write(key_padded.c_str(), 7);
            file.put('\0'); // 第8字节是null

            // 写入字符串数据到当前位置（按照Python的struct.pack('<H', char)写入）
            file.seekp(current_data_pos);
            for (uint16_t code : utf16Data) {
                file.write(reinterpret_cast<const char*>(&code), sizeof(uint16_t));
            }

            // 更新当前位置
            current_data_pos += static_cast<uint32_t>(utf16Data.size() * 2);
        }

        file.close();
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

// 保存GTA IV格式 (GXT2) - 16位字符
bool GXTEditor::saveAsGTA_IV(const std::string& path) {
    try {
        std::ofstream file = openFileForWriting(path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
            return false;
        }

        // 写入版本信息
        uint16_t version = 4;
        uint16_t bits = 16; // GTA IV 始终使用 16 位字符
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&bits), sizeof(bits));

        // 按照Python版本排序：MAIN表优先
        std::vector<size_t> tableOrder;

        // 首先处理MAIN表
        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name == "MAIN") {
                tableOrder.push_back(i);
                break;
            }
        }

        // 然后处理其他表
        for (size_t i = 0; i < tables.size(); ++i) {
            if (tables[i].name != "MAIN") {
                tableOrder.push_back(i);
            }
        }

        // 计算TABL块大小
        uint32_t num_tables = static_cast<uint32_t>(tableOrder.size());
        uint32_t tabl_size = num_tables * 12;

        // 写入TABL块头
        file.write("TABL", 4);
        file.write(reinterpret_cast<const char*>(&tabl_size), sizeof(tabl_size));

        // 计算每个表的偏移量
        std::unordered_map<std::string, uint32_t> table_offsets;
        uint32_t file_offset = 4 + 8 + tabl_size; // 文件头+TABL头+TABL表项

        for (size_t tableIdx : tableOrder) {
            const auto& table = tables[tableIdx];
            table_offsets[table.name] = file_offset;

            // 如果不是MAIN表，先加8字节表名
            if (table.name != "MAIN") {
                file_offset += 8;
            }

            // TKEY
            uint32_t tkey_data_size = static_cast<uint32_t>(table.entries.size() * 8); // 偏移(4) + 哈希(4) = 8字节
            file_offset += 8 + tkey_data_size; // 含头+数据

            // TDAT - 按照Python代码计算：len(text.encode('utf-16le')) + 2
            uint32_t str_total_len = 0;
            for (const auto& entry : table.entries) {
                auto utf16Data = utf8_to_utf16le(entry.value);  // 返回UTF-16LE字节串
                // UTF-16LE数据长度 + 2字节null终止符
                str_total_len += static_cast<uint32_t>(utf16Data.size() + 2);
            }
            file_offset += 8 + str_total_len; // 含头+数据
        }

        // 写入TABL表项
        for (size_t tableIdx : tableOrder) {
            const auto& table = tables[tableIdx];
            std::string name_padded = table.name;
            if (name_padded.size() > 7) name_padded.resize(7);
            name_padded.resize(8, '\0');
            file.write(name_padded.c_str(), 8);
            uint32_t offset = table_offsets[table.name];
            file.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
        }

        // 写入各表数据
        for (size_t tableIdx : tableOrder) {
            const auto& table = tables[tableIdx];

            // 非MAIN表先写表名
            if (table.name != "MAIN") {
                std::string name_padded = table.name;
                if (name_padded.size() > 7) name_padded.resize(7);
                name_padded.resize(8, '\0');
                file.write(name_padded.c_str(), 8);
            }

            // 写入TKEY块头
            file.write("TKEY", 4);
            uint32_t tkey_data_size = static_cast<uint32_t>(table.entries.size() * 8);
            file.write(reinterpret_cast<const char*>(&tkey_data_size), sizeof(tkey_data_size));

            // 计算字符串偏移量
            std::vector<uint32_t> str_offsets;
            uint32_t cur_off = 0;
            std::vector<std::vector<uint8_t>> data_blocks;  // 存储UTF-16LE数据块

            for (const auto& entry : table.entries) {
                str_offsets.push_back(cur_off);
                // 转换为UTF-16LE字节数组
                auto utf16Data = utf8_to_utf16le(entry.value);
                std::vector<uint8_t> data_block;
                data_block.reserve(utf16Data.size() + 2);
                for (char c : utf16Data) {
                    data_block.push_back(static_cast<uint8_t>(c));
                }
                // 添加2字节null终止符
                data_block.push_back(0x00);
                data_block.push_back(0x00);
                data_blocks.push_back(data_block);
                cur_off += static_cast<uint32_t>(data_block.size());
            }

            // 写入TKEY条目 (偏移 + 哈希)
            for (size_t i = 0; i < table.entries.size(); ++i) {
                const auto& entry = table.entries[i];

                // 写入偏移
                file.write(reinterpret_cast<const char*>(&str_offsets[i]), sizeof(str_offsets[i]));

                // 使用GTA4哈希算法计算键的哈希值
                std::string normalizedKey = normalizeKey(entry.key);
                uint32_t hash_val = calculateGTA4Hash(normalizedKey);
                file.write(reinterpret_cast<const char*>(&hash_val), sizeof(hash_val));
            }

            // 写入TDAT块头
            file.write("TDAT", 4);
            uint32_t tdat_data_size = 0;
            for (const auto& data_block : data_blocks) {
                tdat_data_size += static_cast<uint32_t>(data_block.size());
            }
            file.write(reinterpret_cast<const char*>(&tdat_data_size), sizeof(tdat_data_size));

            // 写入TDAT数据
            for (const auto& data_block : data_blocks) {
                file.write(reinterpret_cast<const char*>(data_block.data()), data_block.size());
            }
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

