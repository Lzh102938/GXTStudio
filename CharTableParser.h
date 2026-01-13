#pragma once

#include <QString>
#include <QVector>

struct CharTableData {
    enum Type { GTA_VC, GTA_IV } type = GTA_VC;
    int rows = 0;
    int cols = 0;
    QVector<uint16_t> cells; // row-major, 0 means empty
    QString sourceFile;
};

class CharTableParser {
public:
    // Load GTA:VC char table: dat mapping (wm_vcchs.dat) contains mapping from codepoint -> (row,col)
    // This function reads the dat directly and does not require an external CHARACTERS.txt
    static bool loadGtaVc(const QString& datPath, CharTableData& out);
    // Load GTA:IV char table - best-effort (try reading as sequence of UTF-16LE characters)
    static bool loadGtaIv(const QString& datPath, CharTableData& out);
    // Save GTA:VC char table
    static bool saveGtaVc(const QString& datPath, const CharTableData& data);
    // Save GTA:IV char table
    static bool saveGtaIv(const QString& datPath, const CharTableData& data);
};
