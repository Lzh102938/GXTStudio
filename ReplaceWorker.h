#ifndef REPLACEWORKER_H
#define REPLACEWORKER_H

#include <QObject>
#include <QString>
#include <vector>
#include <tuple>
#include <set>
#include <atomic>

class GXTStudio;  // 前向声明
class FileTab;    // 前向声明

class ReplaceWorker : public QObject
{
    Q_OBJECT

public:
    struct ReplaceTask {
        QString findText;
        QString replaceText;
        bool caseSensitive;
        std::vector<std::tuple<int, int, int>> matches; // (tableIndex, entryIndex, position)
        std::set<std::pair<int, int>> processedEntries; // 已处理的条目
        FileTab* currentTab; // 当前标签页指针
    };

    explicit ReplaceWorker(GXTStudio* mainWindow, QObject *parent = nullptr);
    ~ReplaceWorker();

    // 设置替换任务
    void setReplaceTask(const ReplaceTask& task);
    
public slots:
    // 开始替换操作
    void startReplace();
    
public:
    // 请求取消操作
    void requestCancel();

signals:

    // 进度更新信号
    void progressUpdated(int completed, int total, const QString& message);
    
    // 替换完成信号
    void replaceFinished(int replacedEntries, const QString& message);
    
    // 错误信号
    void errorOccurred(const QString& error);
    
    // 单个条目替换完成信号
    void entryReplaced(int tableIndex, int entryIndex, int replaceCount);

private slots:
    void processReplace();

private:
    ReplaceTask m_task;
    std::atomic<bool> m_cancelRequested;
    GXTStudio* m_mainWindow;  // 主窗口指针
};

#endif // REPLACEWORKER_H