#pragma once

#include <QAbstractTableModel>
#include <QMap>
#include <QFont>
#include <QApplication>
#include <QDebug>
#include <cstdint>
#include <QHash>

// 前向声明
struct FileTab;
enum class GXTVersion;

/**
 * @brief 极简高效的表格模型 - 专为性能优化
 * 
 * 支持 GXT/GXT2、WHM、DAT 三种文件格式的数据展示和编辑
 * 特点：
 * - 缓存行数计算，减少重复遍历
 * - 缓存显示数据，避免重复格式化
 * - 支持 SATKEY/IVTKEY 映射查找
 * - 支持 GTA SA/IV/GXT2 版本的哈希计算
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
    
    // SATKEY映射（静态，全局共享）
    static QMap<uint32_t, QString> s_satKeyMap;
    static QMap<QString, uint32_t> s_satKeyReverseMap;  // 反向映射：key -> hash
    
    // IVTKEY映射（静态，全局共享）
    static QMap<uint32_t, QString> s_ivtKeyMap;
    static QMap<QString, uint32_t> s_ivtKeyReverseMap;  // 反向映射：key -> hash (小写)
};