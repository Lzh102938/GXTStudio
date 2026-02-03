#ifndef GXT_PARSER_H
#define GXT_PARSER_H

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

// zlib support
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

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

// RSC压缩头部结构
struct RSCHeader {
    uint32_t flags;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
};

// HTML节点类型（完全按照Python代码）
enum HtmlNodeType {
    Node_HtmlNode = 0,
    Node_HtmlDataNode = 1,
    Node_HtmlTableNode = 2,
    Node_HtmlTableElementNode = 3
};

// 指针类型（完全按照Python代码）
enum PtrElementType {
    Cpu_Type = 5,
    Gpu_Type = 6
};

// HTML渲染状态（完全按照Python代码）
struct HtmlRenderState {
    uint32_t eDisplay;
    float fWidth;
    float fHeight;
    float _fC;
    float _f10;
    uint8_t _f14[4];
    float _f18;
    float _f1C;
    uint32_t dwBgColor;
    uint32_t pBackgroundImage;  // pgPtr
    uint32_t _f28h;
    uint32_t _f28l;
    uint32_t backgroundRepeat;
    uint32_t dwColor;
    uint32_t eAlign;
    uint32_t eValign;
    uint32_t eTextDecoration;
    uint32_t _f44;
    uint32_t eFontSize;
    int32_t nFontStyle;
    int32_t nFontWeight;
    float _f54;
    uint32_t dwBorderBottomColor;
    uint32_t eBorderBottomStyle;
    float fBorderBottomWidth;
    uint32_t dwBorderLeftColor;
    uint32_t eBorderLeftStyle;
    float dwBorderLeftWidth;
    uint32_t dwBorderRightColor;
    uint32_t eBorderRightStyle;
    float fBorderRightWidth;
    uint32_t dwBorderTopColor;
    uint32_t eBorderTopStyle;
    float fBorderTopWidth;
    float fMarginBottom;
    float fMarginLeft;
    float fMarginRight;
    float fMarginTop;
    float fPaddingBottom;
    float fPaddingLeft;
    float fPaddingRight;
    float fPaddingTop;
    float fCellPadding;
    float fCellSpacing;
    int32_t nColSpan;
    int32_t nRowSpan;
    bool hasBackground;
    bool isLink;
    uint8_t _BA[2];
    uint32_t dwLinkColor;
    uint32_t _fC0;
};

// pgPtr指针结构（完全按照Python代码）
struct pgPtr {
    uint32_t _value;
    
    uint32_t o() const { return _value & 0x0FFFFFFF; }
    uint32_t t() const { return (_value >> 28) & 0xF; }
};

// pgString指针结构（完全按照Python代码）
struct pgString : public pgPtr {
};

// pgObjectArray结构（完全按照Python代码）
struct pgObjectArray {
    uint32_t _ptr_value;
    uint16_t count;
    uint16_t size;
    
    uint32_t o() const { return _ptr_value & 0x0FFFFFFF; }
    uint32_t t() const { return (_ptr_value >> 28) & 0xF; }
};

// pgObjectPtrArray结构（完全按照Python代码）
struct pgObjectPtrArray : public pgObjectArray {
};

// CHtmlNode结构（完全按照Python代码）
struct CHtmlNode {
    uint32_t vtbl;              // 虚函数表指针
    uint32_t m_eNodeType;       // 节点类型
    pgPtr m_pParentNode;        // 父节点
    pgObjectPtrArray m_children;  // 子节点数组
    HtmlRenderState m_renderState;  // 渲染状态
};

// CHtmlDataNode结构（完全按照Python代码）
struct CHtmlDataNode {
    CHtmlNode node_base;
    pgString m_pData;  // 数据指针
};

// CHtmlElementNode结构（完全按照Python代码）
struct CHtmlElementNode {
    CHtmlNode node_base;
    uint32_t m_eHtmlTag;
    pgString m_pszTagName;
    pgObjectArray m_nodeParam;
};

// CHtmlTableNode结构（完全按照Python代码）
struct CHtmlTableNode {
    CHtmlElementNode element_base;
    pgPtr _fE8;
    pgPtr _fEC;
    pgPtr _fF0;
    pgPtr _fF4;
    pgPtr _fF8;
    uint32_t m_dwCellCount;
    uint32_t _f100;
};

// CHtmlTableElementNode结构（完全按照Python代码）
struct CHtmlTableElementNode {
    CHtmlElementNode element_base;
    int32_t _fE8;
    int32_t _fEC;
};

// CHtmlDocument结构（完全按照Python代码）
struct CHtmlDocument {
    pgPtr m_pRootElement;
    pgPtr m_pBody;
    pgString m_pszTitle;
    pgPtr m_pTxd;
    pgObjectPtrArray _f10;
    pgObjectPtrArray m_childNodes;
    pgObjectPtrArray m_pStylesheet;
    uint8_t pad[3];
    uint8_t _f2B;
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
    // WHM/RSC格式相关方法
    bool parseWHMRSC(const std::string& filePath, std::vector<WHMEntry>& outEntries) const;
    bool saveWHMRSC(const std::string& filePath, const std::vector<WHMEntry>& entries) const;
    
    // GTA4 WHM 文件解析（增强版 RSC/HTML 解析）
    bool parseWHMEx(const std::string& filePath, std::vector<WHMEntry>& outEntries) const;
    
    // 辅助方法
    uint32_t fnv1a_hash(const char* str) const;
    bool isValidTextString(const std::string& str) const;
    
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

    // zlib 压缩/解压方法 (x64)
#ifdef HAVE_ZLIB
    // 压缩数据（返回压缩后的数据）
    static std::vector<uint8_t> compressData(const uint8_t* data, size_t length, int level = Z_DEFAULT_COMPRESSION);
    static std::vector<uint8_t> compressData(const std::vector<uint8_t>& data, int level = Z_DEFAULT_COMPRESSION);
    
    // 解压数据（返回解压后的数据）
    static std::vector<uint8_t> decompressData(const uint8_t* data, size_t length);
    static std::vector<uint8_t> decompressData(const std::vector<uint8_t>& data);
    
    // 获取 zlib 版本信息
    static std::string getZlibVersion();
    
    // 检查 zlib 是否可用
    static bool isZlibAvailable() { return true; }
#else
    static bool isZlibAvailable() { return false; }
#endif

};

#endif // GXT_PARSER_H