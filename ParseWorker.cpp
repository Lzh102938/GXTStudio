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
    result.originalHasTABL = true;  // 默认为true
    result.tabIndex = task.tabIndex;
    result.parseTime = timer;
    result.success = false;

    try {
        GXTParser parser;

        parser.setLogCallback([](const std::string& msg) {
            qDebug() << "解析日志:" << QString::fromStdString(msg);
        });

        parser.setProgressCallback([this](int percentage, const std::string& message) {
            emit parseProgress(percentage, QString::fromStdString(message));
        });

        if (task.isDAT) {
            emit parseProgress(5, "正在解析DAT文件...");
            result.datEntries.clear();
            result.success = parser.parseDAT(task.narrowPath, result.datEntries);
            result.version = GXTVersion::UNKNOWN;
            if (!result.success) {
                result.errorMessage = "DAT解析失败";
            }
            emit parseProgress(100, "解析完成");
        } else if (task.isWHM) {
            emit parseProgress(5, "正在解析WHM文件...");
            result.whmEntries.clear();
            result.isWHM = true;
            result.isWHMRSC = task.isWHMRSC;
            result.version = GXTVersion::UNKNOWN;

            if (task.isWHMRSC) {
                emit parseProgress(10, "检测到RSC格式，正在解压...");
                qDebug() << "检测到RSC格式，使用增强WHM解析器";
                result.success = parser.parseWHMEx(task.narrowPath, result.whmEntries);
                
                if (!result.success) {
                    qDebug() << "增强解析器失败，尝试旧版RSC解析器";
                    result.success = parser.parseWHMRSC(task.narrowPath, result.whmEntries);
                }
            } else {
                emit parseProgress(10, "正在解析WHM数据...");
                result.success = parser.parseWHM(task.narrowPath, result.whmEntries);
            }

            if (!result.success) {
                result.errorMessage = result.isWHMRSC ? "WHM/RSC解析失败" : "WHM解析失败";
            }
            emit parseProgress(100, "解析完成");
        } else {
            emit parseProgress(5, "正在解析GXT文件...");
            result.success = parser.parse(task.narrowPath);
            result.version = parser.getDetectedVersion();
            if (result.success) {
                result.tables = parser.getTables();
                result.noTablEntries = parser.getNoTablEntries();
                result.originalHasTABL = parser.getOriginalHasTABL();
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
