#include "SaveWorker.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <fstream>

#include "GXTStudio.h"  // 用于 FileTab 定义
#include "GXTEditor.h"

SaveWorker::SaveWorker(QObject* parent)
    : QObject(parent)
{
}

void SaveWorker::saveFile(const SaveTask& task)
{
    QElapsedTimer timer;
    timer.start();

    SaveResult result;
    result.filePath = task.filePath;
    result.isNewPath = task.isNewPath;
    result.isAutoSave = task.isAutoSave;  // 传递自动保存标志
    
    try {
        if (!task.tabPtr) {
            result.success = false;
            result.errorMessage = "无效的标签页指针";
            emit saveCompleted(result);
            return;
        }
        
        // 设置文件名
        QFileInfo fileInfo(task.filePath);
        result.fileName = fileInfo.fileName();
        
        // 发送进度信号
        emit saveProgress(10, "准备保存...");
        
        emit saveProgress(80, "写入文件...");
        
        bool success = false;

        // 处理WHM文件
        if (task.tabPtr->isWHM) {
            success = saveWHMDirectlyQt(task.tabPtr, task.filePath);
            if (!success) {
                result.errorMessage = "WHM文件保存失败 - 检查日志了解详情";
                qDebug() << "SaveWorker::saveFile - WHM保存失败:" << task.filePath;
            }
        } else if (task.tabPtr->isDAT) {
            // 处理DAT文件（完全独立于WHM）
            success = saveDATDirectlyQt(task.tabPtr, task.filePath);
            if (!success) {
                result.errorMessage = "DAT文件保存失败 - 检查日志了解详情";
                qDebug() << "SaveWorker::saveFile - DAT保存失败:" << task.filePath;
            }
        } else {
            // 使用GXTEditor保存GXT文件
            GXTEditor editor;

            // 设置版本信息
            editor.setVersion(task.tabPtr->version);

            // 检查是否为无表文件（没有TABL块且tables为空）
            if (!task.tabPtr->originalHasTABL && task.tabPtr->tables.empty() && !task.tabPtr->noTablEntries.empty()) {
                // 无表文件：创建一个MAIN表存储键值对
                if (editor.addTable("MAIN")) {
                    size_t tableIndex = editor.getTableCount() - 1;
                    std::vector<std::pair<std::string, std::string>> entriesBatch;
                    entriesBatch.reserve(task.tabPtr->noTablEntries.size());
                    for (const auto& entry : task.tabPtr->noTablEntries) {
                        entriesBatch.emplace_back(entry.key, entry.value);
                    }
                    editor.addEntriesBatch(tableIndex, entriesBatch);
                }
            } else {
                // 有表文件：直接添加表格和条目数据 - 使用批量添加提高性能
                for (const auto& table : task.tabPtr->tables) {
                    if (!editor.addTable(table.name)) {
                        continue; // 跳过无效的表
                    }

                    size_t tableIndex = editor.getTableCount() - 1;
                    // 批量添加条目（跳过重复检查，O(n) 复杂度）
                    std::vector<std::pair<std::string, std::string>> entriesBatch;
                    entriesBatch.reserve(table.entries.size());
                    for (const auto& entry : table.entries) {
                        entriesBatch.emplace_back(entry.key, entry.value);
                    }
                    editor.addEntriesBatch(tableIndex, entriesBatch);
                }
            }

            // 执行保存 - 使用Qt的QFile处理路径
            QFile qfile(task.filePath);
            if (!qfile.open(QIODevice::WriteOnly)) {
                result.errorMessage = QString("无法打开GXT文件: %1").arg(qfile.errorString());
                success = false;
            } else {
                std::string stdPath = task.filePath.toStdString();
                // 如果用户选择保存为 .txt，则使用文本导出，保持内容为纯文本
                if (task.filePath.endsWith(".txt", Qt::CaseInsensitive)) {
                    success = editor.saveAsText(stdPath);
                } else {
                    success = editor.saveToFile(stdPath);
                }
                qfile.close();
                if (!success) {
                    result.errorMessage = "GXT文件保存失败";
                }
            }
        }
        
        emit saveProgress(100, "保存完成");
        
        result.success = success;
        result.bytesWritten = success ? QFileInfo(task.filePath).size() : 0;
        
        if (!success && result.errorMessage.isEmpty()) {
            result.errorMessage = "文件保存失败 - 未知错误";
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = QString("保存异常: %1").arg(e.what());
        qDebug() << "SaveWorker::saveFile - 异常:" << result.errorMessage;
    } catch (...) {
        result.success = false;
        result.errorMessage = "未知保存异常";
        qDebug() << "SaveWorker::saveFile - 未知异常";
    }
    
    result.elapsedMs = timer.elapsed();
    emit saveCompleted(result);
}

bool SaveWorker::saveWHMDirectly(FileTab* tab, const std::string& path)
{
    if (!tab || !tab->isWHM) {
        qDebug() << "SaveWorker::saveWHMDirectly - tab无效或不是WHM文件";
        return false;
    }
    
    try {
        // 直接从QString转换为宽字符路径
        std::wstring widePath(path.begin(), path.end());
        
        // 使用宽字符路径打开文件（支持中文）
        std::ofstream file(widePath, std::ios::binary);
        
        if (!file.is_open()) {
            qDebug() << "SaveWorker::saveWHMDirectly - 无法打开文件:" << path.c_str();
            return false;
        }
        
        // 构建blob（文本数据块）
        std::vector<uint32_t> offsets;
        std::string blob;
        
        for (const auto& entry : tab->whmEntries) {
            offsets.push_back(blob.size());
            const std::string& text = entry.text;
            blob.append(text);
            blob.push_back('\0'); // null终止符
        }
        
        // 写入条目数
        uint32_t count = static_cast<uint32_t>(tab->whmEntries.size());
        file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
        
        // 写入条目表（hash + offset）
        for (size_t i = 0; i < tab->whmEntries.size(); ++i) {
            uint32_t hash = tab->whmEntries[i].hash;
            uint32_t offset = offsets[i];
            file.write(reinterpret_cast<const char*>(&hash), sizeof(uint32_t));
            file.write(reinterpret_cast<const char*>(&offset), sizeof(uint32_t));
        }
        
        // 写入blob大小
        uint32_t blobSize = static_cast<uint32_t>(blob.size());
        file.write(reinterpret_cast<const char*>(&blobSize), sizeof(uint32_t));
        
        // 写入blob数据
        file.write(blob.c_str(), blob.size());
        
        // 检查写入是否成功
        if (!file.good()) {
            qDebug() << "SaveWorker::saveWHMDirectly - 文件写入失败";
            file.close();
            return false;
        }
        
        file.close();
        
        qDebug() << "SaveWorker::saveWHMDirectly - 成功保存WHM文件:" << path.c_str();
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "WHM保存异常:" << e.what();
        return false;
    }
}

// DAT文件保存方法（完全独立于WHM）
bool SaveWorker::saveDATDirectly(FileTab* tab, const std::string& path)
{
    if (!tab || !tab->isDAT) {
        qDebug() << "SaveWorker::saveDATDirectly - tab无效或不是DAT文件";
        return false;
    }
    
    try {
        // 直接从std::string转换为宽字符路径
        std::wstring widePath(path.begin(), path.end());
        
        // 使用宽字符路径打开文件（支持中文）
        std::ofstream file(widePath, std::ios::binary);
        
        if (!file.is_open()) {
            qDebug() << "SaveWorker::saveDATDirectly - 无法打开文件:" << path.c_str();
            return false;
        }
        
        // 构建blob（文本数据块）
        std::vector<uint32_t> offsets;
        std::string blob;
        
        for (const auto& entry : tab->datEntries) {
            offsets.push_back(blob.size());
            const std::string& text = entry.text;
            blob.append(text);
            blob.push_back('\0'); // null终止符
        }
        
        // 写入条目数
        uint32_t count = static_cast<uint32_t>(tab->datEntries.size());
        file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
        
        // 写入条目表（hash + offset）
        for (size_t i = 0; i < tab->datEntries.size(); ++i) {
            uint32_t hash = tab->datEntries[i].hash;
            uint32_t offset = offsets[i];
            file.write(reinterpret_cast<const char*>(&hash), sizeof(uint32_t));
            file.write(reinterpret_cast<const char*>(&offset), sizeof(uint32_t));
        }
        
        // 写入blob大小
        uint32_t blobSize = static_cast<uint32_t>(blob.size());
        file.write(reinterpret_cast<const char*>(&blobSize), sizeof(uint32_t));
        
        // 写入blob数据
        file.write(blob.c_str(), blob.size());
        
        // 检查写入是否成功
        if (!file.good()) {
            qDebug() << "SaveWorker::saveDATDirectly - 文件写入失败";
            file.close();
            return false;
        }
        
        file.close();
        
        qDebug() << "SaveWorker::saveDATDirectly - 成功保存DAT文件:" << path.c_str();
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "DAT保存异常:" << e.what();
        return false;
    }
}

// Qt版本的WHM保存方法 - 使用QFile处理中文路径
bool SaveWorker::saveWHMDirectlyQt(FileTab* tab, const QString& filePath)
{
    if (!tab || !tab->isWHM) {
        qDebug() << "SaveWorker::saveWHMDirectlyQt - tab无效或不是WHM文件";
        return false;
    }
    
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qDebug() << "SaveWorker::saveWHMDirectlyQt - 无法打开文件:" << filePath << "错误:" << file.errorString();
            return false;
        }
        
        // 构建blob（文本数据块）
        std::vector<uint32_t> offsets;
        std::string blob;
        
        for (const auto& entry : tab->whmEntries) {
            offsets.push_back(blob.size());
            const std::string& text = entry.text;
            blob.append(text);
            blob.push_back('\0'); // null终止符
        }
        
        // 写入条目数
        uint32_t count = static_cast<uint32_t>(tab->whmEntries.size());
        file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
        
        // 写入条目表（hash + offset）
        for (size_t i = 0; i < tab->whmEntries.size(); ++i) {
            uint32_t hash = tab->whmEntries[i].hash;
            uint32_t offset = offsets[i];
            file.write(reinterpret_cast<const char*>(&hash), sizeof(uint32_t));
            file.write(reinterpret_cast<const char*>(&offset), sizeof(uint32_t));
        }
        
        // 写入blob大小
        uint32_t blobSize = static_cast<uint32_t>(blob.size());
        file.write(reinterpret_cast<const char*>(&blobSize), sizeof(uint32_t));
        
        // 写入blob数据
        file.write(blob.c_str(), blob.size());
        
        file.close();
        
        qDebug() << "SaveWorker::saveWHMDirectlyQt - 成功保存WHM文件:" << filePath;
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "WHM保存异常:" << e.what();
        return false;
    }
}

// Qt版本的DAT保存方法 - 使用QFile处理中文路径
bool SaveWorker::saveDATDirectlyQt(FileTab* tab, const QString& filePath)
{
    if (!tab || !tab->isDAT) {
        qDebug() << "SaveWorker::saveDATDirectlyQt - tab无效或不是DAT文件";
        return false;
    }
    
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qDebug() << "SaveWorker::saveDATDirectlyQt - 无法打开文件:" << filePath << "错误:" << file.errorString();
            return false;
        }
        
        // 构建blob（文本数据块）
        std::vector<uint32_t> offsets;
        std::string blob;
        
        for (const auto& entry : tab->datEntries) {
            offsets.push_back(blob.size());
            const std::string& text = entry.text;
            blob.append(text);
            blob.push_back('\0'); // null终止符
        }
        
        // 写入条目数
        uint32_t count = static_cast<uint32_t>(tab->datEntries.size());
        file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
        
        // 写入条目表（hash + offset）
        for (size_t i = 0; i < tab->datEntries.size(); ++i) {
            uint32_t hash = tab->datEntries[i].hash;
            uint32_t offset = offsets[i];
            file.write(reinterpret_cast<const char*>(&hash), sizeof(uint32_t));
            file.write(reinterpret_cast<const char*>(&offset), sizeof(uint32_t));
        }
        
        // 写入blob大小
        uint32_t blobSize = static_cast<uint32_t>(blob.size());
        file.write(reinterpret_cast<const char*>(&blobSize), sizeof(uint32_t));
        
        // 写入blob数据
        file.write(blob.c_str(), blob.size());
        
        file.close();
        
        qDebug() << "SaveWorker::saveDATDirectlyQt - 成功保存DAT文件:" << filePath;
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "DAT保存异常:" << e.what();
        return false;
    }
}
