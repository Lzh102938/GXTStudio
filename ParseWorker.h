#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QString>
#include <vector>

#include "GXTParser.h"

// 前向声明
struct FileTab;

// 解析任务数据结构
struct ParseTask {
    QString filePath;
    QString fileName;
    std::string narrowPath;
    bool isWHM;
    bool isWHMRSC;  // 压缩的 HTML 文档格式
    bool isDAT;  // 完全区分DAT和WHM
    int tabIndex;
};

// 解析结果数据结构
struct ParseResult {
    bool success;
    QString filePath;
    QString fileName;
    bool isWHM;
    bool isWHMRSC;  // 压缩的 HTML 文档格式
    bool isDAT;  // 完全区分DAT和WHM
    std::vector<GXTTabl> tables;
    std::vector<WHMEntry> whmEntries;
    std::vector<DATEntry> datEntries;  // DAT条目（完全独立）
    QString errorMessage;
    int tabIndex;
    QElapsedTimer parseTime;
    GXTVersion version; // 文件版本信息
};

// 子线程解析器
class ParseWorker : public QObject {
    Q_OBJECT

public:
    ParseWorker(QObject* parent = nullptr);

public slots:
    void parseFile(const ParseTask& task);

signals:
    void parseCompleted(const ParseResult& result);
};
