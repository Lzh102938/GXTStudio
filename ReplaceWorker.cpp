#include "ReplaceWorker.h"
#include "GXTStudio.h"
#include <QDebug>
#include <QApplication>
#include <QTimer>

ReplaceWorker::ReplaceWorker(GXTStudio* mainWindow, QObject *parent)
    : QObject(parent)
    , m_cancelRequested(false)
    , m_mainWindow(mainWindow)
{
}

ReplaceWorker::~ReplaceWorker()
{
    requestCancel();
    // 简单的析构函数，不进行复杂的线程管理
}

void ReplaceWorker::setReplaceTask(const ReplaceTask& task)
{
    m_task = task;
    m_cancelRequested = false;
}

void ReplaceWorker::startReplace()
{
    // 重置取消标志
    m_cancelRequested = false;
    
    // 使用单次定时器来模拟后台执行，避免阻塞UI
    QTimer::singleShot(0, this, [this]() {
        processReplace();
    });
    
    emit progressUpdated(0, m_task.processedEntries.size(), "开始替换操作...");
}

void ReplaceWorker::requestCancel()
{
    m_cancelRequested = true;
}

void ReplaceWorker::processReplace()
{
    int totalEntries = m_task.processedEntries.size();
    int completedEntries = 0;
    int totalReplacements = 0;
    
    // 主窗口指针将在构造函数中设置
    GXTStudio* mainWindow = m_mainWindow;
    if (!mainWindow) {
        emit errorOccurred("未设置主窗口实例");
        return;
    }
    
    // 检查标签页有效性
    FileTab* currentTab = m_task.currentTab;
    if (!currentTab) {
        emit errorOccurred("无效的标签页指针");
        return;
    }
    
    try {
        // 遍历所有需要处理的条目
        for (const auto& entryKey : m_task.processedEntries) {
            if (m_cancelRequested) {
                emit progressUpdated(completedEntries, totalEntries, "操作已取消");
                emit replaceFinished(completedEntries, "替换操作已取消");
                return;
            }
            
            int tableIndex = entryKey.first;
            int entryIndex = entryKey.second;
            
            // 使用传递的当前标签页
            FileTab* tab = currentTab;  // 使用预先检查过的指针
            // 再次检查表格索引有效性
            if (tableIndex < 0 || tableIndex >= static_cast<int>(tab->tables.size())) {
                continue;  // 跳过无效的表格索引
            }
            if (entryIndex < 0 || entryIndex >= static_cast<int>(tab->tables[tableIndex].entries.size())) {
                continue;  // 跳过无效的条目索引
            }
            
            // 直接计算预期替换次数（基于原始值）
            QString oldValue = QString::fromStdString(tab->tables[tableIndex].entries[entryIndex].value);
            Qt::CaseSensitivity cs = m_task.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
            int replacements = 0;
            int pos = 0;
            while ((pos = oldValue.indexOf(m_task.findText, pos, cs)) != -1) {
                replacements++;
                pos += m_task.findText.length();
            }
            
            // 使用线程安全的方式调用主线程方法执行实际替换
            bool invokeSuccess = QMetaObject::invokeMethod(mainWindow, "replaceAllMatchesInEntry", Qt::QueuedConnection,
                                    Q_ARG(FileTab*, tab),
                                    Q_ARG(int, tableIndex),
                                    Q_ARG(int, entryIndex),
                                    Q_ARG(QString, m_task.findText),
                                    Q_ARG(QString, m_task.replaceText),
                                    Q_ARG(bool, m_task.caseSensitive));
            
            if (!invokeSuccess) {
                emit errorOccurred(QString("调用replaceAllMatchesInEntry失败: 表%1 条目%2").arg(tableIndex).arg(entryIndex));
            }
            
            completedEntries++;
            totalReplacements += replacements;
            
            // 更新进度
            QString message = QString("正在替换条目 %1/%2...").arg(completedEntries).arg(totalEntries);
            emit progressUpdated(completedEntries, totalEntries, message);
            emit entryReplaced(tableIndex, entryIndex, replacements);
            
            // 【性能优化】移除 msleep，Qt 信号槽机制会自动处理 UI 更新
        }
        
        QString finishMessage = QString("替换完成！共处理 %1 个条目，执行 %2 次替换")
                              .arg(completedEntries).arg(totalReplacements);
        emit replaceFinished(completedEntries, finishMessage);
        
    } catch (const std::exception& e) {
        emit errorOccurred(QString("替换过程中发生错误: %1").arg(e.what()));
    }
}