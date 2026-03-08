#include "CharTableParser.h"
#include <QFile>
#include <QDataStream>
#include <QDebug>

#pragma pack(push,1)
struct CharPos {
    quint8 rowIndex;
    quint8 columnIndex;
};
#pragma pack(pop)

bool CharTableParser::loadGtaVc(const QString& datPath, CharTableData& out)
{
    QFile dat(datPath);
    if (!dat.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开 dat 文件:" << datPath;
        return false;
    }

    const qint64 expectSize = 0x10000 * 2; // 65536 entries, each 2 bytes
    if (dat.size() != expectSize) {
        // GTA_VC 字符表必须是固定大小，直接返回 false 让系统尝试其他格式
        dat.close();
        return false;
    }

    QByteArray datBytes = dat.readAll();
    dat.close();

    // Prepare output grid: 64x64
    const int ROWS = 64;
    const int COLS = 64;
    out.type = CharTableData::GTA_VC;
    out.rows = ROWS;
    out.cols = COLS;
    out.cells.fill(0, ROWS * COLS);
    out.sourceFile = datPath;

    // Interpret datBytes as mapping table: index = unicode codepoint (0..0xFFFF), value = (row, col)
    int entries = qMin<int>(0x10000, datBytes.size() / 2);
    for (int code = 0; code < entries; ++code) {
        int pos = code * 2;
        quint8 row = static_cast<quint8>(datBytes[pos]);
        quint8 col = static_cast<quint8>(datBytes[pos + 1]);
        // default unused value in generator is (63,63)
        if (row == 63 && col == 63) continue;
        if (row < ROWS && col < COLS) {
            out.cells[row * COLS + col] = static_cast<uint16_t>(code);
        }
    }

    return true;
}

bool CharTableParser::loadGtaIv(const QString& datPath, CharTableData& out)
{
    // IV字符表格式：前4字节为字符数量(uint32_t LE)，后面是UTF-32LE编码的字符序列（每个字符4字节）
    QFile f(datPath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开 IV dat 文件:" << datPath;
        return false;
    }

    QByteArray bytes = f.readAll();
    f.close();

    if (bytes.size() < 8) {
        qWarning() << "IV dat 文件太小:" << datPath;
        return false;
    }

    // 读取字符数量（前4字节，小端序）
    quint32 charCount = static_cast<quint8>(bytes[0]) |
                        (static_cast<quint8>(bytes[1]) << 8) |
                        (static_cast<quint8>(bytes[2]) << 16) |
                        (static_cast<quint8>(bytes[3]) << 24);

    // 从偏移4开始读取UTF-32LE字符序列（每个字符4字节）
    int offset = 4;

    // 读取UTF-32LE字符序列，过滤掉空白字符和换行符
    QVector<uint16_t> characters;
    characters.reserve(charCount);

    for (quint32 i = 0; i < charCount && (offset + 4) <= bytes.size(); ++i) {
        // 小端序：低字节在前
        quint32 codepoint = static_cast<quint8>(bytes[offset]) |
                           (static_cast<quint8>(bytes[offset + 1]) << 8) |
                           (static_cast<quint8>(bytes[offset + 2]) << 16) |
                           (static_cast<quint8>(bytes[offset + 3]) << 24);
        offset += 4;

        // 跳过空白字符
        if (codepoint != 0x0000 && codepoint != 0x0020 && codepoint != 0x0009 &&
            codepoint != 0x000A && codepoint != 0x000D) {
            // 如果码点超过16位，截断或跳过
            if (codepoint <= 0xFFFF) {
                characters.append(static_cast<uint16_t>(codepoint));
            }
        }
    }

    if (characters.isEmpty()) {
        qWarning() << "IV dat 文件不包含有效字符:" << datPath;
        return false;
    }

    // 固定使用64列布局
    int cols = 64;

    // 计算需要的行数
    int count = characters.size();
    int rows = (count + cols - 1) / cols;

    // 填充输出结构
    out.type = CharTableData::GTA_IV;
    out.rows = rows;
    out.cols = cols;
    out.cells.fill(0, rows * cols);
    out.sourceFile = datPath;

    // 按行优先顺序填充网格
    for (int idx = 0; idx < characters.size(); ++idx) {
        out.cells[idx] = characters[idx];
    }

    return true;
}

bool CharTableParser::saveGtaVc(const QString& datPath, const CharTableData& data)
{
    if (data.type != CharTableData::GTA_VC) {
        qWarning() << "数据类型不是GTA_VC格式";
        return false;
    }

    QFile file(datPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法创建 dat 文件:" << datPath;
        return false;
    }

    // 创建65536个条目的映射表，每个条目2字节(row, col)
    QByteArray mappingTable(0x10000 * 2, 0);

    // 默认所有位置为(63, 63)表示未使用
    for (int i = 0; i < 0x10000; ++i) {
        mappingTable[i * 2] = 63;
        mappingTable[i * 2 + 1] = 63;
    }

    // 反向映射：从单元格位置到Unicode码点
    for (int row = 0; row < data.rows; ++row) {
        for (int col = 0; col < data.cols; ++col) {
            int idx = row * data.cols + col;
            if (idx < data.cells.size()) {
                uint16_t codepoint = data.cells[idx];
                if (codepoint != 0 && row < 64 && col < 64) {
                    // 设置映射：codepoint -> (row, col)
                    mappingTable[codepoint * 2] = static_cast<char>(row);
                    mappingTable[codepoint * 2 + 1] = static_cast<char>(col);
                }
            }
        }
    }

    qint64 written = file.write(mappingTable);
    file.close();

    if (written != mappingTable.size()) {
        qWarning() << "写入 dat 文件不完整:" << datPath;
        return false;
    }

    return true;
}

bool CharTableParser::saveGtaIv(const QString& datPath, const CharTableData& data)
{
    if (data.type != CharTableData::GTA_IV) {
        qWarning() << "数据类型不是GTA_IV格式";
        return false;
    }

    QFile file(datPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法创建 IV dat 文件:" << datPath;
        return false;
    }

    // 收集有效字符（排除空白字符）
    QVector<uint32_t> validChars;
    for (int idx = 0; idx < data.cells.size(); ++idx) {
        uint16_t ch = data.cells[idx];
        if (ch != 0x0000 && ch != 0x0020 && ch != 0x0009 && ch != 0x000A && ch != 0x000D) {
            validChars.append(ch);
        }
    }

    // 写入字符数量（4字节，小端序）
    quint32 charCount = static_cast<quint32>(validChars.size());
    file.putChar(static_cast<char>(charCount & 0xFF));
    file.putChar(static_cast<char>((charCount >> 8) & 0xFF));
    file.putChar(static_cast<char>((charCount >> 16) & 0xFF));
    file.putChar(static_cast<char>((charCount >> 24) & 0xFF));

    // 按UTF-32LE格式写入每个字符（每个字符4字节）
    for (quint32 codepoint : validChars) {
        file.putChar(static_cast<char>(codepoint & 0xFF));
        file.putChar(static_cast<char>((codepoint >> 8) & 0xFF));
        file.putChar(static_cast<char>((codepoint >> 16) & 0xFF));
        file.putChar(static_cast<char>((codepoint >> 24) & 0xFF));
    }

    file.close();
    return true;
}