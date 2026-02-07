#pragma once

#include "FontAwesome.h"
#include "MultiThreadProgressDialog.h"
#include "GXTTableModel.h"
#include "ParseWorker.h"
#include "SaveWorker.h"
#include "ItemPool.h"

#include <QFont>
#include <QFontDatabase>
#include <QChar>
#include <QStringList>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <functional>

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
    bool isWHMRSC = false;  // 压缩的 HTML 文档格式
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
    QProgressBar* m_saveProgressBar;  // 保存进度条（位于状态栏中间）

    // 自动保存相关
    QToolButton* m_autoSaveButton;  // 自动保存开关按钮
    QTimer* m_autoSaveTimer;  // 自动保存定时器
    bool m_autoSaveEnabled;  // 自动保存是否启用
    QFutureWatcher<SaveResult>* m_autoSaveFutureWatcher;  // 自动保存任务监视器
    static const int AUTOSAVE_DELAY = 3000;  // 自动保存延迟（3秒）
    
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