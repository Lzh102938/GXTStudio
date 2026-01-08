#pragma once

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QMetaObject>
#include <QtCore/QGenericArgument>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDateTime>
#include <QtCore/QSharedPointer>
#include <memory>

struct TranslateTask {
    QString key;
    QString value;
    int row;
    
    TranslateTask(const QString& k, const QString& v, int r) 
        : key(k), value(v), row(r) {}
};

struct TranslateResult {
    QString key;           // 添加key字段，用于查找对应条目
    QString originalValue;
    QString translatedValue;
    int row;
    bool success;
    QString errorMessage;
    
    TranslateResult(const QString& k, const QString& orig, int r) 
        : key(k), originalValue(orig), translatedValue(orig), row(r), success(false) {}
};

struct BatchRequest;

class SmartTranslator : public QObject
{
    Q_OBJECT

public:
    explicit SmartTranslator(QObject *parent = nullptr);
    ~SmartTranslator();
    
    // 添加性能监控
    qint64 getTotalTranslationTime() const { return m_totalTranslationTime; }
    int getSuccessfulRequests() const { return m_successfulRequests; }
    int getFailedRequests() const { return m_failedRequests; }
    double getSuccessRate() const {
        int total = m_successfulRequests + m_failedRequests;
        return total > 0 ? (double)m_successfulRequests / total : 0.0;
    }
    int getCurrentRequestInterval() const { return m_currentRequestInterval; }
    bool isRateLimited() const { return m_currentRequestInterval >= RATE_LIMIT_DELAY; }
    int getAverageResponseTime() const { return m_averageResponseTime; }
    int getAdaptiveConcurrentLimit() const { return m_adaptiveConcurrentLimit; }
    
    // 获取当前已完成的翻译结果
    QList<TranslateResult> getCurrentResults() const;
    
    // 添加线程安全检查
    bool isThreadAlive() const;
    
    bool isReady() const;
    
    static const int DEFAULT_BATCH_SIZE = 32;   // 优化批次大小
    static const int DEFAULT_MAX_CONCURRENT_REQUESTS = 12;  // 降低并发数以减轻服务器压力
    static const int DEFAULT_MAX_RETRIES = 3;               // 参考Python版本，减少重试次数
    static const int REQUEST_TIMEOUT = 180; // 180秒超时，提高稳定性
    
    // 智能频率控制常量
    static const int MIN_REQUEST_INTERVAL = 100;  // 增加最小请求间隔(ms)
    static const int MAX_REQUEST_INTERVAL = 5000; // 增加最大请求间隔(ms)
    static const int RATE_LIMIT_DELAY = 10000;   // 触发限流后的延迟(ms)
    static const int CONSECUTIVE_ERRORS_THRESHOLD = 2; // 降低连续错误阈值
    
    // 内存安全限制 - 优化限制以支持更高并发
    static const int MAX_SAFE_TASKS = 50000;       // 提高任务限制
    static const int MAX_MEMORY_USAGE_MB = 512;     // 提高内存限制
    static const int MAX_QLIST_SAFE_ITEMS = 100000;  // 提高QList限制
    
    // 动态配置 - 添加Q_INVOKABLE以便通过元对象系统调用
    Q_INVOKABLE void setBatchSize(int size);
    Q_INVOKABLE void setMaxConcurrentRequests(int count);
    Q_INVOKABLE void setMaxRetries(int retries);

public slots:
    void setApiKey(const QString& apiKey);
    void setSystemPrompt(const QString& prompt);
    void setBatchPrompt(const QString& prompt);
    
    // 主要翻译入口，参考Python版本的异步并发逻辑
    void translateTexts(const QList<TranslateTask>& tasks);
    void cancelTranslation();

signals:
    void translationProgress(int completed, int total, const QString& message);
    void translationCompleted(const QList<TranslateResult>& results);
    void translationError(const QString& error);
    void translationCancelled();

private slots:
    void onBatchRequestFinished(QSharedPointer<BatchRequest> batchRequest, bool success, const QString& result);
    void processNextBatch();

private:
    // 异步批处理方法，参考Python版本
    void translateBatchAsync(const QList<TranslateTask>& batch, int batchId);
    void sendBatchRequest(const QList<TranslateTask>& batch, int batchId);
    void handleBatchResult(QSharedPointer<BatchRequest> batchRequest, bool success, const QString& result);
    QList<TranslateResult> parseBatchResult(const QString& result, const QList<TranslateTask>& originalTasks);
    QString buildBatchContent(const QList<TranslateTask>& tasks);
    void cleanupBatchRequest(QSharedPointer<BatchRequest> batchRequest);

    // 智能频率控制方法
    int calculateAdaptiveDelay();
    void updateRequestStats(bool success);
    void updateResponseTime(qint64 responseTimeMs);
    void checkStuckRequests();  // 定期检查卡住的请求

    // 结构化内容过滤辅助函数（静态，不依赖实例状态）
    static bool isConfirmationPhrase(const QString& text);  // 判断是否为API确认短语
    static bool isTechnicalMetadata(const QString& text);    // 判断是否为技术性元信息

private:
    QString m_apiKey;
    QString m_systemPrompt;
    QString m_batchPrompt;
    
    // 动态性能配置
    int m_batchSize;
    int m_maxConcurrentRequests;
    int m_maxRetries;
    
    QList<TranslateTask> m_pendingTasks;
    QList<TranslateResult> m_results;
    QList<QSharedPointer<BatchRequest>> m_activeRequests;
    
    int m_completedCount;    // 成功完成的任务数
    int m_processedCount;     // 已处理的任务数（包括失败的）
    int m_totalCount;
    int m_currentBatchId;
    bool m_cancelled;
    int m_consecutiveErrors;  // 连续错误计数器
    
    // 智能频率控制
    int m_currentRequestInterval;        // 当前请求间隔
    int m_consecutiveErrorCount;        // 连续错误计数
    QElapsedTimer m_lastRequestTime;      // 最后请求时间
    QElapsedTimer m_rateLimitTimer;       // 限流计时器

    // 超时检测定时器（独立的卡住请求检测）
    QTimer* m_timeoutCheckTimer;
    
    // 性能统计
    qint64 m_totalTranslationTime;
    qint64 m_successfulRequests;     // 使用 qint64 防止溢出
    qint64 m_failedRequests;         // 使用 qint64 防止溢出
    qint64 m_totalResponseTime;       // 总响应时间
    int m_responseSampleCount;        // 响应时间样本数
    int m_averageResponseTime;        // 平均响应时间
    int m_adaptiveConcurrentLimit;     // 自适应并发限制
    
    mutable QMutex m_mutex;
    
    static const QString API_URL;
};

// BatchRequest的完整定义放在类定义之后
struct BatchRequest {
    QList<TranslateTask> tasks;
    int batchId;
    int retryCount;
    QString result;
    bool completed;
    QDateTime lastUpdateTime;
    
    BatchRequest() : batchId(0), retryCount(0), completed(false) {
        lastUpdateTime = QDateTime::currentDateTime();
    }
};