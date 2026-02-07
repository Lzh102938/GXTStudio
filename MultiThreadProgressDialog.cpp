#include "MultiThreadProgressDialog.h"
#include <QDebug>
#include <QColor>

MultiThreadProgressDialog::MultiThreadProgressDialog(QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_progressWidget(nullptr)
    , m_overallProgressLabel(nullptr)
    , m_overallProgressBar(nullptr)
    , m_statsWidget(nullptr)
    , m_successLabel(nullptr)
    , m_failedLabel(nullptr)
    , m_activeLabel(nullptr)
    , m_speedLabel(nullptr)
    , m_concurrentLabel(nullptr)
    , m_etaLabel(nullptr)
    , m_threadScrollArea(nullptr)
    , m_threadContainer(nullptr)
    , m_threadListLayout(nullptr)
    , m_threadGroup(nullptr)
    , m_cancelButton(nullptr)
    , m_successCount(0)
    , m_failedCount(0)
    , m_activeCount(0)
    , m_startTime(0)
    , m_statsUpdateTimer(nullptr)
    , m_totalThreads(0)
    , m_overallCompleted(0)
    , m_overallTotal(0)
    , m_isTranslationCompleted(false)
{
    setupUI();
    setWindowTitle("翻译进度");
    setModal(true);

    // 设置合理的窗口尺寸
    setMinimumSize(600, 450);
    resize(750, 550);

    // 设置窗口位置在屏幕中央
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }

    // 设置窗口标志
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    // 启动统计更新定时器（每1秒更新一次）
    m_statsUpdateTimer = new QTimer(this);
    m_statsUpdateTimer->setInterval(1000);
    connect(m_statsUpdateTimer, &QTimer::timeout, this, &MultiThreadProgressDialog::updateStatistics);
    m_statsUpdateTimer->start();
}

MultiThreadProgressDialog::~MultiThreadProgressDialog()
{
    // 停止定时器
    if (m_statsUpdateTimer) {
        m_statsUpdateTimer->stop();
    }

    // Qt会自动清理子控件
}

void MultiThreadProgressDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);

    // ========== 标题和总体进度区域 ==========
    setupStatisticsPanel();
    m_mainLayout->addWidget(m_statsWidget);

    // ========== 分隔线 ==========
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("background-color: #dcdcdc; max-height: 1px;");
    m_mainLayout->addWidget(line);

    // ========== 请求列表区域（IDM风格）==========
    setupCompactThreadView();
    m_mainLayout->addWidget(m_threadScrollArea);

    // ========== 底部按钮 ==========
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton("取消翻译", this);
    m_cancelButton->setMinimumWidth(90);
    m_cancelButton->setMinimumHeight(30);
    m_cancelButton->setStyleSheet(
        "QPushButton {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f5f5f5, stop:1 #e0e0e0);"
        "color: #333;"
        "border: 1px solid #aaa;"
        "border-radius: 4px;"
        "padding: 6px 16px;"
        "font-weight: 500;"
        "font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e8f0fe, stop:1 #d2e3fc);"
        "border-color: #1a73e8;"
        "color: #1a73e8;"
        "}"
        "QPushButton:pressed {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #d2e3fc, stop:1 #c5d8ff);"
        "}"
        "QPushButton:disabled {"
        "background: #f5f5f5;"
        "color: #9aa0a6;"
        "border-color: #dadce0;"
        "}"
    );
    connect(m_cancelButton, &QPushButton::clicked, this, &MultiThreadProgressDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    m_mainLayout->addLayout(buttonLayout);
}

void MultiThreadProgressDialog::setupStatisticsPanel()
{
    m_statsWidget = new QWidget(this);
    QVBoxLayout* statsLayout = new QVBoxLayout(m_statsWidget);
    statsLayout->setSpacing(8);
    statsLayout->setContentsMargins(0, 0, 0, 0);

    // 标题
    m_overallProgressLabel = new QLabel("翻译进度", this);
    m_overallProgressLabel->setStyleSheet(
        "font-weight: bold; font-size: 14px; color: #333;"
    );
    statsLayout->addWidget(m_overallProgressLabel);

    // 总体进度条（IDM风格）
    m_overallProgressBar = new QProgressBar(this);
    m_overallProgressBar->setRange(0, 100);
    m_overallProgressBar->setValue(0);
    m_overallProgressBar->setMinimumHeight(24);
    m_overallProgressBar->setTextVisible(true);
    m_overallProgressBar->setFormat("%p%");
    m_overallProgressBar->setStyleSheet(QString(R"(
        QProgressBar {
            border: 1px solid #c0c0c0;
            border-radius: 3px;
            text-align: center;
            font-weight: 500;
            font-size: 11px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f9f9f9, stop:1 #e8e8e8);
            color: #333;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a90e2, stop:1 #357abd);
            border-radius: 2px;
        }
    )"));
    statsLayout->addWidget(m_overallProgressBar);

    // 统计信息网格（IDM风格的紧凑布局）
    QWidget* gridWidget = new QWidget(this);
    QGridLayout* gridLayout = new QGridLayout(gridWidget);
    gridLayout->setSpacing(12);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    // 成功数
    m_successLabel = new QLabel("0", this);
    m_successLabel->setStyleSheet(getLabelStyle("#27ae60", 12));
    QLabel* successText = new QLabel("成功", this);
    successText->setStyleSheet("font-size: 11px; color: #666;");
    gridLayout->addWidget(m_successLabel, 0, 0, Qt::AlignRight);
    gridLayout->addWidget(successText, 0, 1);

    // 失败数
    m_failedLabel = new QLabel("0", this);
    m_failedLabel->setStyleSheet(getLabelStyle("#e74c3c", 12));
    QLabel* failedText = new QLabel("失败", this);
    failedText->setStyleSheet("font-size: 11px; color: #666;");
    gridLayout->addWidget(m_failedLabel, 0, 2, Qt::AlignRight);
    gridLayout->addWidget(failedText, 0, 3);

    // 处理中
    m_activeLabel = new QLabel("0", this);
    m_activeLabel->setStyleSheet(getLabelStyle("#f39c12", 12));
    QLabel* activeText = new QLabel("进行中", this);
    activeText->setStyleSheet("font-size: 11px; color: #666;");
    gridLayout->addWidget(m_activeLabel, 0, 4, Qt::AlignRight);
    gridLayout->addWidget(activeText, 0, 5);

    // 速度
    m_speedLabel = new QLabel("0.0 条/秒", this);
    m_speedLabel->setStyleSheet(getLabelStyle("#333", 12));
    QLabel* speedText = new QLabel("速度", this);
    speedText->setStyleSheet("font-size: 11px; color: #666;");
    gridLayout->addWidget(m_speedLabel, 0, 6, Qt::AlignRight);
    gridLayout->addWidget(speedText, 0, 7);

    // 并发数
    m_concurrentLabel = new QLabel("12", this);
    m_concurrentLabel->setStyleSheet(getLabelStyle("#333", 12));
    QLabel* concurrentText = new QLabel("并发", this);
    concurrentText->setStyleSheet("font-size: 11px; color: #666;");
    gridLayout->addWidget(m_concurrentLabel, 0, 8, Qt::AlignRight);
    gridLayout->addWidget(concurrentText, 0, 9);

    // 预计剩余时间
    m_etaLabel = new QLabel("计算中...", this);
    m_etaLabel->setStyleSheet(getLabelStyle("#333", 12));
    QLabel* etaText = new QLabel("剩余时间", this);
    etaText->setStyleSheet("font-size: 11px; color: #666;");
    gridLayout->addWidget(m_etaLabel, 0, 10, Qt::AlignRight);
    gridLayout->addWidget(etaText, 0, 11);

    statsLayout->addWidget(gridWidget);
}

void MultiThreadProgressDialog::setupCompactThreadView()
{
    m_threadScrollArea = new QScrollArea(this);
    m_threadScrollArea->setWidgetResizable(true);
    m_threadScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_threadScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_threadScrollArea->setMinimumHeight(180);
    m_threadScrollArea->setFrameShape(QFrame::NoFrame);
    m_threadScrollArea->setStyleSheet(
        "QScrollArea {"
        "border: 1px solid #dcdcdc;"
        "background-color: #fafafa;"
        "}"
        "QScrollBar:vertical {"
        "border: none;"
        "background: #f5f5f5;"
        "width: 12px;"
        "margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "background: #c0c0c0;"
        "min-height: 30px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "background: #a0a0a0;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "height: 0px;"
        "}"
    );

    m_threadContainer = new QWidget();
    m_threadContainer->setStyleSheet("background-color: #fafafa;");
    m_threadListLayout = new QVBoxLayout(m_threadContainer);
    m_threadListLayout->setSpacing(4);
    m_threadListLayout->setContentsMargins(8, 8, 8, 8);
    m_threadListLayout->addStretch();

    m_threadScrollArea->setWidget(m_threadContainer);
}

void MultiThreadProgressDialog::updateThreadProgress(int threadId, int completed, int total, const QString& message)
{
    Q_UNUSED(message)
    
    // 直接使用传入的参数更新总体进度
    m_overallCompleted = completed;
    m_overallTotal = total;
    updateOverallProgress(completed, total);

    if (!m_threads.contains(threadId)) {
        // 创建新的请求信息（IDM风格的单行布局）
        ThreadInfo info;
        info.container = new QWidget(this);
        info.container->setStyleSheet(
            "QWidget {"
            "background-color: #ffffff;"
            "border: 1px solid #e0e0e0;"
            "border-radius: 3px;"
            "}"
        );
        QHBoxLayout* containerLayout = new QHBoxLayout(info.container);
        containerLayout->setSpacing(6);
        containerLayout->setContentsMargins(6, 4, 6, 4);

        // ID标签
        info.idLabel = new QLabel(QString("R%1").arg(threadId + 1), this);
        info.idLabel->setMinimumWidth(45);
        info.idLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: #e74c3c;");
        containerLayout->addWidget(info.idLabel);

        // 状态标签
        info.statusLabel = new QLabel("进行中", this);
        info.statusLabel->setMinimumWidth(55);
        info.statusLabel->setStyleSheet("font-size: 10px; color: #f39c12; font-weight: bold;");
        containerLayout->addWidget(info.statusLabel);

        // 进度条（IDM风格）
        info.progressBar = new QProgressBar(this);
        info.progressBar->setRange(0, total > 0 ? total : 100);
        info.progressBar->setValue(completed);
        info.progressBar->setMinimumHeight(16);
        info.progressBar->setStyleSheet(getProgressBarStyle("#4a90e2"));
        containerLayout->addWidget(info.progressBar);

        // 添加到列表（在stretch之前）
        m_threadListLayout->insertWidget(m_threadListLayout->count() - 1, info.container);

        info.completed = completed;
        info.total = total;
        info.status = "进行中";
        info.lastUpdateTime = QDateTime::currentDateTime();

        m_threads[threadId] = info;
        m_totalThreads = qMax(m_totalThreads, threadId + 1);
        m_activeCount++;
    } else {
        // 更新现有请求信息
        ThreadInfo& info = m_threads[threadId];
        info.completed = completed;
        info.total = total;
        info.lastUpdateTime = QDateTime::currentDateTime();

        // 计算进度百分比
        int percentage = total > 0 ? (completed * 100 / total) : 0;

        // 更新进度条
        info.progressBar->setRange(0, total > 0 ? total : 100);
        info.progressBar->setValue(completed);

        // 更新状态
        QString newStatus;
        QString newColor;
        QString newIcon;

        if (percentage >= 100) {
            newStatus = "已完成";
            newColor = "#27ae60";
            newIcon = "#27ae60";
            info.progressBar->setStyleSheet(getProgressBarStyle("#27ae60"));
        } else {
            // 检查是否卡住
            int elapsed = info.lastUpdateTime.secsTo(QDateTime::currentDateTime());
            if (elapsed > 30) {
                newStatus = QString("卡住(%1s)").arg(elapsed);
                newColor = "#e74c3c";
                newIcon = "#e74c3c";
            } else {
                newStatus = "进行中";
                newColor = "#f39c12";
                newIcon = "#e74c3c";
            }
        }

        // 更新状态标签和图标
        if (info.status != newStatus) {
            info.statusLabel->setText(newStatus);
            info.statusLabel->setStyleSheet(QString("font-size: 10px; color: %1; font-weight: bold;").arg(newColor));
            info.idLabel->setStyleSheet(QString("font-weight: bold; font-size: 11px; color: %1;").arg(newIcon));
            info.status = newStatus;
        }
    }
}

void MultiThreadProgressDialog::setTotalThreads(int count)
{
    m_totalThreads = count;
}

void MultiThreadProgressDialog::updateOverallProgress(int completed, int total)
{
    // 更新进度数据
    m_overallCompleted = completed;
    m_overallTotal = total;

    // 更新进度条和标签
    if (total > 0) {
        int percentage = (completed * 100) / total;
        m_overallProgressBar->setValue(percentage);
        m_overallProgressLabel->setText(QString("总体进度: %1% (%2/%3)")
                                      .arg(percentage)
                                      .arg(completed)
                                      .arg(total));
    }
}

void MultiThreadProgressDialog::updateStatistics()
{
    // 更新成功/失败/处理中统计
    m_successLabel->setText(QString::number(m_successCount));
    m_failedLabel->setText(QString::number(m_failedCount));
    m_activeLabel->setText(QString::number(m_activeCount));

    // 计算速度
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;
    if (elapsed > 0 && m_startTime > 0) {
        double speed = (m_overallCompleted * 1000.0) / elapsed; // 条/秒
        m_speedLabel->setText(QString("%1 条/秒").arg(speed, 0, 'f', 1));

        // 计算预计剩余时间
        if (m_overallTotal > m_overallCompleted) {
            int remaining = m_overallTotal - m_overallCompleted;
            int eta = static_cast<int>(remaining / speed);
            m_etaLabel->setText(formatTime(eta));
        } else {
            m_etaLabel->setText("已完成");
        }
    }
}

void MultiThreadProgressDialog::updateSuccessCount(int count)
{
    m_successCount = count;
}

void MultiThreadProgressDialog::updateFailedCount(int count)
{
    m_failedCount = count;
}

void MultiThreadProgressDialog::setStartTime(qint64 startTime)
{
    m_startTime = startTime;
}

QString MultiThreadProgressDialog::formatTime(int seconds)
{
    if (seconds < 60) {
        return QString("%1秒").arg(seconds);
    } else if (seconds < 3600) {
        int minutes = seconds / 60;
        int secs = seconds % 60;
        return QString("%1分%2秒").arg(minutes).arg(secs);
    } else {
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        return QString("%1小时%2分").arg(hours).arg(minutes);
    }
}

QString MultiThreadProgressDialog::getProgressBarStyle(const QString& chunkColor) const
{
    return QString(R"(
        QProgressBar {
            border: 1px solid #c0c0c0;
            border-radius: 2px;
            text-align: center;
            font-size: 9px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f9f9f9, stop:1 #e8e8e8);
            color: #333;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);
            border-radius: 1px;
        }
    )").arg(chunkColor).arg(QColor(chunkColor).darker(20).name());
}

QString MultiThreadProgressDialog::getLabelStyle(const QString& color, int size) const
{
    return QString("font-size: %1px; color: %2; font-weight: bold;").arg(size).arg(color);
}

void MultiThreadProgressDialog::translationCompleted()
{
    // 立即标记翻译已完成
    m_isTranslationCompleted = true;

    // 停止统计定时器
    if (m_statsUpdateTimer) {
        m_statsUpdateTimer->stop();
    }

    // 更新进度条为绿色完成状态（IDM风格）
    m_overallProgressBar->setValue(100);
    m_overallProgressLabel->setText("翻译完成");
    m_overallProgressBar->setStyleSheet(getProgressBarStyle("#27ae60"));

    // 更新所有线程状态为完成
    for (auto& info : m_threads) {
        info.statusLabel->setText("已完成");
        info.statusLabel->setStyleSheet(getLabelStyle("#27ae60", 10));
        info.idLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: #27ae60;");
        info.progressBar->setValue(100);
        info.progressBar->setStyleSheet(getProgressBarStyle("#27ae60"));
    }

    // 最终统计更新
    updateStatistics();
    m_etaLabel->setText("已完成");

    // 更新按钮
    m_cancelButton->setText("关闭");

    // 更新活动数（应变为0）
    m_activeCount = 0;
    m_activeLabel->setText("0");

    qWarning() << "translationCompleted: m_isTranslationCompleted=" << m_isTranslationCompleted
               << "button text=" << m_cancelButton->text();
}

void MultiThreadProgressDialog::reset()
{
    // 停止定时器
    if (m_statsUpdateTimer) {
        m_statsUpdateTimer->stop();
    }

    // 清除所有线程信息
    for (auto& info : m_threads) {
        if (info.container) {
            info.container->deleteLater();
        }
    }
    m_threads.clear();
    m_threadListLayout->addStretch(); // 添加回stretch

    // 重置统计
    m_successCount = 0;
    m_failedCount = 0;
    m_activeCount = 0;
    m_startTime = 0;

    // 重置UI（IDM风格）
    m_totalThreads = 0;
    m_overallCompleted = 0;
    m_overallTotal = 0;
    m_overallProgressBar->setValue(0);
    m_overallProgressLabel->setText("翻译进度");
    m_overallProgressBar->setStyleSheet(getProgressBarStyle("#4a90e2"));

    m_successLabel->setText("0");
    m_failedLabel->setText("0");
    m_activeLabel->setText("0");
    m_speedLabel->setText("0.0 条/秒");
    m_etaLabel->setText("计算中...");
    m_concurrentLabel->setText("12");

    m_cancelButton->setText("取消翻译");
    m_cancelButton->setEnabled(true);
    m_isTranslationCompleted = false;

    // 重启统计定时器
    m_statsUpdateTimer->start();
}

void MultiThreadProgressDialog::closeEvent(QCloseEvent* event)
{
    qWarning() << "closeEvent - m_isTranslationCompleted:" << m_isTranslationCompleted
               << "buttonText:" << (m_cancelButton ? m_cancelButton->text() : "null");

    // 情况1：翻译已完成，允许关闭
    if (m_isTranslationCompleted || (m_cancelButton && m_cancelButton->text() == "关闭")) {
        qWarning() << "翻译已完成，允许关闭对话框";
        event->accept();
        return;
    }

    // 情况2：翻译未完成，调用取消按钮的逻辑（但不立即关闭窗口）
    if (m_cancelButton && m_cancelButton->text() == "取消翻译") {
        qWarning() << "翻译未完成，触发取消翻译逻辑";
        // 忽略关闭事件，让 onCancelClicked() 处理后续逻辑
        event->ignore();
        onCancelClicked();
        return;
    }

    // 其他情况，允许关闭
    event->accept();
}

void MultiThreadProgressDialog::onCancelClicked()
{
    qWarning() << "onCancelClicked - m_isTranslationCompleted:" << m_isTranslationCompleted
               << "buttonText:" << m_cancelButton->text()
               << "buttonEnabled:" << m_cancelButton->isEnabled();

    // 情况1：翻译已完成，直接关闭对话框
    if (m_isTranslationCompleted || m_cancelButton->text() == "关闭") {
        qWarning() << "翻译已完成，直接关闭对话框";
        accept();
        return;
    }

    // 情况2：翻译未完成，弹出确认对话框
    if (m_cancelButton->text() == "取消翻译") {
        qWarning() << "翻译未完成，弹出确认对话框";

        // 创建自定义对话框
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("确认取消翻译");
        msgBox.setText("确定要取消翻译吗？");
        msgBox.setInformativeText("请选择如何处理已完成的翻译结果：");

        // "确定" - 取消翻译，保留已完成的翻译
        QAbstractButton* keepButton = msgBox.addButton("确定", QMessageBox::AcceptRole);
        // "不保留" - 取消翻译，不保留已完成的翻译
        QAbstractButton* discardButton = msgBox.addButton("不保留", QMessageBox::DestructiveRole);
        // "取消" - 不取消翻译，回到进度条窗口
        QAbstractButton* noCancelButton = msgBox.addButton("取消", QMessageBox::RejectRole);

        msgBox.setIcon(QMessageBox::Question);

        msgBox.exec();

        QAbstractButton* clickedButton = msgBox.clickedButton();

        if (clickedButton == keepButton) {
            qWarning() << "用户选择：取消翻译并保留已完成结果";
            // 禁用按钮防止重复点击
            m_cancelButton->setEnabled(false);
            m_cancelButton->setText("取消中...");

            // 发出取消请求信号，带上保留选项
            emit cancelTranslationRequested(CancelAndKeep);

            // 添加超时保护：如果 5 秒后对话框还没关闭，强制关闭
            QTimer::singleShot(5000, this, [this]() {
                if (m_cancelButton && m_cancelButton->text() == "取消中...") {
                    qWarning() << "取消超时，强制关闭对话框";
                    reject();
                }
            });
        } else if (clickedButton == discardButton) {
            qWarning() << "用户选择：取消翻译并丢弃已完成结果";
            // 禁用按钮防止重复点击
            m_cancelButton->setEnabled(false);
            m_cancelButton->setText("取消中...");

            // 发出取消请求信号，带上不保留选项
            emit cancelTranslationRequested(CancelAndDiscard);

            // 添加超时保护：如果 5 秒后对话框还没关闭，强制关闭
            QTimer::singleShot(5000, this, [this]() {
                if (m_cancelButton && m_cancelButton->text() == "取消中...") {
                    qWarning() << "取消超时，强制关闭对话框";
                    reject();
                }
            });
        } else {
            qWarning() << "用户选择：不取消翻译";
            // 不做任何事情，保持对话框打开
        }
    } else {
        // 其他情况，直接关闭
        qWarning() << "未知按钮状态，直接关闭对话框";
        accept();
    }
}
