#include "SmartTranslator.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QRegularExpression>
#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

const QString SmartTranslator::API_URL = "https://api.xiaomimimo.com/v1/chat/completions";

SmartTranslator::SmartTranslator(QObject *parent)
    : QObject(parent)
    , m_batchSize(DEFAULT_BATCH_SIZE)                    // 32行每批次
    , m_maxConcurrentRequests(DEFAULT_MAX_CONCURRENT_REQUESTS)  // 降低至12个并发请求
    , m_maxRetries(DEFAULT_MAX_RETRIES)                   // 3次重试
    , m_completedCount(0)
    , m_processedCount(0)
    , m_totalCount(0)
    , m_currentBatchId(0)
    , m_cancelled(false)
    , m_consecutiveErrors(0)
    , m_currentRequestInterval(MIN_REQUEST_INTERVAL)
    , m_consecutiveErrorCount(0)
    , m_totalResponseTime(0)
    , m_responseSampleCount(0)
    , m_averageResponseTime(0)
    , m_adaptiveConcurrentLimit(DEFAULT_MAX_CONCURRENT_REQUESTS)
    , m_timeoutCheckTimer(nullptr)
    , m_totalTranslationTime(0)
    , m_successfulRequests(0)     // 初始化为0
    , m_failedRequests(0)         // 初始化为0
{
    qDebug() << "初始化SmartTranslator，使用" << DEFAULT_MAX_CONCURRENT_REQUESTS << "并发线程和事件驱动架构";

    // 初始化计时器
    m_lastRequestTime.start();
    m_rateLimitTimer.start();

    // 创建超时检测定时器（每5秒检查一次）
    m_timeoutCheckTimer = new QTimer(this);
    m_timeoutCheckTimer->setInterval(5000);
    connect(m_timeoutCheckTimer, &QTimer::timeout, this, &SmartTranslator::checkStuckRequests);
}

SmartTranslator::~SmartTranslator()
{

    // 确保翻译被取消
    cancelTranslation();

    // 清理所有活动批次请求 - 智能指针会自动管理内存
    {
        QMutexLocker locker(&m_mutex);
        qDebug() << "析构函数: activeRequests=" << m_activeRequests.size()
                   << "completed=" << m_completedCount << "total=" << m_totalCount;
        m_activeRequests.clear();  // 清空列表，智能指针自动释放
    }

}

void SmartTranslator::setApiKey(const QString& apiKey)
{
    QMutexLocker locker(&m_mutex);
    m_apiKey = apiKey.trimmed();
}

void SmartTranslator::setSystemPrompt(const QString& prompt)
{
    QMutexLocker locker(&m_mutex);
    m_systemPrompt = prompt;
}

void SmartTranslator::setBatchPrompt(const QString& prompt)
{
    QMutexLocker locker(&m_mutex);
    m_batchPrompt = prompt;
}

void SmartTranslator::setBatchSize(int size)
{
    QMutexLocker locker(&m_mutex);
    m_batchSize = size;
}

void SmartTranslator::setMaxConcurrentRequests(int count)
{
    QMutexLocker locker(&m_mutex);
    m_maxConcurrentRequests = count;
}

void SmartTranslator::setMaxRetries(int retries)
{
    QMutexLocker locker(&m_mutex);
    m_maxRetries = retries;
}

bool SmartTranslator::isReady() const
{
    QMutexLocker locker(&m_mutex);
    return !m_apiKey.trimmed().isEmpty();
}

QList<TranslateResult> SmartTranslator::getCurrentResults() const
{
    QMutexLocker locker(&m_mutex);
    return m_results;
}

bool SmartTranslator::isThreadAlive() const
{
    // 检查当前线程是否仍在运行
    QThread* currentThread = QThread::currentThread();
    return currentThread && !currentThread->isInterruptionRequested();
}

void SmartTranslator::translateTexts(const QList<TranslateTask>& tasks)
{
    
    // 检查线程状态
    if (QThread::currentThread()->isInterruptionRequested()) {
        return;
    }

    // 基本检查
    if (tasks.isEmpty()) {
        emit translationError("没有需要翻译的内容");
        return;
    }
    
    // 安全检查
    if (tasks.size() > MAX_QLIST_SAFE_ITEMS) {
        emit translationError(QString("任务数量 %1 超过QList安全限制 %2，可能导致程序崩溃")
                            .arg(tasks.size()).arg(MAX_QLIST_SAFE_ITEMS));
        return;
    }
    
    if (tasks.size() > MAX_SAFE_TASKS) {
        emit translationError(QString("任务数量 %1 超过安全限制 %2").arg(tasks.size()).arg(MAX_SAFE_TASKS));
        return;
    }
    
    // 验证每个任务
    for (int i = 0; i < tasks.size(); ++i) {
        const auto& task = tasks[i];
        if (task.value.length() > 10000 || task.key.length() > 1000) {
            emit translationError(QString("任务 %1 数据过长").arg(i));
            return;
        }
    }
    
    {
        QMutexLocker locker(&m_mutex);
        qDebug() << "translateTexts: activeRequests=" << m_activeRequests.size()
                   << "completed=" << m_completedCount << "total=" << m_totalCount;
        if (m_apiKey.trimmed().isEmpty()) {
            emit translationError("API密钥未配置");
            return;
        }

        // 重置状态
        m_cancelled = false;
        m_consecutiveErrors = 0;

        // 安全地复制任务列表
        m_pendingTasks.clear();
        m_results.clear();

        try {
            m_pendingTasks = tasks;
            m_results.reserve(tasks.size());
        } catch (...) {
            emit translationError("内存不足，无法复制任务列表");
            return;
        }

        m_completedCount = 0;
        m_totalCount = tasks.size();
        m_currentBatchId = 0;

        // 清理之前的请求
        for (const auto& request : m_activeRequests) {
            cleanupBatchRequest(request);
        }
        m_activeRequests.clear();
    }

    // 启动超时检测定时器
    if (m_timeoutCheckTimer) {
        m_timeoutCheckTimer->start();
    }

    emit translationProgress(0, tasks.size(), QString("开始翻译 %1 条 | 批次:%2 | 自适应并发:%3")
                            .arg(tasks.size())
                            .arg(m_batchSize)
                            .arg(m_adaptiveConcurrentLimit));

   qDebug() << "总任务数:" << tasks.size() << "批次大小:" << m_batchSize << "自适应并发:" << m_adaptiveConcurrentLimit;

    // 事件驱动：立即开始并发处理
    QTimer::singleShot(0, this, &SmartTranslator::processNextBatch);
}

void SmartTranslator::cancelTranslation()
{

    // 停止超时检测定时器
    if (m_timeoutCheckTimer) {
        m_timeoutCheckTimer->stop();
    }

    QMutexLocker locker(&m_mutex);
    m_cancelled = true;

    // 【修改】不清空 m_results，保留已完成的翻译结果
    // 之前的行为是清空结果，现在改为保留
    // 清空活跃请求列表（智能指针会自动释放）
    m_activeRequests.clear();

    locker.unlock();

    emit translationCancelled();
}

void SmartTranslator::processNextBatch()
{
    // 检查线程是否仍在运行
    if (QThread::currentThread()->isInterruptionRequested()) {
        return;
    }

    QList<TranslateTask> batch;
    bool shouldFinish = false;

    {
        QMutexLocker locker(&m_mutex);

        if (m_cancelled) {
            return;
        }

        // 检查是否所有任务完成（使用已处理数而不是完成数）
        if (m_pendingTasks.isEmpty() && m_activeRequests.isEmpty() && m_processedCount >= m_totalCount) {
            shouldFinish = true;
        qDebug() << "active:" << m_activeRequests.size()
                   << "completed:" << m_completedCount << "processed:" << m_processedCount << "total:" << m_totalCount;
        }
        // 【修复】检测异常状态：无待处理任务但活跃请求为空且已处理数不足
        else if (m_pendingTasks.isEmpty() && m_activeRequests.isEmpty() && m_processedCount < m_totalCount) {
        qDebug() << " - processed:" << m_processedCount << "total:" << m_totalCount << "，强制完成";
            shouldFinish = true;
        }
        // 检查并发限制和频率控制
        else if (!m_pendingTasks.isEmpty() && m_activeRequests.size() < m_adaptiveConcurrentLimit) {
            // 检查当前请求频率，避免过于密集的请求
            qint64 timeSinceLastRequest = m_lastRequestTime.elapsed();
            if (timeSinceLastRequest < m_currentRequestInterval) {
                // 请求过于频繁，延迟后重试
                QTimer::singleShot(m_currentRequestInterval - timeSinceLastRequest, this, &SmartTranslator::processNextBatch);
                return;
            }

            // 提取下一个批次
            int batchSize = qMin(m_batchSize, m_pendingTasks.size());
            batchSize = qMin(batchSize, 64);

            // 记录请求时间
            m_lastRequestTime.restart();

            if (batchSize > 0) {
                try {
                    batch.reserve(batchSize);
                    for (int i = 0; i < batchSize && !m_pendingTasks.isEmpty(); ++i) {
                        const auto& task = m_pendingTasks.takeFirst();
                        if (task.value.length() <= 10000 && task.key.length() <= 1000) {
                            batch.append(task);
                        }
                    }
                } catch (...) {
                    return;
                }
            }

        qDebug() << "active=" << m_activeRequests.size()
                   << "completed=" << m_completedCount << "total=" << m_totalCount;
        }
        // 没有待处理任务但有活跃请求，等待请求完成（事件驱动，不需要重试）
        else if (m_pendingTasks.isEmpty() && !m_activeRequests.isEmpty()) {
        // 等待活跃请求完成，不需要做任何事情
        // handleBatchResult 会触发下一次 processNextBatch
            return;
        }
        // 达到并发限制，等待有空位
        else {
        // 等待有空闲槽位，不需要做任何事情
        // handleBatchResult 会触发下一次 processNextBatch
            return;
        }
    }

    if (shouldFinish) {
        // 所有任务完成
        qWarning() << "所有任务完成";

        // 停止超时检测定时器
        if (m_timeoutCheckTimer) {
            m_timeoutCheckTimer->stop();
        }

        QList<TranslateResult> results;
        {
            QMutexLocker locker(&m_mutex);
            results = m_results;
        }

        // 计算统计信息
        int successCount = 0;
        int failureCount = 0;
        for (const auto& result : results) {
            if (result.success) {
                successCount++;
            } else {
                failureCount++;
            }
        }

        QString progressMsg = QString("翻译完成 | 成功:%1 失败:%2 总计:%3")
                            .arg(successCount)
                            .arg(failureCount)
                            .arg(m_totalCount);

        // 确保最后发送一次100%的进度
        emit translationProgress(m_totalCount, m_totalCount, progressMsg);
        emit translationCompleted(results);
        return;
    }

    if (!batch.isEmpty()) {

        // 异步处理批次
        translateBatchAsync(batch, m_currentBatchId);

        // 事件驱动：不需要立即重试，等待请求完成
        // handleBatchResult 会触发下一次 processNextBatch
        // 如果还有待处理任务且有空闲槽位，可以延迟后再次尝试
        if (!m_pendingTasks.isEmpty()) {
            QTimer::singleShot(m_currentRequestInterval, this, &SmartTranslator::processNextBatch);
        }
    }
}

void SmartTranslator::translateBatchAsync(const QList<TranslateTask>& batch, int batchId)
{
    qDebug() << "任务数:" << batch.size();

    // 创建批次请求对象 - 使用智能指针管理
    QSharedPointer<BatchRequest> batchRequest = QSharedPointer<BatchRequest>::create();
    batchRequest->tasks = batch;
    batchRequest->batchId = batchId;
    batchRequest->retryCount = 0;
    batchRequest->completed = false;
    batchRequest->lastUpdateTime = QDateTime::currentDateTime();

    {
        QMutexLocker locker(&m_mutex);

        qDebug() << "activeRequests=" << m_activeRequests.size()
                   << "completed=" << m_completedCount << "total=" << m_totalCount;
        m_activeRequests.append(batchRequest);
        qDebug() << "已添加到活跃列表，当前活跃数:" << m_activeRequests.size();
    }

    // 使用QtConcurrent运行异步任务，参考Python版本的asyncio
    QFuture<void> future = QtConcurrent::run([this, batchRequest]() {
        QElapsedTimer timer;
        timer.start();

        // 【关键修复】在请求实际开始处理时更新 lastUpdateTime
        // 这样可以避免新请求被误判为卡住
        {
            QMutexLocker locker(&m_mutex);
            batchRequest->lastUpdateTime = QDateTime::currentDateTime();
        }

        try {
            // 构建API请求
            QString batchContents = buildBatchContent(batchRequest->tasks);

            // 参考Python版本的请求格式
            QJsonObject requestObj;
            requestObj["model"] = "mimo-v2-flash";  // 使用MiMo API模型
            requestObj["temperature"] = 0.3;
            requestObj["max_completion_tokens"] = 4096;
            requestObj["top_p"] = 0.95;
            requestObj["stream"] = false;
            requestObj["frequency_penalty"] = 0;
            requestObj["presence_penalty"] = 0;

            // 添加thinking配置
            QJsonObject thinking;
            thinking["type"] = "disabled";
            requestObj["thinking"] = thinking;

            // 使用配置的提示词，如果为空则使用默认
            QString systemPrompt = m_systemPrompt.isEmpty()
                ? "你是一个专业的游戏文本翻译助手，擅长准确翻译游戏内容。"
                : m_systemPrompt;

            QString userPrompt;
            if (m_batchPrompt.isEmpty()) {
                // 默认批量提示词
                userPrompt = QString("请将以下内容翻译成中文，保持游戏风格和格式：\n%1\n\n要求：\n1. 按行翻译，保持编号格式\n2. 保留~g~、~y~、~r~、~b~、~w~等颜色代码不变\n3. 按字面意思翻译，保持游戏文本风格").arg(batchContents);
            } else {
                // 使用配置的批量提示词，替换{contents}占位符
                userPrompt = m_batchPrompt;
                // 确保{contents}被正确替换
                if (userPrompt.contains("{contents}")) {
                    userPrompt = userPrompt.replace("{contents}", batchContents);
                }
            }

            // 调试日志：显示使用的提示词（最多100字符）
            qWarning() << "批次" << batchRequest->batchId << "使用的系统提示词:" << systemPrompt.left(300) << (systemPrompt.length() > 300 ? "..." : "");

            QJsonArray messages;
            messages.append(QJsonObject{
                {"role", "system"},
                {"content", systemPrompt}
            });
            messages.append(QJsonObject{
                {"role", "user"},
                {"content", userPrompt}
            });
            requestObj["messages"] = messages;

            // 发送HTTP请求 - 优化连接配置
            QNetworkAccessManager networkManager;
            QNetworkRequest request = QNetworkRequest(QUrl(API_URL));
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            request.setRawHeader("api-key", m_apiKey.toUtf8());  // 使用 api-key 头而不是 Authorization: Bearer
            request.setRawHeader("User-Agent", "GXTStudio-Translator/2.0");
            request.setRawHeader("Accept", "application/json");
            request.setRawHeader("Connection", "keep-alive");
            request.setRawHeader("Keep-Alive", "timeout=30, max=32");  // 优化keep-alive
            request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

            // 优化超时和重试配置
            request.setTransferTimeout(REQUEST_TIMEOUT * 1000);  // 使用类常量
            QJsonDocument doc(requestObj);
            QByteArray requestData = doc.toJson();

            QNetworkReply* reply = networkManager.post(request, requestData);

            // 设置超时
            QEventLoop loop;
            QTimer timeoutTimer;
            timeoutTimer.setSingleShot(true);
            timeoutTimer.setInterval(REQUEST_TIMEOUT * 1000); // 转换为毫秒
            QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

            timeoutTimer.start();
            loop.exec(); // 等待请求完成或超时
            bool success = false;
            QString result;

            if (timeoutTimer.isActive()) {
                timeoutTimer.stop();
                if (reply->error() == QNetworkReply::NoError) {
                    QByteArray responseData = reply->readAll();
                    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

                    if (!responseDoc.isNull() && responseDoc.isObject()) {
                        QJsonObject responseObj = responseDoc.object();
                        if (responseObj.contains("choices")) {
                            QJsonArray choices = responseObj["choices"].toArray();
                            if (!choices.isEmpty()) {
                                QJsonObject choice = choices[0].toObject();
                                if (choice.contains("message")) {
                                    QJsonObject message = choice["message"].toObject();
                                    result = message["content"].toString();
                                    success = !result.isEmpty();
                                }
                            }
                        }
                    }
                } else {
                    result = QString("网络错误: %1").arg(reply->errorString());
                }
            } else {
                reply->abort();
                result = "请求超时";
            }

            reply->deleteLater();

        // 统计性能数据
        qint64 elapsed = timer.elapsed();
        {
            QMutexLocker locker(&m_mutex);
            m_totalTranslationTime += elapsed;
        }

        // 更新响应时间统计（自适应并发控制）
        if (success) {
            updateResponseTime(elapsed);
        }

        // 更新请求统计（包含智能频率控制）
        updateRequestStats(success);

            // 【关键修复】使用智能指针传递，确保对象生命周期安全
            QMetaObject::invokeMethod(this, "onBatchRequestFinished",
                                    Qt::QueuedConnection,
                                    Q_ARG(QSharedPointer<BatchRequest>, batchRequest),
                                    Q_ARG(bool, success),
                                    Q_ARG(QString, result));

        } catch (...) {
            QMetaObject::invokeMethod(this, "onBatchRequestFinished",
                                    Qt::QueuedConnection,
                                    Q_ARG(QSharedPointer<BatchRequest>, batchRequest),
                                    Q_ARG(bool, false),
                                    Q_ARG(QString, "处理异常"));
        }
    });

    // 使用QFutureWatcher跟踪异步任务 - 修复竞态条件
    // 先连接信号，再设置future，避免"connecting after calling setFuture"警告
    QFutureWatcher<void>* watcher = new QFutureWatcher<void>(this);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, [watcher]() {
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void SmartTranslator::onBatchRequestFinished(QSharedPointer<BatchRequest> batchRequest, bool success, const QString& result)
{
    if (batchRequest.isNull()) return;


    // 调用 handleBatchResult 处理结果（内部会自动触发 processNextBatch）
    handleBatchResult(batchRequest, success, result);

    // 【修复】不再在这里调用 processNextBatch，避免重复调用
    // handleBatchResult 内部会触发 processNextBatch
}

void SmartTranslator::sendBatchRequest(const QList<TranslateTask>& batch, int batchId)
{
    // 现在使用translateBatchAsync方法来处理异步请求
    translateBatchAsync(batch, batchId);
}

QString SmartTranslator::buildBatchContent(const QList<TranslateTask>& tasks)
{
    // 参考Python版本的简洁格式：编号. 文本内容
    QStringList contents;
    int totalTextLength = 0;

    // 调试日志：记录批次内信息
    qWarning() << "buildBatchContent: 批次任务数:" << tasks.size();

    for (int i = 0; i < tasks.size(); ++i) {
        const auto& task = tasks[i];

        // 预处理文本：保留游戏颜色代码，但移除其他控制字符
        QString cleanValue = task.value;

        // 移除其他控制字符，但保留基本标点和颜色代码
        cleanValue.replace(QRegularExpression("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F]"), "");

        // 清理多余的空白字符，但保留颜色代码的位置
        cleanValue = cleanValue.simplified();

        // 检查清理后的文本是否为空
        if (cleanValue.trimmed().isEmpty()) {
            continue;
        }

            // 检查总文本长度，防止请求数据过大 - 优化限制
            totalTextLength += cleanValue.length();
            if (totalTextLength > 16000) { // 16KB限制，支持更大批次
                break;
            }

        // 参考Python版本的简洁格式：编号. 文本内容
        contents.append(QString("%1. %2").arg(contents.size() + 1).arg(cleanValue));

        // 调试日志：记录每个任务
        if (i < 3) { // 只记录前3个，避免日志过多
        }
    }

    QString result = contents.join("\n");

    return result;
}





void SmartTranslator::handleBatchResult(QSharedPointer<BatchRequest> batchRequest, bool success, const QString& result)
{
    if (batchRequest.isNull()) return;

    qDebug() << "success=" << success << "result=" << result.left(100);

    QList<TranslateResult> batchResults;

    // 解析翻译结果
    if (success) {
        try {
            batchResults = parseBatchResult(result, batchRequest->tasks);
            // 如果解析返回空结果，说明API返回了无效数据，标记为失败
            if (batchResults.isEmpty()) {
                success = false;
            }
        } catch (...) {
            success = false;
        }
    }

    // 【关键修复】失败处理和重试逻辑 - 彻底重构
    if (!success) {
        bool isAuthError = result.contains("API密钥无效") || result.contains("401");

        if (isAuthError) {
            emit translationError("API密钥无效，请检查配置");
            {
                QMutexLocker locker(&m_mutex);
                m_cancelled = true;
                m_activeRequests.removeOne(batchRequest);
            }
            return;
        }

        // 如果还有重试次数，将任务放回队列并触发重试
        if (batchRequest->retryCount < m_maxRetries) {
            int newRetryCount = batchRequest->retryCount + 1;
            int delay = 1000 * (1 << (newRetryCount - 1)); // 指数退避
            if (delay > 8000) delay = 8000;

       qDebug() << "批次" << batchRequest->batchId << "第" << newRetryCount << "/" << m_maxRetries << "次重试";

            // 提取任务列表用于重试
            QList<TranslateTask> tasksToRetry = batchRequest->tasks;
            int oldBatchId = batchRequest->batchId;

            // 从活跃列表移除并清理旧请求
            {
                QMutexLocker locker(&m_mutex);
                bool removed = m_activeRequests.removeOne(batchRequest);
        qDebug() << "从活跃列表移除" << (removed ? "成功" : "失败，可能已不在列表")
                   << ")，当前活跃请求数:" << m_activeRequests.size();

                // 【关键修复】不要将失败任务放回队列，直接跳过它
                // 避免无限重试导致99%卡住的问题
                // 检查结果是否为空或无效
                bool shouldSkipRetry = result.isEmpty() ||
                                     result.contains("API返回空结果") ||
                                     result.contains("网络错误") ||
                                     result.contains("请求超时") ||
                                     result.length() < 10;

                if (shouldSkipRetry) {
                qDebug() << batchRequest->tasks.size() << "个任务为失败，不再重试。原因：" << result.left(100);
                    // 不放回队列，直接跳过
                } else {
                    // 将任务放回待处理队列（放回头部，优先处理）
                    for (int i = tasksToRetry.size() - 1; i >= 0; --i) {
                        m_pendingTasks.prepend(tasksToRetry[i]);
                    }
                qDebug() << tasksToRetry.size() << "个任务放回待处理队列，当前待处理数:" << m_pendingTasks.size();
                }
            }

            // 智能指针会自动管理内存，无需手动删除
            // 延迟后触发处理，确保任务被重新处理
            QTimer::singleShot(delay, this, &SmartTranslator::processNextBatch);
            return;
        }

        // 重试次数用尽，创建失败结果
        for (const auto& task : batchRequest->tasks) {
            TranslateResult translateResult(task.key, task.value, task.row);
            translateResult.success = false;
            translateResult.errorMessage = result;
            batchResults.append(translateResult);
        }
    }

    // 【成功或重试用尽】更新进度和清理请求
    {
        QMutexLocker locker(&m_mutex);

        qDebug() << "成功=" << success
                   << "pendingTasks=" << m_pendingTasks.size()
                   << "activeRequests=" << m_activeRequests.size()
                   << "completed=" << m_completedCount << "total=" << m_totalCount;

        // 统计成功和失败的任务数
        int successCount = 0;
        int failureCount = 0;
        for (const auto& result : batchResults) {
            if (result.success) {
                successCount++;
            } else {
                failureCount++;
            }
        }

        // 更新完成计数（只统计成功的结果，失败的不计入进度）
        int oldCompleted = m_completedCount;
        int oldProcessed = m_processedCount;
        m_completedCount += successCount;  // 只增加成功的数量
        m_processedCount += batchResults.size();  // 增加已处理的总数
        qDebug() << "完成数更新：" << oldCompleted << "->" << m_completedCount
                   << "(成功+" << successCount << ", 总计+" << batchResults.size() << ")"
                   << "已处理：" << oldProcessed << "->" << m_processedCount;

        // 添加结果
        try {
            for (const auto& result : batchResults) {
                if (m_results.size() < MAX_SAFE_TASKS) {
                    m_results.append(result);
                }
            }
        } catch (...) {
        }

        // 发送进度更新
        int completed = qMin(m_completedCount, m_totalCount);
        double successRate = getSuccessRate();
        QString rateStatus = isRateLimited() ? "限流中" : QString("间隔%1ms").arg(m_currentRequestInterval);

        int totalForProgress = m_totalCount > 0 ? m_totalCount : 1;

        QString batchProgressMsg = QString("批次%1完成 | 成功%2条|失败%3条| 成功率%4% | %5 | 进度 %6/%7")
                            .arg(batchRequest->batchId)
                            .arg(successCount)
                            .arg(failureCount)
                            .arg(successRate, 0, 'f', 1)
                            .arg(rateStatus)
                            .arg(completed)
                            .arg(m_totalCount);

        emit translationProgress(completed, totalForProgress, batchProgressMsg);

        // 【关键】从活跃列表移除请求（确保只移除一次）
        bool removed = m_activeRequests.removeOne(batchRequest);
        if (!removed) {
       qDebug() << "不在活跃列表中，可能已被移除";
        } else {
       qDebug() << "已从活跃列表移除，当前活跃数:" << m_activeRequests.size();
        }
    }

    // 智能指针自动管理内存，无需手动删除

    // 触发下一次处理
    QTimer::singleShot(10, this, &SmartTranslator::processNextBatch);
}

// 静态辅助函数：判断是否为API确认性短语（基于结构特征）
bool SmartTranslator::isConfirmationPhrase(const QString& text) {
    if (text.isEmpty()) return false;

    QString trimmed = text.trimmed();

    // 规则1：基于长度的智能过滤 - 确认短语通常很短（4-20字）
    int length = trimmed.length();
    if (length < 4 || length > 20) {
        return false; // 太长或太短，不是确认短语
    }

    // 规则2：完全匹配确认短语（使用精确匹配，而非contains）
    QStringList confirmationPhrases = {
        "好的",
        "明白",
        "没问题",
        "收到",
        "了解",
        "OK",
        "好的，我明白了",
        "好的，收到",
        "明白，我知道了",
        "好的，好的",
        "没问题，明白"
    };

    if (confirmationPhrases.contains(trimmed)) {
        return true;
    }

    // 规则3：匹配以特定模式开头的确认语句
    if (trimmed.startsWith("好的，这是") ||
        trimmed.startsWith("明白，这是") ||
        trimmed.startsWith("没问题，这是") ||
        trimmed.startsWith("这是您要求的") ||
        trimmed.startsWith("这是您的翻译") ||
        trimmed.startsWith("为您翻译") ||
        trimmed.startsWith("这是您要求的翻译") ||
        trimmed.startsWith("以下是翻译结果") ||
        trimmed.startsWith("翻译结果如下") ||
        trimmed.startsWith("以下") ||
        trimmed.startsWith("这是翻译")) {
        return true;
    }

    // 规则4：以标点结尾的简短确认短语（4-10字）
    QRegularExpression shortConfirmPattern(R"(^(好的|明白|收到|OK|没问题)[，。！,.!]{0,2}$)");
    if (shortConfirmPattern.match(trimmed).hasMatch()) {
        return true;
    }

    // 规则5：包含特定元关键词的长句子（AI生成的开场白/结束语）
    if (trimmed.length() > 10) {
        QStringList metaKeywords = {
            "严格遵循",
            "格式要求",
            "风格要求",
            "这是您要求的",
            "这是您的翻译",
            "翻译已完成",
            "请查收",
            "如下所示"
        };
        for (const QString& keyword : metaKeywords) {
            if (trimmed.contains(keyword)) {
                return true;
            }
        }
    }

    return false;
}

// 静态辅助函数：判断是否为技术性元信息（非游戏文本）
bool SmartTranslator::isTechnicalMetadata(const QString& text) {
    QString trimmed = text.trimmed();

    // 技术性标记
    if (trimmed.startsWith("```") ||
        trimmed.startsWith("[") ||
        trimmed.startsWith("{") ||
        trimmed.startsWith("JSON") ||
        trimmed.startsWith("json")) {
        return true;
    }

    // 错误或状态信息
    if (trimmed.contains("error", Qt::CaseInsensitive) ||
        trimmed.contains("错误") ||
        trimmed.contains("失败") ||
        trimmed.contains("API") ||
        trimmed.contains("Model") ||
        trimmed.contains("请求超时") ||
        trimmed.contains("网络错误")) {
        return true;
    }

    return false;
}

QList<TranslateResult> SmartTranslator::parseBatchResult(const QString& result, const QList<TranslateTask>& originalTasks)
{
    QList<TranslateResult> results;

    if (result.isEmpty()) {
        // 如果结果为空，返回失败结果
        for (int i = 0; i < originalTasks.size() && i < MAX_SAFE_TASKS; ++i) {
            const auto& task = originalTasks[i];
            TranslateResult translateResult(task.key, task.value, task.row);
            translateResult.success = false;
            translateResult.errorMessage = "API返回空结果";
            results.append(translateResult);
        }
        return results;
    }

    // 【关键修复】优先尝试解析带index的JSON格式: {"result": [{"index": 0, "text": "..."}, ...]}
    QString cleanResult = result.trimmed();
    if (cleanResult.startsWith("```json")) {
        cleanResult = cleanResult.mid(7);
    }
    if (cleanResult.endsWith("```")) {
        cleanResult = cleanResult.left(cleanResult.length() - 3);
    }
    cleanResult = cleanResult.trimmed();


    try {
        if (cleanResult.length() <= 500000) { // 50万字符限制
            QJsonDocument doc = QJsonDocument::fromJson(cleanResult.toUtf8());

            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                // 检查是否有result字段
                if (obj.contains("result") && obj["result"].isArray()) {
                    QJsonArray resultArray = obj["result"].toArray();

                    // 【修复】按顺序直接使用翻译，不依赖index
                    // 避免index重复导致的问题
                    if (resultArray.size() >= originalTasks.size()) {

                        for (int i = 0; i < originalTasks.size() && i < MAX_SAFE_TASKS; ++i) {
                            const auto& task = originalTasks[i];
                            TranslateResult translateResult(task.key, task.value, task.row);

                            QJsonObject item = resultArray[i].toObject();
                            if (item.contains("text")) {
                                QString text = item["text"].toString();
                                translateResult.translatedValue = text;
                                translateResult.success = true;
                            } else {
                                translateResult.success = false;
                                translateResult.errorMessage = "缺少text字段";
                            }

                            results.append(translateResult);
                        }

                        return results;
                    } else {

                        // 创建index到翻译的映射
                        QMap<int, QString> translationMap;
                        for (int i = 0; i < resultArray.size(); ++i) {
                            QJsonObject item = resultArray[i].toObject();
                            if (item.contains("index") && item.contains("text")) {
                                int index = item["index"].toInt();
                                QString text = item["text"].toString();
                                translationMap[index] = text;
                            }
                        }

                        // 按原始任务的index匹配翻译结果
                        for (int i = 0; i < originalTasks.size() && i < MAX_SAFE_TASKS; ++i) {
                            const auto& task = originalTasks[i];
                            TranslateResult translateResult(task.key, task.value, task.row);

                           // 使用任务在批次中的索引来查找对应的翻译
                           int taskIndex = i; // 任务在批次中的索引
                           if (translationMap.contains(taskIndex)) {
                                translateResult.translatedValue = translationMap[taskIndex];
                                translateResult.success = true;
                            } else {
                                translateResult.success = false;
                                translateResult.errorMessage = QString("未找到index %1的翻译结果").arg(taskIndex);
                            }

                            results.append(translateResult);
                        }

                        return results;
                    }
                }
            }
        }
    } catch (...) {
    }

    // 【新增】尝试修复格式错误的JSON
    // 处理类似这样的错误格式："result": ["index": 0, "text": "..."}, "index": 1, "text": "..."}

    // 检查是否是格式错误的JSON（有result但缺少花括号）
    if (cleanResult.contains("result") && cleanResult.contains("index") && cleanResult.contains("text")) {
        QString repairedJson;

        // 尝试手动解析并重建正确的JSON
        QRegularExpression indexRe(R"(\"index\"\s*:\s*(\d+))");
        QRegularExpression textRe(R"(\"text\"\s*:\s*\"([^\"]*)\")");

        QStringList indexMatches;
        QStringList textMatches;

        QRegularExpressionMatchIterator indexIt = indexRe.globalMatch(cleanResult);
        while (indexIt.hasNext()) {
            QRegularExpressionMatch match = indexIt.next();
            indexMatches.append(match.captured(1));
        }

        QRegularExpressionMatchIterator textIt = textRe.globalMatch(cleanResult);
        while (textIt.hasNext()) {
            QRegularExpressionMatch match = textIt.next();
            textMatches.append(match.captured(1));
        }


        // 使用最小值作为实际条目数
        int count = qMin(indexMatches.size(), textMatches.size());
        if (count > 0 && count >= originalTasks.size() / 2) {
            // 重建正确的JSON
            repairedJson = "{\"result\": [";
            for (int i = 0; i < count && i < originalTasks.size(); ++i) {
                repairedJson += QString("{\"index\": %1, \"text\": \"%2\"}").arg(indexMatches[i]).arg(textMatches[i].replace("\"", "\\\""));
                if (i < count - 1) {
                    repairedJson += ", ";
                }
            }
            repairedJson += "]}";


            // 尝试解析重建的JSON
            QJsonDocument repairedDoc = QJsonDocument::fromJson(repairedJson.toUtf8());
            if (!repairedDoc.isNull() && repairedDoc.isObject()) {
                QJsonObject repairedObj = repairedDoc.object();
                if (repairedObj.contains("result") && repairedObj["result"].isArray()) {
                    QJsonArray repairedArray = repairedObj["result"].toArray();

                    for (int i = 0; i < originalTasks.size() && i < MAX_SAFE_TASKS; ++i) {
                        const auto& task = originalTasks[i];
                        TranslateResult translateResult(task.key, task.value, task.row);

                        if (i < repairedArray.size()) {
                            QJsonObject item = repairedArray[i].toObject();
                            if (item.contains("text")) {
                                QString text = item["text"].toString();
                                translateResult.translatedValue = text;
                                translateResult.success = true;
                            } else {
                                translateResult.success = false;
                                translateResult.errorMessage = "缺少text字段";
                            }
                        } else {
                            translateResult.success = false;
                            translateResult.errorMessage = "翻译结果不足";
                        }

                        results.append(translateResult);
                    }

                    if (!results.isEmpty()) {
                        return results;
                    }
                }
            }
        }
    }


    // JSON解析失败，回退到按行解析
    qWarning() << "回退到按行解析";
    QStringList lines = result.split('\n', Qt::SkipEmptyParts);
    QStringList translations;
    int totalLines = lines.size();

    // 正则模式：匹配数字序号开头的行
    QRegularExpression re(R"(^(\d+)\.\s*(.*)$)");

    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        QString trimmedLine = lines[lineIndex].trimmed();
        if (trimmedLine.isEmpty()) continue;

        QRegularExpressionMatch match = re.match(trimmedLine);

        if (match.hasMatch()) {
            // 有序号：提取序号和内容
            QString translation = match.captured(2).trimmed();
            int lineNumber = match.captured(1).toInt();

            if (!translation.isEmpty()) {
                // 基于结构的过滤：跳过确认短语和技术性内容
                if (!isConfirmationPhrase(translation) && !isTechnicalMetadata(translation)) {
                    translations.append(translation);
                } else {
                }
            }
        } else {
            // 无序号：根据位置和结构特征判断
            // 规则：第一行或最后两行可能是确认语
            bool isFirstLine = (lineIndex == 0);
       bool isLastLines = (lineIndex >= totalLines - 2);

        if ((isFirstLine || isLastLines) && (isConfirmationPhrase(trimmedLine) || isTechnicalMetadata(trimmedLine))) {
            // 首尾的确认语/技术信息，跳过
            continue;
        }

        // 非首尾行且不是确认语，视为翻译内容
        if (!isConfirmationPhrase(trimmedLine) && !isTechnicalMetadata(trimmedLine)) {
                translations.append(trimmedLine);
            } else {
                // 中间的确认语也跳过
                qWarning() << "跳过中间确认/技术信息：" << trimmedLine.left(50);
            }
        }
    }
    
    // 如果简单解析失败，尝试JSON解析（备用方案）
    if (translations.isEmpty()) {
        QString cleanResult = result.trimmed();
        if (cleanResult.startsWith("```json")) {
            cleanResult = cleanResult.mid(7);
        }
        if (cleanResult.endsWith("```")) {
            cleanResult = cleanResult.left(cleanResult.length() - 3);
        }
        cleanResult = cleanResult.trimmed();


        try {
            if (cleanResult.length() <= 500000) { // 50万字符限制
                QJsonDocument doc = QJsonDocument::fromJson(cleanResult.toUtf8());

                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();
                    if (obj.contains("choices") && obj["choices"].isArray()) {
                        QJsonArray choices = obj["choices"].toArray();
                        if (!choices.isEmpty()) {
                            QJsonObject choice = choices[0].toObject();
                            if (choice.contains("message")) {
                                QJsonObject message = choice["message"].toObject();
                                QString content = message["content"].toString();

                                // 从content中按行解析
                                QStringList contentLines = content.split('\n', Qt::SkipEmptyParts);
                                for (const QString& line : contentLines) {
                                    QString trimmedLine = line.trimmed();
                                    if (trimmedLine.isEmpty()) continue;

                                    QRegularExpressionMatch match = re.match(trimmedLine);
                                    if (match.hasMatch()) {
                                        QString translation = match.captured(2).trimmed();
                                        if (!translation.isEmpty()) {
                                            // 使用结构化过滤，而非关键词过滤
                                            if (!isConfirmationPhrase(translation) && !isTechnicalMetadata(translation)) {
                                                translations.append(translation);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } catch (...) {
        }
    }

    // 如果解析成功，创建翻译结果
    if (!translations.isEmpty()) {
        
        for (int i = 0; i < originalTasks.size() && i < MAX_SAFE_TASKS; ++i) {
            const auto& task = originalTasks[i];
            TranslateResult translateResult(task.key, task.value, task.row);
            
            if (i < translations.size()) {
                translateResult.translatedValue = translations[i];
                translateResult.success = true;
            } else {
                translateResult.success = false;
                translateResult.errorMessage = "翻译结果数量不匹配";
            }
            
            results.append(translateResult);
        }
    } else {
        // 如果解析失败，返回失败结果
        qWarning() << "解析失败，返回原始文本作为失败结果";
        for (int i = 0; i < originalTasks.size() && i < MAX_SAFE_TASKS; ++i) {
            const auto& task = originalTasks[i];
            TranslateResult translateResult(task.key, task.value, task.row);
            translateResult.success = false;
            translateResult.errorMessage = "无法解析翻译结果";
            results.append(translateResult);
        }
    }
    
    return results;
}

void SmartTranslator::cleanupBatchRequest(QSharedPointer<BatchRequest> batchRequest)
{
    if (batchRequest.isNull()) return;

    try {
        // 从活跃请求列表中移除 - 使用线程安全的方式
        bool found = false;
        {
            QMutexLocker locker(&m_mutex);
            int beforeCount = m_activeRequests.size();
            found = m_activeRequests.removeOne(batchRequest);
            int afterCount = m_activeRequests.size();
            if (found) {
            } else {
            }
        }

        // 【修复】智能指针会自动管理内存，无需手动delete
    } catch (...) {
    }
}

// 智能频率控制：计算自适应延迟（基于响应时间和成功率双重调整）
int SmartTranslator::calculateAdaptiveDelay()
{
    QMutexLocker locker(&m_mutex);

    // 基础延迟
    int baseDelay = MIN_REQUEST_INTERVAL;

    // 1. 根据平均响应时间调整（响应时间越长，间隔越大）
    if (m_responseSampleCount >= 5) {  // 至少5个样本才使用响应时间
        if (m_averageResponseTime < 1000) {
            // 响应很快，可以减少间隔
            baseDelay = MIN_REQUEST_INTERVAL;
        } else if (m_averageResponseTime < 2000) {
            // 响应正常
            baseDelay = MIN_REQUEST_INTERVAL + (m_averageResponseTime - 1000) / 20;
        } else if (m_averageResponseTime < 5000) {
            // 响应较慢
            baseDelay = MIN_REQUEST_INTERVAL + 50 + (m_averageResponseTime - 2000) / 10;
        } else {
            // 响应很慢，显著增加间隔
            baseDelay = MIN_REQUEST_INTERVAL + 300 + (m_averageResponseTime - 5000) / 5;
        }
    }

    // 2. 根据并发数调整（并发越多，间隔越大）
    int concurrencyFactor = (m_adaptiveConcurrentLimit - 8) * 10;  // 每超过8个并发，间隔增加10ms
    baseDelay += qMax(0, concurrencyFactor);

    // 3. 根据成功率调整
    if (m_successfulRequests + m_failedRequests > 0) {
        double successRate = (double)m_successfulRequests / (m_successfulRequests + m_failedRequests);

        if (successRate < 0.8) {
            // 成功率低于80%，大幅增加延迟
            baseDelay *= 3;
            m_consecutiveErrorCount++;
        } else if (successRate < 0.9) {
            // 成功率在80-90%，适度增加延迟
            baseDelay *= 2;
            m_consecutiveErrorCount++;
        } else if (successRate > 0.95) {
            // 成功率高于95%，适度减少延迟
            baseDelay = qMax(MIN_REQUEST_INTERVAL, baseDelay / 2);
            m_consecutiveErrorCount = 0;
        } else {
            // 成功率正常（90-95%），微调延迟
            m_consecutiveErrorCount = 0;
        }
    }

    // 4. 连续错误惩罚
    if (m_consecutiveErrorCount >= CONSECUTIVE_ERRORS_THRESHOLD) {
        baseDelay = RATE_LIMIT_DELAY;
    }

    // 5. 限制在合理范围内
    m_currentRequestInterval = qBound(MIN_REQUEST_INTERVAL, baseDelay, MAX_REQUEST_INTERVAL);

    return m_currentRequestInterval;
}

// 智能频率控制：更新请求统计
void SmartTranslator::updateRequestStats(bool success)
{
    QMutexLocker locker(&m_mutex);

    if (success) {
        m_successfulRequests++;
        m_consecutiveErrorCount = 0;
    } else {
        m_failedRequests++;
        m_consecutiveErrorCount++;
    }

    // 每10个请求重新评估一次频率
    if ((m_successfulRequests + m_failedRequests) % 10 == 0) {
       qDebug() << "失败:" << m_failedRequests
                  << "成功率:" << (m_successfulRequests * 100.0 / (m_successfulRequests + m_failedRequests)) << "%"
                  << "当前间隔:" << m_currentRequestInterval << "ms";
    }
}

// 智能频率控制：更新响应时间统计
void SmartTranslator::updateResponseTime(qint64 responseTimeMs)
{
    QMutexLocker locker(&m_mutex);

    m_totalResponseTime += responseTimeMs;
    m_responseSampleCount++;

    // 计算移动平均响应时间（使用指数移动平均）
    if (m_responseSampleCount == 1) {
        m_averageResponseTime = responseTimeMs;
    } else {
        // 使用0.2的权重，平滑响应时间变化
        m_averageResponseTime = static_cast<int>(m_averageResponseTime * 0.8 + responseTimeMs * 0.2);
    }

    // 根据响应时间自适应调整并发限制
    // 响应时间越短，可以支持更多并发；响应时间越长，减少并发
    if (m_responseSampleCount >= 3) {
        int newConcurrentLimit;

        if (m_averageResponseTime < 800) {
            // 响应很快，可以增加并发
            newConcurrentLimit = qMin(DEFAULT_MAX_CONCURRENT_REQUESTS * 2, 24);
        } else if (m_averageResponseTime < 1500) {
            // 响应正常，使用默认并发
            newConcurrentLimit = DEFAULT_MAX_CONCURRENT_REQUESTS;
        } else if (m_averageResponseTime < 3000) {
            // 响应较慢，适度降低并发
            newConcurrentLimit = qMax(6, DEFAULT_MAX_CONCURRENT_REQUESTS - 4);
        } else if (m_averageResponseTime < 5000) {
            // 响应慢，进一步降低并发
            newConcurrentLimit = qMax(4, DEFAULT_MAX_CONCURRENT_REQUESTS - 6);
        } else {
            // 响应很慢，使用最低并发
            newConcurrentLimit = 3;
        }

        // 平滑过渡并发限制变化
        if (m_adaptiveConcurrentLimit != newConcurrentLimit) {
       qDebug() << "自适应并发从" << m_adaptiveConcurrentLimit << "调整至" << newConcurrentLimit;
            m_adaptiveConcurrentLimit = newConcurrentLimit;
        }
    }

    // 每20个样本打印一次统计信息
    if (m_responseSampleCount % 20 == 0) {
       qDebug() << "平均响应:" << m_averageResponseTime << "ms"
                  << "自适应并发:" << m_adaptiveConcurrentLimit;
    }
}

// 独立的卡住请求检测（每5秒由定时器调用）
void SmartTranslator::checkStuckRequests()
{
    QMutexLocker locker(&m_mutex);

    if (m_cancelled || m_activeRequests.isEmpty()) {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QList<QSharedPointer<BatchRequest>> stuckRequests;

    for (const auto& request : m_activeRequests) {
        if (request) {
            int elapsed = request->lastUpdateTime.secsTo(now);
            // 超过30秒视为卡住（缩短超时时间以更快检测）
            if (elapsed > 30) {
                stuckRequests.append(request);
            }
        }
    }

    // 清理卡住的请求
    if (!stuckRequests.isEmpty()) {

        for (const auto& request : stuckRequests) {
            // 【修复】也要增加已处理计数，否则会卡住无法完成
            m_processedCount += request->tasks.size();

            // 创建失败结果
            for (const auto& task : request->tasks) {
                TranslateResult result(task.key, task.value, task.row);
                result.success = false;
                result.errorMessage = QString("请求超时（已运行%1秒）").arg(
                    request->lastUpdateTime.secsTo(now));
                m_results.append(result);
                m_completedCount++;
            }
            // 从活跃列表移除（智能指针会自动管理内存，无需手动删除）
            m_activeRequests.removeOne(request);
        }

        locker.unlock();

        // 发送进度更新
        int completed = qMin(m_completedCount, m_totalCount);
        int totalForProgress = m_totalCount > 0 ? m_totalCount : 1;
        emit translationProgress(completed, totalForProgress,
            QString("清理了%1个超时请求，继续处理").arg(stuckRequests.size()));

        // 触发下一批处理
        QTimer::singleShot(0, this, &SmartTranslator::processNextBatch);
    }
}
