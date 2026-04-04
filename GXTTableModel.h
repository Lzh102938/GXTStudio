#pragma once

#include <QAbstractTableModel>
#include <QMap>
#include <QFont>
#include <QDebug>
#include <cstdint>
#include <QHash>
#include <QCache>
#include <QString>
#include <mutex>
#include <memory>

// 前向声明
struct FileTab;
enum class GXTVersion;

// ============================================================================
// 全局缓存管理器 - 单例模式
// ============================================================================
class GlobalCache {
public:
    static GlobalCache& instance() {
        static GlobalCache inst;
        return inst;
    }
    
    // SATKEY/IVTKEY 映射缓存
    QHash<uint32_t, QString> satKeyMap;
    QHash<QString, uint32_t> satKeyReverseMap;
    QHash<uint32_t, QString> ivtKeyMap;
    QHash<QString, uint32_t> ivtKeyReverseMap;
    
    // 映射是否已加载
    bool satKeyLoaded = false;
    bool ivtKeyLoaded = false;
    
    // 线程安全锁
    std::mutex satKeyMutex;
    std::mutex ivtKeyMutex;
    
    // 清除所有缓存
    void clear() {
        std::lock_guard<std::mutex> lock1(satKeyMutex);
        std::lock_guard<std::mutex> lock2(ivtKeyMutex);
        satKeyMap.clear();
        satKeyReverseMap.clear();
        ivtKeyMap.clear();
        ivtKeyReverseMap.clear();
        satKeyLoaded = false;
        ivtKeyLoaded = false;
    }
    
    // 获取缓存统计
    size_t satKeyCount() const { return satKeyMap.size(); }
    size_t ivtKeyCount() const { return ivtKeyMap.size(); }
    
private:
    GlobalCache() = default;
    GlobalCache(const GlobalCache&) = delete;
    GlobalCache& operator=(const GlobalCache&) = delete;
};

// ============================================================================
// 显示缓存项 - 用于缓存格式化后的显示数据
// ============================================================================
struct DisplayCacheItem {
    QString key;
    QString value;
    bool valid = false;
    
    DisplayCacheItem() = default;
    DisplayCacheItem(const QString& k, const QString& v) : key(k), value(v), valid(true) {}
};

// ============================================================================
// 极简高效的表格模型 - 专为性能优化
// ============================================================================
/**
 * @brief 极简高效的表格模型 - 专为性能优化
 * 
 * 支持 GXT/GXT2、WHM、DAT 三种文件格式的数据展示和编辑
 * 特点：
 * - 缓存行数计算，减少重复遍历
 * - 缓存显示数据，避免重复格式化
 * - 支持 SATKEY/IVTKEY 映射查找
 * - 支持 GTA SA/IV/GXT2 版本的哈希计算
 * - 延迟初始化键映射，按需加载
 */
class GXTTableModel : public QAbstractTableModel {
    Q_OBJECT
    
public:
    enum Column { KeyColumn = 0, ValueColumn = 1, ColumnCount = 2 };
    
    explicit GXTTableModel(QObject* parent = nullptr);
    
    // 设置关联的FileTab数据
    void setTab(FileTab* tab);
    
    // 设置是否可编辑
    void setEditable(bool editable);
    
    // 加载SATKEY映射（静态方法，延迟初始化）
    static void loadSATKeyMap();
    
    // 加载IVTKEY映射（静态方法，延迟初始化）
    static void loadIVTKeyMap();
    
    // 确保SATKEY已加载（延迟初始化入口）
    static void ensureSATKeyLoaded() {
        if (!GlobalCache::instance().satKeyLoaded) {
            loadSATKeyMap();
        }
    }
    
    // 确保IVTKEY已加载（延迟初始化入口）
    static void ensureIVTKeyLoaded() {
        if (!GlobalCache::instance().ivtKeyLoaded) {
            loadIVTKeyMap();
        }
    }
    
    // 查找SATKEY映射（静态方法）
    static bool findSATKey(uint32_t hash, QString& outKey);
    
    // 查找IVTKEY映射（静态方法）
    static bool findIVTKey(uint32_t hash, QString& outKey);
    
    // 反向查找：根据字符串查找hash（静态方法）
    static bool findSATHash(const QString& key, uint32_t& outHash);
    static bool findIVTHash(const QString& key, uint32_t& outHash);
    
    // 极简重置方法 - 强制更新以确保表格切换时无残留
    void resetModel();
    
    // 强制数据重置 - 用于表格切换时确保无残留
    void forceDataReset();
    
    // 清除显示缓存
    void invalidateDisplayCache();
    
    // 【关键修复】清除所有缓存，避免切换表时残留
    void clearAllCache();

    // 检查缓存是否有效（用于避免不必要的刷新）
    bool cacheValid() const { return m_displayCacheValid && !m_displayCache.isEmpty(); }

    // 检查映射是否为空（静态方法）
    static bool isSATKeyMapEmpty();
    static bool isIVTKeyMapEmpty();

    // QAbstractTableModel 接口实现
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    
signals:
    void dataModified();
    void cellEdited(int row, int column, const QString& oldKey, const QString& oldValue, const QString& newKey, const QString& newValue);
    
public:
    // 【性能优化】行缓存结构
    struct RowCache {
        QString key;      // 格式化后的键
        QString value;    // 格式化后的值
        bool valid;       // 缓存是否有效
    };
    
    // 【性能优化】构建显示缓存
    void buildDisplayCache() const;
    
    // 【极致优化】按需构建显示缓存 - 只构建可见区域的数据
    void buildPartialDisplayCache(int firstRow, int lastRow) const;
    
    // 公共接口：获取缓存大小
    int getCacheSize() const { return m_displayCache.size(); }
    
private:
    FileTab* m_tab;
    bool m_editable;
    
    // 性能优化缓存
    mutable int m_cachedRowCount;
    int m_currentTableIndex;
    
    // 【性能优化】显示数据缓存
    mutable QVector<RowCache> m_displayCache;
    mutable bool m_displayCacheValid;
    
    // 缓存的版本信息，避免每次调用都从tab获取
    mutable int m_cachedVersion;
};