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
#include <set>
#include <QByteArray>

static std::function<void(const std::string&)> g_logCallback = nullptr;
static std::function<void(int, const std::string&)> g_progressCallback = nullptr;

void GXTParser::setLogCallback(std::function<void(const std::string&)> callback) {
    g_logCallback = callback;
}

void GXTParser::setProgressCallback(std::function<void(int, const std::string&)> callback) {
    g_progressCallback = callback;
}

static void addParseLog(const std::string& msg) {
    if (g_logCallback) {
        g_logCallback(msg);
    }
}

static void reportProgress(int percentage, const std::string& message) {
    if (g_progressCallback) {
        g_progressCallback(percentage, message);
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
    reportProgress(0, "正在打开文件...");
    
    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "rb");
    if (!f) {
        addParseLog("无法打开文件");
        return false;
    }

    tables.clear();
    detectedVersion = GXTVersion::UNKNOWN;

    reportProgress(5, "正在检测文件格式...");

    fseek(f, 0, SEEK_SET);
    uint8_t head[8] = {};
    if (fread(head, 1, 8, f) == 8) {
        if (head[0] == '2' && head[1] == 'T' && head[2] == 'X' && head[3] == 'G') {
            this->detectedVersion = GXTVersion::GXT2;
            reportProgress(10, "检测到GXT2格式，正在解析...");
            bool ok = parseGXT2(f, tables);
            fclose(f);
            if (ok) reportProgress(100, "解析完成");
            return ok;
        }
    }

    fseek(f, 0, SEEK_SET);
    bool isSA = false;
    int bitsPerChar = 8;
    if (fread(head, 1, 8, f) == 8) {
        uint16_t version = head[0] | (head[1] << 8);
        uint16_t bits = head[2] | (head[3] << 8);
        if (version == 4 && bits == 16) {
            isSA = true;
            bitsPerChar = bits;
            this->detectedVersion = GXTVersion::GTA_IV;
        } 
        else if (version == 4 && (bits == 8 || bits == 16) && 
                 head[4] == 'T' && head[5] == 'A' && head[6] == 'B' && head[7] == 'L') {
            isSA = true;
            bitsPerChar = bits;
            this->detectedVersion = GXTVersion::GTA_SA;
        }
        else if (head[0] == 'T' && head[1] == 'A' && head[2] == 'B' && head[3] == 'L') {
            this->detectedVersion = GXTVersion::GTA_VC;
        }
        else if (head[0] == 'T' && head[1] == 'K' && head[2] == 'E' && head[3] == 'Y') {
            this->detectedVersion = GXTVersion::GTA_III;
        }
    }
    fseek(f, isSA ? 4 : 0, SEEK_SET);

    reportProgress(10, "正在解析数据块...");

    int tableCount = 0;
    while (!feof(f)) {
        char tag[5] = {0};
        if (fread(tag, 1, 4, f) != 4) break;
        uint32_t size = read_u32(f);
        long blockStart = ftell(f);
        
        // === 检测无 TABL 格式（直接以 TKEY 开头） ===
        if (strncmp(tag, "TKEY", 4) == 0) {
            this->originalHasTABL = false;
            
            // 改进版本识别逻辑：优先检查IV版本特征，避免误判
            bool isIV = (size % 8 == 0) && isSA && (bitsPerChar == 16);
            bool isIII = !isIV && (size % 12 == 0);
            
            if (isIII) {
                this->detectedVersion = GXTVersion::GTA_III;
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

                // 无表文件，不创建MAIN表，直接存储到noTablEntries
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
                    noTablEntries.push_back({key, val, key});
                }
                break;
            }

            // 重写IV版本解析逻辑，与Python版本保持一致
            if (isIV) {
                this->detectedVersion = GXTVersion::GTA_IV;
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

                // 无表文件，不创建MAIN表，直接存储到noTablEntries

                // 每个TKEY条目包含offset和crc两个字节
                size_t entry_count = tkeyData.size() / 2;
                for (size_t i = 0; i < entry_count; ++i) {
                    uint32_t offset = tkeyData[i * 2];
                    uint32_t crc = tkeyData[i * 2 + 1];

                    // 偏移是按字节计算的，转换为16位索引
                    size_t start_idx = offset / 2;

                    // 确保起始位置有效
                    if (start_idx >= data16_len) {
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
                                std::string raw;
                                raw.reserve(entry_len);
                                for (size_t k = 0; k < entry_len; ++k) {
                                    raw.push_back(static_cast<char>(entry_data[k] & 0xFF));
                                }
                                text = this->guess_decode(raw);
                            } else {
                                // 视为真正UTF-16LE 并转换为 UTF-8
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
                                    std::string raw(reinterpret_cast<const char*>(entry_data), entry_len * sizeof(uint16_t));
                                    text = this->guess_decode(raw);
                                }
                            }
                        }
                    }

                    char key_buffer[16];
                    snprintf(key_buffer, sizeof(key_buffer), "0x%08X", crc);
                    noTablEntries.push_back({std::string(key_buffer), text});
                }

                break;
            }
        }

        // 如果版本仍未确定，尝试根据块结构推断
        if (this->detectedVersion == GXTVersion::UNKNOWN) {
            if (strncmp(tag, "TABL", 4) == 0) {
                this->detectedVersion = GXTVersion::GTA_VC;
            } else if (strncmp(tag, "TKEY", 4) == 0) {
                if (size % 12 == 0) {
                    this->detectedVersion = GXTVersion::GTA_III;
                } else if (size % 8 == 0) {
                    this->detectedVersion = GXTVersion::GTA_IV;
                }
            }
        }

        if (strncmp(tag, "TABL", 4) != 0) {
            fseek(f, size, SEEK_CUR);
            continue;
        }

        // === TABL 格式 ===
        this->originalHasTABL = true;
        
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

        for (auto& te : tabs) {
            tableCount++;
            std::string name(te.name, 8);
            name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
            GXTTabl tabl; tabl.name = name;
            
            int progress = 10 + (tableCount * 80 / static_cast<int>(tabs.size()));
            reportProgress(progress, "正在解析表: " + name);

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
            

            
            // 通过TKEY条目大小判断是否为VC版本
            // VC版本条目大小为2字节（offset(4) + key(8)）
            // SA/IV版本条目大小为字节（offset(4) + crc(4)）
            bool isVC = ((tkeySize % 12) == 0) && (this->detectedVersion == GXTVersion::GTA_VC || this->detectedVersion == GXTVersion::UNKNOWN);
            bool isSAVersion = !isVC && ((tkeySize % 8) == 0) && (this->detectedVersion == GXTVersion::GTA_SA || this->detectedVersion == GXTVersion::UNKNOWN);
            
            if (isVC) {
                this->detectedVersion = GXTVersion::GTA_VC;
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
                    // SA 8-bit 版本处理逻辑 - 完全重写，参考Python算法
                    
                    // 预先收集所有空终止符的位置，用于快速查找
                    std::vector<size_t> zeroIdx;
                    for (size_t i = 0; i < tdatSize; ++i) {
                        if (TDat.get()[i] == 0) {
                            zeroIdx.push_back(i);
                        }
                    }
                    
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
                                } else {
                                    throw std::runtime_error("Invalid UTF-8");
                                }
                            } catch (...) {
                                try {
                                    // 如果UTF-8失败，尝试GBK -> CP1252 -> UTF-8的转换
                                    // 参考Python：gbk_bytes = raw.decode('gbk', errors='strict').encode('cp1252', errors='replace')
                                    // 这里简化：直接使用CP1252转换
                                    val = this->cp1252_to_utf8(raw);
                                } catch (...) {
                                    // 最后回退到guess_decode
                                    val = this->guess_decode(raw);
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
        }
    }

    fclose(f);
    
    // 最终版本确定：如果版本仍为UNKNOWN，根据解析结果推断
    if (this->detectedVersion == GXTVersion::UNKNOWN && !tables.empty()) {
        if (this->originalHasTABL) {
            this->detectedVersion = GXTVersion::GTA_VC;
        } else {
            if (tables.size() == 1 && tables[0].name == "MAIN") {
                if (!tables[0].entries.empty() && tables[0].entries[0].key.find("0x") == 0) {
                    this->detectedVersion = GXTVersion::GTA_IV;
                } else {
                    this->detectedVersion = GXTVersion::GTA_III;
                }
            } else {
                this->detectedVersion = GXTVersion::GTA_SA;
            }
        }
    }
    
    // 根据是否有表显示不同的日志
    if (tables.empty() && !noTablEntries.empty()) {
        addParseLog("文件解析完成，无表文件，共 " + std::to_string(noTablEntries.size()) + " 个条目，版本: " + getVersionName(this->detectedVersion));
    } else {
        addParseLog("文件解析完成，共解析 " + std::to_string(tables.size()) + " 个表，版本: " + getVersionName(this->detectedVersion));
    }
    reportProgress(100, "解析完成");
    // 无表文件：tables为空但noTablEntries不为空时也算成功
    return !tables.empty() || !noTablEntries.empty();
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

    // 先读取前20字节用于调试
    unsigned char header[20];
    fread(header, 1, 20, f);
    rewind(f);

    addParseLog("文件头部（前20字节）:");
    for (int i = 0; i < 20; i++) {
        char hexStr[10];
        snprintf(hexStr, sizeof(hexStr), "0x%02X", header[i]);
        addParseLog("  [" + std::to_string(i) + "] = " + hexStr + " (" + std::to_string(header[i]) + ")");
    }

    // 读取条目数量
    uint32_t count = read_u32(f);
    addParseLog("WHM文件包含 " + std::to_string(count) + " 个条目");

    // 如果条目数异常，尝试其他读取方式
    if (count > 100000 || count == 0) {
        addParseLog("条目数异常，尝试其他格式...");

        // 尝试从偏移12开始读取（RSC header是12字节）
        fseek(f, 12, SEEK_SET);
        count = read_u32(f);
        addParseLog("从偏移12读取条目数: " + std::to_string(count));

        if (count > 0 && count < 100000) {
            fseek(f, 16, SEEK_SET);  // 移动到条目表开始位置（跳过12字节头部+4字节count）
        } else {
            addParseLog("条目数仍然异常，无法解析");
            fclose(f);
            return false;
        }
    }
    
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

// ===== WHM/RSC 格式实现 =====

// FNV-1a 哈希算法
uint32_t GXTParser::fnv1a_hash(const char* str) const {
    uint32_t hash = 2166136261u;  // FNV offset basis
    while (*str) {
        hash ^= (uint8_t)(*str);
        hash *= 16777619u;  // FNV prime
        str++;
    }
    return hash;
}

// 验证字符串是否为有效的文本字符串
bool GXTParser::isValidTextString(const std::string& str) const {
    if (str.empty()) return false;

    // 检查是否包含 EFIGS 字符
    bool hasValidChar = false;
    for (char c : str) {
        unsigned char uc = static_cast<unsigned char>(c);
        if ((uc >= 'a' && uc <= 'z') ||
            (uc >= 'A' && uc <= 'Z') ||
            (uc >= 0xC0)) {  // 扩展 ASCII
            hasValidChar = true;
            break;
        }
    }

    if (!hasValidChar) return false;

    // 检查是否为 URL（跳过 URL）
    bool allUrlChars = true;
    bool hasDot = false;
    for (size_t i = 0; i < str.size(); i++) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        bool isUrlChar = (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') ||
                        c == '.' || c == '%' || c == '@' ||
                        c == '-' || c == '_';
        if (!isUrlChar) {
            allUrlChars = false;
            break;
        }
        if (c == '.') {
            hasDot = true;
            // URL 的第一个和最后一个字符不能是 '.'
            if (i == 0 || i == str.size() - 1) {
                return false;
            }
        }
    }

    // 如果全是 URL 字符且至少有一个点，则认为是 URL
    if (allUrlChars && hasDot && str.size() > 3) {
        return false;
    }

    return true;
}

// RSC 解压函数（使用 Qt 内置的 qUncompress）
static std::vector<uint8_t> decompressRSC(FILE* f, const RSCHeader& header) {
    addParseLog("RSC 解压: 压缩大小=" + std::to_string(header.compressedSize) +
               ", 解压后大小=" + std::to_string(header.uncompressedSize));

    std::vector<uint8_t> compressed(header.compressedSize);
    fread(compressed.data(), 1, header.compressedSize, f);

    // 使用 Qt 的 qUncompress 解压
    QByteArray compressedData(reinterpret_cast<const char*>(compressed.data()), header.compressedSize);
    QByteArray uncompressedData = qUncompress(compressedData);

    if (uncompressedData.isEmpty()) {
        addParseLog("RSC 解压失败");
        return std::vector<uint8_t>();
    }

    addParseLog("RSC 解压成功，解压后大小: " + std::to_string(uncompressedData.size()));

    // 转换为 std::vector<uint8_t>
    std::vector<uint8_t> uncompressed(uncompressedData.size());
    std::copy(uncompressedData.begin(), uncompressedData.end(), uncompressed.begin());

    return uncompressed;
}

// 解析 HTML 文档并提取文本字符串
static void extractHtmlTexts(const uint8_t* data, size_t size,
                           std::vector<WHMEntry>& entries) {
    addParseLog("开始解析 HTML 文档，大小: " + std::to_string(size) + " 字节");

    std::set<uint32_t> seenHashes;

    // 简化的 HTML 解析：查找文本内容
    // 真实的 HTML 文档需要完整的 DOM 解析器，这里使用简化的方法
    size_t pos = 0;
    while (pos < size) {
        // 查找文本开始标记（简化版本，实际需要完整解析）
        // 跳过 HTML 标签
        while (pos < size && data[pos] == '<') {
            // 跳过整个标签
            while (pos < size && data[pos] != '>') {
                pos++;
            }
            pos++;  // 跳过 '>'
        }

        if (pos >= size) break;

        // 查找文本结束（下一个 '<' 或结尾）
        size_t textStart = pos;
        while (pos < size && data[pos] != '<') {
            pos++;
        }

        if (pos > textStart) {
            std::string text(reinterpret_cast<const char*>(data + textStart), pos - textStart);

            // 清理文本：去除空白字符
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t' ||
                                   text.back() == '\n' || text.back() == '\r')) {
                text.pop_back();
            }
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t' ||
                                   text.front() == '\n' || text.front() == '\r')) {
                text.erase(0, 1);
            }

            if (text.empty()) continue;

            // 验证是否为有效文本字符串
            GXTParser parser;  // 临时实例以访问验证方法
            if (!parser.isValidTextString(text)) {
                continue;
            }

            // 计算 FNV 哈希
            uint32_t hash = parser.fnv1a_hash(text.c_str());

            // 检查是否已经见过这个哈希
            if (seenHashes.find(hash) != seenHashes.end()) {
                continue;
            }
            seenHashes.insert(hash);

            // 添加条目
            WHMEntry entry;
            entry.hash = hash;
            entry.offset = 0;  // 在保存时重新计算
            entry.text = text;
            entries.push_back(entry);
        }
    }

    addParseLog("HTML 解析完成，提取文本数量: " + std::to_string(entries.size()));
}

// 解析 WHM/RSC 文件
bool GXTParser::parseWHMRSC(const std::string& filePath, std::vector<WHMEntry>& outEntries) const {
    addParseLog("开始解析 WHM/RSC 文件: " + filePath);

    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "rb");
    if (!f) {
        addParseLog("无法打开文件");
        return false;
    }

    // 读取前16字节
    uint8_t headerBytes[16];
    fread(headerBytes, 1, 16, f);

    addParseLog("RSC 头部检查:");
    addParseLog("  Magic: " + std::string(reinterpret_cast<char*>(headerBytes), 4));

    // 检查是否为RSC格式（前4字节应该是"RSC" + 某个版本）
    if (headerBytes[0] != 'R' || headerBytes[1] != 'S' || headerBytes[2] != 'C') {
        addParseLog("不是RSC格式");
        fclose(f);
        return false;
    }

    // 读取RSC头部结构
    // offset 0-3: magic (4 bytes)
    // offset 4-7: type (4 bytes)
    // offset 8-11: flags (4 bytes)
    // 注意：Python中rsc_header总共12字节，不是16字节
    uint32_t typeValue = *reinterpret_cast<uint32_t*>(&headerBytes[4]);
    uint32_t flagsValue = *reinterpret_cast<uint32_t*>(&headerBytes[8]);

    char typeHex[20];
    snprintf(typeHex, sizeof(typeHex), "0x%08X", typeValue);
    char flagsHex[20];
    snprintf(flagsHex, sizeof(flagsHex), "0x%08X", flagsValue);

    addParseLog("  RSC Version: " + std::to_string(static_cast<int>(headerBytes[3])));
    addParseLog("  Type: " + std::string(typeHex));
    addParseLog("  Flags: " + std::string(flagsHex));

    // 解析flags结构（完全按照Python的rsc_flag_bits）
    // bits 0: virtual1xPageFlag
    // bits 1: virtual2xPageFlag
    // bits 2: virtual4xPageFlag
    // bits 3: virtual8xPageFlag
    // bits 4-10: virtual16xPageCount (7 bits)
    // bits 11-14: virtual1xPageSize (4 bits)
    // bits 15: physical1xPageFlag (1 bit)
    // bits 16: physical2xPageFlag (1 bit)
    // bits 17: physical4xPageFlag (1 bit)
    // bits 18: physical8xPageFlag (1 bit)
    // bits 19-25: physical16xPageCount (7 bits)
    // bits 26-29: physical1xPageSize (4 bits)
    // bits 30-31: useless (2 bits)

    // 提取所有标志位
    bool virtual1xPageFlag = (flagsValue & 0x00000001) != 0;
    bool virtual2xPageFlag = (flagsValue & 0x00000002) != 0;
    bool virtual4xPageFlag = (flagsValue & 0x00000004) != 0;
    bool virtual8xPageFlag = (flagsValue & 0x00000008) != 0;
    uint32_t virtual16xPageCount = (flagsValue >> 4) & 0x7F;
    uint32_t virtual1xPageSize = (flagsValue >> 11) & 0x0F;

    bool physical1xPageFlag = (flagsValue & 0x00008000) != 0;
    bool physical2xPageFlag = (flagsValue & 0x00010000) != 0;
    bool physical4xPageFlag = (flagsValue & 0x00020000) != 0;
    bool physical8xPageFlag = (flagsValue & 0x00040000) != 0;
    uint32_t physical16xPageCount = (flagsValue >> 19) & 0x7F;
    uint32_t physical1xPageSize = (flagsValue >> 26) & 0x0F;

    // 计算页面大小
    uint32_t virtualPageSize = 1 << (virtual1xPageSize + 8);
    uint32_t physicalPageSize = 1 << (physical1xPageSize + 8);

    // 计算virtual size - 完全按照Python的GetVirtualSize()
    uint32_t virtualSize = virtualPageSize * (
        (virtual1xPageFlag ? 1 : 0) +
        (virtual2xPageFlag ? 2 : 0) +
        (virtual4xPageFlag ? 4 : 0) +
        (virtual8xPageFlag ? 8 : 0) +
        (virtual16xPageCount * 16)
    );

    // 计算physical size - 完全按照Python的GetPhysicalSize()
    uint32_t physicalSize = physicalPageSize * (
        (physical1xPageFlag ? 1 : 0) +
        (physical2xPageFlag ? 2 : 0) +
        (physical4xPageFlag ? 4 : 0) +
        (physical8xPageFlag ? 8 : 0) +
        (physical16xPageCount * 16)
    );

    uint32_t uncompressedSize = virtualSize + physicalSize;

    addParseLog("  virtual1xPageSize: " + std::to_string(virtual1xPageSize) + " -> " + std::to_string(virtualPageSize));
    addParseLog("  physical1xPageSize: " + std::to_string(physical1xPageSize) + " -> " + std::to_string(physicalPageSize));
    addParseLog("  virtual1xPageFlag: " + std::to_string(virtual1xPageFlag));
    addParseLog("  virtual2xPageFlag: " + std::to_string(virtual2xPageFlag));
    addParseLog("  virtual4xPageFlag: " + std::to_string(virtual4xPageFlag));
    addParseLog("  virtual8xPageFlag: " + std::to_string(virtual8xPageFlag));
    addParseLog("  virtual16xPageCount: " + std::to_string(virtual16xPageCount));
    addParseLog("  physical1xPageFlag: " + std::to_string(physical1xPageFlag));
    addParseLog("  physical2xPageFlag: " + std::to_string(physical2xPageFlag));
    addParseLog("  physical4xPageFlag: " + std::to_string(physical4xPageFlag));
    addParseLog("  physical8xPageFlag: " + std::to_string(physical8xPageFlag));
    addParseLog("  physical16xPageCount: " + std::to_string(physical16xPageCount));
    addParseLog("  VirtualSize: " + std::to_string(virtualSize));
    addParseLog("  PhysicalSize: " + std::to_string(physicalSize));
    addParseLog("  Total UncompressedSize: " + std::to_string(uncompressedSize));

    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 12, SEEK_SET);  // 跳过12字节RSC头部

    uint32_t compressedSize = fileSize - 12;
    addParseLog("  CompressedSize: " + std::to_string(compressedSize));

    // 读取压缩数据
    std::vector<uint8_t> compressed(compressedSize);
    fread(compressed.data(), 1, compressedSize, f);

    // 使用 Qt 的 qUncompress 解压
    QByteArray compressedData(reinterpret_cast<const char*>(compressed.data()), compressedSize);
    QByteArray uncompressedData = qUncompress(compressedData);

    if (uncompressedData.isEmpty()) {
        addParseLog("RSC 解压失败");
        fclose(f);
        return false;
    }

    addParseLog("RSC 解压成功，解压后大小: " + std::to_string(uncompressedData.size()));

    fclose(f);

    // 解析 HTML 文档并提取文本
    outEntries.clear();
    extractHtmlTexts(reinterpret_cast<const uint8_t*>(uncompressedData.constData()), uncompressedData.size(), outEntries);

    addParseLog("WHM/RSC 解析完成，提取条目数: " + std::to_string(outEntries.size()));
    return true;
}

// 保存 WHM/RSC 文件
bool GXTParser::saveWHMRSC(const std::string& filePath, const std::vector<WHMEntry>& entries) const {
    addParseLog("开始保存 WHM/RSC 文件: " + filePath);

    if (entries.empty()) {
        addParseLog("警告: 没有条目可保存");
        return false;
    }

    // 计算文本数据大小
    uint32_t textDataSize = 0;
    for (const auto& entry : entries) {
        textDataSize += static_cast<uint32_t>(entry.text.size() + 1);  // +1 for null terminator
    }

    // WHM 格式：条目数(uint32) | 条目表(hash+offset) | blob大小(uint32) | 文本数据块
    uint32_t headerSize = 12;  // 条目数 + blob大小
    uint32_t tableSize = static_cast<uint32_t>(entries.size() * 8);  // hash(4) + offset(4)
    uint32_t totalSize = headerSize + tableSize + textDataSize;

    addParseLog("文件大小估算: " + std::to_string(totalSize) + " 字节");

    // 打开文件
    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "wb");
    if (!f) {
        addParseLog("无法创建输出文件");
        return false;
    }

    // 写入条目数量
    uint32_t count = static_cast<uint32_t>(entries.size());
    fwrite(&count, sizeof(count), 1, f);

    // 计算偏移量（文本数据开始位置）
    uint32_t textDataOffset = headerSize + tableSize;

    // 写入条目表
    uint32_t currentOffset = textDataOffset;
    for (const auto& entry : entries) {
        fwrite(&entry.hash, sizeof(entry.hash), 1, f);
        fwrite(&currentOffset, sizeof(currentOffset), 1, f);
        currentOffset += static_cast<uint32_t>(entry.text.size() + 1);
    }

    // 写入文本数据大小
    fwrite(&textDataSize, sizeof(textDataSize), 1, f);

    // 写入文本数据
    for (const auto& entry : entries) {
        fwrite(entry.text.c_str(), 1, entry.text.size(), f);
        fwrite("\0", 1, 1, f);  // null terminator
    }

    fclose(f);
    addParseLog("WHM/RSC 保存完成");
    return true;
}

// ===== zlib 压缩/解压实现 (x64) =====
#ifdef HAVE_ZLIB

std::vector<uint8_t> GXTParser::compressData(const uint8_t* data, size_t length, int level) {
    if (!data || length == 0) {
        return std::vector<uint8_t>();
    }

    // 计算压缩后数据的最大可能大小
    uLongf compressedSize = compressBound(static_cast<uLong>(length));
    std::vector<uint8_t> compressed(compressedSize);

    int result = compress2(compressed.data(), &compressedSize, data, static_cast<uLong>(length), level);
    
    if (result != Z_OK) {
        // 压缩失败
        return std::vector<uint8_t>();
    }

    // 调整向量大小为实际压缩后的大小
    compressed.resize(compressedSize);
    return compressed;
}

std::vector<uint8_t> GXTParser::compressData(const std::vector<uint8_t>& data, int level) {
    return compressData(data.data(), data.size(), level);
}

std::vector<uint8_t> GXTParser::decompressData(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return std::vector<uint8_t>();
    }

    // 初始解压缓冲区大小（可以根据需要调整）
    uLongf uncompressedSize = length * 4;  // 初始猜测：4倍压缩数据大小
    std::vector<uint8_t> uncompressed;
    
    int result;
    do {
        uncompressed.resize(uncompressedSize);
        result = uncompress(uncompressed.data(), &uncompressedSize, data, static_cast<uLong>(length));
        
        if (result == Z_BUF_ERROR) {
            // 缓冲区太小，扩大2倍重试
            uncompressedSize *= 2;
            if (uncompressedSize > 100 * 1024 * 1024) {  // 最大100MB限制
                return std::vector<uint8_t>();  // 解压数据过大
            }
        }
    } while (result == Z_BUF_ERROR);

    if (result != Z_OK) {
        // 解压失败
        return std::vector<uint8_t>();
    }

    // 调整向量大小为实际解压后的大小
    uncompressed.resize(uncompressedSize);
    return uncompressed;
}

std::vector<uint8_t> GXTParser::decompressData(const std::vector<uint8_t>& data) {
    return decompressData(data.data(), data.size());
}

std::string GXTParser::getZlibVersion() {
    return std::string(zlibVersion());
}

#endif  // HAVE_ZLIB

// ===== GTA4 WHM 增强解析实现 =====

// RSC 头部结构（12字节）- 完全按照Python代码
struct RSCHeaderEx {
    uint32_t magic;      // "RSC" + 版本号
    uint32_t type;       // 资源类型
    uint32_t flags;      // 页面大小和数量标志
    
    // 从 flags 计算虚拟页面大小 - 完全按照Python代码
    uint32_t getVirtualPageSize() const {
        uint32_t virtual1xPageSize = (flags >> 11) & 0x0F;
        return 1 << (virtual1xPageSize + 8);
    }
    
    // 从 flags 计算物理页面大小 - 完全按照Python代码
    uint32_t getPhysicalPageSize() const {
        uint32_t physical1xPageSize = (flags >> 26) & 0x0F;
        return 1 << (physical1xPageSize + 8);
    }
    
    // 计算虚拟内存总大小 - 完全按照Python代码
    uint32_t getVirtualSize() const {
        bool virtual1xPageFlag = (flags & 0x00000001) != 0;
        bool virtual2xPageFlag = (flags & 0x00000002) != 0;
        bool virtual4xPageFlag = (flags & 0x00000004) != 0;
        bool virtual8xPageFlag = (flags & 0x00000008) != 0;
        uint32_t virtual16xPageCount = (flags >> 4) & 0x7F;
        
        return getVirtualPageSize() * (
            (virtual1xPageFlag ? 1 : 0) +
            (virtual2xPageFlag ? 2 : 0) +
            (virtual4xPageFlag ? 4 : 0) +
            (virtual8xPageFlag ? 8 : 0) +
            (virtual16xPageCount * 16)
        );
    }
    
    // 计算物理内存总大小 - 完全按照Python代码
    uint32_t getPhysicalSize() const {
        bool physical1xPageFlag = (flags & 0x00008000) != 0;
        bool physical2xPageFlag = (flags & 0x00010000) != 0;
        bool physical4xPageFlag = (flags & 0x00020000) != 0;
        bool physical8xPageFlag = (flags & 0x00040000) != 0;
        uint32_t physical16xPageCount = (flags >> 19) & 0x7F;
        
        return getPhysicalPageSize() * (
            (physical1xPageFlag ? 1 : 0) +
            (physical2xPageFlag ? 2 : 0) +
            (physical4xPageFlag ? 4 : 0) +
            (physical8xPageFlag ? 8 : 0) +
            (physical16xPageCount * 16)
        );
    }
    
    // 获取解压后总大小
    uint32_t getUncompressedSize() const {
        return getVirtualSize() + getPhysicalSize();
    }
};

// FNV-1a 32位哈希（与Python版本完全一致）
static uint32_t fnv1a_32_hash(const uint8_t* data, size_t len) {
    const uint32_t FNV_PRIME_32 = 0x01000193;
    const uint32_t FNV_OFFSET_BASIS_32 = 0x811C9DC5;
    
    uint32_t hash = FNV_OFFSET_BASIS_32;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= FNV_PRIME_32;
        hash &= 0xFFFFFFFF;
    }
    return hash;
}

// 验证字符是否为数字（完全按照Python代码）
static bool validate_digit_char(uint8_t c) {
    return c >= '0' && c <= '9';
}

// 验证字符是否为英文字母（完全按照Python代码）
static bool validate_english_char(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// 验证字符是否为EFIGS字符（完全按照Python代码）
static bool validate_efigs_char(uint8_t c) {
    return validate_english_char(c) || c >= 0xC0;
}

// 验证字符是否为URL字符（完全按照Python代码）
static bool validate_url_char(uint8_t c) {
    return validate_english_char(c) || validate_digit_char(c) ||
           c == '.' || c == '%' || c == '@' || c == '-' || c == '_';
}

// 验证是否为URL（完全按照Python代码）
static bool validate_url(const uint8_t* data, size_t len) {
    if (len == 0) return false;
    
    // 检查所有字符是否都是URL合法字符
    for (size_t i = 0; i < len; i++) {
        if (!validate_url_char(data[i])) {
            return false;
        }
    }
    
    // 查找第一个和最后一个点
    int first_dot_pos = -1;
    int last_dot_pos = -1;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '.') {
            if (first_dot_pos == -1) first_dot_pos = (int)i;
            last_dot_pos = (int)i;
        }
    }
    
    // 验证URL格式：必须包含点号，且不在开头或结尾
    return (first_dot_pos != -1 && first_dot_pos != 0 && 
            last_dot_pos != (int)(len - 1));
}

// 验证字符串（完全按照Python代码）
static bool validate_string(const uint8_t* data, size_t len) {
    if (len == 0) return false;
    
    // 检查是否包含EFIGS字符
    bool has_efigs = false;
    for (size_t i = 0; i < len; i++) {
        if (validate_efigs_char(data[i])) {
            has_efigs = true;
            break;
        }
    }
    
    // 包含EFIGS字符且不是URL
    return has_efigs && !validate_url(data, len);
}

// 尝试添加字符串（完全按照Python代码）
static void try_append_string(std::vector<WHMEntry>& container,
                             const uint8_t* ptr, size_t len,
                             std::set<uint32_t>& hashes) {
    if (!ptr || len == 0) return;
    
    if (validate_string(ptr, len)) {
        uint32_t hash_val = fnv1a_32_hash(ptr, len);
        if (hashes.find(hash_val) == hashes.end()) {
            hashes.insert(hash_val);
            
            WHMEntry entry;
            entry.hash = hash_val;
            entry.offset = 0;
            entry.text = std::string(reinterpret_cast<const char*>(ptr), len);
            container.push_back(entry);
        }
    }
}

// 解析pgPtr指针指向的CHtmlNode（完全按照Python代码的locate方法）
static const CHtmlNode* locate_html_node(const uint8_t* block, size_t blockSize, pgPtr ptr) {
    if (!block || ptr.t() != PtrElementType::Cpu_Type) {
        return nullptr;
    }
    
    uint32_t offset = ptr.o();
    if (offset + sizeof(CHtmlNode) > blockSize) {
        return nullptr;
    }
    
    return reinterpret_cast<const CHtmlNode*>(block + offset);
}

// 解析pgString指针指向的字符串（完全按照Python代码的locate_str方法）
static std::string locate_string(const uint8_t* block, size_t blockSize, pgString ptr) {
    if (!block || ptr.t() != PtrElementType::Cpu_Type) {
        return "";
    }
    
    uint32_t offset = ptr.o();
    if (offset >= blockSize) {
        return "";
    }
    
    // 查找null终止符
    size_t end = offset;
    while (end < blockSize && block[end] != 0) {
        end++;
    }
    
    if (end == blockSize) {
        return "";  // 没找到终止符
    }
    
    return std::string(reinterpret_cast<const char*>(block + offset), end - offset);
}

// 解析pgObjectPtrArray数组（完全按照Python代码的get_span方法）
static std::vector<pgPtr> get_ptr_array(const uint8_t* block, size_t blockSize, pgObjectPtrArray arr) {
    std::vector<pgPtr> result;
    
    if (!block || arr.t() != PtrElementType::Cpu_Type) {
        return result;
    }
    
    uint32_t offset = arr.o();
    size_t num_elements = std::max((uint16_t)arr.count, (uint16_t)arr.size);
    
    if (offset + num_elements * sizeof(pgPtr) > blockSize) {
        num_elements = arr.count;
        if (offset + num_elements * sizeof(pgPtr) > blockSize) {
            return result;  // 越界
        }
    }
    
    const pgPtr* elements = reinterpret_cast<const pgPtr*>(block + offset);
    for (size_t i = 0; i < num_elements; i++) {
        result.push_back(elements[i]);
    }
    
    return result;
}

// 递归提取HTML节点中的字符串（完全按照Python代码的ExtractNodeStrings方法）
static void extract_node_strings(const CHtmlNode* node,
                                 const uint8_t* block, size_t blockSize,
                                 std::vector<WHMEntry>& container,
                                 std::set<uint32_t>& hashes,
                                 std::function<void(const std::string&)> logCallback) {
    if (!node) return;
    
    // 递归处理子节点
    std::vector<pgPtr> children = get_ptr_array(block, blockSize, node->m_children);
    for (const pgPtr& child_ptr : children) {
        const CHtmlNode* child_node = locate_html_node(block, blockSize, child_ptr);
        if (child_node) {
            extract_node_strings(child_node, block, blockSize, container, hashes, logCallback);
        }
    }
    
    // 根据节点类型处理
    HtmlNodeType nodeType = static_cast<HtmlNodeType>(node->m_eNodeType);
    
    if (nodeType == HtmlNodeType::Node_HtmlDataNode) {
        // 处理数据节点
        const CHtmlDataNode* data_node = reinterpret_cast<const CHtmlDataNode*>(node);
        std::string data = locate_string(block, blockSize, data_node->m_pData);
        if (!data.empty()) {
            try_append_string(container, 
                             reinterpret_cast<const uint8_t*>(data.c_str()), 
                             data.length(), hashes);
        }
    } else if (nodeType == HtmlNodeType::Node_HtmlNode ||
               nodeType == HtmlNodeType::Node_HtmlTableNode ||
               nodeType == HtmlNodeType::Node_HtmlTableElementNode) {
        // 这些节点类型不包含文本数据，跳过
    } else {
        if (logCallback) {
            logCallback("警告: 未处理的节点类型 " + std::to_string(nodeType));
        }
    }
}

// 从HTML文档提取字符串（完全按照Python代码的ExtractWhmStrings方法）
static void extract_whm_strings(const uint8_t* block, size_t blockSize,
                                std::vector<WHMEntry>& container,
                                std::set<uint32_t>& hashes,
                                std::function<void(const std::string&)> logCallback) {
    if (!block || blockSize < sizeof(CHtmlDocument)) {
        if (logCallback) logCallback("警告: 数据块太小，无法解析HTML文档");
        return;
    }
    
    try {
        const CHtmlDocument* doc = reinterpret_cast<const CHtmlDocument*>(block);
        const CHtmlNode* body_node = locate_html_node(block, blockSize, doc->m_pBody);
        
        if (body_node) {
            extract_node_strings(body_node, block, blockSize, container, hashes, logCallback);
        } else {
            if (logCallback) logCallback("警告: 无法找到body节点");
        }
    } catch (const std::exception& e) {
        if (logCallback) logCallback(std::string("解析文档结构时出错: ") + e.what());
    }
}

// 增强的WHM解析（GTA4格式）- 完全按照Python代码
bool GXTParser::parseWHMEx(const std::string& filePath, std::vector<WHMEntry>& outEntries) const {
    addParseLog("开始解析 WHM 文件 (增强版): " + filePath);
    
    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "rb");
    if (!f) {
        addParseLog("无法打开文件");
        return false;
    }
    
    // 读取RSC头部
    RSCHeaderEx header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        addParseLog("无法读取RSC头部");
        fclose(f);
        return false;
    }
    
    // 检查magic - "RSC" + 版本号 (小端序: R=0x52, S=0x53, C=0x43)
    // magic = "RSC\x05" -> [0x52, 0x53, 0x43, 0x05] -> 0x05435352 (小端uint32_t)
    uint32_t rscMagic = header.magic & 0x00FFFFFF;
    if (rscMagic != 0x00435352) {  // 'R'|'S'<<8|'C'<<16 = 0x52|0x53<<8|0x43<<16 = 0x00435352
        char actualMagic[4];
        memcpy(actualMagic, &header.magic, 4);
        addParseLog(std::string("不是有效的RSC格式文件，Magic: ") + 
                   std::to_string((int)actualMagic[0]) + " " +
                   std::to_string((int)actualMagic[1]) + " " +
                   std::to_string((int)actualMagic[2]) + " " +
                   std::to_string((int)actualMagic[3]));
        fclose(f);
        return false;
    }
    
    char magicStr[5] = {};
    memcpy(magicStr, &header.magic, 4);
    addParseLog("RSC Magic: " + std::string(magicStr));
    addParseLog("RSC Version: " + std::to_string((header.magic >> 24) & 0xFF));
    
    char typeHex[16], flagsHex[16];
    snprintf(typeHex, sizeof(typeHex), "%08X", header.type);
    snprintf(flagsHex, sizeof(flagsHex), "%08X", header.flags);
    addParseLog(std::string("RSC Type: 0x") + typeHex);
    addParseLog(std::string("RSC Flags: 0x") + flagsHex);
    
    // 计算解压大小 - 完全按照Python代码
    uint32_t virtualSize = header.getVirtualSize();
    uint32_t physicalSize = header.getPhysicalSize();
    uint32_t uncompressedSize = header.getUncompressedSize();
    
    addParseLog("Virtual Size: " + std::to_string(virtualSize));
    addParseLog("Physical Size: " + std::to_string(physicalSize));
    addParseLog("Total Uncompressed Size: " + std::to_string(uncompressedSize));
    
    // 获取文件大小和压缩数据大小
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, sizeof(RSCHeaderEx), SEEK_SET);
    
    uint32_t compressedSize = fileSize - sizeof(RSCHeaderEx);
    addParseLog("Compressed Size: " + std::to_string(compressedSize));
    
    // 读取压缩数据
    std::vector<uint8_t> compressed(compressedSize);
    if (fread(compressed.data(), 1, compressedSize, f) != compressedSize) {
        addParseLog("无法读取压缩数据");
        fclose(f);
        return false;
    }
    fclose(f);
    
    // 解压数据 - 完全按照Python代码的UnpackWhm方法
    std::vector<uint8_t> uncompressed;
    uncompressed.resize(uncompressedSize);
    
#ifdef HAVE_ZLIB
    // 使用zlib解压 - 完全按照Python代码的zlib.decompress
    uLongf destLen = uncompressedSize;
    int zResult = uncompress(uncompressed.data(), &destLen, 
                             compressed.data(), compressedSize);
    
    if (zResult != Z_OK) {
        addParseLog("zlib解压失败，错误码: " + std::to_string(zResult));
        return false;
    }
    
    // 检查解压后大小 - 完全按照Python代码
    if (destLen != uncompressedSize) {
        addParseLog("警告: 解压大小不匹配，预期 " + std::to_string(uncompressedSize) + 
                   "，实际 " + std::to_string(destLen));
        // 调整缓冲区大小以匹配实际数据
        if (destLen < uncompressedSize) {
            // 如果解压数据小于预期，填充零
            uncompressed.resize(uncompressedSize);
            memset(uncompressed.data() + destLen, 0, uncompressedSize - destLen);
        } else {
            // 如果解压数据大于预期，截断
            uncompressed.resize(destLen);
        }
    }
    addParseLog("zlib解压成功");
#else
    // 使用Qt解压 - 需要添加4字节头部（大端大小）
    QByteArray qtCompressed;
    qtCompressed.reserve(4 + compressedSize);
    // 添加大端格式的解压大小头部
    qtCompressed.append(static_cast<char>((uncompressedSize >> 24) & 0xFF));
    qtCompressed.append(static_cast<char>((uncompressedSize >> 16) & 0xFF));
    qtCompressed.append(static_cast<char>((uncompressedSize >> 8) & 0xFF));
    qtCompressed.append(static_cast<char>(uncompressedSize & 0xFF));
    qtCompressed.append(reinterpret_cast<const char*>(compressed.data()), compressedSize);
    
    QByteArray uncompressedData = qUncompress(qtCompressed);
    if (uncompressedData.isEmpty()) {
        addParseLog("Qt解压失败");
        return false;
    }
    uncompressed.resize(uncompressedData.size());
    memcpy(uncompressed.data(), uncompressedData.constData(), uncompressedData.size());
#endif
    
    addParseLog("解压成功，大小: " + std::to_string(uncompressed.size()) + " 字节");
    
    // 检查解压后大小 - 完全按照Python代码
    if (uncompressed.size() != uncompressedSize) {
        addParseLog("警告: 解压后大小不匹配，预期 " + std::to_string(uncompressedSize) + 
                   "，实际 " + std::to_string(uncompressed.size()));
    }
    
    // 提取HTML文本 - 完全按照Python代码的ExtractWhmStrings方法
    outEntries.clear();
    std::set<uint32_t> seenHashes;
    extract_whm_strings(uncompressed.data(), uncompressed.size(), outEntries, seenHashes, g_logCallback);
    
    addParseLog("WHM解析完成，提取条目数: " + std::to_string(outEntries.size()));
    return !outEntries.empty();
}