#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QChar>
#include <QStringList>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <functional>

// 外部声明 Font Awesome 字体 ID（从 main.cpp 导出）
extern int g_fontAwesomeSolidId;

// Font Awesome 图标工具类
namespace FA {
    // Font Awesome 7 图标 Unicode
    constexpr QChar Search = QChar(0xf002);                    // 搜索图标 (Solid)
    constexpr QChar CaseSensitive = QChar(0x0041);            // 大写字母 A (Solid)
    constexpr QChar Regex = QChar(0xf069);                    // asterisk 图标 (Solid)
    constexpr QChar QChevronLeft = QChar(0xf053);          // 左箭头 (Solid) - 上一个
    constexpr QChar QChevronRight = QChar(0xf054);           // 右箭头 (Solid) - 下一个
    constexpr QChar QChevronDown = QChar(0xf078);        // 下箭头 (Solid) - 展开
    constexpr QChar QChevronUp = QChar(0xf077);          // 上箭头 (Solid) - 收起
    constexpr QChar QCheck = QChar(0xf00c);                   // 勾选图标 (Solid) - 成功
    constexpr QChar QTimes = QChar(0xf00d);                   // 叉号图标 (Solid) - 失败
    constexpr QChar QTrash = QChar(0xf1f8);                   // 垃圾桶图标 (Solid) - 删除
    constexpr QChar QExclamationTriangle = QChar(0xf071);    // 三角警告图标 (Solid) - 警告
    constexpr QChar QChartBar = QChar(0xf080);                // 条形图 (Solid) - 统计
    constexpr QChar QLineChart = QChar(0xf201);                // 折线图 (Solid) - 趋势
    constexpr QChar QEdit = QChar(0xf15b);                     // 编辑图标 (Solid) - 编辑
    constexpr QChar QGift = QChar(0xf02d);                      // 礼物图标 (Solid) - 庆祝
    constexpr QChar QCog = QChar(0xf013);                         // 齿轮图标 (Solid) - 设置/配置
    constexpr QChar QKey = QChar(0xf084);                          // 钥匙图标 (Solid) - API 密钥
    constexpr QChar QPen = QChar(0xf304);                           // 笔图标 (Solid) - 提示词
    constexpr QChar QSync = QChar(0xf021);                          // 同步/加载图标 (Solid)
    constexpr QChar QHourglassHalf = QChar(0xf252);              // 沙漏图标 (Solid) - 等待/测试中
    constexpr QChar QLightbulb = QChar(0xf0eb);                     // 灯泡图标 (Solid) - 提示/帮助
    constexpr QChar QFolder = QChar(0xf07b);                       // 文件夹图标 (Solid)
    constexpr QChar QWrench = QChar(0xf0ad);                      // 扳手图标 (Solid) - 工具/编辑
    constexpr QChar QClipboardList = QChar(0xf46d);            // 列表图标 (Solid) - 管理
    constexpr QChar QSearchMagnifyingGlass = QChar(0xf002);   // 放大镜图标 (Solid) - 搜索
    constexpr QChar QSave = QChar(0xf0c7);                         // 保存图标 (Solid)
    constexpr QChar QBullseye = QChar(0xf140);                     // 靶心图标 (Solid) - 性能/目标
    constexpr QChar QPalette = QChar(0xf53f);                     // 调色板图标 (Solid) - 界面/设计
    constexpr QChar QGamepadModern = QChar(0xf5cc);          // 现代游戏手柄图标 (Solid) - 主标题
    constexpr QChar QBoltLightning = QChar(0xf0e7);        // 闪电图标 (Solid) - 快速操作
    constexpr QChar QFileAlt = QChar(0xf15c);                    // 文件图标 (Solid) - 文件格式
    constexpr QChar QKeyboard = QChar(0xf11c);                  // 键盘图标 (Solid) - 快捷键
    constexpr QChar QGlobe = QChar(0xf0ac);                      // 地球图标 (Solid) - 游戏平台
    constexpr QChar QTable = QChar(0xf0ce);                      // 表格图标 (Solid) - 表格
    constexpr QChar QDatabase = QChar(0xf1c0);                  // 数据库图标 (Solid) - 数据
    constexpr QChar QLanguage = QChar(0xf1ab);                  // 语言图标 (Solid) - 语言
    constexpr QChar QBuilding = QChar(0xf1ad);                  // 建筑图标 (Solid) - 团队
    constexpr QChar QCodeBranch = QChar(0xf126);              // 代码分支图标 (Solid) - 技术
    constexpr QChar QTag = QChar(0xf02b);                        // 标签图标 (Solid) - 版本
    constexpr QChar QPlus = QChar(0xf102);                       // 加号图标 (Solid) - 添加
    constexpr QChar QFolderOpen = QChar(0xf07c);              // 打开的文件夹 (Solid) - 打开文件
    constexpr QChar QUpload = QChar(0xf093);                    // 上传图标 (Solid) - 拖拽上传
    constexpr QChar QInfoCircle = QChar(0xf05a);              // 信息圆圈 (Solid) - 关于信息
    constexpr QChar QFileCode = QChar(0xf1c9);                  // 代码文件图标 (Solid) - 格式
    constexpr QChar QCalendar = QChar(0xf133);               // 日历图标 (Solid) - 日期
    constexpr QChar QUser = QChar(0xf007);                     // 用户图标 (Solid) - 作者/开发者
    constexpr QChar QLink = QChar(0xf0c1);                       // 链接图标 (Solid) - 外部链接
    constexpr QChar QUserGroup = QChar(0xf0c0);              // 用户组图标 (Solid) - 团队组织

    // 获取 Font Awesome 7 Free Solid 字体族名称
    inline QString solidFontFamily() {
        static QString cachedFamily;
        if (!cachedFamily.isEmpty()) {
            return cachedFamily;
        }
        
        static QFontDatabase db;
        if (g_fontAwesomeSolidId != -1) {
            QStringList families = db.applicationFontFamilies(g_fontAwesomeSolidId);
            if (!families.isEmpty()) {
                cachedFamily = families.first();
                return cachedFamily;
            }
        }
        
        // 如果字体加载失败，返回空字符串
        cachedFamily = QString();
        return cachedFamily;
    }

    // 获取 Font Awesome 7 Free Solid 字体 - 直接从加载的 otf 文件获取
    inline QFont solidFont(int size = 14) {
        QFont font;
        QString family = solidFontFamily();
        if (!family.isEmpty()) {
            font = QFont(family);
            font.setPointSize(size);
            return font;
        }
        
        // 如果字体加载失败，使用默认字体
        font = QFont();
        font.setPointSize(size);
        return font;
    }
}

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QCheckBox>
#include <QDialog>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QGroupBox>
#include <QGridLayout>
#include <QMap>
#include <QDateTime>
#include <QMessageBox>
#include <QtWidgets/QMenu>
#include <QAction>
#include <QtCore/QTimer>
#include <QtCore/QSet>
#include <QClipboard>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtCore/QThread>
#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QToolTip>
#include "GXTEditor.h"
#include <QDrag>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtConcurrent/QtConcurrent>
#include <QtGui/QFont>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtWidgets/QStyledItemDelegate>
#include <QtCore/QCache>
#include <QRandomGenerator>
#include <QDesktopServices>
#include <QApplication>
#include <QStandardPaths>
#include <QDateTime>
#include <QUrl>
#include <QToolButton>
#include <QMenu>
#include <functional>
#include <map>

// 前向声明
class GXTTableModel;
class OptimizedItemDelegate;
class GXTStudio;
class SmartTranslator;
class TranslateConfigDialog;
class TextRenderWidget;
class CharTableWidget;

// 多线程进度条窗口类 - 简约高效设计
class MultiThreadProgressDialog : public QDialog {
    Q_OBJECT

public:
    enum CancelOption {
        CancelAndKeep = 0,  // 取消翻译并保留已完成的结果
        CancelAndDiscard = 1,  // 取消翻译并丢弃已完成的结果
        NoCancel = 2  // 不取消翻译
    };

    explicit MultiThreadProgressDialog(QWidget *parent = nullptr);
    ~MultiThreadProgressDialog();

    void updateThreadProgress(int threadId, int completed, int total, const QString& message);
    void setTotalThreads(int count);
    void updateOverallProgress(int completed, int total);
    void translationCompleted();
    void reset();
    void updateSuccessCount(int count);  // 更新成功数
    void updateFailedCount(int count);   // 更新失败数
    void setStartTime(qint64 startTime);  // 设置开始时间用于速度计算

signals:
    void cancelTranslationRequested(CancelOption option);  // 请求取消翻译，带选项

private slots:
    void onCancelClicked();
    void updateStatistics();  // 定时更新统计信息
    
    // UI辅助方法 - 提取重复样式
    QString getProgressBarStyle(const QString& chunkColor = "#4a90e2") const;
    QString getLabelStyle(const QString& color, int size) const;

private:
    void setupUI();
    void setupCompactThreadView();
    void setupStatisticsPanel();
    void updateThreadInfo(int threadId);
    QString formatTime(int seconds);
    void closeEvent(QCloseEvent* event) override;  // 重写关闭事件，使其与"取消翻译"按钮一致

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
        QWidget* container;  // 容器widget
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

// TranslateResult结构体前向声明
struct TranslateResult;

#include "ui_GXTStudio.h"
#include "GXTParser.h"
#include "GXTEditor.h"
#include "TranslateConfigDialog.h"
#include "SmartTranslator.h"
#include "CodeTableConverter.h"
#include "CharTableParser.h"
#include "CharTableWidget.h"

// FileTab结构体定义 - 需要在SaveTask之前定义
struct FileTab {
    QString filePath;
    QString fileName;
    std::vector<GXTTabl> tables;
    std::vector<WHMEntry> whmEntries;
    std::vector<DATEntry> datEntries;  // DAT文件条目（完全独立于WHM）
    bool isWHM = false;
    bool isDAT = false;  // 区分DAT和WHM文件
    bool isCharTable = false;  // 字符表文件（VC/IV）
    bool isModified = false;
    int currentTableIndex = 0;
    GXTVersion version = GXTVersion::UNKNOWN; // 文件版本信息
    
    // Qt控件
    QListWidget* tableList = nullptr;
    QTableView* entryTableView = nullptr;  // 改为QTableView
    GXTTableModel* entryTableModel = nullptr; // 新增模型
    QLineEdit* searchEdit = nullptr;
    QPushButton* searchPrevButton = nullptr; // 上一个按钮
    QPushButton* searchNextButton = nullptr; // 下一个按钮
    QToolButton* caseSensitiveButton = nullptr;  // 大小写敏感图标按钮
    QToolButton* regexButton = nullptr;           // 正则表达式图标按钮
    QToolButton* addTableButton = nullptr;        // 添加新表按钮

    // 添加键值对控件
    QLineEdit* keyEdit = nullptr;       // 键输入框
    QLineEdit* valueEdit = nullptr;     // 值输入框
    QPushButton* addEntryButton = nullptr; // 添加条目按钮
    
    // 文本渲染预览控件
    class TextRenderWidget* textRenderWidget = nullptr;

    // 字符表相关控件和数据
    class CharTableWidget* charTableWidget = nullptr;  // 字符表显示控件
    CharTableData charTableData;  // 字符表数据
    
    // 异步解析状态
    bool isParsing = false;
    QProgressDialog* progressDialog = nullptr;
    
    // 简化的缓存状态
    struct {
        QTimer* resizeTimer = nullptr;
        QTimer* scrollTimer = nullptr;
        QSize lastSize;
        bool layoutNeedsUpdate = false;
    } cache;
    
    // 极致优化的列表缓存
    struct {
        std::vector<QString> cachedDisplayTexts;   // 预计算的显示文本
        std::vector<QString> cachedTableNames;     // 缓存的表名
        std::vector<QString> cachedCounts;          // 缓存的计数
        std::vector<bool> cachedMainFlags;         // 缓存的主表标志
        size_t lastTableCount = 0;                 // 上次的表数量
        bool needsRebuild = true;                   // 是否需要重建缓存
        QTimer* cleanupTimer = nullptr;             // 缓存清理定时器
    } listCache;
};

// 支持拖拽的表格列表
class DraggableTableList : public QListWidget {
    Q_OBJECT
    
public:
    DraggableTableList(QWidget* parent = nullptr) : QListWidget(parent) {
        // 启用拖拽
        setDragEnabled(true);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            dragStartPos = event->position().toPoint();
        }
        QListWidget::mousePressEvent(event);
    }
    
    void mouseMoveEvent(QMouseEvent* event) override {
        if (!(event->buttons() & Qt::LeftButton)) {
            return;
        }
        
        if ((event->position().toPoint() - dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
            return;
        }
        
        // 获取选中的表
        QListWidgetItem* item = itemAt(dragStartPos);
        if (item) {
            int tableIndex = item->data(Qt::UserRole).toInt();
            if (tableIndex >= 0) {
                QString tableName = item->data(Qt::UserRole + 1).toString();
                
                // 创建临时文件
                QString tempFilePath = createTempTableFile(tableIndex, tableName);
                if (!tempFilePath.isEmpty()) {
                    // 创建拖拽操作
                    QDrag* drag = new QDrag(this);
                    QMimeData* mimeData = new QMimeData();
                    
                    // 设置文件URL列表进行拖拽
                    QList<QUrl> urls;
                    urls << QUrl::fromLocalFile(tempFilePath);
                    mimeData->setUrls(urls);
                    
                    drag->setMimeData(mimeData);
                    
                    // 执行拖拽
                    Qt::DropAction dropAction = drag->exec(Qt::CopyAction);
                    
                    // 拖拽完成后可以选择删除临时文件，或者让用户自己处理
                    // 这里保留文件，让用户决定是否删除
                }
            }
        }
    }

private:
    QPoint dragStartPos;
    
    QString createTempTableFile(int tableIndex, const QString& tableName);
};

// 极致优化的列表项委托 - 专门为左侧表格列表设计
class OptimizedListItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
    
public:
    explicit OptimizedListItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {
        m_deleteIconRect = QRect(); // 初始化删除图标区域
    }
    
    // 核心绘制函数 - 使用原生绘制避免QWidget开销
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        
        // 启用抗锯齿以获得更好的文本渲染效果
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);
        
        // 绘制圆角矩形背景
        bool isSelected = (option.state & QStyle::State_Selected);
        bool isHovered = (option.state & QStyle::State_MouseOver);
        
        QColor bgColor;
        if (isSelected) {
            bgColor = QColor(227, 242, 253);
        } else if (isHovered) {
            bgColor = QColor(248, 249, 250);
        } else {
            bgColor = Qt::white;
        }
        
        // 绘制圆角矩形背景
        painter->setPen(Qt::NoPen);
        painter->setBrush(bgColor);
        painter->drawRoundedRect(option.rect.adjusted(4, 2, -4, -2), 6, 6);
        
        // 绘制微妙的底部边框（只在非悬停和未选中时）
        if (!isSelected && !isHovered) {
            painter->setPen(QColor(240, 240, 240));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(option.rect.adjusted(4, 2, -4, -2), 6, 6);
        }
        
        // 获取数据 - 从UserRole获取分开的表名和计数
        QString tableName = index.data(Qt::UserRole + 1).toString();
        QString countText = index.data(Qt::UserRole + 2).toString(); // 保留但不显示
        bool isMain = index.data(Qt::UserRole + 3).toBool();
        
        // 计算文本区域 - 留出删除图标的空间（只在悬停时）
        int iconSize = 24;
        int iconMargin = 8;
        int rightMargin = isHovered ? (iconSize + iconMargin + 12) : 12;
        QRect nameRect = option.rect.adjusted(12, 0, -rightMargin, 0);
        
        // 绘制表格名称 - 使用原始字体
        painter->setFont(option.font);
        painter->setPen(isMain ? QColor(25, 118, 210) : QColor(33, 37, 41));
        
        // 文本省略处理
        QString elidedName = painter->fontMetrics().elidedText(
            tableName, Qt::ElideRight, nameRect.width());
        painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft, elidedName);
        
        // 只在鼠标悬停时绘制删除图标
        if (isHovered) {
            QRect iconRect(option.rect.right() - iconSize - iconMargin,
                           option.rect.center().y() - iconSize / 2,
                           iconSize, iconSize);
            
            // 设置字体为 Font Awesome - 使用较小的字体
            QFont iconFont = FA::solidFont(14);
            painter->setFont(iconFont);
            
            // 设置图标颜色 - 浅灰色
            painter->setPen(QColor(180, 180, 180));
            
            // 绘制删除图标 (FA::QTrash - 垃圾桶)
            painter->drawText(iconRect, Qt::AlignCenter, FA::QTrash);
        }
        
        painter->restore();
    }
    
    // 统一项目高度以优化布局性能
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(200, 36); // 增加高度以获得更好的视觉效果
    }
    
    // 处理鼠标事件，检测删除图标点击
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                    const QStyleOptionViewItem& option, const QModelIndex& index) override {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                // 计算删除图标区域（与 paint 方法中的一致）
                int iconSize = 24;
                int iconMargin = 8;
                QRect iconRect(option.rect.right() - iconSize - iconMargin,
                               option.rect.center().y() - iconSize / 2,
                               iconSize, iconSize);
                
                // 获取鼠标位置（兼容 Qt 5 和 Qt 6）
                QPoint mousePos;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                mousePos = mouseEvent->position().toPoint();
#else
                mousePos = mouseEvent->pos();
#endif
                
                // 检查是否点击了删除图标
                if (iconRect.contains(mousePos)) {
                    emit deleteTableRequested(index);
                    return true;
                }
            }
        }
        
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

signals:
    void deleteTableRequested(const QModelIndex& index);

private:
    QRect m_deleteIconRect; // 删除图标区域
};

// 解析任务数据结构
struct ParseTask {
    QString filePath;
    QString fileName;
    std::string narrowPath;
    bool isWHM;
    bool isDAT;  // 完全区分DAT和WHM
    int tabIndex;
};

// 解析结果数据结构
struct ParseResult {
    bool success;
    QString filePath;
    QString fileName;
    bool isWHM;
    bool isDAT;  // 完全区分DAT和WHM
    std::vector<GXTTabl> tables;
    std::vector<WHMEntry> whmEntries;
    std::vector<DATEntry> datEntries;  // DAT条目（完全独立）
    QString errorMessage;
    int tabIndex;
    QElapsedTimer parseTime;
    GXTVersion version; // 文件版本信息
};

// 子线程解析器
class ParseWorker : public QObject {
    Q_OBJECT

public:
    ParseWorker(QObject* parent = nullptr) : QObject(parent) {}

public slots:
    void parseFile(const ParseTask& task) {
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
                result.success = parser.parseWHM(task.narrowPath, result.whmEntries);
                result.version = GXTVersion::UNKNOWN; // WHM文件没有版本信息
                if (!result.success) {
                    result.errorMessage = "WHM解析失败";
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

signals:
    void parseCompleted(const ParseResult& result);
};

// 保存任务数据结构
struct SaveTask {
    FileTab* tabPtr;
    QString filePath;
    bool isNewPath;
    bool isAutoSave;  // 是否为自动保存
};

// 保存结果数据结构
struct SaveResult {
    bool success;
    QString filePath;
    QString fileName;
    QString errorMessage;
    qint64 elapsedMs;
    size_t bytesWritten;
    bool isNewPath;
    bool isAutoSave;  // 是否为自动保存
};

// 自动保存数据副本 - 完全独立于主线程数据
struct AutoSaveData {
    QString filePath;
    GXTVersion version;
    bool isWHM;
    bool isDAT;
    // GXT/GXT2 数据 (使用GXTTabl与FileTab一致)
    std::vector<GXTTabl> tables;
    // WHM 数据
    std::vector<WHMEntry> whmEntries;
    // DAT 数据  
    std::vector<DATEntry> datEntries;
};

// 子线程保存器
class SaveWorker : public QObject {
    Q_OBJECT

public:
    explicit SaveWorker(QObject* parent = nullptr) : QObject(parent) {}

public slots:
    void saveFile(const SaveTask& task);

private:
    // 保存WHM文件的辅助方法（用于SaveWorker）
    bool saveWHMDirectly(FileTab* tab, const std::string& path);
    // DAT文件保存方法（完全独立于WHM）
    bool saveDATDirectly(FileTab* tab, const std::string& path);
    // Qt版本的保存方法，使用QString处理中文路径
    bool saveWHMDirectlyQt(FileTab* tab, const QString& filePath);
    bool saveDATDirectlyQt(FileTab* tab, const QString& filePath);
    
signals:
    void saveProgress(int percentage, const QString& message);
    void saveCompleted(const SaveResult& result);
};

// 表格项缓存池
class ItemPool {
public:
    static ItemPool& instance() {
        static ItemPool pool;
        return pool;
    }
    
    QTableWidgetItem* getItem() {
        QMutexLocker locker(&m_mutex);
        if (!m_pool.isEmpty()) {
            return m_pool.takeLast();
        }
        return new QTableWidgetItem();
    }
    
    void returnItem(QTableWidgetItem* item) {
        if (!item) return;
        
        QMutexLocker locker(&m_mutex);
        // 清理项目数据
        item->setText("");
        item->setToolTip("");
        item->setData(Qt::UserRole, QVariant());
        
        if (m_pool.size() < 1000) { // 限制缓存池大小
            m_pool.append(item);
        } else {
            delete item;
        }
    }
    
    void clear() {
        QMutexLocker locker(&m_mutex);
        qDeleteAll(m_pool);
        m_pool.clear();
    }

private:
    ItemPool() = default;
    ~ItemPool() {
        clear();
    }
    
    QList<QTableWidgetItem*> m_pool;
    QMutex m_mutex;
};

    // 简化的搜索高亮委托
class HighlightDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit HighlightDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    void setSearchText(const QString& text) { m_searchText = text; }
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyledItemDelegate::paint(painter, option, index);
        
        if (!m_searchText.isEmpty()) {
            QString text = index.data(Qt::DisplayRole).toString();
            if (text.contains(m_searchText, Qt::CaseInsensitive)) {
                QRect rect = option.rect.adjusted(2, 1, -2, -1);
                painter->fillRect(rect, QColor(255, 255, 0, 60));
            }
        }
    }
    
private:
    QString m_searchText;
    bool m_isHashKeyColumn = false;
    bool m_useRegex = false;  // 是否使用正则表达式
    bool m_caseSensitive = false;  // 是否区分大小写
};


class GXTStudio : public QMainWindow
{
    Q_OBJECT

public:
    GXTStudio(QWidget *parent = nullptr);
    ~GXTStudio();
    
    // 公共工具方法 - 需要在public中以便DraggableTableList访问
    FileTab* getCurrentTab();
    FileTab* getTab(int index);
    void showLogMessage(const QString& message);
    void showSaveSuccessDialog(const SaveResult& result);
    
    // 提供给后台线程的公共保存方法
    SaveResult performSaveTask(const SaveTask& task);
    
    // 提供给码表转换器的公共方法
    void updateEntryTable();
    
    // 格式化键值显示（添加0x前缀）
    static QString formatKeyForDisplay(const QString& key);

private slots:
    // 文件操作
    void openFile();
    void saveFile();
    void saveAsFile();
    void exportFile();
    void closeFile();
    void exitApp();
    
    // 编辑操作
    void findText();
    void replaceText();
    void toggleReadOnly(bool readOnly);
    
    // 视图操作
    void increaseFont();
    void decreaseFont();
    
    // 内部方法
    void updateFontSizes();
    
    // 帮助
    void showAbout();
    
    // 表和条目操作
    void switchToTable(int newTableIndex);
    void onTableSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void onAddTableClicked(const QModelIndex& index);
    void onEditTableName(const QModelIndex& index);
    void onTableNameChanged(const QModelIndex& index, const QString& newName);

    void onSearchTextChanged();
    void onSearchNext();
    void onSearchPrevious();
    void onCodeTableConvert(); // 已弃用
    void onVersionChanged(const QModelIndex& selectedIndex);
    void onSmartTranslate();
    void onConfigTranslate();
    void onExecuteTranslate();
    
    // 码表转换相关
    void onMountCodeTable(); // 挂载码表
    void onConvertCodeTable(); // 执行转换
    void onUnmountCodeTable(); // 卸载码表

    // 字符表相关
    void onCharTableCharacterSelected(uint16_t charCode, const QString& displayChar);
    
    // 工具方法
    void updateCodeTableButtonText(); // 更新码表按钮文本
    
    void addNewEntry();
    void addEntryFromInputs(QLineEdit* keyEdit, QLineEdit* valueEdit);
    void deleteEntry();
    void deleteSelectedRows();
    void addNewTable();
    void onTableDoubleClicked(QListWidgetItem* item);
    void renameTable(int tableIndex);
    void deleteTable();
    void onDeleteTableFromList(const QModelIndex& index);
    
    // 导出操作
    void exportCurrentFile();
    void exportSelectedTable();
    void exportTableToFolder(int tableIndex, const QString& basePath);
    void showTableContextMenu(const QPoint& pos);
    
    // 右键菜单操作
    void showContextMenu(const QPoint& pos);
    void copySelectedKeys();
    void copySelectedValues();
    void copySelectedKeyValues();
    void editCellAsDialog();
    void openEditDialog(int row, int column);
    void translateSelectedRows();
    
    // 标签页操作
    void onTabChanged(int index);
    void onTabMoved(int from, int to);  // 处理标签页拖拽移动
    void closeTab(int index);
    
    // 异步保存相关
    void setupSaveThread();
    void onSaveProgress(int percentage, const QString& message);
    void onSaveCompleted(const SaveResult& result);

    // 自动保存相关
    void onAutoSaveToggle();  // 切换自动保存开关
    void onAutoSaveTimer();  // 自动保存定时器触发
    void onAutoSaveProgress(int percentage, const QString& message);  // 自动保存进度
    void onAutoSaveCompleted(const SaveResult& result);  // 自动保存完成

    // 智能翻译相关
    void setupTranslationProgress();
    void onTranslationProgress(int completed, int total, const QString& message);
    void onTranslationCompleted(const QList<TranslateResult>& results);
    void onTranslationError(const QString& error);
    void onCancelTranslationRequested(MultiThreadProgressDialog::CancelOption option);
    void onTranslationCancelled();
    void applyPartialTranslationResults(const QList<TranslateResult>& results);
    void onTranslationFinished(const QList<TranslateResult>& results);

private:
    QPair<int, int> applyTranslationResultsToFile(const QList<TranslateResult>& results);
    void onTranslationRequestProgress(int completed, int total, const QString& message);
    void onTranslationBatchProgress(int completed, int total, const QString& message);
    void onTranslationTotalProgress(int completed, int total, const QString& message);
    
    // 工具方法
    QString calculateRemainingTime(int completed, int total);
    
    // UI辅助方法 - 优化样式代码复用
    QString getCardStyle(const QString& color) const;
    QString getButtonStyle(const QString& color, bool darker = false) const;
    QWidget* createWelcomeTab();
    CharTableWidget* createCharTableTab(const QString& fileName, const CharTableData& data); // 创建字符表标签页
    bool loadCharTableFile(const QString& filePath, const QString& fileName); // 加载字符表文件

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    Ui::GXTStudioClass ui;
    
    // 主要控件
    QTabWidget* m_tabWidget;
    QLabel* m_statusLabel;
    
    // 菜单和工具栏
    QMenuBar* m_menuBar;
    QToolBar* m_toolBar;
    QStatusBar* m_statusBar;
    
    // 动作
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_exportAction;
    QAction* m_closeAction;
    QAction* m_exitAction;
    QAction* m_findAction;
    QAction* m_replaceAction;
    QAction* m_readOnlyAction;
    QAction* m_increaseFontAction;
    QAction* m_decreaseFontAction;
    QAction* m_aboutAction;
    QAction* m_codeTableAction;    // 码表转换（已弃用，改为下拉按钮）
    QAction* m_smartTranslateAction; // 智能翻译
    QAction* m_configTranslateAction; // 配置翻译
    QAction* m_executeTranslateAction; // 执行翻译
    
    // 码表转换相关
    QToolButton* m_codeTableButton; // 码表转换下拉按钮
    QAction* m_mountCodeTableAction; // 挂载码表
    QAction* m_convertCodeTableAction; // 执行转换
    QAction* m_unmountCodeTableAction; // 卸载码表
    
    // 窗口大小调整优化
    QTimer* m_resizeDebouncer;
    QSize m_lastWindowSize;
    static const int RESIZE_DEBOUNCE_DELAY = 400; // 增加到400ms以大幅减少触发频率
    
    // 布局优化相关
    QTimer* m_cleanupTimer;
    
    std::vector<FileTab> m_fileTabs;
    int m_currentTabIndex;
    bool m_isReadOnly;
    int m_fontSize;
    
    // 异步解析相关
    QThread* m_parseThread;
    ParseWorker* m_parseWorker;
    
    // 异步保存相关
    QThread* m_saveThread;
    SaveWorker* m_saveWorker;
    QProgressDialog* m_saveProgressDialog;

    // 自动保存相关
    QToolButton* m_autoSaveButton;  // 自动保存开关按钮
    QProgressBar* m_autoSaveProgressBar;  // 自动保存进度条
    QTimer* m_autoSaveTimer;  // 自动保存定时器
    bool m_autoSaveEnabled;  // 自动保存是否启用
    QFutureWatcher<SaveResult>* m_autoSaveFutureWatcher;  // 自动保存任务监视器
    static const int AUTOSAVE_DELAY = 3000;  // 自动保存延迟（3秒）
    
    // 静态保存函数（供 QtConcurrent 使用）
    static bool saveWHMFromData(const AutoSaveData& data);
    static bool saveDATFromData(const AutoSaveData& data);
    static bool saveGXTFromData(const AutoSaveData& data);
    
    // 智能翻译相关
    SmartTranslator* m_smartTranslator;
    QThread* m_smartTranslatorThread;
    MultiThreadProgressDialog* m_translateProgressDialog;
    MultiThreadProgressDialog::CancelOption m_cancelOption; // 用户选择的取消选项
    QToolButton* m_smartTranslateButton; // 带下拉菜单的翻译按钮
    
    // 码表转换相关
    CodeTableConverter* m_codeTableConverter; // 码表转换器
    
    // 版本感知保存系统 - SaveStrategy结构体定义
    struct SaveStrategy {
        GXTVersion version;
        std::function<bool(GXTEditor&, const std::string&)> saveFunction;
        std::string description;
        std::vector<std::string> fileExtensions;
    };
    
    std::map<GXTVersion, SaveStrategy> m_saveStrategies;
    std::vector<SaveStrategy> m_allSaveStrategies;
    
    // 搜索状态
    struct SearchState {
        QString searchText;
        int currentTableIndex;
        int currentEntryIndex;
        std::vector<std::pair<int, int>> allMatches; // (tableIndex, entryIndex) 对
        bool isValid;
        bool caseSensitive;  // 大小写敏感
        bool useRegex;       // 使用正则表达式
        
        SearchState() : currentTableIndex(-1), currentEntryIndex(-1), isValid(false), 
                        caseSensitive(false), useRegex(false) {}
        void clear() {
            searchText.clear();
            currentTableIndex = -1;
            currentEntryIndex = -1;
            allMatches.clear();
            isValid = false;
            caseSensitive = false;
            useRegex = false;
        }
    };
    SearchState m_searchState;
    
    // 初始化方法
    void setupUI();
    void setupMenus();
    void setupToolBars();
    void setupStatusBar();
    void setupCentralWidget();
    void connectSignals();
    
    // 更新方法
    void updateTableList();
    void updateWindowTitle();
    void updateStatusBar();
    void updateActions();
    
    // 工具方法
    void addFileTab(const QString& filePath);
    void removeFileTab(int index);
    void cleanupDelegatesCache(); // 清理委托缓存
    void cleanupListCache(FileTab* tab); // 清理列表缓存
    QString getVersionString(GXTVersion version) const;
    std::string getVersionName(GXTVersion version);
    GXTVersion selectVersionDialog(); // 版本选择对话框
    int calculateKeyColumnWidth(); // 计算键列宽度
    
    // 重构的保存逻辑 - 版本感知保存系统
    // SaveStrategy结构体已在前面定义
    
    // 保存策略注册和管理
    void initializeSaveStrategies();
    bool saveFileWithVersion(FileTab* tab, const QString& filePath = QString());
    bool saveFileUsingStrategy(FileTab* tab, const SaveStrategy& strategy, const QString& filePath);
    SaveStrategy getSaveStrategyForVersion(GXTVersion version, const QString& fileExtension);
    SaveStrategy detectBestSaveStrategy(FileTab* tab, const QString& filePath);
    GXTVersion detectCompatibleVersion(GXTVersion originalVersion, const QString& targetExtension);
    bool validateVersionCompatibility(GXTVersion sourceVersion, GXTVersion targetVersion);
    
    // 保存辅助方法
    void prepareEditorForSave(FileTab* tab, GXTEditor& editor);
    bool performVersionSpecificSave(FileTab* tab, const QString& filePath);
    bool handleSaveError(const QString& operation, const std::exception& e, const QString& filePath);
    void logSaveOperation(const QString& operation, GXTVersion version, const QString& filePath, bool success);
    bool saveWHMDirectly(FileTab* tab, const QString& filePath);
    bool saveDATDirectly(FileTab* tab, const QString& filePath);  // DAT文件保存方法（完全独立于WHM）
    
    // 版本兼容性检查
    std::vector<GXTVersion> getCompatibleVersions(GXTVersion sourceVersion);
    bool canConvertBetweenVersions(GXTVersion from, GXTVersion to);
    std::string getVersionCompatibilityInfo(GXTVersion version);
    

    
    // 表格优化
    void setupTableOptimizations(QTableView* table);
    void precomputeTableMetrics();
    void setupResizeOptimizations();
    void performDelayedResize();
    void preloadVisibleArea(FileTab* tab);
    // isHashKeyColumn函数已移除，因为现在统一处理GXT和WHM的键列宽度
    
    // 搜索相关辅助方法
    void performSearch(const QString& searchText);
    void jumpToMatch(int matchIndex);
    void highlightMatch(int entryIndex);
    void clearSearchHighlight();
    
    // 异步解析方法
    void setupParseThread();
    void startAsyncParse(const QString& filePath);
    void onParseCompleted(const ParseResult& result);
    void createTabContent(FileTab& tab, int tabIndex);
};


// 极简高效的表格模型 - 专为性能优化
class GXTTableModel : public QAbstractTableModel {
    Q_OBJECT
    
public:
    enum Column { KeyColumn = 0, ValueColumn = 1, ColumnCount = 2 };
    
    explicit GXTTableModel(QObject* parent = nullptr) 
        : QAbstractTableModel(parent), m_tab(nullptr), m_editable(false), 
          m_cachedRowCount(-1), m_currentTableIndex(-1) {}
    
    void setTab(FileTab* tab) {
        beginResetModel();
        m_tab = tab;
        m_cachedRowCount = -1;
        // 【关键修复】总是从tab获取最新的currentTableIndex，确保与标签页状态同步
        m_currentTableIndex = tab ? tab->currentTableIndex : -1;
        endResetModel();
        
        // 【关键修复】强制清除所有缓存，确保下次访问时重新计算
        m_cachedRowCount = -1;
    }
    
    void setEditable(bool editable) { m_editable = editable; }
    
    // 加载SATKEY映射（静态方法，全局只加载一次）
    static void loadSATKeyMap();
    
    // 加载IVTKEY映射（静态方法，全局只加载一次）
    static void loadIVTKeyMap();
    
    // 查找SATKEY映射（静态方法）
    static bool findSATKey(uint32_t hash, QString& outKey);
    
    // 查找IVTKEY映射（静态方法）
    static bool findIVTKey(uint32_t hash, QString& outKey);
    
    // 反向查找：根据字符串查找hash（静态方法）
    static bool findSATHash(const QString& key, uint32_t& outHash);
    static bool findIVTHash(const QString& key, uint32_t& outHash);
    
    // 极简重置方法 - 强制更新以确保表格切换时无残留
    void resetModel() {
        if (!m_tab) return;
        
        // 总是执行重置以确保表格切换时无内容残留
        beginResetModel();
        m_currentTableIndex = m_tab->currentTableIndex;
        if (m_tab->isWHM) {
            m_cachedRowCount = static_cast<int>(m_tab->whmEntries.size());
        } else if (m_tab->isDAT) {
            m_cachedRowCount = static_cast<int>(m_tab->datEntries.size());
        } else if (m_currentTableIndex >= 0 && m_currentTableIndex < static_cast<int>(m_tab->tables.size())) {
            m_cachedRowCount = static_cast<int>(m_tab->tables[m_currentTableIndex].entries.size());
        } else {
            m_cachedRowCount = 0;
        }
        endResetModel();
    }
    
    // 强制数据重置 - 用于表格切换时确保无残留
    void forceDataReset() {
        beginResetModel();
        // 【关键修复】清除所有缓存
        m_cachedRowCount = -1;
        
        // 【关键修复】从tab重新获取currentTableIndex，确保状态同步
        if (m_tab) {
            m_currentTableIndex = m_tab->currentTableIndex;
            
            // 【关键修复】根据最新的tab状态重新计算行数
            if (m_tab->isWHM) {
                m_cachedRowCount = static_cast<int>(m_tab->whmEntries.size());
            } else if (m_tab->isDAT) {
                m_cachedRowCount = static_cast<int>(m_tab->datEntries.size());
            } else if (m_currentTableIndex >= 0 && m_currentTableIndex < static_cast<int>(m_tab->tables.size())) {
                m_cachedRowCount = static_cast<int>(m_tab->tables[m_currentTableIndex].entries.size());
            } else {
                m_cachedRowCount = 0;
            }
        } else {
            m_currentTableIndex = -1;
        }
        endResetModel();
    }
    
    // 快速行数计算 - 使用缓存和异常处理
    int rowCount(const QModelIndex& = QModelIndex()) const override {
        if (m_cachedRowCount >= 0) return m_cachedRowCount;
        if (!m_tab) return 0;
        
        try {
            int count = 0;
            if (m_tab->isWHM) {
                count = static_cast<int>(m_tab->whmEntries.size());
            } else if (m_tab->isDAT) {
                count = static_cast<int>(m_tab->datEntries.size());
            } else if (!m_tab->tables.empty() && 
                       m_tab->currentTableIndex >= 0 && 
                       m_tab->currentTableIndex < static_cast<int>(m_tab->tables.size())) {
                count = static_cast<int>(m_tab->tables[m_tab->currentTableIndex].entries.size());
            }
            
            const_cast<GXTTableModel*>(this)->m_cachedRowCount = count;
            return count;
        } catch (const std::exception& e) {
            qWarning() << "RowCount calculation failed:" << e.what();
            return 0;
        } catch (...) {
            qWarning() << "Unknown exception during rowCount calculation";
            return 0;
        }
    }
    
    int columnCount(const QModelIndex& = QModelIndex()) const override { return ColumnCount; }
    
    // 优化的数据访问 - 减少字符串转换开销，增强安全性
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        // 基本验证
        if (!index.isValid() || !m_tab) return QVariant();
        
        const int row = index.row();
        const int col = index.column();
        
        // 列边界检查
        if (col < 0 || col >= ColumnCount) return QVariant();
        
        // 行边界检查和数据获取
        if (m_tab->isWHM) {
            // WHM模式检查 - 增强安全性
            if (m_tab->whmEntries.empty()) {
                return QVariant();
            }
            
            if (row < 0 || row >= static_cast<int>(m_tab->whmEntries.size())) {
                return QVariant();
            }
            
            try {
                // 显示和编辑角色
                if (role == Qt::DisplayRole || role == Qt::EditRole) {
                    const auto& entry = m_tab->whmEntries[row];
                    if (col == KeyColumn) {
                        // 缓存格式化结果
                        static thread_local QString hashStr;
                        hashStr = QString("0x%1").arg(entry.hash, 8, 16, QChar('0'));
                        return hashStr;
                    } else {
                        // 安全地转换字符串，防止内存异常
                        try {
                            return QString::fromStdString(entry.text);
                        } catch (const std::exception& e) {
                            qWarning() << "String conversion failed:" << e.what();
                            return QStringLiteral("[转换失败]");
                        } catch (...) {
                            qWarning() << "Unknown exception during string conversion";
                            return QStringLiteral("[未知错误]");
                        }
                    }
                }
            } catch (const std::exception& e) {
                qWarning() << "Data access failed at row" << row << ":" << e.what();
                return QVariant();
            } catch (...) {
                qWarning() << "Unknown exception during data access at row" << row;
                return QVariant();
            }
        } else if (m_tab->isDAT) {
            // DAT模式检查 - 与WHM完全独立处理
            if (m_tab->datEntries.empty()) {
                return QVariant();
            }
            
            if (row < 0 || row >= static_cast<int>(m_tab->datEntries.size())) {
                return QVariant();
            }
            
            try {
                // 显示和编辑角色
                if (role == Qt::DisplayRole || role == Qt::EditRole) {
                    const auto& entry = m_tab->datEntries[row];
                    if (col == KeyColumn) {
                        // 缓存格式化结果
                        static thread_local QString hashStr;
                        hashStr = QString("0x%1").arg(entry.hash, 8, 16, QChar('0'));
                        return hashStr;
                    } else {
                        // 安全地转换字符串，防止内存异常
                        try {
                            return QString::fromStdString(entry.text);
                        } catch (const std::exception& e) {
                            qWarning() << "String conversion failed:" << e.what();
                            return QStringLiteral("[转换失败]");
                        } catch (...) {
                            qWarning() << "Unknown exception during string conversion";
                            return QStringLiteral("[未知错误]");
                        }
                    }
                }
            } catch (const std::exception& e) {
                qWarning() << "DAT data access failed at row" << row << ":" << e.what();
                return QVariant();
            } catch (...) {
                qWarning() << "Unknown exception during DAT data access at row" << row;
                return QVariant();
            }
        } else {
            // GXT模式检查 - 增强安全性和异常处理
            if (m_tab->tables.empty()) {
                return QVariant();
            }
            
            if (m_tab->currentTableIndex < 0 || 
                m_tab->currentTableIndex >= static_cast<int>(m_tab->tables.size())) {
                return QVariant();
            }
            
            try {
                const auto& table = m_tab->tables[m_tab->currentTableIndex];
                if (table.entries.empty() || row < 0 || row >= static_cast<int>(table.entries.size())) {
                    return QVariant();
                }
                
                // 显示和编辑角色
                if (role == Qt::DisplayRole || role == Qt::EditRole) {
                    const auto& entry = table.entries[row];
                    try {
                        if (col == KeyColumn) {
                            // 如果是GTA_SA版本，尝试使用SATKEY替换
                            if (m_tab->version == GXTVersion::GTA_SA && !s_satKeyMap.isEmpty()) {
                                // 将key（hex字符串）转换为uint32
                                bool ok;
                                uint32_t hash = QString::fromStdString(entry.key).toUInt(&ok, 16);
                                if (ok) {
                                    // 查找SATKEY映射
                                    auto it = s_satKeyMap.find(hash);
                                    if (it != s_satKeyMap.end()) {
                                        // 找到匹配，返回SATKEY字符串
                                        return it.value();
                                    }
                                }
                            }
                            // 如果是GTA_IV版本，尝试使用IVTKEY替换
                            else if (m_tab->version == GXTVersion::GTA_IV && !s_ivtKeyMap.isEmpty()) {
                                // 将key（hex字符串）转换为uint32
                                bool ok;
                                uint32_t hash = QString::fromStdString(entry.key).toUInt(&ok, 16);
                                if (ok) {
                                    // 查找IVTKEY映射
                                    auto it = s_ivtKeyMap.find(hash);
                                    if (it != s_ivtKeyMap.end()) {
                                        // 找到匹配，返回IVTKEY字符串
                                        return it.value();
                                    }
                                }
                            }
                            // 优先显示原始键值（如果存在）
                            if (!entry.originalKey.empty()) {
                                return QString::fromStdString(entry.originalKey);
                            }
                            
                            // 如果没有原始键值，检查是否是lst中的键
                            QString keyStr = QString::fromStdString(entry.key);
                            uint32_t hash = keyStr.toUInt(nullptr, 16);
                            
                            if (m_tab->version == GXTVersion::GTA_SA && !s_satKeyMap.isEmpty()) {
                                auto it = s_satKeyMap.find(hash);
                                if (it != s_satKeyMap.end()) {
                                    return it.value();
                                }
                            } else if (m_tab->version == GXTVersion::GTA_IV && !s_ivtKeyMap.isEmpty()) {
                                auto it = s_ivtKeyMap.find(hash);
                                if (it != s_ivtKeyMap.end()) {
                                    return it.value();
                                }
                            }
                            
                            // 最后，如果是8位hex字符，添加0x前缀
                            if (keyStr.length() == 8) {
                                bool allHex = true;
                                for (const QChar& c : keyStr) {
                                    if (!c.isDigit() && (c.toLower() < 'a' || c.toLower() > 'f')) {
                                        allHex = false;
                                        break;
                                    }
                                }
                                if (allHex) {
                                    keyStr = "0x" + keyStr;
                                }
                            }
                            return keyStr;
                        } else {
                            return QString::fromStdString(entry.value);
                        }
                    } catch (const std::exception& e) {
                        qWarning() << "String conversion failed at row" << row << ":" << e.what();
                        return QStringLiteral("[转换失败]");
                    } catch (...) {
                        qWarning() << "Unknown exception during string conversion at row" << row;
                        return QStringLiteral("[未知错误]");
                    }
                }
            } catch (const std::exception& e) {
                qWarning() << "GXT data access failed at row" << row << ":" << e.what();
                return QVariant();
            } catch (...) {
                qWarning() << "Unknown exception during GXT data access at row" << row;
                return QVariant();
            }
        }
        
        // 字体角色 - 键列使用等宽字体，值列使用微软雅黑
        if (role == Qt::FontRole) {
            QFont font;
            if (col == KeyColumn) {
                // 键列：等宽字体
                font.setFamily("Consolas, 'Courier New', monospace");
                font.setStyleHint(QFont::Monospace);
                font.setFixedPitch(true);
            } else {
                // 值列：微软雅黑
                font.setFamily("Microsoft YaHei");
            }
            font.setPointSize(QApplication::font().pointSize());
            return font;
        }
        
        // 工具提示 - 仅在需要时返回，避免递归调用
        if (role == Qt::ToolTipRole) {
            if (role == Qt::DisplayRole || role == Qt::EditRole) {
                // 重新获取显示数据，避免递归
                QString tooltip;
                if (m_tab->isWHM) {
                    if (row < static_cast<int>(m_tab->whmEntries.size())) {
                        const auto& entry = m_tab->whmEntries[row];
                        tooltip = col == KeyColumn ? 
                                  QString("0x%1").arg(entry.hash, 8, 16, QChar('0')) :
                                  QString::fromStdString(entry.text);
                    }
                } else if (m_tab->isDAT) {
                    if (row < static_cast<int>(m_tab->datEntries.size())) {
                        const auto& entry = m_tab->datEntries[row];
                        tooltip = col == KeyColumn ? 
                                  QString("0x%1").arg(entry.hash, 8, 16, QChar('0')) :
                                  QString::fromStdString(entry.text);
                    }
                } else if (m_tab->currentTableIndex >= 0 && 
                          m_tab->currentTableIndex < static_cast<int>(m_tab->tables.size())) {
                    const auto& table = m_tab->tables[m_tab->currentTableIndex];
                    if (row < static_cast<int>(table.entries.size())) {
                        const auto& entry = table.entries[row];
                        if (col == KeyColumn) {
                            // 优先显示原始键值（如果存在）
                            if (!entry.originalKey.empty()) {
                                tooltip = QString::fromStdString(entry.originalKey);
                            } else {
                                // 如果没有原始键值，检查是否是lst中的键
                                QString keyStr = QString::fromStdString(entry.key);
                                uint32_t hash = keyStr.toUInt(nullptr, 16);
                                
                                bool foundInLst = false;
                                if (m_tab->version == GXTVersion::GTA_SA && !s_satKeyMap.isEmpty()) {
                                    auto it = s_satKeyMap.find(hash);
                                    if (it != s_satKeyMap.end()) {
                                        tooltip = it.value();
                                        foundInLst = true;
                                    }
                                } else if (m_tab->version == GXTVersion::GTA_IV && !s_ivtKeyMap.isEmpty()) {
                                    auto it = s_ivtKeyMap.find(hash);
                                    if (it != s_ivtKeyMap.end()) {
                                        tooltip = it.value();
                                        foundInLst = true;
                                    }
                                }
                                
                                // 最后，如果是8位hex字符，添加0x前缀
                                if (!foundInLst) {
                                    if (keyStr.length() == 8) {
                                        bool allHex = true;
                                        for (const QChar& c : keyStr) {
                                            if (!c.isDigit() && (c.toLower() < 'a' || c.toLower() > 'f')) {
                                                allHex = false;
                                                break;
                                            }
                                        }
                                        if (allHex) {
                                            keyStr = "0x" + keyStr;
                                        }
                                    }
                                    tooltip = keyStr;
                                }
                            }
                        } else {
                            tooltip = QString::fromStdString(entry.value);
                        }
                    }
                }
                
                if (tooltip.length() > 100) {
                    return tooltip.left(97) + "...";
                }
                return tooltip;
            }
        }
        
        return QVariant();
    }
    
    // 缓存的表头数据
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();
        static const QStringList whmHeaders = {"Hash", "值"};
        static const QStringList datHeaders = {"Hash", "值"};  // DAT和WHM使用相同的表头
        static const QStringList gxtHeaders = {"键", "值"};
        const QStringList& headers = (m_tab && (m_tab->isWHM || m_tab->isDAT)) ? (m_tab->isDAT ? datHeaders : whmHeaders) : gxtHeaders;
        return (section < ColumnCount) ? headers[section] : QVariant();
    }
    
    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid()) return Qt::NoItemFlags;
        auto flags = QAbstractTableModel::flags(index);
        // 始终允许选择，但在只读模式下不允许编辑
        flags |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        return m_editable ? (flags | Qt::ItemIsEditable) : flags;
    }
    
    // 优化的数据设置 - 减少不必要的模型更新
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override {
        if (!index.isValid() || !m_tab || role != Qt::EditRole || !m_editable) return false;
        
        const int row = index.row();
        const int col = index.column();
        std::string newValue;
        try {
            newValue = value.toString().toStdString();
        } catch (const std::exception& e) {
            qWarning() << "String conversion failed in setData:" << e.what();
            return false;
        }
        
        bool success = false;
        try {
            if (m_tab->isWHM) {
                if (row >= 0 && row < static_cast<int>(m_tab->whmEntries.size()) && 
                    !m_tab->whmEntries.empty() && col == ValueColumn) {
                    auto& entry = m_tab->whmEntries[row];
                    if (entry.text != newValue) {
                        entry.text = newValue;
                        success = true;
                    }
                }
            } else if (m_tab->isDAT) {
                // DAT模式 - 与WHM完全独立
                if (row >= 0 && row < static_cast<int>(m_tab->datEntries.size()) && 
                    !m_tab->datEntries.empty() && col == ValueColumn) {
                    auto& entry = m_tab->datEntries[row];
                    if (entry.text != newValue) {
                        entry.text = newValue;
                        success = true;
                    }
                }
            } else {
                if (m_tab->currentTableIndex >= 0 && m_tab->currentTableIndex < static_cast<int>(m_tab->tables.size())) {
                    auto& table = m_tab->tables[m_tab->currentTableIndex];
                    if (row < static_cast<int>(table.entries.size())) {
                        auto& entry = table.entries[row];
                        if (col == KeyColumn && entry.key != newValue) {
                            // 调试日志：记录键修改
                            QString debugOldKey = QString::fromStdString(entry.key);
                            QString debugNewKey = QString::fromStdString(newValue);
                            if (debugNewKey.contains("FF_WARN", Qt::CaseInsensitive) || debugOldKey.contains("FF_WARN", Qt::CaseInsensitive)) {
                                qInfo() << "[setData] 修改键: row=" << row << ", 旧键=" << debugOldKey << ", 新键=" << debugNewKey;
                            }
                            
                            // 【关键修改】输入字符串后，立即进行hash计算或映射转换
                            // 表面显示明文，但实际存储hash值
                            QString inputKey = QString::fromStdString(newValue);
                            QString processedKey = inputKey;
                            
                            // 根据版本进行不同的处理
                            if (m_tab->version == GXTVersion::GTA_SA) {
                                // GTA SA: 使用CKeyGen计算hash
                                std::string upperKey = inputKey.toUpper().toStdString();
                                uint32_t hash = CKeyGen::GetUppercaseKey(upperKey.c_str());
                                processedKey = QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                                qDebug() << "[Hash转换] SA版本: 输入=" << inputKey << " -> Hash=0x" << processedKey;
                            } else if (m_tab->version == GXTVersion::GTA_IV) {
                                // GTA IV: 使用GTA4 hash算法 - 需要包含GXTEditor头文件
                                // 由于这是在header文件中，我们使用简单的hash计算
                                uint32_t hash = 0;
                                std::string str = inputKey.toLower().toStdString();
                                for (char c : str) {
                                    if (c >= 'A' && c <= 'Z') c += 32; // 转小写
                                    else if (c == '\\') c = '/'; // 反斜杠替换
                                    uint32_t cVal = static_cast<uint32_t>(static_cast<unsigned char>(c)) & 0xFF;
                                    uint32_t tmp = (hash + cVal) & 0xFFFFFFFF;
                                    uint32_t mult = (1025 * tmp) & 0xFFFFFFFF;
                                    hash = ((mult >> 6) ^ mult) & 0xFFFFFFFF;
                                }
                                uint32_t a = (9 * hash) & 0xFFFFFFFF;
                                uint32_t aXor = (a ^ (a >> 11)) & 0xFFFFFFFF;
                                hash = (32769 * aXor) & 0xFFFFFFFF;
                                processedKey = QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                                qDebug() << "[Hash转换] IV版本: 输入=" << inputKey << " -> Hash=0x" << processedKey;
                            } else if (m_tab->version == GXTVersion::GXT2) {
                                // GXT2: 使用JOAAT hash - 简化实现
                                uint32_t hash = 0;
                                std::string str = inputKey.toStdString();
                                for (char c : str) {
                                    hash += static_cast<uint8_t>(c);
                                    hash &= 0xFFFFFFFF;
                                    hash += (hash << 10);
                                    hash &= 0xFFFFFFFF;
                                    hash ^= (hash >> 6);
                                }
                                hash += (hash << 3);
                                hash &= 0xFFFFFFFF;
                                hash ^= (hash >> 11);
                                hash += (hash << 15);
                                hash &= 0xFFFFFFFF;
                                processedKey = QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                                qDebug() << "[Hash转换] GXT2版本: 输入=" << inputKey << " -> Hash=0x" << processedKey;
                            } else {
                                // 其他版本: 尝试解析为hex，如果失败则计算hash
                                bool isHex = false;
                                uint32_t hexValue = inputKey.toUInt(&isHex, 16);
                                if (isHex && inputKey.length() == 8) {
                                    // 已经是合法的8位hex，保持原样
                                    processedKey = inputKey.toUpper();
                                    qDebug() << "[Hash转换] Hex格式: 输入=" << inputKey << " -> 保持=" << processedKey;
                                } else {
                                    // 不是hex，计算JOAAT hash
                                    uint32_t hash = 0;
                                    std::string str = inputKey.toStdString();
                                    for (char c : str) {
                                        hash += static_cast<uint8_t>(c);
                                        hash &= 0xFFFFFFFF;
                                        hash += (hash << 10);
                                        hash &= 0xFFFFFFFF;
                                        hash ^= (hash >> 6);
                                    }
                                    hash += (hash << 3);
                                    hash &= 0xFFFFFFFF;
                                    hash ^= (hash >> 11);
                                    hash += (hash << 15);
                                    hash &= 0xFFFFFFFF;
                                    processedKey = QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                                    qDebug() << "[Hash转换] 默认JOAAT: 输入=" << inputKey << " -> Hash=0x" << processedKey;
                                }
                            }
                            
                            // 保存原始键值用于显示，但实际存储转换后的hash
                            entry.originalKey = newValue;  // 保存原始输入用于显示
                            entry.key = processedKey.toStdString();  // 实际存储hash值
                            success = true;
                        } else if (col == ValueColumn && entry.value != newValue) {
                            entry.value = newValue;
                            success = true;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            qWarning() << "Data modification failed at row" << row << ":" << e.what();
            return false;
        } catch (...) {
            qWarning() << "Unknown exception during data modification at row" << row;
            return false;
        }
        
        if (success) {
            m_tab->isModified = true;
            // 精确的数据更新信号 - 只更新修改的单元格
            emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
            // 发出数据修改信号，用于触发自动保存
            emit dataModified();
        }
        return success;
    }
    
signals:
    void dataModified();
    
private:
    FileTab* m_tab;
    bool m_editable;
    
    // 性能优化缓存
    mutable int m_cachedRowCount;
    int m_currentTableIndex;
    
    // SATKEY映射（静态，全局共享）
    static QMap<uint32_t, QString> s_satKeyMap;
    
    // IVTKEY映射（静态，全局共享）
    static QMap<uint32_t, QString> s_ivtKeyMap;
    
public:
    // 检查映射是否为空（静态方法）
    static bool isSATKeyMapEmpty();
    static bool isIVTKeyMapEmpty();
};

// 极致性能优化的表格委托 - 大幅提升渲染帧率
class OptimizedItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
    
public:
    explicit OptimizedItemDelegate(QObject* parent = nullptr) 
        : QStyledItemDelegate(parent), m_isHashKeyColumn(false) {
        // 预创建字体对象，避免每次绘制都创建
        m_monospaceFont.setFamily("Consolas");
        m_monospaceFont.setStyleHint(QFont::Monospace);
        m_monospaceFont.setFixedPitch(true);
        m_monospaceFont.setHintingPreference(QFont::PreferFullHinting);
        m_monospaceFont.setStyleStrategy(QFont::PreferAntialias);
        m_monospaceFont.setPointSize(QApplication::font().pointSize()); // 立即设置字体大小
        
        m_yaheiFont.setFamily("Microsoft YaHei");
        m_yaheiFont.setHintingPreference(QFont::PreferFullHinting);
        m_yaheiFont.setStyleStrategy(QFont::PreferAntialias);
        m_yaheiFont.setPointSize(QApplication::font().pointSize()); // 立即设置字体大小
        
        // 预计算颜色，避免每次都查询
        m_zebraColor = QColor(240, 248, 255);
        m_highlightColor = QColor(255, 235, 59, 180);
    }
    
    void setSearchText(const QString& text, bool useRegex = false, bool caseSensitive = false) {
        m_searchText = text;
        m_useRegex = useRegex;
        m_caseSensitive = caseSensitive;
        // 更新字体大小
        updateFontSizes();
    }
    
    // 设置是否为哈希键列（使用等宽字体）
    void setHashKeyColumn(bool isHashKey) { 
        m_isHashKeyColumn = isHashKey;
    }
    
    // 极致优化的绘制方法 - 减少条件判断和函数调用
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const QString text = index.data(Qt::DisplayRole).toString();
        if (text.isEmpty()) {
            return;
        }
        
        // 【性能优化1】使用预计算的颜色，避免频繁查询调色板
        const bool isSelected = (option.state & QStyle::State_Selected);
        
        // 【性能优化2】快速绘制背景 - 斑马纹只在奇数行绘制
        if (isSelected) {
            painter->fillRect(option.rect, option.palette.highlight());
        } else if (index.row() & 1) { // 使用位运算代替取模，更快速
            painter->fillRect(option.rect, m_zebraColor);
        }
        
        // 【性能优化3】预计算文本颜色，减少条件判断
        if (isSelected) {
            painter->setPen(option.palette.highlightedText().color());
        } else {
            painter->setPen(option.palette.text().color());
        }
        
        // 【性能优化4】按列设置字体 - 减少不必要的设置
        painter->setFont(index.column() == 0 ? m_monospaceFont : m_yaheiFont);
        
        // 预计算文本区域
        const QRect textRect = option.rect.adjusted(4, 2, -4, -2);
        
        // 【性能优化5】搜索高亮优化 - 快速判断，避免复杂逻辑
        if (!m_searchText.isEmpty()) {
            if (text.contains(m_searchText, m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive)) {
                paintTextWithSimpleHighlight(painter, text, textRect);
                return;
            }
        }
        
        // 【性能优化6】快速文本绘制 - 省略号优化
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, 
                        painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
    }
    
    // 更新字体大小
    void updateFontSizes() {
        const int pointSize = QApplication::font().pointSize();
        m_monospaceFont.setPointSize(pointSize);
        m_yaheiFont.setPointSize(pointSize);
    }
    
    // 简化的编辑器创建
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QLineEdit* editor = new QLineEdit(parent);
        
        // 使用预创建的字体对象
        editor->setFont(index.column() == 0 ? m_monospaceFont : m_yaheiFont);
        
        // 简化的样式
        editor->setStyleSheet(QString(
            "QLineEdit {"
            "border: 1px solid %1;"
            "background: white;"
            "padding: 2px 4px;"
            "border-radius: 3px;"
            "}"
        ).arg(index.column() == 0 ? "#2E86AB" : "#4CAF50"));
        
        editor->setClearButtonEnabled(false);
        editor->setFrame(false);
        editor->installEventFilter(const_cast<OptimizedItemDelegate*>(this));
        
        return editor;
    }
    
    // 简化的编辑器数据设置
    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor)) {
            lineEdit->setText(index.model()->data(index, Qt::EditRole).toString());
            // 值列选中全部文本，键列不选中
            if (index.column() == 1) {
                lineEdit->selectAll();
            }
            lineEdit->setPlaceholderText(index.column() == 0 ? "键" : "值");
        }
    }
    
    // 简化的模型数据设置
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor)) {
            QString text = lineEdit->text().trimmed();
            // 简化的键列验证
            if (index.column() == 0 && text.isEmpty()) {
                text = index.data(Qt::EditRole).toString();
                if (!text.isEmpty()) {
                    lineEdit->setText(text);
                    return;
                }
                text = "UNDEFINED";
            }
            model->setData(index, text, Qt::EditRole);
        }
    }
    
    // 简化的编辑器几何更新
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index)
        QRect rect = option.rect.adjusted(4, 2, -4, -2);
        if (rect.height() < editor->sizeHint().height()) {
            rect.setHeight(editor->sizeHint().height());
            rect.moveTop(option.rect.top() + (option.rect.height() - rect.height()) / 2);
        }
        editor->setGeometry(rect);
        editor->raise();
        editor->show();
        editor->setFocus();
    }
    
    // 简化的键盘事件处理
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::KeyPress) {
            return QStyledItemDelegate::eventFilter(watched, event);
        }
        
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        // QTableView的键盘导航
        QTableView* tableView = qobject_cast<QTableView*>(watched);
        if (!tableView || !tableView->selectionModel()) {
            return QStyledItemDelegate::eventFilter(watched, event);
        }
        
        QModelIndex currentIndex = tableView->selectionModel()->currentIndex();
        if (!currentIndex.isValid()) {
            return QStyledItemDelegate::eventFilter(watched, event);
        }
        
        // Enter/Return键: 移动到下一个单元格
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            int nextRow = currentIndex.row();
            int nextCol = (currentIndex.column() + 1) % 2;
            if (nextCol == 0) nextRow++;
            
            QModelIndex nextIndex = tableView->model()->index(nextRow, nextCol);
            if (nextIndex.isValid()) {
                tableView->setCurrentIndex(nextIndex);
                tableView->edit(nextIndex);
            }
            return true;
        }
        
        // Tab键: 在列间移动
        if (keyEvent->key() == Qt::Key_Tab) {
            int nextCol = (currentIndex.column() + 1) % 2;
            QModelIndex nextIndex = tableView->model()->index(currentIndex.row(), nextCol);
            if (nextIndex.isValid()) {
                tableView->setCurrentIndex(nextIndex);
                tableView->edit(nextIndex);
            }
            return true;
        }
        
        // F2键: 进入编辑模式
        if (keyEvent->key() == Qt::Key_F2) {
            tableView->edit(currentIndex);
            return true;
        }
        
        return QStyledItemDelegate::eventFilter(watched, event);
    }
    
    // 空实现 - 已移除缓存系统
    void clearCache() const {}
    
    // 空实现 - 已移除缓存系统
    int getCacheSize() const { return 0; }
    
    // 优化的搜索高亮绘制 - 减少函数调用和内存分配
    void paintTextWithSimpleHighlight(QPainter* painter, const QString& text, const QRect& rect) const {
        painter->save();
        QFontMetrics fm = painter->fontMetrics();
        const Qt::CaseSensitivity cs = m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        
        int pos = 0;
        int lastPos = 0;
        const int textLength = text.length();
        const int searchTextLength = m_searchText.length();
        const int rectTop = rect.top();
        const int rectBottom = rect.bottom() - fm.descent();
        const int rectLeft = rect.left();
        const int rectHeight = rect.height();
        
        // 预计算累计宽度，避免重复计算
        int currentXOffset = 0;
        
        while ((pos = text.indexOf(m_searchText, pos, cs)) != -1) {
            // 绘制匹配前的文本
            const int beforeLength = pos - lastPos;
            if (beforeLength > 0) {
                const QString before = text.mid(lastPos, beforeLength);
                const int xOffset = rectLeft + currentXOffset;
                painter->drawText(xOffset, rectBottom, before);
                currentXOffset += fm.horizontalAdvance(before);
            }
            
            // 计算并绘制高亮背景
            const int matchWidth = fm.horizontalAdvance(m_searchText);
            const int xOffset = rectLeft + currentXOffset;
            painter->fillRect(xOffset, rectTop, matchWidth, rectHeight, m_highlightColor);
            
            // 绘制匹配文本
            painter->drawText(xOffset, rectBottom, m_searchText);
            currentXOffset += matchWidth;
            
            pos += searchTextLength;
            lastPos = pos;
        }
        
        // 绘制剩余文本
        if (lastPos < textLength) {
            const QString remaining = text.mid(lastPos);
            const int xOffset = rectLeft + currentXOffset;
            painter->drawText(xOffset, rectBottom, remaining);
        }
        
        painter->restore();
    }
    
private:
    QString m_searchText;
    bool m_isHashKeyColumn = false;
    bool m_useRegex = false;
    bool m_caseSensitive = false;
    
    // 预创建的字体对象，避免每次绘制都创建
    QFont m_monospaceFont;
    QFont m_yaheiFont;
    
    // 预计算的颜色，提升性能
    QColor m_zebraColor;
    QColor m_highlightColor;
};