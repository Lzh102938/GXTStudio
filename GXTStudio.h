#pragma once

#include "FontAwesome.h"
#include "MultiThreadProgressDialog.h"
#include "GXTTableModel.h"
#include "ParseWorker.h"
#include "SaveWorker.h"
#include "ItemPool.h"
#include "ReplaceWorker.h"
#include "UndoSystem.h"

#include <QFont>
#include <QFontDatabase>
#include <QChar>
#include <QStringList>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <QRegularExpression>
#include <QShortcut>
#include <functional>
#include <QtCore/QCache>

namespace TextColor {
    constexpr const char* DEFAULT_COLOR = "#495057";
    constexpr const char* DARK_TEXT_COLOR = "#495057";
    constexpr const char* LIGHT_TEXT_COLOR = "#ffffff";
}

class TextColorCalculator {
public:
    // 更新背景图片缓存（在背景图片变化时调用）
    void updateBackground(const QPixmap& pixmap, Qt::AspectRatioMode aspectRatioMode, qreal opacity) {
        m_backgroundImage = pixmap.toImage();
        m_aspectRatioMode = aspectRatioMode;
        m_opacity = opacity;
        m_cachedTargetSize = QSize(); // 清除缓存的大小

        // 清除所有颜色缓存
        m_colorCache.clear();
        m_gridCache.clear();
    }

    // 启用/禁用背景
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    QColor calculateColor(const QPoint& pos, const QRect& targetRect) {
        if (!m_enabled || m_backgroundImage.isNull()) {
            return m_defaultColor;
        }

        qint64 cacheKey = (static_cast<qint64>(pos.x()) << 32) | static_cast<qint64>(pos.y());
        if (auto* cachedColor = m_colorCache.object(cacheKey)) {
            return *cachedColor;
        }

        int gridX = pos.x() / GRID_SIZE;
        int gridY = pos.y() / GRID_SIZE;
        qint64 gridKey = (static_cast<qint64>(gridX) << 32) | static_cast<qint64>(gridY);

        QColor* gridColor = m_gridCache.object(gridKey);
        if (gridColor && targetRect.size() == m_cachedTargetSize) {
            QColor* cachedColor = new QColor(*gridColor);
            m_colorCache.insert(cacheKey, cachedColor);
            return *gridColor;
        }

        QColor result = calculateColorInternal(pos, targetRect);

        QColor* cachedColor = new QColor(result);
        m_colorCache.insert(cacheKey, cachedColor);

        QColor* newGridColor = new QColor(result);
        m_gridCache.insert(gridKey, newGridColor);

        return result;
    }

    QColor calculateColorForRegion(const QRect& region, const QRect& targetRect) {
        if (!m_enabled || m_backgroundImage.isNull()) {
            return m_defaultColor;
        }
        return calculateColor(region.center(), targetRect);
    }

    void clearCache() {
        m_colorCache.clear();
        m_gridCache.clear();
    }

    int getColorCacheSize() const { return m_colorCache.size(); }
    int getGridCacheSize() const { return m_gridCache.size(); }

    void setDefaultColor(const QColor& color) { m_defaultColor = color; }
    void setDarkTextColor(const QColor& color) { m_darkTextColor = color; }
    void setLightTextColor(const QColor& color) { m_lightTextColor = color; }

private:
    QColor calculateColorInternal(const QPoint& pos, const QRect& targetRect) {
        if (targetRect.size() != m_cachedTargetSize) {
            m_cachedTargetSize = targetRect.size();
            m_cachedScaledImage = m_backgroundImage.scaled(
                targetRect.size(),
                m_aspectRatioMode,
                Qt::SmoothTransformation
            );
            m_cachedImgX = targetRect.x() + (targetRect.width() - m_cachedScaledImage.width()) / 2;
            m_cachedImgY = targetRect.y() + (targetRect.height() - m_cachedScaledImage.height()) / 2;
        }

        int pixelX = pos.x() - m_cachedImgX;
        int pixelY = pos.y() - m_cachedImgY;

        if (pixelX < 0 || pixelX >= m_cachedScaledImage.width() ||
            pixelY < 0 || pixelY >= m_cachedScaledImage.height()) {
            return m_defaultColor;
        }

        QColor bgColor = m_cachedScaledImage.pixelColor(pixelX, pixelY);

        int luminanceInt = (76 * bgColor.red() + 150 * bgColor.green() + 29 * bgColor.blue()) >> 8;

        int alpha = bgColor.alpha();
        if (alpha == 255) {
            return (luminanceInt > 127) ? m_darkTextColor : m_lightTextColor;
        } else if (alpha == 0) {
            return (242 > 127) ? m_darkTextColor : m_lightTextColor;
        } else {
            int opacityInt = static_cast<int>(m_opacity * 255);
            int mixedLuminance = (luminanceInt * alpha * opacityInt + 242 * (255 - alpha * opacityInt)) >> 16;
            return (mixedLuminance > 127) ? m_darkTextColor : m_lightTextColor;
        }
    }

    static const int GRID_SIZE = 32;

    QImage m_backgroundImage;
    Qt::AspectRatioMode m_aspectRatioMode = Qt::KeepAspectRatioByExpanding;
    qreal m_opacity = 1.0;
    bool m_enabled = false;

    QImage m_cachedScaledImage;
    QSize m_cachedTargetSize;
    int m_cachedImgX = 0;
    int m_cachedImgY = 0;

    mutable QCache<qint64, QColor> m_colorCache;
    mutable QCache<qint64, QColor> m_gridCache;

    QColor m_defaultColor = QColor(TextColor::DEFAULT_COLOR);
    QColor m_darkTextColor = QColor(TextColor::DARK_TEXT_COLOR);
    QColor m_lightTextColor = QColor(TextColor::LIGHT_TEXT_COLOR);
};

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
#include "ReplaceDialog.h"

// 前向声明
class GXTTableModel;
class OptimizedItemDelegate;
class GXTStudio;
class SmartTranslator;
class TranslateConfigDialog;
class TextRenderWidget;
class CharTableWidget;
class BackgroundConfigDialog;

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
    bool isWHMReadOnlyLocked = false;  // WHM文件只读锁定标志，禁止用户解除只读
    bool isModified = false;
    bool originalHasTABL = true;  // 原始文件是否有TABL块
    int currentTableIndex = 0;
    GXTVersion version = GXTVersion::UNKNOWN; // 文件版本信息
    
    // 无表文件的键值对（当originalHasTABL为false时使用）
    std::vector<GXTEntry> noTablEntries;
    
    // 标签页Widget（用于CharTable等特殊标签页）
    QWidget* pageWidget = nullptr;
    
    // Qt控件
    QListWidget* tableList = nullptr;
    QTableView* entryTableView = nullptr;  // 改为QTableView
    GXTTableModel* entryTableModel = nullptr; // 新增模型
    QLineEdit* searchEdit = nullptr;
    QPushButton* searchPrevButton = nullptr; // 上一个按钮
    QPushButton* searchNextButton = nullptr; // 下一个按钮
    QToolButton* filterButton = nullptr;            // 过滤器模式图标按钮
    QToolButton* caseSensitiveButton = nullptr;  // 大小写敏感图标按钮
    QToolButton* regexButton = nullptr;           // 正则表达式图标按钮
    QToolButton* addTableButton = nullptr;        // 添加新表按钮

    // 过滤模式：隐藏不匹配的行
    QSet<int> filteredRows;

    // 添加键值对控件
    QLineEdit* keyEdit = nullptr;       // 键输入框
    QLineEdit* valueEdit = nullptr;     // 值输入框
    QPushButton* addEntryButton = nullptr; // 添加条目按钮
    
    // 文本渲染预览控件
    class TextRenderWidget* textRenderWidget = nullptr;
    
    // 标题标签（用于动态颜色更新）
    QLabel* tableListLabel = nullptr;
    QLabel* entryTableLabel = nullptr;

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
        : QStyledItemDelegate(parent), m_cachedTextColor(33, 37, 41) {
        m_deleteIconRect = QRect(); // 初始化删除图标区域
    }
    
    // 设置缓存的文本颜色（由外部在背景变化时更新）
    void setCachedTextColor(const QColor& color) {
        m_cachedTextColor = color;
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
        
        // 绘制圆角矩形背景 - 未选中状态完全透明
        if (isSelected) {
            QColor bgColor = QColor(187, 222, 251, 220);  // 加深选中背景色：从(227,242,253,200)到(187,222,251,220)
            painter->setPen(Qt::NoPen);
            painter->setBrush(bgColor);
            painter->drawRoundedRect(option.rect.adjusted(4, 2, -4, -2), 6, 6);
        } else if (isHovered) {
            QColor bgColor = QColor(227, 242, 253, 180);  // 加深悬停背景色：从(248,249,250,150)到(227,242,253,180)
            painter->setPen(Qt::NoPen);
            painter->setBrush(bgColor);
            painter->drawRoundedRect(option.rect.adjusted(4, 2, -4, -2), 6, 6);
        }
        
        // 未选中状态不绘制边框，保持完全透明
        
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
        // MAIN表使用更深的蓝色，其他表使用缓存的自动判断颜色
        QColor textColor = isMain ? QColor(13, 71, 161) : m_cachedTextColor;  // 加深蓝色：从(25,118,210)到(13,71,161)
        painter->setPen(textColor);
        
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
    QColor m_cachedTextColor; // 缓存的文本颜色（避免每次绘制重复计算）
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

    // 自定义背景相关公共方法
    void setBackgroundImage(const QString& imagePath);  // 设置背景图片路径
    void setBackgroundOpacity(qreal opacity);           // 设置背景透明度 (0.0 - 1.0)
    void setBackgroundEnabled(bool enabled);            // 启用/禁用背景
    void clearBackground();                             // 清除背景
    bool isBackgroundEnabled() const;                   // 获取背景是否启用
    qreal backgroundOpacity() const;                    // 获取当前透明度
    
    // 提供给后台线程的公共保存方法
    SaveResult performSaveTask(const SaveTask& task);
    
    // 提供给码表转换器的公共方法
    void updateEntryTable();
    
    // 格式化键值显示（添加0x前缀）
    static QString formatKeyForDisplay(const QString& key);
    
    // 提供给后台替换工作者的公共方法

private slots:
    // 文件操作
    void newFile();
    void openFile();
    // 从文本文件导入（自动判断键值对或字符表）
    void importTxtFile(const QString& filePath);
    void saveFile();
    void saveAsFile();
    void exportFile();
    void batchExport();
    bool exportTabToFile(FileTab& tab, const QString& filePath);
    void closeFile();
    void exitApp();
    
    // 编辑操作
    void findText();
    void replaceText();
    void showReplaceDialog();
    
    // 替换对话框信号处理函数
    void handleFindNext(const QString& findText, bool caseSensitive);
    void handleReplace(const QString& findText, const QString& replaceText, bool caseSensitive, ReplaceDialog::ReplaceScope scope);
    void handleReplaceAll(const QString& findText, const QString& replaceText, bool caseSensitive, ReplaceDialog::ReplaceScope scope);
    void replaceInCurrentTable(const QString& findText, const QString& replaceText, bool caseSensitive);
    void replaceInAllTables(const QString& findText, const QString& replaceText, bool caseSensitive);
    void replaceInSelectedArea(const QString& findText, const QString& replaceText, bool caseSensitive);
    void replaceEntry(FileTab* tab, int tableIndex, int entryIndex, const QString& replaceText, int position = -1);
    
    // 后台替换相关方法
    void startBackgroundReplace(const QString& findText, const QString& replaceText, bool caseSensitive, ReplaceDialog::ReplaceScope scope);
    void onReplaceProgressUpdated(int completed, int total, const QString& message);
    void onReplaceFinished(int replacedEntries, const QString& message);
    void onReplaceError(const QString& error);
    void onEntryReplaced(int tableIndex, int entryIndex, int replaceCount);
    void toggleReadOnly(bool readOnly);
    
    // 撤销/重做操作
    void undo();
    void redo();
    void onUndoStackChanged(bool canUndo, bool canRedo);
    
    // 最近文件列表
    void openRecentFile();
    void clearRecentFiles();
    
    // 后台替换功能
    void replaceAllMatchesInEntry(FileTab* tab, int tableIndex, int entryIndex, const QString& findText, const QString& replaceText, bool caseSensitive);

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
    void onToggleLoopSearch(bool checked);
    void onCodeTableConvert(); // 已弃用
    void onVersionChanged(const QModelIndex& selectedIndex);
    void onSmartTranslate();
    void onConfigTranslate();
    void onExecuteTranslate();
    void onGenerateCharTable(); // 生成字符表DAT

    // 背景设置相关
    void onSetBackground();    // 设置背景图片
    void onClearBackground();  // 清除背景图片
    void onBackgroundSettingsChanged(int blurRadius, int brightness, int opacity);  // 背景设置变化

    // 调试相关
    void onDebugConfig();  // 打开调试配置编辑器

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
    void closeTabInternal(int index);  // 内部关闭标签页（不检查保存状态）
    
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

    // 延迟调度表格列表更新（防抖合并多次更新请求）
    void scheduleUpdateTableList(int delayMs = 80);

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
    void paintEvent(QPaintEvent* event) override;  // 自定义背景绘制

private:
    Ui::GXTStudioClass ui;

    // 自定义背景相关
    QPixmap m_originalBackgroundPixmap;  // 原始背景图片（未缩放）
    QPixmap m_backgroundPixmap;  // 当前使用的背景图片（可能已缩放）
    QPixmap m_processedBackgroundPixmap;  // 处理后的背景图片（模糊、亮度）
    QPixmap m_cachedBackgroundPixmap;  // 【关键优化】缓存的缩放背景图片
    QSize m_cachedBackgroundSize;  // 【关键优化】缓存时的尺寸
    qreal m_backgroundOpacity;   // 背景透明度 (0.0 - 1.0)
    bool m_backgroundEnabled;    // 是否启用背景
    Qt::AspectRatioMode m_backgroundAspectRatioMode;  // 图片缩放模式
    int m_backgroundBlurRadius;  // 高斯模糊半径 (0-50)
    int m_backgroundBrightness;  // 亮度调节 (-100 到 100)
    QString m_backgroundImagePath;  // 背景图片路径（用于重新加载）
    QTimer m_resizeTimer;  // 窗口缩放防抖定时器
    bool m_isResizing;  // 是否正在缩放中
    TextColorCalculator m_textColorCalculator;  // 优化的文本颜色计算器
    void drawBackground(QPainter* painter);  // 绘制背景方法
    QColor getTextColorForPosition(const QPoint& pos);  // 根据背景位置获取文字颜色（包装方法）
    void updateLabelColors();
    void updateSearchUIColors(FileTab& tab);
    void resetSearchUIColors(FileTab& tab);
    void applyBackgroundEffects();
    void onResizeTimerTimeout();  // 窗口缩放防抖处理
    void saveBackgroundSettings();  // 保存背景设置到配置
    void loadBackgroundSettings();  // 从配置加载背景设置
    
    // 最近文件列表相关
    void addToRecentFiles(const QString& filePath);
    void updateRecentFilesMenu();
    void saveRecentFiles();
    void loadRecentFiles();
    void openRecentFileInternal(const QString& filePath);

    // 主要控件
    QTabWidget* m_tabWidget;
    QLabel* m_statusLabel;
    
    // 菜单和工具栏
    QMenuBar* m_menuBar;
    QToolBar* m_toolBar;
    QStatusBar* m_statusBar;
    
    // 动作
    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_exportAction;
    QAction* m_batchExportAction;
    QAction* m_closeAction;
    QAction* m_exitAction;
    QAction* m_findAction;
    QAction* m_replaceAction;
    QAction* m_readOnlyAction;
    QAction* m_loopSearchAction;  // 循环搜索动作
    ReplaceDialog* m_replaceDialog;
    QAction* m_aboutAction;
    QAction* m_codeTableAction;    // 码表转换（已弃用，改为下拉按钮）
    QAction* m_smartTranslateAction; // 智能翻译
    QAction* m_generateCharTableAction; // 生成字符表DAT
    QAction* m_configTranslateAction; // 配置翻译
    QAction* m_executeTranslateAction; // 执行翻译
    QAction* m_setBackgroundAction;    // 设置背景图片
    QAction* m_clearBackgroundAction;  // 清除背景图片
    QAction* m_debugConfigAction;      // 调试配置编辑器
    
    // 撤销/重做相关
    QAction* m_undoAction;
    QAction* m_redoAction;
    class UndoSystem* m_undoSystem;
    
    // 最近文件列表相关
    static const int MAX_RECENT_FILES = 10;
    QStringList m_recentFiles;
    QMenu* m_recentFilesMenu;
    QList<QAction*> m_recentFileActions;
    QAction* m_clearRecentFilesAction;
      
    // 码表转换相关
    QToolButton* m_codeTableButton; // 码表转换下拉按钮
    QAction* m_mountCodeTableAction; // 挂载码表
    QAction* m_convertCodeTableAction; // 执行转换
    QAction* m_unmountCodeTableAction; // 卸载码表
    
    // 窗口大小调整优化
    QSize m_lastWindowSize;
    
    // 布局优化相关
    QTimer* m_cleanupTimer;
    
    std::vector<FileTab> m_fileTabs;
    int m_currentTabIndex;
    bool m_isReadOnly;
    int m_fontSize;

    // 异步解析相关
    QThread* m_parseThread;
    ParseWorker* m_parseWorker;
    // 批量并行解析相关
    QFutureWatcher<ParseResult>* m_batchParseWatcher;
    QVector<ParseTask> m_pendingParseTasks;
    int m_batchParseTotal;
    int m_batchParseCompleted;
    MultiThreadProgressDialog* m_batchParseProgressDialog;
    // 表格列表更新防抖定时器
    QTimer* m_tableListUpdateTimer;
    
    // 异步保存相关
    QThread* m_saveThread;
    SaveWorker* m_saveWorker;
    QProgressBar* m_saveProgressBar;  // 保存进度条（位于状态栏中间）
    
    // 字符表图片导出相关
    QThread* m_charTableExportThread;
    class CharTableExportWorker* m_charTableExportWorker;
    
    // 异步替换相关
    ReplaceWorker* m_replaceWorker;
    QThread* m_replaceThread;
    MultiThreadProgressDialog* m_replaceProgressDialog;
    // 替换后恢复选择与搜索
    int m_pendingReplaceTable;
    int m_pendingReplaceRow;
    QString m_lastReplaceFindText;
    ReplaceDialog::ReplaceScope m_lastReplaceScope;
    
    // 保存后关闭标签页
    int m_pendingCloseTabIndex;  // 保存完成后需要关闭的标签页索引，-1表示无

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
        int currentMatchPosition; // 当前匹配在该条目中的位置
        std::vector<std::tuple<int, int, int>> allMatches; // (tableIndex, entryIndex, position) 三元组
        bool isValid;
        bool caseSensitive;  // 大小写敏感
        bool useRegex;       // 使用正则表达式
        bool loopSearch;     // 是否循环搜索
        bool filterMode;     // 过滤模式 - 只显示匹配的行

        SearchState() : currentTableIndex(-1), currentEntryIndex(-1), currentMatchPosition(-1),
                        isValid(false), caseSensitive(false), useRegex(false), loopSearch(false), filterMode(false) {}
        void clear() {
            searchText.clear();
            currentTableIndex = -1;
            currentEntryIndex = -1;
            currentMatchPosition = -1;
            allMatches.clear();
            isValid = false;
            caseSensitive = false;
            useRegex = false;
            loopSearch = false;
            filterMode = false;
        }
    };
    SearchState m_searchState;

    // 【性能优化】搜索结果缓存结构
    struct SearchResultCache {
        QString searchText;
        bool caseSensitive;
        bool useRegex;
        std::vector<std::tuple<int, int, int>> matches;
        QString cacheKey;
        int tabIndex;

        SearchResultCache() : caseSensitive(false), useRegex(false), tabIndex(-1) {}

        QString generateKey() const {
            return QString::number(tabIndex) + "|" + searchText + "|" + QString::number(caseSensitive) + "|" + QString::number(useRegex);
        }

        bool isValid(const QString& search, bool cs, bool regex, int tabIdx) const {
            return !search.isEmpty() && searchText == search &&
                   caseSensitive == cs && useRegex == regex && tabIndex == tabIdx;
        }
    };

    // 【性能优化】搜索缓存（最近20次搜索结果）
    SearchResultCache* m_searchResultCache;
    int m_cacheIndex;
    static const int MAX_SEARCH_CACHE = 20;

    // 【性能优化】样式字符串缓存系统
    struct CachedStyles {
        QString tabWidgetStyle;
        QString toolBarStyle;
        QString toolButtonStyle;
        QString splitterStyle;
        QString tableListStyle;
        QString tableViewStyle;
        QString searchEditStyleTemplate;
        QString navButtonStyleTemplate;
        QString labelStyleTemplate;
        QString addEntryButtonStyle;
        QString iconButtonStyleTemplate;
        bool initialized = false;
    } m_cachedStyles;
    
    void initializeCachedStyles();
    QString getSearchEditStyle(const QColor& textColor);
    QString getNavButtonStyle(const QColor& textColor);
    QString getLabelStyle(const QColor& textColor);
    QString getIconButtonStyle(const QColor& textColor);

    // 控制搜索时是否跳转（用于替换操作重建搜索结果时保持焦点）
    bool m_suppressSearchJump;
    
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
    void clearSearchCache(); // 清除搜索缓存
    QString getVersionString(GXTVersion version) const;
    QIcon getVersionIcon(GXTVersion version) const;
    QIcon getFileTypeIcon(const FileTab& tab) const;
    std::string getVersionName(GXTVersion version);
    GXTVersion selectVersionDialog(); // 版本选择对话框
    int calculateKeyColumnWidth(); // 计算键列宽度
    
    // 新建文件相关
    struct NewFileOptions {
        QString fileType;       // "GXT", "DAT_VC", "DAT_IV", "GXT2"
        GXTVersion version;     // 对应的GXT版本
        QString defaultName;    // 默认文件名
    };
    NewFileOptions showNewFileDialog(); // 显示新建文件对话框
    
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
    void preloadVisibleArea(FileTab* tab);
    
    // 延迟初始化
    void initializeDeferredComponents();
    
    // 搜索相关辅助方法
    void performSearch(const QString& searchText);
    void jumpToMatch(int matchIndex);
    void highlightMatch(int entryIndex);
    void clearSearchHighlight();
    void applyFilterMode();
    void clearFilterMode();
    
    // 异步解析方法
    void setupParseThread();
    void startAsyncParse(const QString& filePath);
    void startBatchAsyncParse(const QStringList& filePaths);
    void onParseCompleted(const ParseResult& result);
    void onBatchParseResultReadyAt(int index);
    void onBatchParseFinished();
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
        m_zebraColor = QColor(227, 242, 253, 200);  // 加深斑马纹蓝色：从(240,248,255,180)到(227,242,253,200)
        m_highlightColor = QColor(255, 235, 59, 180);
        
        // 初始化缓存系统
        m_textWidthCache.setMaxCost(500);  // 缓存500个文本宽度
        m_highlightCache.setMaxCost(100);   // 缓存100个高亮结果
    }
    
    void setSearchText(const QString& text, bool useRegex = false, bool caseSensitive = false) {
        if (text != m_searchText || m_caseSensitive != caseSensitive || m_useRegex != useRegex) {
            m_highlightCache.clear();
            m_regexValid = false;
        }
        m_searchText = text;
        m_useRegex = useRegex;
        m_caseSensitive = caseSensitive;
        
        // 【关键优化】预编译正则表达式
        if (m_useRegex && !m_searchText.isEmpty()) {
            QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
            if (!m_caseSensitive) {
                options |= QRegularExpression::CaseInsensitiveOption;
            }
            m_cachedRegex.setPattern(m_searchText);
            m_cachedRegex.setPatternOptions(options);
            m_regexValid = m_cachedRegex.isValid();
        } else {
            m_regexValid = false;
        }
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

        const bool isSelected = (option.state & QStyle::State_Selected);

        if (isSelected) {
            painter->fillRect(option.rect, option.palette.highlight());
        } else if (index.row() & 1) {
            painter->fillRect(option.rect, m_zebraColor);
        }

        painter->setPen(isSelected ? option.palette.highlightedText().color() : option.palette.text().color());
        painter->setFont(index.column() == 0 ? m_monospaceFont : m_yaheiFont);

        const QRect textRect = option.rect.adjusted(4, 2, -4, -2);

        if (!m_searchText.isEmpty()) {
            bool shouldHighlight = false;
            if (m_useRegex && m_regexValid) {
                shouldHighlight = m_cachedRegex.match(text).hasMatch();
            } else {
                Qt::CaseSensitivity cs = m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
                shouldHighlight = text.contains(m_searchText, cs);
            }
            
            if (shouldHighlight) {
                paintTextWithSimpleHighlight(painter, text, textRect);
                return;
            }
        }

        // 【关键优化】直接绘制文本，不使用 elidedText（非常慢）
        // 使用 QFontMetrics::elidedText 每次都要计算文本宽度，非常耗时
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
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
    
    // 清理缓存 - 当需要强制刷新时调用
    void clearCache() const {
        m_textWidthCache.clear();
        m_highlightCache.clear();
    }
    
    // 获取缓存大小 - 用于调试和监控
    int getCacheSize() const {
        return m_textWidthCache.size() + m_highlightCache.size();
    }
    
    // 优化的搜索高亮绘制 - 使用缓存减少重复计算（支持正则表达式）
    void paintTextWithSimpleHighlight(QPainter* painter, const QString& text, const QRect& rect) const {
        // 检查缓存（包含正则表达式模式标记）
        QString cacheKey = text + "|" + m_searchText + "|" + 
                          QString::number(m_caseSensitive ? 1 : 0) + "|" +
                          QString::number(m_useRegex ? 1 : 0);
        HighlightCacheEntry* cachedEntry = m_highlightCache.object(cacheKey);
        
        if (cachedEntry && cachedEntry->textHash == qHash(text)) {
            // 使用缓存的高亮片段
            painter->save();
            const int rectTop = rect.top();
            const int rectBottom = rect.bottom() - painter->fontMetrics().descent();
            const int rectLeft = rect.left();
            const int rectHeight = rect.height();
            
            // 使用缓存的片段信息进行绘制
            int currentXOffset = 0;
            for (const auto& fragment : cachedEntry->fragments) {
                const int xOffset = rectLeft + currentXOffset;
                
                if (fragment.isMatch) {
                    // 绘制高亮背景和匹配文本
                    painter->fillRect(xOffset, rectTop, fragment.width, rectHeight, m_highlightColor);
                    painter->drawText(xOffset, rectBottom, fragment.text);
                } else {
                    // 绘制普通文本
                    painter->drawText(xOffset, rectBottom, fragment.text);
                }
                currentXOffset += fragment.width;
            }
            painter->restore();
            return;
        }
        
        // 缓存未命中，执行完整的计算和绘制
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
        
        // 创建缓存条目
        HighlightCacheEntry* cacheEntry = new HighlightCacheEntry();
        cacheEntry->textHash = qHash(text);
        cacheEntry->fragments.reserve(10); // 预分配，减少重新分配
        
        if (m_useRegex && m_regexValid) {
            // 【关键优化】使用预编译的正则表达式
            QRegularExpressionMatchIterator matchIterator = m_cachedRegex.globalMatch(text);
            
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                const int matchStart = match.capturedStart();
                const int matchEnd = match.capturedEnd();
                const int matchLen = matchEnd - matchStart;
                
                // 绘制匹配前的文本
                if (matchStart > lastPos) {
                    const QString before = text.mid(lastPos, matchStart - lastPos);
                    const int beforeWidth = fm.horizontalAdvance(before);
                    const int xOffset = rectLeft + currentXOffset;
                    painter->drawText(xOffset, rectBottom, before);
                    currentXOffset += beforeWidth;
                    cacheEntry->fragments.append({before, beforeWidth, false});
                }
                
                // 计算并绘制高亮背景
                const QString matched = text.mid(matchStart, matchLen);
                const int matchWidth = fm.horizontalAdvance(matched);
                const int xOffset = rectLeft + currentXOffset;
                painter->fillRect(xOffset, rectTop, matchWidth, rectHeight, m_highlightColor);
                
                // 绘制匹配文本
                painter->drawText(xOffset, rectBottom, matched);
                currentXOffset += matchWidth;
                
                // 添加到缓存片段
                cacheEntry->fragments.append({matched, matchWidth, true});
                
                lastPos = matchEnd;
            }
            
            // 绘制剩余文本
            if (lastPos < textLength) {
                const QString remaining = text.mid(lastPos);
                const int remainingWidth = fm.horizontalAdvance(remaining);
                const int xOffset = rectLeft + currentXOffset;
                painter->drawText(xOffset, rectBottom, remaining);
                currentXOffset += remainingWidth;
                cacheEntry->fragments.append({remaining, remainingWidth, false});
            }
        } else {
            // 普通字符串模式（原逻辑）
            while ((pos = text.indexOf(m_searchText, pos, cs)) != -1) {
                // 绘制匹配前的文本
                const int beforeLength = pos - lastPos;
                if (beforeLength > 0) {
                    const QString before = text.mid(lastPos, beforeLength);
                    const int beforeWidth = fm.horizontalAdvance(before);
                    const int xOffset = rectLeft + currentXOffset;
                    painter->drawText(xOffset, rectBottom, before);
                    currentXOffset += beforeWidth;
                    
                    // 添加到缓存片段
                    cacheEntry->fragments.append({before, beforeWidth, false});
                }
                
                // 计算并绘制高亮背景
                const QString matched = text.mid(pos, searchTextLength);  // 从原始文本中提取匹配部分
                const int matchWidth = fm.horizontalAdvance(matched);
                const int xOffset = rectLeft + currentXOffset;
                painter->fillRect(xOffset, rectTop, matchWidth, rectHeight, m_highlightColor);

                // 绘制匹配文本（使用原始文本中的大小写样式）
                painter->drawText(xOffset, rectBottom, matched);
                currentXOffset += matchWidth;

                // 添加到缓存片段
                cacheEntry->fragments.append({matched, matchWidth, true});
                
                pos += searchTextLength;
                lastPos = pos;
            }
            
            // 绘制剩余文本
            if (lastPos < textLength) {
                const QString remaining = text.mid(lastPos);
                const int remainingWidth = fm.horizontalAdvance(remaining);
                const int xOffset = rectLeft + currentXOffset;
                painter->drawText(xOffset, rectBottom, remaining);
                currentXOffset += remainingWidth;
                
                // 添加到缓存片段
                cacheEntry->fragments.append({remaining, remainingWidth, false});
            }
        }
        
        // 存储到缓存
        m_highlightCache.insert(cacheKey, cacheEntry);
        
        painter->restore();
    }
    
private:
    // 高亮片段结构体
    struct HighlightFragment {
        QString text;    // 文本内容
        int width;       // 文本宽度
        bool isMatch;    // 是否为匹配的搜索文本
    };
    
    // 高亮缓存条目结构体
    struct HighlightCacheEntry {
        uint textHash;                        // 文本哈希值，用于验证缓存有效性
        QList<HighlightFragment> fragments;    // 高亮片段列表
    };
    
    QString m_searchText;
    bool m_isHashKeyColumn = false;
    bool m_useRegex = false;
    bool m_caseSensitive = false;
    
    // 【关键优化】缓存的正则表达式，避免每次绘制都编译
    mutable QRegularExpression m_cachedRegex;
    mutable bool m_regexValid = false;
    
    // 预创建的字体对象，避免每次绘制都创建
    QFont m_monospaceFont;
    QFont m_yaheiFont;
    
    // 预计算的颜色，提升性能
    QColor m_zebraColor;
    QColor m_highlightColor;
    
    // 【新增性能优化】文本宽度缓存 - 避免重复计算
    mutable QCache<QString, int> m_textWidthCache;  // 缓存文本字符串的宽度
    
    // 【新增性能优化】搜索高亮缓存 - 避免重复计算高亮片段
    mutable QCache<QString, HighlightCacheEntry> m_highlightCache;  // 缓存高亮结果
};