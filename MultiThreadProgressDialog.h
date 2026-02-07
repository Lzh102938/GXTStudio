#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include <QCloseEvent>
#include <QMessageBox>
#include <QScreen>
#include <QGuiApplication>
#include <QFrame>
#include <QAbstractButton>

/**
 * @brief 多线程进度条窗口类 - 简约高效设计
 * 
 * 用于显示多线程任务的进度，支持：
 * - 总体进度显示
 * - 每个线程的独立进度
 * - 实时统计（成功/失败/速度/ETA）
 * - 取消选项（保留/丢弃结果）
 */
class MultiThreadProgressDialog : public QDialog {
    Q_OBJECT

public:
    enum CancelOption {
        CancelAndKeep = 0,      // 取消翻译并保留已完成的结果
        CancelAndDiscard = 1,   // 取消翻译并丢弃已完成的结果
        NoCancel = 2            // 不取消翻译
    };

    explicit MultiThreadProgressDialog(QWidget *parent = nullptr);
    ~MultiThreadProgressDialog();

    // 更新指定线程的进度
    void updateThreadProgress(int threadId, int completed, int total, const QString& message);
    
    // 设置线程总数
    void setTotalThreads(int count);
    
    // 更新总体进度
    void updateOverallProgress(int completed, int total);
    
    // 标记翻译完成
    void translationCompleted();
    
    // 重置对话框状态
    void reset();
    
    // 更新统计计数
    void updateSuccessCount(int count);
    void updateFailedCount(int count);
    
    // 设置开始时间（用于计算速度）
    void setStartTime(qint64 startTime);

signals:
    // 请求取消翻译，带选项
    void cancelTranslationRequested(CancelOption option);

private slots:
    void onCancelClicked();
    void updateStatistics();
    
    // UI辅助方法
    QString getProgressBarStyle(const QString& chunkColor = "#4a90e2") const;
    QString getLabelStyle(const QString& color, int size) const;

private:
    void setupUI();
    void setupCompactThreadView();
    void setupStatisticsPanel();
    void updateThreadInfo(int threadId);
    QString formatTime(int seconds);
    void closeEvent(QCloseEvent* event) override;

    QVBoxLayout* m_mainLayout;

    // 总体进度区域
    QWidget* m_progressWidget;
    QLabel* m_overallProgressLabel;
    QProgressBar* m_overallProgressBar;

    // 统计信息区域（紧凑网格）
    QWidget* m_statsWidget;
    QLabel* m_successLabel;
    QLabel* m_failedLabel;
    QLabel* m_activeLabel;
    QLabel* m_speedLabel;
    QLabel* m_concurrentLabel;
    QLabel* m_etaLabel;

    // 请求列表区域（紧凑视图）
    QScrollArea* m_threadScrollArea;
    QWidget* m_threadContainer;
    QVBoxLayout* m_threadListLayout;
    QGroupBox* m_threadGroup;

    // 底部按钮
    QPushButton* m_cancelButton;

    // 统计数据
    int m_successCount;
    int m_failedCount;
    int m_activeCount;
    qint64 m_startTime;
    QTimer* m_statsUpdateTimer;

    struct ThreadInfo {
        QWidget* container;
        QLabel* idLabel;
        QLabel* statusLabel;
        QProgressBar* progressBar;
        int completed;
        int total;
        QString status;
        QDateTime lastUpdateTime;
    };

    bool m_isTranslationCompleted;
    QMap<int, ThreadInfo> m_threads;
    int m_totalThreads;
    int m_overallCompleted;
    int m_overallTotal;
};
