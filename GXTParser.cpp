#include "GXTParser.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <functional>

// 添加日志回调函数指针
static std::function<void(const std::string&)> g_logCallback = nullptr;

// 设置日志回调函数
void GXTParser::setLogCallback(std::function<void(const std::string&)> callback) {
    g_logCallback = callback;
}

// 添加日志输出函数
static void addParseLog(const std::string& msg) {
    if (g_logCallback) {
        g_logCallback(msg);
    }
}

// 获取版本名称的辅助函数
static std::string getVersionName(GXTVersion version) {
    switch (version) {
        case GXTVersion::GTA_III: return "GTA III";
        case GXTVersion::GTA_VC: return "GTA Vice City";
        case GXTVersion::GTA_SA: return "GTA San Andreas";
        case GXTVersion::GTA_IV: return "GTA IV";
        case GXTVersion::GXT2: return "GXT2";
        default: return "Unknown";
    }
}

uint16_t GXTParser::read_u16(FILE* f) const {
    uint16_t v = 0;
    fread(&v, 2, 1, f);
    return v;
}

uint32_t GXTParser::read_u32(FILE* f) const {
    uint32_t v = 0;
    fread(&v, 4, 1, f);
    return v;
}

std::wstring GXTParser::read_wstring(FILE* f) const {
    std::wstring w;
    wchar_t ch;
    while (fread(&ch, sizeof(wchar_t), 1, f) == 1 && ch != 0)
        w.push_back(ch);
    return w;
}

std::string GXTParser::cp1252_to_utf8(const std::string& s) const {
    if (s.empty()) return {};
    
    // 首先尝试使用MultiByteToWideChar进行转换
    int wide_len = MultiByteToWideChar(1252, 0, s.data(), (int)s.size(), nullptr, 0);
    if (wide_len == 0) {
        // 如果CP1252转换失败，尝试使用ISO-8859-1
        wide_len = MultiByteToWideChar(28591, 0, s.data(), (int)s.size(), nullptr, 0);
        if (wide_len == 0) return {};
        
        std::wstring w(wide_len, 0);
        MultiByteToWideChar(28591, 0, s.data(), (int)s.size(), &w[0], wide_len);
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, nullptr, 0, nullptr, nullptr);
        if (utf8_len == 0) return {};
        std::string out(utf8_len, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, &out[0], utf8_len, nullptr, nullptr);
        return out;
    }
    
    std::wstring w(wide_len, 0);
    MultiByteToWideChar(1252, 0, s.data(), (int)s.size(), &w[0], wide_len);
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, nullptr, 0, nullptr, nullptr);
    if (utf8_len == 0) return {};
    std::string out(utf8_len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, &out[0], utf8_len, nullptr, nullptr);
    return out;
}

std::string GXTParser::gbk_to_utf8(const std::string& s) const {
    if (s.empty()) return {};
    
    int wide_len = MultiByteToWideChar(936, 0, s.data(), (int)s.size(), nullptr, 0);
    if (wide_len == 0) return {};
    
    std::wstring w(wide_len, 0);
    MultiByteToWideChar(936, 0, s.data(), (int)s.size(), &w[0], wide_len);
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, nullptr, 0, nullptr, nullptr);
    if (utf8_len == 0) return {};
    std::string out(utf8_len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, &out[0], utf8_len, nullptr, nullptr);
    return out;
}

std::string GXTParser::utf16le_to_utf8(const std::string& s) const {
    if (s.empty() || s.size() % 2 != 0) return {};
    
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (wide_len == 0) return {};
    
    std::wstring w(wide_len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], wide_len);
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, nullptr, 0, nullptr, nullptr);
    if (utf8_len == 0) return {};
    std::string out(utf8_len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, &out[0], utf8_len, nullptr, nullptr);
    return out;
}

std::string GXTParser::decode_with_encoding(const std::string& raw, int encoding) const {
    if (raw.empty()) return {};
    
    switch (encoding) {
        case 1: // UTF-8
            return raw;
        case 2: // CP1252
            return cp1252_to_utf8(raw);
        case 3: // GBK
            return gbk_to_utf8(raw);
        case 4: // UTF-16LE
            return utf16le_to_utf8(raw);
        default: // Auto
            return guess_decode(raw);
    }
}

// 快速UTF-8 验证（简单高性能实现）
static bool is_valid_utf8(const char* s, size_t len) {
    const unsigned char* bytes = (const unsigned char*)s;
    size_t i = 0;
    while (i < len) {
        unsigned char b = bytes[i];
        if (b <= 0x7F) { // ASCII
            i++;
            continue;
        } else if ((b >> 5) == 0x6) { // 110x xxxx, 2 bytes
            if (i + 1 >= len) return false;
            if ((bytes[i+1] >> 6) != 0x2) return false;
            i += 2;
        } else if ((b >> 4) == 0xE) { // 1110 xxxx, 3 bytes
            if (i + 2 >= len) return false;
            if ((bytes[i+1] >> 6) != 0x2 || (bytes[i+2] >> 6) != 0x2) return false;
            i += 3;
        } else if ((b >> 3) == 0x1E) { // 11110 xxx, 4 bytes
            if (i + 3 >= len) return false;
            if ((bytes[i+1] >> 6) != 0x2 || (bytes[i+2] >> 6) != 0x2 || (bytes[i+3] >> 6) != 0x2) return false;
            i += 4;
        } else return false;
    }
    return true;
}

// 尝试多种编码：优先UTF-8，其次CP1252，然后ISO-8859-1，最后回退GBK
std::string GXTParser::guess_decode(const std::string& raw) const {
    if (raw.empty()) return {};

    // 快速ASCII / UTF-8 快路
    bool all_ascii = true;
    for (unsigned char c : raw) if (c >= 0x80) { all_ascii = false; break; }
    if (all_ascii) return raw; // ASCII 即为有效 UTF-8

    if (is_valid_utf8(raw.data(), raw.size()))
        return raw; // 已是 UTF-8

    // 第一优先：CP1252 (Windows Latin)
    {
        int wide_len = MultiByteToWideChar(1252, 0, raw.data(), (int)raw.size(), nullptr, 0);
        if (wide_len > 0) {
            std::wstring w(wide_len, 0);
            MultiByteToWideChar(1252, 0, raw.data(), (int)raw.size(), &w[0], wide_len);
            int utf8_len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, nullptr, 0, nullptr, nullptr);
            std::string out(utf8_len, 0);
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, &out[0], utf8_len, nullptr, nullptr);
            return out;
        }
    }

    // 第二优先：ISO-8859-1 (Latin-1)
    {
        int wide_len = MultiByteToWideChar(28591, 0, raw.data(), (int)raw.size(), nullptr, 0);
        if (wide_len > 0) {
            std::wstring w(wide_len, 0);
            MultiByteToWideChar(28591, 0, raw.data(), (int)raw.size(), &w[0], wide_len);
            int utf8_len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, nullptr, 0, nullptr, nullptr);
            std::string out(utf8_len, 0);
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, &out[0], utf8_len, nullptr, nullptr);
            return out;
        }
    }

    // 第三优先：GBK（仅当以上编码失败）
    {
        int wide_len = MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, raw.data(), (int)raw.size(), nullptr, 0);
        if (wide_len > 0) {
            std::wstring w(wide_len, 0);
            MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, raw.data(), (int)raw.size(), &w[0], wide_len);
            int utf8_len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, nullptr, 0, nullptr, nullptr);
            std::string out(utf8_len, 0);
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wide_len, &out[0], utf8_len, nullptr, nullptr);
            return out;
        }
    }

    return raw;
}


void GXTParser::makeDir(const std::string& path) const {
    CreateDirectoryA(path.c_str(), nullptr);
}

//-------------------------------------
// 辅助：uint32 转十六进制字符串
//-------------------------------------
static std::string u32_to_hex(uint32_t v) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << v;
    return ss.str();
}

// 专门解析 GTA5 GXT2 格式
bool GXTParser::parseGXT2(FILE* f, std::vector<GXTTabl>& outTables) {
    // 设置检测到的版本
    this->detectedVersion = GXTVersion::GXT2;
    addParseLog("检测到版本: " + getVersionName(GXTVersion::GXT2));
    
    // 读取整个文件到内存
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 8) return false;
    fseek(f, 0, SEEK_SET);
    std::vector<char> data((size_t)fsize);
    if (fread(data.data(), 1, (size_t)fsize, f) != (size_t)fsize) return false;

    // 检查头
    if (fsize < 8 || data[0] != '2' || data[1] != 'T' || data[2] != 'X' || data[3] != 'G') return false;
    uint32_t num_entries = *(uint32_t*)(data.data() + 4);
    size_t pos = 8;
    std::vector<std::pair<uint32_t,uint32_t>> entries;
    for (uint32_t i = 0; i < num_entries; ++i) {
        if (pos + 8 > data.size()) return false;
        uint32_t h = *(uint32_t*)(data.data() + pos);
        uint32_t offset = *(uint32_t*)(data.data() + pos + 4);
        pos += 8;
        entries.emplace_back(h, offset);
    }
    // 第二头部验证
    if (pos + 8 > data.size()) return false;
    if (!(data[pos] == '2' && data[pos+1] == 'T' && data[pos+2] == 'X' && data[pos+3] == 'G')) return false;
    // uint32_t second_val = *(uint32_t*)(data.data() + pos + 4);
    pos += 8; // now pos points to start of string area (string_start)

    GXTTabl tabl;
    tabl.name = "GXT2";
    for (auto &ent : entries) {
        uint32_t h = ent.first;
        uint32_t off = ent.second;
        std::string val;
        if (off < data.size()) {
            size_t p = off;
            while (p < data.size() && data[p] != 0) {
                val.push_back(data[p]);
                ++p;
            }
            // 使用guess_decode函数替代原来的UTF-8验证和CP1252转换
            val = guess_decode(val);
        }
        std::ostringstream keyss;
        keyss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << h;
        tabl.entries.push_back({keyss.str(), val});
    }
    outTables.push_back(std::move(tabl));
    return !outTables.empty();
}

//-------------------------------------
// 主解析函数
//-------------------------------------
bool GXTParser::parse(const std::string& filePath) {
    addParseLog("开始解析文件 " + filePath);
    
    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "rb");
    if (!f) {
        addParseLog("无法打开文件");
        return false;
    }

    tables.clear();
    // 重置检测到的版本
    detectedVersion = GXTVersion::UNKNOWN;

    // === 检测GTA5 GXT2 ===
    fseek(f, 0, SEEK_SET);
    uint8_t head[8] = {}; // 读取8个字节用于版本检测
    if (fread(head, 1, 8, f) == 8) {
        // 优先检测GTA5 GXT2格式（最独特的格式）
        if (head[0] == '2' && head[1] == 'T' && head[2] == 'X' && head[3] == 'G') {
            addParseLog("检测到GTA5 GXT2格式");
            this->detectedVersion = GXTVersion::GXT2;
            bool ok = parseGXT2(f, tables);
            fclose(f);
            return ok;
        }
    }

    // === 检测其他格式 ===
    fseek(f, 0, SEEK_SET);
    bool isSA = false;
    int bitsPerChar = 8;
    if (fread(head, 1, 8, f) == 8) {
        // 检查IV版本特征：头4字节为version和bits_per_char
        uint16_t version = head[0] | (head[1] << 8);
        uint16_t bits = head[2] | (head[3] << 8);
        if (version == 4 && bits == 16) {
            isSA = true;
            bitsPerChar = bits;
            addParseLog("检测到GTA IV格式，版本 " + std::to_string(version) + ", 字符位数: " + std::to_string(bits));
            this->detectedVersion = GXTVersion::GTA_IV;
        } 
        // 检查SA版本特征：前4字节为version和bits，后4字节为'TABL'
        else if (version == 4 && (bits == 8 || bits == 16) && 
                 head[4] == 'T' && head[5] == 'A' && head[6] == 'B' && head[7] == 'L') {
            isSA = true;
            bitsPerChar = bits;
            addParseLog("检测到GTA SA格式，版本 " + std::to_string(version) + ", 字符位数: " + std::to_string(bits));
            this->detectedVersion = GXTVersion::GTA_SA;
        }
        // 检查VC版本特征：直接以'TABL'开头
        else if (head[0] == 'T' && head[1] == 'A' && head[2] == 'B' && head[3] == 'L') {
            addParseLog("检测到GTA VC格式");
            this->detectedVersion = GXTVersion::GTA_VC;
        }
        // 检查III版本特征：直接以'TKEY'开头
        else if (head[0] == 'T' && head[1] == 'K' && head[2] == 'E' && head[3] == 'Y') {
            addParseLog("检测到GTA III格式");
            this->detectedVersion = GXTVersion::GTA_III;
        }
        else {
            addParseLog("未识别的版本格式，版本 " + std::to_string(version) + ", 字符位数: " + std::to_string(bits));
        }
    }
    fseek(f, isSA ? 4 : 0, SEEK_SET);

    // === 主循环 ===
    int tableCount = 0;
    while (!feof(f)) {
        char tag[5] = {0};
        if (fread(tag, 1, 4, f) != 4) break;
        uint32_t size = read_u32(f);
        long blockStart = ftell(f);
        
        addParseLog("发现 " + std::string(tag) + ", 大小: " + std::to_string(size));

        // === 检测无 TABL 格式（直接以 TKEY 开头） ===
        if (strncmp(tag, "TKEY", 4) == 0) {
            this->originalHasTABL = false;
            addParseLog("检测到无TABL格式文件");
            
            // 改进版本识别逻辑：优先检查IV版本特征，避免误判
            bool isIV = (size % 8 == 0) && isSA && (bitsPerChar == 16);
            bool isIII = !isIV && (size % 12 == 0);
            
            addParseLog("版本检测结果- IV: " + std::string(isIV ? "是" : "否") + 
                       ", III: " + std::string(isIII ? "是" : "否"));

            if (isIII) {
                this->detectedVersion = GXTVersion::GTA_III;
                addParseLog("确定版本: " + getVersionName(GXTVersion::GTA_III));
                struct IIIEntry { uint32_t offset; char key[8]; };
                std::vector<IIIEntry> entries(size / 12);
                fread(entries.data(), sizeof(IIIEntry), entries.size(), f);

                fseek(f, blockStart + size, SEEK_SET);
                char tag2[5] = {0};
                fread(tag2, 1, 4, f);
                if (strncmp(tag2, "TDAT", 4) != 0) {
                    addParseLog("缺少TDAT块，解析失败");
                    break;
                }
                uint32_t tdatSize = read_u32(f);
                
                // 一次性读取所有TDAT数据到内存中
                std::unique_ptr<uint8_t[]> tdatBuf(new uint8_t[tdatSize]);
                fread(tdatBuf.get(), 1, tdatSize, f);

                const uint16_t* arr = reinterpret_cast<const uint16_t*>(tdatBuf.get());
                size_t arrLen = tdatSize / 2;
                
                // 查找所有终止符位置
                std::vector<size_t> zeroIdx;
                for (size_t i = 0; i < arrLen; ++i) {
                    if (arr[i] == 0) {
                        zeroIdx.push_back(i);
                    }
                }

                GXTTabl tabl; tabl.name = "MAIN";
                addParseLog("开始解析MAIN表，共 " + std::to_string(entries.size()) + " 个条目");
                
                for (auto& e : entries) {
                    std::string key(e.key, 8);
                    key.erase(std::find(key.begin(), key.end(), '\0'), key.end());

                    size_t off = e.offset / 2;
                    // 安全处理起始位置
                    off = (off < arrLen) ? off : arrLen;
                    
                    // 查找终止符位置
                    size_t end = arrLen;
                    for (size_t z : zeroIdx) {
                        if (z > off) {
                            end = z;
                            break;
                        }
                    }

                    std::string val;
                    if (end > off) {
                        // 使用UTF-16LE解码
                        int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)(arr + off),
                                                    (int)(end - off), nullptr, 0, nullptr, nullptr);
                        if (len > 0) {
                            val.resize(len);
                            WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)(arr + off),
                                                (int)(end - off), &val[0], len, nullptr, nullptr);
                        }
                    }
                    // 对于GTA_III格式，key和originalKey都使用原始字符串键
                    tabl.entries.push_back({key, val, key});
                }
                tables.push_back(std::move(tabl));
                addParseLog("MAIN表解析完成，共 " + std::to_string(tabl.entries.size()) + " 个条目");
                break;
            }

            // 重写IV版本解析逻辑，与Python版本保持一致
            if (isIV) {
                this->detectedVersion = GXTVersion::GTA_IV;
                addParseLog("确定版本: " + getVersionName(GXTVersion::GTA_IV));
                // 读取TKEY数据
                std::vector<uint32_t> tkeyData(size / sizeof(uint32_t));
                fread(tkeyData.data(), sizeof(uint32_t), tkeyData.size(), f);
                
                // 重新定位到TDAT
                fseek(f, blockStart + size, SEEK_SET);
                char tag2[5] = {0};
                fread(tag2, 1, 4, f);
                if (strncmp(tag2, "TDAT", 4) != 0) {
                    addParseLog("缺少TDAT块，解析失败");
                    break;
                }
                uint32_t tdatSize = read_u32(f);
                
                // 一次性读取所有TDAT数据到内存中
                std::unique_ptr<uint8_t[]> tdatBuf(new uint8_t[tdatSize]);
                fread(tdatBuf.get(), 1, tdatSize, f);

                // IV版本6位字符
                const uint16_t* data16 = reinterpret_cast<const uint16_t*>(tdatBuf.get());
                size_t data16_len = tdatSize / 2;

                // 预先收集所有空终止符的位置，用于快速查找
                std::vector<size_t> null_positions;
                for (size_t i = 0; i < data16_len; ++i) {
                    if (data16[i] == 0) {
                        null_positions.push_back(i);
                    }
                }

                GXTTabl tabl;
                tabl.name = "MAIN";
                addParseLog("开始解析MAIN表，共 " + std::to_string(tkeyData.size()/2) + " 个条目");

                // 每个TKEY条目包含offset和crc两个字节
                size_t entry_count = tkeyData.size() / 2;
                for (size_t i = 0; i < entry_count; ++i) {
                    uint32_t offset = tkeyData[i * 2];
                    uint32_t crc = tkeyData[i * 2 + 1];

                    // 偏移是按字节计算的，转换为16位索引
                    size_t start_idx = offset / 2;

                    // 确保起始位置有效
                    if (start_idx >= data16_len) {
                        addParseLog("警告: 条目偏移超出范围，跳过");
                        continue;
                    }

                    // 查找下一个空终止符
                    size_t end_idx = data16_len;
                    for (size_t pos : null_positions) {
                        if (pos > start_idx) {
                            end_idx = pos;
                            break;
                        }
                    }

                    std::string text;
                    if (end_idx > start_idx) {
                        // 提取u16列表
                        std::vector<uint16_t> u16_list;
                        for (size_t j = start_idx; j < end_idx; ++j) {
                            u16_list.push_back(data16[j]);
                        }
                            
                        // 确保终止
                        if (u16_list.empty() || u16_list.back() != 0) {
                            u16_list.push_back(0);
                        }
                            
                        if (!u16_list.empty() && u16_list.back() == 0) {
                            u16_list.pop_back();
                        }

                        // 尝试UTF-16LE解码
                        if (!u16_list.empty()) {
                            const uint16_t* entry_data = u16_list.data();
                            size_t entry_len = u16_list.size();

                            // 检查是否所有字符都在0-255范围内（"fake 16-bit"）
                            bool isFake16Bit = true;
                            for (size_t k = 0; k < entry_len; ++k) {
                                if (entry_data[k] > 0xFF) {
                                    isFake16Bit = false;
                                    break;
                                }
                            }

                            if (isFake16Bit) {
                                // 解释为单字节序列并用 guess_decode 处理
                                addParseLog("检测到Fake 16-bit编码，使用guess_decode解码条目 0x" + u32_to_hex(crc));
                                std::string raw;
                                raw.reserve(entry_len);
                                for (size_t k = 0; k < entry_len; ++k) {
                                    raw.push_back(static_cast<char>(entry_data[k] & 0xFF));
                                }
                                text = this->guess_decode(raw);
                            } else {
                                // 视为真正UTF-16LE 并转换为 UTF-8
                                addParseLog("检测到UTF-16LE编码，直接转换条目0x" + u32_to_hex(crc));
                                int utf8_size = WideCharToMultiByte(CP_UTF8, 0, 
                                                                  reinterpret_cast<LPCWCH>(entry_data),
                                                                  static_cast<int>(entry_len),
                                                                  nullptr, 0, nullptr, nullptr);
                                if (utf8_size > 0) {
                                    text.resize(utf8_size);
                                    WideCharToMultiByte(CP_UTF8, 0,
                                                      reinterpret_cast<LPCWCH>(entry_data),
                                                      static_cast<int>(entry_len),
                                                      &text[0], utf8_size, nullptr, nullptr);
                                } else {
                                    // 容错：如果UTF-16LE转换失败，回退到guess_decode
                                    addParseLog("UTF-16LE转换失败，回退到guess_decode条目 0x" + u32_to_hex(crc));
                                    std::string raw(reinterpret_cast<const char*>(entry_data), entry_len * sizeof(uint16_t));
                                    text = this->guess_decode(raw);
                                }
                            }
                        }
                    }

                    char key_buffer[16];
                    snprintf(key_buffer, sizeof(key_buffer), "0x%08X", crc);
                    tabl.entries.push_back({std::string(key_buffer), text});
                }

                tables.push_back(std::move(tabl));
                addParseLog("MAIN表解析完成，共 " + std::to_string(tabl.entries.size()) + " 个条目");
                break;
            }
        }

        // 如果版本仍未确定，尝试根据块结构推断
        if (this->detectedVersion == GXTVersion::UNKNOWN) {
            if (strncmp(tag, "TABL", 4) == 0) {
                // 有TABL块，可能是VC格式
                this->detectedVersion = GXTVersion::GTA_VC;
                addParseLog("未明确检测到版本，根据TABL块推断为GTA VC格式");
            } else if (strncmp(tag, "TKEY", 4) == 0) {
                // 无TABL直接TKEY，根据大小判断
                if (size % 12 == 0) {
                    this->detectedVersion = GXTVersion::GTA_III;
                    addParseLog("未明确检测到版本，根据TKEY块大小推断为GTA III格式");
                } else if (size % 8 == 0) {
                    this->detectedVersion = GXTVersion::GTA_IV;
                    addParseLog("未明确检测到版本，根据TKEY块大小推断为GTA IV格式");
                }
            }
        }

        if (strncmp(tag, "TABL", 4) != 0) {
            fseek(f, size, SEEK_CUR);
            continue;
        }

        // === TABL 格式 ===
        this->originalHasTABL = true;
        addParseLog("检测到TABL格式文件");
        
        struct TblEntry { char name[8]; uint32_t offset; };
        std::vector<TblEntry> tabs;
        long tablEnd = blockStart + size;
        
        // 读取所有表条目
        fseek(f, blockStart, SEEK_SET);
        while (ftell(f) < tablEnd) {
            TblEntry t;
            if (fread(&t, sizeof(t), 1, f) != 1) break;
            tabs.push_back(t);
        }
        
        addParseLog("发现 " + std::to_string(tabs.size()) + " 个表");

        for (auto& te : tabs) {
            tableCount++;
            std::string name(te.name, 8);
            name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
            GXTTabl tabl; tabl.name = name;
            
            addParseLog("开始解析表 #" + std::to_string(tableCount) + " (" + name + ")");

            // 定位到表的TKEY
            long absPos = (long)te.offset;
            fseek(f, absPos, SEEK_SET);
            
            // 检查是否需要跳过字节的表名
            char peek8[8] = {0};
            size_t got = fread(peek8, 1, 8, f);
            fseek(f, -(long)got, SEEK_CUR);

            bool hasTableName = false;
            if (got >= 4 && strncmp(peek8, "TKEY", 4) != 0) {
                // 字节表名，跳过
                fseek(f, 8, SEEK_CUR);
                hasTableName = true;
            }
            
            if (hasTableName) {
                addParseLog("表包含字节表名");
            }

            // === 读取 TKEY ===
            char tkeyTag[5] = {0};
            fread(tkeyTag, 1, 4, f);
            if (strncmp(tkeyTag, "TKEY", 4) != 0) {
                addParseLog("TKEY块缺失，跳过此表");
                continue;
            }
            uint32_t tkeySize = read_u32(f);
            long tkeyStart = ftell(f);
            long tkeyEnd = tkeyStart + tkeySize;
            
            addParseLog("TKEY块大小 " + std::to_string(tkeySize));
            
            // 通过TKEY条目大小判断是否为VC版本
            // VC版本条目大小为2字节（offset(4) + key(8)）
            // SA/IV版本条目大小为字节（offset(4) + crc(4)）
            bool isVC = ((tkeySize % 12) == 0) && (this->detectedVersion == GXTVersion::GTA_VC || this->detectedVersion == GXTVersion::UNKNOWN);
            bool isSAVersion = !isVC && ((tkeySize % 8) == 0) && (this->detectedVersion == GXTVersion::GTA_SA || this->detectedVersion == GXTVersion::UNKNOWN);
            
            addParseLog("版本检测结果- VC: " + std::string(isVC ? "是" : "否") + 
                       ", SA: " + std::string(isSAVersion ? "是" : "否"));

            if (isVC) {
                this->detectedVersion = GXTVersion::GTA_VC;
                addParseLog("确定版本: " + getVersionName(GXTVersion::GTA_VC));
                // VC版本: offset(uint32) + key(8 bytes string)
                struct VCEntry { uint32_t offset; char key[8]; };
                std::vector<VCEntry> entries(tkeySize / 12);
                fread(entries.data(), sizeof(VCEntry), entries.size(), f);

                // === TDAT ===
                char tag2[5] = {0};
                fread(tag2, 1, 4, f);
                if (strncmp(tag2, "TDAT", 4) != 0) {
                    addParseLog("缺少TDAT块，跳过此表");
                    continue;
                }
                uint32_t tdatSize = read_u32(f);
                
                // 一次性读取所有TDAT数据到内存中
                std::unique_ptr<uint8_t[]> TDat(new uint8_t[tdatSize]);
                fread(TDat.get(), 1, tdatSize, f);

                const uint16_t* arr = reinterpret_cast<const uint16_t*>(TDat.get());
                size_t arrLen = tdatSize / 2;
                
                // 查找所有终止符位置
                std::vector<size_t> zeroIdx;
                for (size_t i = 0; i < arrLen; ++i) {
                    if (arr[i] == 0) {
                        zeroIdx.push_back(i);
                    }
                }
                
                addParseLog("开始解码 " + std::to_string(entries.size()) + " 个条目");
                
                for (auto& e : entries) {
                    std::string key(e.key, 8);
                    key.erase(std::find(key.begin(), key.end(), '\0'), key.end());
                    
                    size_t off = e.offset / 2;
                    // 安全处理起始位置
                    off = (off < arrLen) ? off : arrLen;
                    
                    // 查找终止符位置
                    size_t end = arrLen;
                    for (size_t z : zeroIdx) {
                        if (z > off) {
                            end = z;
                            break;
                        }
                    }

                    std::string val;
                    if (end > off) {
                        int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)(arr + off),
                                                    (int)(end - off), nullptr, 0, nullptr, nullptr);
                        if (len > 0) {
                            val.resize(len);
                            WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)(arr + off),
                                                (int)(end - off), &val[0], len, nullptr, nullptr);
                        }
                    }
                    // 对于GTA_VC格式，key和originalKey都使用原始字符串键
                    tabl.entries.push_back({key, val, key});
                }
            } else {
                // SA/IV版本: offset(uint32) + crc(uint32)
                // 解析偏移与CRC
                std::vector<uint32_t> offsets, crcs;
                offsets.reserve(tkeySize / 8);
                crcs.reserve(tkeySize / 8);
                for (uint32_t i = 0; i < tkeySize / 8; ++i) {
                    uint32_t off = read_u32(f);
                    uint32_t crc = read_u32(f);
                    offsets.push_back(off);
                    crcs.push_back(crc);
                }

                // === TDAT ===
                char tag2[5] = {0};
                fread(tag2, 1, 4, f);
                if (strncmp(tag2, "TDAT", 4) != 0) {
                    addParseLog("缺少TDAT块，跳过此表");
                    continue;
                }
                uint32_t tdatSize = read_u32(f);
                
                // 一次性读取所有TDAT数据到内存中
                std::unique_ptr<uint8_t[]> TDat(new uint8_t[tdatSize]);
                fread(TDat.get(), 1, tdatSize, f);

                // === 解析字符 ===
                if (bitsPerChar == 16) {
                    this->detectedVersion = GXTVersion::GTA_IV;
                    addParseLog("确定版本: " + getVersionName(GXTVersion::GTA_IV) + " (16位字符)");
                    // SA 16-bit 版本处理逻辑 (实际上对应IV版本)
                    const uint16_t* arr = reinterpret_cast<const uint16_t*>(TDat.get());
                    size_t arrLen = tdatSize / 2;
                    
                    // 查找所有终止符位置
                    std::vector<size_t> zeroIdx;
                    for (size_t i = 0; i < arrLen; ++i) {
                        if (arr[i] == 0) {
                            zeroIdx.push_back(i);
                        }
                    }
                    
                    addParseLog("开始解码 " + std::to_string(offsets.size()) + " 个条目");
                    
                    for (size_t i = 0; i < offsets.size(); ++i) {
                        size_t start = offsets[i] / 2;
                        // 安全处理起始位置
                        start = (start < arrLen) ? start : arrLen;
                        
                        // 查找终止符位置
                        size_t end = arrLen;
                        for (size_t z : zeroIdx) {
                            if (z > start) {
                                end = z;
                                break;
                            }
                        }

                        std::string val;
                        if (end > start) {
                            // 修复：对于GTA IV，强制使用UTF-16LE解码，不再回退到guess_decode
                            int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)(arr + start),
                                                        (int)(end - start), nullptr, 0, nullptr, nullptr);
                            if (len > 0) {
                                val.resize(len);
                                WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)(arr + start),
                                                    (int)(end - start), &val[0], len, nullptr, nullptr);
                            }
                        }
                        tabl.entries.push_back({u32_to_hex(crcs[i]), val});
                    }

                } else {
                    this->detectedVersion = GXTVersion::GTA_SA;
                    addParseLog("确定版本: " + getVersionName(GXTVersion::GTA_SA) + " (8位字符)");
                    // SA 8-bit 版本处理逻辑 - 完全重写，参考Python算法
                    
                    // 预先收集所有空终止符的位置，用于快速查找
                    std::vector<size_t> zeroIdx;
                    for (size_t i = 0; i < tdatSize; ++i) {
                        if (TDat.get()[i] == 0) {
                            zeroIdx.push_back(i);
                        }
                    }
                    
                    addParseLog("开始解码 " + std::to_string(offsets.size()) + " 个条目");
                    
                    // 使用更高效的终止符查找算法
                    for (size_t i = 0; i < offsets.size(); ++i) {
                        size_t start = offsets[i];
                        // 安全处理起始位置
                        start = (start < tdatSize) ? start : tdatSize;
                        
                        // 查找终止符位置- 使用二分查找提高效率
                        size_t end = tdatSize;
                        if (!zeroIdx.empty()) {
                            // 使用二分查找找到第一个大于start的终止符位置
                            auto it = std::upper_bound(zeroIdx.begin(), zeroIdx.end(), start);
                            if (it != zeroIdx.end()) {
                                end = *it;
                            }
                        }

                        std::string val;
                        if (end > start) {
                            // 提取原始字节数据
                            std::string raw(reinterpret_cast<const char*>(TDat.get() + start), end - start);
                            
                            // 参考Python算法的解码逻辑：UTF-8优先，失败则尝试GBK->CP1252->UTF-8
                            try {
                                // 首先尝试UTF-8严格解码
                                if (is_valid_utf8(raw.data(), raw.size())) {
                                    val = raw;
                                    addParseLog("使用UTF-8解码条目 0x" + u32_to_hex(crcs[i]));
                                } else {
                                    throw std::runtime_error("Invalid UTF-8");
                                }
                            } catch (...) {
                                try {
                                    // 如果UTF-8失败，尝试GBK -> CP1252 -> UTF-8的转换
                                    // 参考Python：gbk_bytes = raw.decode('gbk', errors='strict').encode('cp1252', errors='replace')
                                    // 这里简化：直接使用CP1252转换
                                    val = this->cp1252_to_utf8(raw);
                                    addParseLog("使用CP1252解码条目 0x" + u32_to_hex(crcs[i]));
                                } catch (...) {
                                    // 最后回退到guess_decode
                                    val = this->guess_decode(raw);
                                    addParseLog("使用自动检测解码条目 0x" + u32_to_hex(crcs[i]));
                                }
                            }
                            
                            // 去除末尾的\x00字符
                            val.erase(std::find(val.begin(), val.end(), '\0'), val.end());
                        }
                        tabl.entries.push_back({u32_to_hex(crcs[i]), val});
                    }
                }
            }
            tables.push_back(std::move(tabl));
            addParseLog("表#" + std::to_string(tableCount) + " (" + name + ") 解析完成，共 " + std::to_string(tabl.entries.size()) + " 个条目");
        }
    }

    fclose(f);
    
    // 最终版本确定：如果版本仍为UNKNOWN，根据解析结果推断
    if (this->detectedVersion == GXTVersion::UNKNOWN && !tables.empty()) {
        if (this->originalHasTABL) {
            // 有TABL块，默认为VC格式
            this->detectedVersion = GXTVersion::GTA_VC;
            addParseLog("根据TABL块存在，最终推断为GTA VC格式");
        } else {
            // 无TABL块，根据表结构推断
            if (tables.size() == 1 && tables[0].name == "MAIN") {
                // 单表MAIN，可能是III或IV格式
                if (!tables[0].entries.empty() && tables[0].entries[0].key.find("0x") == 0) {
                    // 键以0x开头，可能是IV格式（哈希键）
                    this->detectedVersion = GXTVersion::GTA_IV;
                    addParseLog("根据哈希键格式，最终推断为GTA IV格式");
                } else {
                    // 普通键，可能是III格式
                    this->detectedVersion = GXTVersion::GTA_III;
                    addParseLog("根据普通键格式，最终推断为GTA III格式");
                }
            } else {
                // 多表结构，可能是SA格式
                this->detectedVersion = GXTVersion::GTA_SA;
                addParseLog("根据多表结构，最终推断为GTA SA格式");
            }
        }
    }
    
    addParseLog("文件解析完成，共解析 " + std::to_string(tables.size()) + " 个表");
    addParseLog("最终确定版本 " + getVersionName(this->detectedVersion));
    return !tables.empty();
}


void GXTParser::exportToTxt(const std::string& outDir) const {
    makeDir(outDir);

    // 如果解析时检测到原始文件包含 TABL，则保持写表头行为（即MAIN 也写 [MAIN]）
    // 只有当原始文件**没有** TABL（originalHasTABL == false）并且仅有一张MAIN 表时，才不写 [MAIN]
    bool isSingleMainNoTabl = (!this->originalHasTABL && tables.size() == 1 && tables[0].name == "MAIN");

    // 先创_ALL.txt（总是生成）
    std::ofstream all(outDir + "/_ALL.txt", std::ios::binary);
    if (!all.is_open()) {
        return;
    }

    if (isSingleMainNoTabl) {
        // III/IV 无TABL：导出同名txt（目录名stem），并且不写 [MAIN]
        std::filesystem::path dir(outDir);
        std::string stem = dir.stem().string();
        std::ofstream out(outDir + "/" + stem + ".txt", std::ios::binary);
        if (!out.is_open()) {
            return;
        }

        // 预分配缓冲区以提高性能
        size_t totalSize = 0;
        for (const auto& e : tables[0].entries) {
            totalSize += e.key.size() + 1 + e.value.size() + 2; // key + '=' + value + \r\n

        }
        
        std::string buffer;
        buffer.reserve(totalSize);
        
        for (const auto& e : tables[0].entries) {
            buffer.append(e.key);
            buffer.push_back('=');
            buffer.append(e.value);
            buffer.append("\r\n");
        }
        
        out.write(buffer.data(), buffer.size());
        all.write(buffer.data(), buffer.size());
        return;
    }

    // 有TABL 多表情形：为每个表生成独立文件
    for (const auto& t : tables) {
        std::filesystem::path filePath = std::filesystem::path(outDir) / (t.name + ".txt");
        std::ofstream out(filePath, std::ios::binary);
        if (!out.is_open()) {
            continue;
        }

        // 预分配缓冲区
        size_t totalSize = 0;
        if (this->originalHasTABL || t.name != "MAIN") {
            totalSize += t.name.size() + 4; // '[' + name + "]\r\n"
        }
        
        for (const auto& e : t.entries) {
            totalSize += e.key.size() + 1 + e.value.size() + 2; // key + '=' + value + \r\n

        }
        
        std::string buffer;
        buffer.reserve(totalSize);
        
        // 若原始文件没TABL 且只有一张MAIN，则上面分支已处理；
        // 这里决定是否MAIN 写头：仅当originalHasTABL == true 时，MAIN 写[MAIN]
        if (this->originalHasTABL || t.name != "MAIN") {
            buffer.append("[");
            buffer.append(t.name);
            buffer.append("]\r\n");
        }

        for (const auto& e : t.entries) {
            buffer.append(e.key);
            buffer.push_back('=');
            buffer.append(e.value);
            buffer.append("\r\n");
        }
        
        out.write(buffer.data(), buffer.size());
        all.write(buffer.data(), buffer.size());
    }
}


bool GXTParser::parseWHM(const std::string& filePath, std::vector<WHMEntry>& outEntries) const {
    addParseLog("开始解析WHM文件: " + filePath);
    
    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "rb");
    if (!f) {
        addParseLog("无法打开文件");
        return false;
    }

    // 读取条目数量
    uint32_t count = read_u32(f);
    addParseLog("WHM文件包含 " + std::to_string(count) + " 个条目");
    
    // 预分配entries向量
    std::vector<WHMEntry> entries;
    entries.reserve(count);
    
    // 读取条目
    for (uint32_t i = 0; i < count; i++) {
        WHMEntry entry;
        entry.hash = read_u32(f);
        entry.offset = read_u32(f);
        entries.push_back(std::move(entry));
    }

    // 读取blob大小
    uint32_t blob_size = read_u32(f);
    long blob_start = ftell(f);
    addParseLog("文本数据块大小 " + std::to_string(blob_size) + " 字节");

    // 读取所有文本数据（一次性读取以提高速度）
    std::unique_ptr<char[]> blob(new char[blob_size]);
    size_t readn = fread(blob.get(), 1, blob_size, f);
    if (readn != static_cast<size_t>(blob_size)) {
        addParseLog("实际读取文本数据大小: " + std::to_string(readn) + " 字节");
    }

    // 解析每个条目的文本（使用 memchr 查找终止符，避免逐字 push_back）
    int decodedCount = 0;
    for (auto& entry : entries) {
        if (static_cast<size_t>(entry.offset) < readn) {
            const char* ptr = blob.get() + entry.offset;
            size_t maxlen = readn - entry.offset;
            const char* nul = (const char*)memchr(ptr, 0, maxlen);
            size_t len = nul ? (size_t)(nul - ptr) : maxlen;
            if (len == 0) {
                entry.text.clear();
            } else {
                std::string raw;
                raw.assign(ptr, len);
                entry.text = this->guess_decode(raw);
                decodedCount++;
            }
        } else {
            entry.text = "[BINARY]";
        }
    }
    
    addParseLog("成功解码 " + std::to_string(decodedCount) + " 个条目");

    fclose(f);
    outEntries = std::move(entries);
    return true;
}

void GXTParser::exportWHMToTxt(const std::string& whmPath, const std::string& txtPath) const {
    std::vector<WHMEntry> entries;
    if (!parseWHM(whmPath, entries)) {
        std::cerr << "无法解析WHM文件" << whmPath << std::endl;
        return;
    }

    // 按哈希值排序以保持输出一致
    std::sort(entries.begin(), entries.end(), 
        [](const WHMEntry& a, const WHMEntry& b) { return a.hash < b.hash; });

    // 预分配输出缓冲（减少磁盘 I/O），换行使用 CRLF
    size_t total = 0;
    for (const auto& e : entries) {
        total += 2 + 8 + 1; // 0x + 8 hex digits + '='
        total += e.text.size();
        total += 2; // CRLF
    }
    std::string buf;
    buf.reserve(total + 16);

    char hexbuf[16];
    for (const auto& entry : entries) {
        // 快速格式化 0xXXXXXXXX
        int n = snprintf(hexbuf, sizeof(hexbuf), "0x%08X", entry.hash);
        buf.append(hexbuf, n);
        buf.push_back('=');
        buf.append(entry.text);
        buf.append("\r\n");
    }

    // 一次性写入
    std::ofstream out(txtPath, std::ios::binary);
    if (!out) {
        std::cerr << "无法创建输出文件" << txtPath << std::endl;
        return;
    }
    out.write(buf.data(), (std::streamsize)buf.size());
}

// DAT文件解析方法（完全独立于WHM）
// DAT格式：条目数(uint32) | 条目表(hash+offset) | blob大小(uint32) | 文本数据块
bool GXTParser::parseDAT(const std::string& filePath, std::vector<DATEntry>& outEntries) const {
    addParseLog("开始解析DAT文件: " + filePath);
    
    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "rb");
    if (!f) {
        addParseLog("无法打开文件");
        return false;
    }

    // 获取文件大小以便进行基本合法性检查，防止误把其它二进制文件当作 DAT 来解析
    std::fseek(f, 0, SEEK_END);
    long file_size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    // 读取条目数量
    uint32_t count = read_u32(f);
    addParseLog("DAT文件包含 " + std::to_string(count) + " 个条目");

    // 基本合理性检查：条目数与文件大小应匹配（每条目 8 字节），并且条目数不过大
    const uint64_t minExpected = 4ULL + static_cast<uint64_t>(count) * 8ULL + 4ULL; // count + entries + blob_size
    if (count == 0 || count > 2000000 || static_cast<uint64_t>(file_size) < minExpected) {
        addParseLog(std::string("DAT 文件头不合法或大小与条目数不匹配（可能不是 DAT 文件）。文件大小=") + std::to_string(file_size) + std::string(" 期望至少=") + std::to_string(minExpected));
        fclose(f);
        return false;
    }

    // 预分配entries向量
    std::vector<DATEntry> entries;
    try {
        entries.reserve(count);
    } catch (const std::bad_alloc&) {
        addParseLog("内存不足：无法为 DAT 条目分配空间");
        fclose(f);
        return false;
    }
    
    // 读取条目（hash + offset）
    for (uint32_t i = 0; i < count; i++) {
        DATEntry entry;
        entry.hash = read_u32(f);
        entry.offset = read_u32(f);
        entries.push_back(std::move(entry));
    }

    // 读取blob大小
    uint32_t blob_size = read_u32(f);
    long blob_start = ftell(f);
    addParseLog("文本数据块大小 " + std::to_string(blob_size) + " 字节");

    // 读取所有文本数据（一次性读取以提高速度）
    std::unique_ptr<char[]> blob(new char[blob_size]);
    size_t readn = fread(blob.get(), 1, blob_size, f);
    if (readn != static_cast<size_t>(blob_size)) {
        addParseLog("实际读取文本数据大小: " + std::to_string(readn) + " 字节");
    }

    // 解析每个条目的文本（使用 memchr 查找终止符）
    int decodedCount = 0;
    for (auto& entry : entries) {
        if (static_cast<size_t>(entry.offset) < readn) {
            const char* ptr = blob.get() + entry.offset;
            size_t maxlen = readn - entry.offset;
            const char* nul = (const char*)memchr(ptr, 0, maxlen);
            size_t len = nul ? (size_t)(nul - ptr) : maxlen;
            if (len == 0) {
                entry.text.clear();
            } else {
                std::string raw;
                raw.assign(ptr, len);
                entry.text = this->guess_decode(raw);
                decodedCount++;
            }
        } else {
            entry.text = "[BINARY]";
        }
    }
    
    addParseLog("成功解码 " + std::to_string(decodedCount) + " 个DAT条目");

    fclose(f);
    outEntries = std::move(entries);
    return true;
}

// DAT文件导出为文本
void GXTParser::exportDATToTxt(const std::string& datPath, const std::string& txtPath) const {
    std::vector<DATEntry> entries;
    if (!parseDAT(datPath, entries)) {
        addParseLog("无法解析DAT文件: " + datPath);
        return;
    }

    // 按哈希值排序以保持输出一致
    std::sort(entries.begin(), entries.end(), 
        [](const DATEntry& a, const DATEntry& b) { return a.hash < b.hash; });

    // 预分配输出缓冲（减少磁盘 I/O），换行使用 CRLF
    size_t total = 0;
    for (const auto& e : entries) {
        total += 2 + 8 + 1; // 0x + 8 hex digits + '='
        total += e.text.size();
        total += 2; // CRLF
    }
    std::string buf;
    buf.reserve(total + 16);

    char hexbuf[16];
    for (const auto& entry : entries) {
        // 快速格式化 0xXXXXXXXX
        int n = snprintf(hexbuf, sizeof(hexbuf), "0x%08X", entry.hash);
        buf.append(hexbuf, n);
        buf.push_back('=');
        buf.append(entry.text);
        buf.append("\r\n");
    }

    // 一次性写入
    std::ofstream out(txtPath, std::ios::binary);
    if (!out) {
        addParseLog("无法创建输出文件: " + txtPath);
        return;
    }
    out.write(buf.data(), (std::streamsize)buf.size());
    addParseLog("DAT导出完成: " + txtPath);
}