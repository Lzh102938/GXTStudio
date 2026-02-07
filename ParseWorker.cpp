#include "ParseWorker.h"

#include <QDebug>
#include <QElapsedTimer>

ParseWorker::ParseWorker(QObject* parent)
    : QObject(parent)
{
}

void ParseWorker::parseFile(const ParseTask& task)
{
    QElapsedTimer timer;
    timer.start();

    ParseResult result;
    result.filePath = task.filePath;
    result.fileName = task.fileName;
    result.isWHM = task.isWHM;
    result.isDAT = task.isDAT;
    result.tabIndex = task.tabIndex;
    result.parseTime = timer;
    result.success = false;

    try {
        GXTParser parser;

        // 设置日志回调（但在子线程中不直接调用UI）
        parser.setLogCallback([](const std::string& msg) {
            // 在子线程中只记录日志，不直接更新UI
            qDebug() << "解析日志:" << QString::fromStdString(msg);
        });

        if (task.isDAT) {
            // 解析DAT文件（完全独立于WHM）
            result.datEntries.clear();
            result.success = parser.parseDAT(task.narrowPath, result.datEntries);
            result.version = GXTVersion::UNKNOWN; // DAT文件没有版本信息
            if (!result.success) {
                result.errorMessage = "DAT解析失败";
            }
        } else if (task.isWHM) {
            // 解析WHM文件
            result.whmEntries.clear();
            result.isWHM = true;
            result.isWHMRSC = task.isWHMRSC;
            result.version = GXTVersion::UNKNOWN;

            if (task.isWHMRSC) {
                // RSC格式（压缩的HTML文档）- 使用增强解析器
                qDebug() << "检测到RSC格式，使用增强WHM解析器";
                result.success = parser.parseWHMEx(task.narrowPath, result.whmEntries);
                
                // 如果增强解析器失败，回退到旧版RSC解析器
                if (!result.success) {
                    qDebug() << "增强解析器失败，尝试旧版RSC解析器";
                    result.success = parser.parseWHMRSC(task.narrowPath, result.whmEntries);
                }
            } else {
                // 简单 WHM 格式
                result.success = parser.parseWHM(task.narrowPath, result.whmEntries);
            }

            if (!result.success) {
                result.errorMessage = result.isWHMRSC ? "WHM/RSC解析失败" : "WHM解析失败";
            }
        } else {
            // 解析GXT文件
            result.success = parser.parse(task.narrowPath);
            result.version = parser.getDetectedVersion(); // 获取检测到的版本
            if (result.success) {
                result.tables = parser.getTables();
            } else {
                result.errorMessage = "GXT解析失败";
            }
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = QString("解析异常: %1").arg(e.what());
    }

    qint64 elapsed = timer.elapsed();
    qDebug() << "解析完成 - 文件:" << result.fileName << "成功:" << result.success << "耗时:" << elapsed << "ms";

    emit parseCompleted(result);
}
