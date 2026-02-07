#pragma once

#include <QObject>
#include <QString>
#include <vector>

#include "GXTParser.h"

// 前向声明
struct FileTab;

// 保存任务数据结构
struct SaveTask {
    FileTab* tabPtr;
    QString filePath;
    bool isNewPath;
    bool isAutoSave;  // 是否为自动保存
};

// 保存结果数据结构
struct SaveResult {
    bool success;
    QString filePath;
    QString fileName;
    QString errorMessage;
    qint64 elapsedMs;
    size_t bytesWritten;
    bool isNewPath;
    bool isAutoSave;  // 是否为自动保存
};

// 自动保存数据副本 - 完全独立于主线程数据
struct AutoSaveData {
    QString filePath;
    GXTVersion version;
    bool isWHM;
    bool isWHMRSC;
    bool isDAT;
    // GXT/GXT2 数据 (使用GXTTabl与FileTab一致)
    std::vector<GXTTabl> tables;
    // WHM 数据
    std::vector<WHMEntry> whmEntries;
    // DAT 数据
    std::vector<DATEntry> datEntries;
};

// 子线程保存器
class SaveWorker : public QObject {
    Q_OBJECT

public:
    explicit SaveWorker(QObject* parent = nullptr);

public slots:
    void saveFile(const SaveTask& task);

private:
    // 保存WHM文件的辅助方法（用于SaveWorker）
    bool saveWHMDirectly(FileTab* tab, const std::string& path);
    // DAT文件保存方法（完全独立于WHM）
    bool saveDATDirectly(FileTab* tab, const std::string& path);
    // Qt版本的保存方法，使用QString处理中文路径
    bool saveWHMDirectlyQt(FileTab* tab, const QString& filePath);
    bool saveDATDirectlyQt(FileTab* tab, const QString& filePath);
    
signals:
    void saveProgress(int percentage, const QString& message);
    void saveCompleted(const SaveResult& result);
};
