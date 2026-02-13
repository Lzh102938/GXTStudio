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
    // IV字符表是UTF-16LE编码的字符序列，参考Python实现
    QFile f(datPath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开 IV dat 文件:" << datPath;
        return false;
    }

    QByteArray bytes = f.readAll();
    f.close();

    if (bytes.isEmpty()) {
        qWarning() << "IV dat 文件为空:" << datPath;
        return false;
    }

    // 检查并跳过BOM (Byte Order Mark: 0xFF 0xFE for UTF-16LE)
    int offset = 0;
    if (bytes.size() >= 2) {
        quint8 byte0 = static_cast<quint8>(bytes[0]);
        quint8 byte1 = static_cast<quint8>(bytes[1]);
        if (byte0 == 0xFF && byte1 == 0xFE) {
            offset = 2;
        }
    }

    // 读取UTF-16LE字符序列，过滤掉空白字符和换行符
    QVector<uint16_t> characters;
    characters.reserve(bytes.size() / 2); // 预分配空间以提高性能

    for (int i = offset; i + 1 < bytes.size(); i += 2) {
        // 小端序：低字节在前，高字节在后
        quint8 lo = static_cast<quint8>(bytes[i]);
        quint8 hi = static_cast<quint8>(bytes[i + 1]);
        quint16 w = static_cast<quint16>((hi << 8) | lo);

        // 跳过空白字符：空字符(0x0000)、空格(0x0020)、制表符(0x0009)、换行符(0x000A)、回车符(0x000D)
        if (w != 0x0000 && w != 0x0020 && w != 0x0009 && w != 0x000A && w != 0x000D) {
            characters.append(w);
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

    // GTA IV char_table.dat 格式：UTF-16LE字符序列，无BOM
    // 按行优先顺序写入有效字符（跳过空白字符：0x0000, 0x0020, 0x0009, 0x000A, 0x000D）
    for (int idx = 0; idx < data.cells.size(); ++idx) {
        uint16_t ch = data.cells[idx];
        // 跳过空白字符（与loadGtaIv保持一致）
        if (ch != 0x0000 && ch != 0x0020 && ch != 0x0009 && ch != 0x000A && ch != 0x000D) {
            // 小端序：低字节在前，高字节在后
            file.putChar(static_cast<char>(ch & 0xFF));
            file.putChar(static_cast<char>((ch >> 8) & 0xFF));
        }
    }

    file.close();
    return true;
}
