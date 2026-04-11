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

    const qint64 expectSize = 0x10000 * 2;
    if (dat.size() != expectSize) {
        dat.close();
        return false;
    }

    const int ROWS = 64;
    const int COLS = 64;
    out.type = CharTableData::GTA_VC;
    out.rows = ROWS;
    out.cols = COLS;
    out.cells.fill(0, ROWS * COLS);
    out.sourceFile = datPath;

    QByteArray datBytes = dat.readAll();
    dat.close();

    const char* data = datBytes.constData();
    int entries = 0x10000;
    
    for (int code = 0; code < entries; ++code) {
        int pos = code * 2;
        quint8 row = static_cast<quint8>(data[pos]);
        quint8 col = static_cast<quint8>(data[pos + 1]);
        if (row == 63 && col == 63) continue;
        if (row < ROWS && col < COLS) {
            out.cells[row * COLS + col] = static_cast<uint16_t>(code);
        }
    }

    return true;
}

bool CharTableParser::loadGtaIv(const QString& datPath, CharTableData& out)
{
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

    const char* data = bytes.constData();
    int dataSize = bytes.size();
    
    quint32 charCount = static_cast<quint8>(data[0]) |
                        (static_cast<quint8>(data[1]) << 8) |
                        (static_cast<quint8>(data[2]) << 16) |
                        (static_cast<quint8>(data[3]) << 24);

    int offset = 4;

    QVector<uint16_t> characters;
    characters.reserve(qMin(charCount, static_cast<quint32>(4096)));

    for (quint32 i = 0; i < charCount && (offset + 4) <= dataSize; ++i) {
        quint32 codepoint = static_cast<quint8>(data[offset]) |
                           (static_cast<quint8>(data[offset + 1]) << 8) |
                           (static_cast<quint8>(data[offset + 2]) << 16) |
                           (static_cast<quint8>(data[offset + 3]) << 24);
        offset += 4;

        if (codepoint != 0x0000 && codepoint != 0x0020 && codepoint != 0x0009 &&
            codepoint != 0x000A && codepoint != 0x000D) {
            if (codepoint <= 0xFFFF) {
                characters.append(static_cast<uint16_t>(codepoint));
            }
        }
    }

    if (characters.isEmpty()) {
        qWarning() << "IV dat 文件不包含有效字符:" << datPath;
        return false;
    }

    int cols = 64;
    int count = characters.size();
    int rows = (count + cols - 1) / cols;

    out.type = CharTableData::GTA_IV;
    out.rows = rows;
    out.cols = cols;
    out.cells.fill(0, rows * cols);
    out.sourceFile = datPath;

    for (int idx = 0; idx < count; ++idx) {
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