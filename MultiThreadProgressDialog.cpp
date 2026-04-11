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
    setMinimumSize(480, 320);
    resize(520, 380);

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        move((screenGeometry.width() - width()) / 2, (screenGeometry.height() - height()) / 2);
    }

    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    m_statsUpdateTimer = new QTimer(this);
    m_statsUpdateTimer->setInterval(1000);
    connect(m_statsUpdateTimer, &QTimer::timeout, this, &MultiThreadProgressDialog::updateStatistics);
    m_statsUpdateTimer->start();
}

MultiThreadProgressDialog::~MultiThreadProgressDialog()
{
    if (m_statsUpdateTimer) m_statsUpdateTimer->stop();
}

void MultiThreadProgressDialog::setupUI()
{
    auto ss = [](const QString& s) { return s; };

    QString baseStyle = R"(
        QDialog { background: #ffffff; }
        QLabel { color: #374151; }
        QProgressBar {
            border: none;
            border-radius: 6px;
            text-align: center;
            font-size: 11px;
            font-weight: 600;
            background: #f3f4f6;
            color: #6b7280;
        }
        QProgressBar::chunk {
            border-radius: 6px;
        }
        QPushButton {
            border: none;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover { opacity: 0.9; }
        QPushButton:pressed { opacity: 0.8; }
    )";

    setStyleSheet(baseStyle);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(16);
    m_mainLayout->setContentsMargins(28, 24, 28, 20);

    // ========== 标题区域 ==========
    QHBoxLayout* headerLayout = new QHBoxLayout();

    m_overallProgressLabel = new QLabel("准备中...");
    m_overallProgressLabel->setStyleSheet("font-size: 15px; font-weight: 700; color: #111827;");
    headerLayout->addWidget(m_overallProgressLabel);
    headerLayout->addStretch();

    // 百分比大数字
    m_concurrentLabel = new QLabel("0%");
    m_concurrentLabel->setStyleSheet("font-size: 22px; font-weight: 800; color: #3b82f6; font-family: 'Segoe UI', Arial;");
    headerLayout->addWidget(m_concurrentLabel);

    m_mainLayout->addLayout(headerLayout);

    // ========== 进度条 ==========
    m_overallProgressBar = new QProgressBar(this);
    m_overallProgressBar->setRange(0, 100);
    m_overallProgressBar->setValue(0);
    m_overallProgressBar->setMinimumHeight(8);
    m_overallProgressBar->setTextVisible(false);
    m_overallProgressBar->setStyleSheet(R"(
        QProgressBar {
            border: none;
            border-radius: 4px;
            background: #e5e7eb;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #3b82f6, stop:1 #8b5cf6);
            border-radius: 4px;
        }
    )");
    m_mainLayout->addWidget(m_overallProgressBar);

    // ========== 统计信息（单行紧凑布局） ==========
    QWidget* statsRow = new QWidget();
    QHBoxLayout* statsLayout = new QHBoxLayout(statsRow);
    statsLayout->setContentsMargins(0, 4, 0, 4);
    statsLayout->setSpacing(20);

    auto makeStatItem = [&](const QString& icon, const QString& label, const QString& value, const QString& color) -> std::pair<QLabel*, QLabel*> {
        QWidget* item = new QWidget();
        QVBoxLayout* itemLayout = new QVBoxLayout(item);
        itemLayout->setSpacing(2);
        itemLayout->setContentsMargins(0, 0, 0, 0);

        QLabel* val = new QLabel(value);
        val->setStyleSheet(QString("font-size: 18px; font-weight: 700; color: %1;").arg(color));
        itemLayout->addWidget(val);

        QLabel* lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 11px; color: #9ca3af;");
        itemLayout->addWidget(lbl);

        statsLayout->addWidget(item);
        return {val, lbl};
    };

    auto [s1, l1] = makeStatItem("✓", "成功", "0", "#22c55e");
    auto [f1, f2] = makeStatItem("✗", "失败", "0", "#ef4444");
    auto [a1, a2] = makeStatItem("⟳", "进行中", "0", "#f59e0b");
    auto [sp1, sp2] = makeStatItem("⚡", "速度", "--", "#6366f1");

    m_successLabel = s1;
    m_failedLabel = f1;
    m_activeLabel = a1;
    m_speedLabel = sp1;

    statsLayout->addStretch();

    auto [e1, e2] = makeStatItem("◷", "剩余", "计算...", "#8b5cf6");
    m_etaLabel = e1;

    m_mainLayout->addWidget(statsRow);

    // ========== 分隔线 ==========
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background: #f3f4f6; max-height: 1px;");
    m_mainLayout->addWidget(line);

    // ========== 线程列表 ==========
    setupCompactThreadView();
    m_mainLayout->addWidget(m_threadScrollArea, 1);

    // ========== 底部按钮 ==========
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 12, 0, 0);
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton("取消翻译");
    m_cancelButton->setMinimumWidth(100);
    m_cancelButton->setFixedHeight(36);
    m_cancelButton->setStyleSheet(R"(
        QPushButton {
            background: #f3f4f6;
            color: #374151;
        }
        QPushButton:hover {
            background: #e5e7eb;
        }
    )");
    connect(m_cancelButton, &QPushButton::clicked, this, &MultiThreadProgressDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    m_mainLayout->addLayout(buttonLayout);
}

void MultiThreadProgressDialog::setupStatisticsPanel() {}

void MultiThreadProgressDialog::setupCompactThreadView()
{
    m_threadScrollArea = new QScrollArea(this);
    m_threadScrollArea->setWidgetResizable(true);
    m_threadScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_threadScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_threadScrollArea->setMinimumHeight(120);
    m_threadScrollArea->setFrameShape(QFrame::NoFrame);
    m_threadScrollArea->setStyleSheet(
        "QScrollArea { border: 1px solid #f3f4f6; border-radius: 8px; background: #fafafa; }"
        "QScrollBar:vertical { border: none; background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #d1d5db; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #9ca3af; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    );

    m_threadContainer = new QWidget();
    m_threadContainer->setStyleSheet("background: transparent;");
    m_threadListLayout = new QVBoxLayout(m_threadContainer);
    m_threadListLayout->setSpacing(3);
    m_threadListLayout->setContentsMargins(8, 8, 8, 8);
    m_threadListLayout->addStretch();

    m_threadScrollArea->setWidget(m_threadContainer);
}

void MultiThreadProgressDialog::updateThreadProgress(int threadId, int completed, int total, const QString& message)
{
    Q_UNUSED(message)

    m_overallCompleted = completed;
    m_overallTotal = total;
    updateOverallProgress(completed, total);

    if (!m_threads.contains(threadId)) {
        ThreadInfo info;
        info.container = new QWidget();
        info.container->setStyleSheet(
            "background: #ffffff;"
            "border-radius: 6px;"
            "border: 1px solid #f3f4f6;"
        );
        QHBoxLayout* containerLayout = new QHBoxLayout(info.container);
        containerLayout->setSpacing(10);
        containerLayout->setContentsMargins(10, 6, 10, 6);

        info.idLabel = new QLabel(QString("#%1").arg(threadId + 1));
        info.idLabel->setFixedWidth(32);
        info.idLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #3b82f6;");
        containerLayout->addWidget(info.idLabel);

        info.statusLabel = new QLabel("请求中");
        info.statusLabel->setFixedWidth(50);
        info.statusLabel->setStyleSheet("font-size: 10px; font-weight: 600; color: #f59e0b;");
        containerLayout->addWidget(info.statusLabel);

        info.progressBar = new QProgressBar();
        info.progressBar->setRange(0, total > 0 ? total : 100);
        info.progressBar->setValue(completed);
        info.progressBar->setFixedHeight(6);
        info.progressBar->setTextVisible(false);
        info.progressBar->setStyleSheet(R"(
            QProgressBar { border: none; border-radius: 3px; background: #e5e7eb; }
            QProgressBar::chunk { border-radius: 3px; background: #93c5fd; }
        )");
        containerLayout->addWidget(info.progressBar, 1);

        m_threadListLayout->insertWidget(m_threadListLayout->count() - 1, info.container);

        info.completed = completed;
        info.total = total;
        info.status = "进行中";
        info.lastUpdateTime = QDateTime::currentDateTime();

        m_threads[threadId] = info;
        m_totalThreads = qMax(m_totalThreads, threadId + 1);
        m_activeCount++;
    } else {
        ThreadInfo& info = m_threads[threadId];
        info.completed = completed;
        info.total = total;
        info.lastUpdateTime = QDateTime::currentDateTime();

        int percentage = total > 0 ? (completed * 100 / total) : 0;
        info.progressBar->setRange(0, total > 0 ? total : 100);
        info.progressBar->setValue(completed);

        QString newStatus, statusColor, idColor, chunkBg;

        if (percentage >= 100) {
            newStatus = "已完成";
            statusColor = "#22c55e";
            idColor = "#22c55e";
            chunkBg = "#86efac";
        } else {
            int elapsed = info.lastUpdateTime.secsTo(QDateTime::currentDateTime());
            if (elapsed > 30) {
                newStatus = QString("超时%1s").arg(elapsed);
                statusColor = "#ef4444";
                idColor = "#ef4444";
                chunkBg = "#fca5a5";
            } else {
                newStatus = "请求中";
                statusColor = "#f59e0b";
                idColor = "#3b82f6";
                chunkBg = "#93c5fd";
            }
        }

        if (info.status != newStatus) {
            info.statusLabel->setText(newStatus);
            info.statusLabel->setStyleSheet(QString("font-size: 10px; font-weight: 600; color: %1;").arg(statusColor));
            info.idLabel->setStyleSheet(QString("font-size: 11px; font-weight: 700; color: %1;").arg(idColor));
            info.progressBar->setStyleSheet(QString(
                "QProgressBar { border: none; border-radius: 3px; background: #e5e7eb; }"
                "QProgressBar::chunk { border-radius: 3px; background: %1; }"
            ).arg(chunkBg));
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
    m_overallCompleted = completed;
    m_overallTotal = total;

    if (total > 0) {
        int percentage = (completed * 100) / total;
        m_overallProgressBar->setValue(percentage);
        m_concurrentLabel->setText(QString("%1%").arg(percentage));

        QString pctText = percentage < 100
            ? QString("翻译中 %1/%2").arg(completed).arg(total)
            : "翻译完成";
        m_overallProgressLabel->setText(pctText);
    }
}

void MultiThreadProgressDialog::updateStatistics()
{
    m_successLabel->setText(QString::number(m_successCount));
    m_failedLabel->setText(QString::number(m_failedCount));
    m_activeLabel->setText(QString::number(m_activeCount));

    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;
    if (elapsed > 0 && m_startTime > 0) {
        double speed = (m_overallCompleted * 1000.0) / elapsed;
        m_speedLabel->setText(QString("%1/s").arg(speed, 0, 'f', 1));

        if (m_overallTotal > m_overallCompleted) {
            int eta = static_cast<int>((m_overallTotal - m_overallCompleted) / speed);
            m_etaLabel->setText(formatTime(eta));
        } else {
            m_etaLabel->setText("--");
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
    if (seconds < 60) return QString("%1s").arg(seconds);
    if (seconds < 3600) return QString("%1m%2s").arg(seconds / 60).arg(seconds % 60);
    return QString("%1h%2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
}

QString MultiThreadProgressDialog::getProgressBarStyle(const QString&) const { return QString(); }

QString MultiThreadProgressDialog::getLabelStyle(const QString&, int) const { return QString(); }

void MultiThreadProgressDialog::translationCompleted()
{
    m_isTranslationCompleted = true;
    if (m_statsUpdateTimer) m_statsUpdateTimer->stop();

    m_overallProgressBar->setValue(100);
    m_overallProgressLabel->setText("翻译完成");
    m_overallProgressBar->setStyleSheet(R"(
        QProgressBar { border: none; border-radius: 4px; background: #e5e7eb; }
        QProgressBar::chunk { border-radius: 4px; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #22c55e,stop:1 #86efac); }
    )");
    m_concurrentLabel->setStyleSheet("font-size: 22px; font-weight: 800; color: #22c55e; font-family: 'Segoe UI', Arial;");

    for (auto& info : m_threads) {
        info.statusLabel->setText("已完成");
        info.statusLabel->setStyleSheet("font-size: 10px; font-weight: 600; color: #22c55e;");
        info.idLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #22c55e;");
        info.progressBar->setValue(100);
        info.progressBar->setStyleSheet(
            "QProgressBar { border: none; border-radius: 3px; background: #e5e7eb; }"
            "QProgressBar::chunk { border-radius: 3px; background: #86efac; }"
        );
    }

    updateStatistics();
    m_etaLabel->setText("--");
    m_cancelButton->setText("关闭");
    m_cancelButton->setStyleSheet(R"(
        QPushButton { background: #3b82f6; color: white; }
        QPushButton:hover { background: #2563eb; }
    )");
    m_activeCount = 0;
    m_activeLabel->setText("0");
}

void MultiThreadProgressDialog::reset()
{
    if (m_statsUpdateTimer) m_statsUpdateTimer->stop();

    for (auto& info : m_threads) {
        if (info.container) info.container->deleteLater();
    }
    m_threads.clear();
    m_threadListLayout->addStretch();

    m_successCount = 0;
    m_failedCount = 0;
    m_activeCount = 0;
    m_startTime = 0;
    m_totalThreads = 0;
    m_overallCompleted = 0;
    m_overallTotal = 0;

    m_overallProgressBar->setValue(0);
    m_overallProgressLabel->setText("准备中...");
    m_overallProgressBar->setStyleSheet(R"(
        QProgressBar { border: none; border-radius: 4px; background: #e5e7eb; }
        QProgressBar::chunk { border-radius: 4px; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #3b82f6,stop:1 #8b5cf6); }
    )");
    m_concurrentLabel->setStyleSheet("font-size: 22px; font-weight: 800; color: #3b82f6; font-family: 'Segoe UI', Arial;");
    m_concurrentLabel->setText("0%");

    m_successLabel->setText("0");
    m_failedLabel->setText("0");
    m_activeLabel->setText("0");
    m_speedLabel->setText("--");
    m_etaLabel->setText("计算...");

    m_cancelButton->setText("取消翻译");
    m_cancelButton->setStyleSheet(R"(
        QPushButton { background: #f3f4f6; color: #374151; }
        QPushButton:hover { background: #e5e7eb; }
    )");
    m_cancelButton->setEnabled(true);
    m_isTranslationCompleted = false;

    m_statsUpdateTimer->start();
}

void MultiThreadProgressDialog::closeEvent(QCloseEvent* event)
{
    if (m_isTranslationCompleted || (m_cancelButton && m_cancelButton->text() == "关闭")) {
        event->accept();
        return;
    }
    event->ignore();
    onCancelClicked();
}

void MultiThreadProgressDialog::onCancelClicked()
{
    if (m_isTranslationCompleted || (m_cancelButton && m_cancelButton->text() == "关闭")) {
        accept();
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认取消");
    msgBox.setText("确定要取消翻译吗？");
    msgBox.setInformativeText("可以选择保留或丢弃已完成的翻译结果。");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.button(QMessageBox::Save)->setText("保留结果");
    msgBox.button(QMessageBox::Discard)->setText("丢弃结果");
    msgBox.button(QMessageBox::Cancel)->setText("继续翻译");
    msgBox.setDefaultButton(QMessageBox::Cancel);

    int result = msgBox.exec();

    switch (result) {
    case QMessageBox::Save:
        emit cancelTranslationRequested(CancelAndKeep);
        break;
    case QMessageBox::Discard:
        emit cancelTranslationRequested(CancelAndDiscard);
        break;
    default:
        return;
    }

    m_cancelButton->setText("关闭");
    m_cancelButton->setEnabled(false);
}
