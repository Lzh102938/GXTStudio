#include "GXTTableModel.h"
#include "GXTParser.h"
#include "GXTEditor.h"
#include <QStringConverter>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>
#include <QApplication>

// FileTab结构体需要在某个地方定义，这里我们使用外部定义
// 在GXTStudio.h中定义的FileTab
#include "GXTStudio.h"

GXTTableModel::GXTTableModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_tab(nullptr)
    , m_editable(false)
    , m_cachedRowCount(-1)
    , m_currentTableIndex(-1)
    , m_displayCacheValid(false)
    , m_cachedVersion(-1)
{
}

void GXTTableModel::setTab(FileTab* tab)
{
    m_tab = tab;
    m_cachedRowCount = -1;
    m_displayCacheValid = false;
    m_currentTableIndex = tab ? tab->currentTableIndex : -1;
    m_cachedVersion = tab ? static_cast<int>(tab->version) : -1;
    beginResetModel();
    endResetModel();
}

void GXTTableModel::invalidateDisplayCache()
{
    m_displayCacheValid = false;
    m_displayCache.clear();
}

void GXTTableModel::setEditable(bool editable)
{
    m_editable = editable;
}

namespace {
    // 高效的键映射加载函数 - 使用移动语义和预分配
    int loadKeyMapInternal(const QString& fileName, QHash<uint32_t, QString>& keyMap, QHash<QString, uint32_t>& reverseMap, bool caseInsensitive) {
        QString keylistPath = QDir::current().filePath(fileName);
        
        if (!QFile::exists(keylistPath)) {
            qWarning() << "文件不存在:" << keylistPath;
            return 0;
        }
        
        QFile file(keylistPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "无法打开文件:" << keylistPath;
            return 0;
        }
        
        // 预估大小以减少重新分配
        qint64 fileSize = file.size();
        int estimatedCount = static_cast<int>(fileSize / 30);  // 平均每行约30字节
        keyMap.reserve(estimatedCount);
        reverseMap.reserve(estimatedCount);
        
        QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
#else
        stream.setCodec("UTF-8");
#endif
        
        int loadedCount = 0;
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith("#")) continue;
            
            QStringList parts = line.split('\t');
            if (parts.size() != 2) continue;
            
            QString keyStr = parts[0].trimmed();
            QString hashStr = parts[1].trimmed();
            
            bool ok;
            uint32_t hash = hashStr.toUInt(&ok, 16);
            if (!ok) continue;
            
            keyMap[hash] = keyStr;
            reverseMap[caseInsensitive ? keyStr.toLower() : keyStr] = hash;
            loadedCount++;
        }
        
        file.close();
        return loadedCount;
    }
}

// 延迟加载SATKEY映射 - 线程安全
void GXTTableModel::loadSATKeyMap()
{
    GlobalCache& cache = GlobalCache::instance();
    if (cache.satKeyLoaded) return;
    
    std::lock_guard<std::mutex> lock(cache.satKeyMutex);
    if (cache.satKeyLoaded) return;  // 双重检查
    
    int count = loadKeyMapInternal("keylist/SATKEY.lst", cache.satKeyMap, cache.satKeyReverseMap, false);
    cache.satKeyLoaded = true;
    qDebug() << "加载SATKEY映射" << count << "条";
}

// 延迟加载IVTKEY映射 - 线程安全
void GXTTableModel::loadIVTKeyMap()
{
    GlobalCache& cache = GlobalCache::instance();
    if (cache.ivtKeyLoaded) return;
    
    std::lock_guard<std::mutex> lock(cache.ivtKeyMutex);
    if (cache.ivtKeyLoaded) return;  // 双重检查
    
    int count = loadKeyMapInternal("keylist/IVTKEY.lst", cache.ivtKeyMap, cache.ivtKeyReverseMap, true);
    cache.ivtKeyLoaded = true;
    qDebug() << "加载IVTKEY映射" << count << "条";
}

bool GXTTableModel::findSATKey(uint32_t hash, QString& outKey)
{
    GlobalCache& cache = GlobalCache::instance();
    if (!cache.satKeyLoaded) {
        loadSATKeyMap();
    }
    
    auto it = cache.satKeyMap.find(hash);
    if (it != cache.satKeyMap.end()) {
        outKey = it.value();
        return true;
    }
    return false;
}

bool GXTTableModel::findIVTKey(uint32_t hash, QString& outKey)
{
    GlobalCache& cache = GlobalCache::instance();
    if (!cache.ivtKeyLoaded) {
        loadIVTKeyMap();
    }
    
    auto it = cache.ivtKeyMap.find(hash);
    if (it != cache.ivtKeyMap.end()) {
        outKey = it.value();
        return true;
    }
    return false;
}

bool GXTTableModel::findSATHash(const QString& key, uint32_t& outHash)
{
    GlobalCache& cache = GlobalCache::instance();
    if (!cache.satKeyLoaded) {
        loadSATKeyMap();
    }
    
    auto it = cache.satKeyReverseMap.find(key);
    if (it != cache.satKeyReverseMap.end()) {
        outHash = it.value();
        return true;
    }
    return false;
}

bool GXTTableModel::findIVTHash(const QString& key, uint32_t& outHash)
{
    GlobalCache& cache = GlobalCache::instance();
    if (!cache.ivtKeyLoaded) {
        loadIVTKeyMap();
    }
    
    auto it = cache.ivtKeyReverseMap.find(key.toLower());
    if (it != cache.ivtKeyReverseMap.end()) {
        outHash = it.value();
        return true;
    }
    return false;
}

bool GXTTableModel::isSATKeyMapEmpty()
{
    return GlobalCache::instance().satKeyMap.isEmpty();
}

bool GXTTableModel::isIVTKeyMapEmpty()
{
    return GlobalCache::instance().ivtKeyMap.isEmpty();
}

void GXTTableModel::resetModel()
{
    if (!m_tab) return;

    // 先更新状态，再调用 beginResetModel
    m_currentTableIndex = m_tab->currentTableIndex;
    if (m_tab->isWHM) {
        m_cachedRowCount = static_cast<int>(m_tab->whmEntries.size());
    } else if (m_tab->isDAT) {
        m_cachedRowCount = static_cast<int>(m_tab->datEntries.size());
    } else if (!m_tab->originalHasTABL && m_tab->tables.empty()) {
        // 无表文件，使用noTablEntries
        m_cachedRowCount = static_cast<int>(m_tab->noTablEntries.size());
    } else if (m_currentTableIndex >= 0 && m_currentTableIndex < static_cast<int>(m_tab->tables.size())) {
        m_cachedRowCount = static_cast<int>(m_tab->tables[m_currentTableIndex].entries.size());
    } else {
        m_cachedRowCount = 0;
    }
    m_displayCacheValid = false;
    
    beginResetModel();
    endResetModel();
}

// 【关键修复】清除所有缓存，避免切换表时残留
void GXTTableModel::clearAllCache()
{
    m_displayCache.clear();
    m_displayCacheValid = false;
    m_cachedRowCount = -1;
}

void GXTTableModel::forceDataReset()
{
    // 清除所有缓存
    m_cachedRowCount = -1;
    m_displayCacheValid = false;
    
    // 从tab重新获取currentTableIndex，确保状态同步
    if (m_tab) {
        m_currentTableIndex = m_tab->currentTableIndex;
        m_cachedVersion = static_cast<int>(m_tab->version);
        
        // 根据最新的tab状态重新计算行数
        if (m_tab->isWHM) {
            m_cachedRowCount = static_cast<int>(m_tab->whmEntries.size());
        } else if (m_tab->isDAT) {
            m_cachedRowCount = static_cast<int>(m_tab->datEntries.size());
        } else if (!m_tab->originalHasTABL && m_tab->tables.empty()) {
            // 无表文件，使用noTablEntries
            m_cachedRowCount = static_cast<int>(m_tab->noTablEntries.size());
            qDebug() << "GXTTableModel::forceDataReset - 无表文件, noTablEntries.size=" << m_cachedRowCount;
        } else if (m_currentTableIndex >= 0 && m_currentTableIndex < static_cast<int>(m_tab->tables.size())) {
            m_cachedRowCount = static_cast<int>(m_tab->tables[m_currentTableIndex].entries.size());
        } else {
            m_cachedRowCount = 0;
        }
    } else {
        m_currentTableIndex = -1;
        m_cachedVersion = -1;
    }
    
    beginResetModel();
    m_displayCache.clear();
    m_displayCacheValid = false;
    endResetModel();
}

int GXTTableModel::rowCount(const QModelIndex&) const
{
    if (m_cachedRowCount >= 0) return m_cachedRowCount;
    if (!m_tab) return 0;
    
    try {
        int count = 0;
        if (m_tab->isWHM) {
            count = static_cast<int>(m_tab->whmEntries.size());
        } else if (m_tab->isDAT) {
            count = static_cast<int>(m_tab->datEntries.size());
        } else if (!m_tab->originalHasTABL && m_tab->tables.empty()) {
            // 无表文件，使用noTablEntries
            count = static_cast<int>(m_tab->noTablEntries.size());
            qDebug() << "GXTTableModel::rowCount - 无表文件, noTablEntries.size=" << count;
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

int GXTTableModel::columnCount(const QModelIndex&) const
{
    return ColumnCount;
}

QVariant GXTTableModel::data(const QModelIndex& index, int role) const
{
    // 【性能优化】快速失败检查
    if (!index.isValid() || !m_tab) return QVariant();
    
    const int row = index.row();
    const int col = index.column();
    
    // 列边界检查
    if (col < 0 || col >= ColumnCount) return QVariant();
    
    // 【性能优化】显示和编辑角色 - 使用缓存
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        // 检查缓存是否有效
        if (m_displayCacheValid) {
            // 快速行边界检查
            if (row < 0 || row >= m_displayCache.size()) return QVariant();
            
            const RowCache& cache = m_displayCache[row];
            return (col == KeyColumn) ? cache.key : cache.value;
        } else {
            // 构建完整的显示缓存
            buildDisplayCache();
            
            // 快速行边界检查
            if (row < 0 || row >= m_displayCache.size()) return QVariant();
            
            const RowCache& cache = m_displayCache[row];
            return (col == KeyColumn) ? cache.key : cache.value;
        }
    }
    
    // 【性能优化】字体角色 - 使用静态字体避免重复创建
    if (role == Qt::FontRole) {
        static QFont keyFont = []() {
            QFont f;
            f.setFamily("Consolas, 'Courier New', monospace");
            f.setStyleHint(QFont::Monospace);
            f.setFixedPitch(true);
            f.setPointSize(QApplication::font().pointSize());
            return f;
        }();
        static QFont valueFont = []() {
            QFont f;
            f.setFamily("Microsoft YaHei");
            f.setPointSize(QApplication::font().pointSize());
            return f;
        }();
        return (col == KeyColumn) ? keyFont : valueFont;
    }
    
    // 工具提示 - 使用已构建的缓存
    if (role == Qt::ToolTipRole) {
        if (!m_displayCacheValid) {
            buildDisplayCache();
        }
        
        if (row < 0 || row >= m_displayCache.size()) return QVariant();
        
        const RowCache& cache = m_displayCache[row];
        QString tooltip = (col == KeyColumn) ? cache.key : cache.value;
        
        if (tooltip.length() > 100) {
            return tooltip.left(97) + "...";
        }
        return tooltip;
    }
    
    return QVariant();
}

namespace {
    bool isAllHex(const QString& s) {
        for (int i = 0; i < s.length(); ++i) {
            QChar c = s[i].toLower();
            if (!c.isDigit() && (c < 'a' || c > 'f')) return false;
        }
        return true;
    }
    
    void fillCacheRow(GXTTableModel::RowCache& cache, uint32_t hash, const std::string& text) {
        cache.key = "0x" + QString::number(hash, 16).toUpper().rightJustified(8, '0');
        cache.value = QString::fromStdString(text);
        cache.valid = true;
    }
    
    void fillCacheRow(GXTTableModel::RowCache& cache, const std::string& key, const std::string& originalKey, 
                      const std::string& value, GXTVersion version) {
        GlobalCache& globalCache = GlobalCache::instance();
        if (version == GXTVersion::GTA_III || version == GXTVersion::GTA_VC) {
            cache.key = QString::fromStdString(key);
        } else if (!originalKey.empty()) {
            cache.key = QString::fromStdString(originalKey);
        } else {
            QString keyStr = QString::fromStdString(key);
            
            if (version == GXTVersion::GTA_SA && globalCache.satKeyLoaded) {
                bool ok;
                uint32_t hash = keyStr.toUInt(&ok, 16);
                if (ok) {
                    auto it = globalCache.satKeyMap.find(hash);
                    cache.key = (it != globalCache.satKeyMap.end()) ? it.value() : 
                               (keyStr.length() == 8 && isAllHex(keyStr) ? "0x" + keyStr : keyStr);
                } else {
                    cache.key = keyStr;
                }
            } else if (version == GXTVersion::GTA_IV && globalCache.ivtKeyLoaded) {
                bool ok;
                uint32_t hash = keyStr.toUInt(&ok, 16);
                if (ok) {
                    auto it = globalCache.ivtKeyMap.find(hash);
                    cache.key = (it != globalCache.ivtKeyMap.end()) ? it.value() : 
                               (keyStr.length() == 8 && isAllHex(keyStr) ? "0x" + keyStr : keyStr);
                } else {
                    cache.key = keyStr;
                }
            } else {
                cache.key = keyStr.length() == 8 && isAllHex(keyStr) ? "0x" + keyStr : keyStr;
            }
        }
        cache.value = QString::fromStdString(value);
        cache.valid = true;
    }
}

void GXTTableModel::buildDisplayCache() const
{
    m_displayCache.clear();
    m_displayCacheValid = true;
    
    if (!m_tab) return;
    
    // 延迟初始化键映射 - 按需加载
    if (m_tab->version == GXTVersion::GTA_SA) {
        ensureSATKeyLoaded();
    } else if (m_tab->version == GXTVersion::GTA_IV) {
        ensureIVTKeyLoaded();
    }
    
    int rowCount = 0;
    if (m_tab->isWHM) {
        rowCount = static_cast<int>(m_tab->whmEntries.size());
        m_displayCache.reserve(rowCount);
        for (const auto& entry : m_tab->whmEntries) {
            RowCache cacheItem;
            fillCacheRow(cacheItem, entry.hash, entry.text);
            m_displayCache.append(cacheItem);
        }
    } else if (m_tab->isDAT) {
        rowCount = static_cast<int>(m_tab->datEntries.size());
        m_displayCache.reserve(rowCount);
        for (const auto& entry : m_tab->datEntries) {
            RowCache cacheItem;
            fillCacheRow(cacheItem, entry.hash, entry.text);
            m_displayCache.append(cacheItem);
        }
    } else if (!m_tab->originalHasTABL && m_tab->tables.empty()) {
        // 无表文件，使用noTablEntries
        rowCount = static_cast<int>(m_tab->noTablEntries.size());
        m_displayCache.reserve(rowCount);
        for (const auto& entry : m_tab->noTablEntries) {
            RowCache cacheItem;
            fillCacheRow(cacheItem, entry.key, entry.originalKey, entry.value, m_tab->version);
            m_displayCache.append(cacheItem);
        }
    } else if (m_tab->currentTableIndex >= 0 && m_tab->currentTableIndex < static_cast<int>(m_tab->tables.size())) {
        const auto& table = m_tab->tables[m_tab->currentTableIndex];
        rowCount = static_cast<int>(table.entries.size());
        m_displayCache.reserve(rowCount);
        for (const auto& entry : table.entries) {
            RowCache cacheItem;
            fillCacheRow(cacheItem, entry.key, entry.originalKey, entry.value, m_tab->version);
            m_displayCache.append(cacheItem);
        }
    }
    
    m_cachedRowCount = rowCount;
}

void GXTTableModel::buildPartialDisplayCache(int firstRow, int lastRow) const
{
    m_displayCacheValid = true;
    if (!m_tab) return;
    
    if (m_displayCache.size() < lastRow + 1) {
        m_displayCache.resize(lastRow + 1);
    }
    
    if (m_tab->isWHM) {
        for (int row = firstRow; row <= lastRow && row < static_cast<int>(m_tab->whmEntries.size()); ++row) {
            const auto& entry = m_tab->whmEntries[row];
            RowCache cache;
            fillCacheRow(cache, entry.hash, entry.text);
            m_displayCache[row] = cache;
        }
    } else if (m_tab->isDAT) {
        for (int row = firstRow; row <= lastRow && row < static_cast<int>(m_tab->datEntries.size()); ++row) {
            const auto& entry = m_tab->datEntries[row];
            RowCache cache;
            fillCacheRow(cache, entry.hash, entry.text);
            m_displayCache[row] = cache;
        }
    } else if (!m_tab->originalHasTABL && m_tab->tables.empty()) {
        // 无表文件，使用noTablEntries
        for (int row = firstRow; row <= lastRow && row < static_cast<int>(m_tab->noTablEntries.size()); ++row) {
            const auto& entry = m_tab->noTablEntries[row];
            RowCache cache;
            fillCacheRow(cache, entry.key, entry.originalKey, entry.value, m_tab->version);
            m_displayCache[row] = cache;
        }
    } else if (m_tab->currentTableIndex >= 0 && m_tab->currentTableIndex < static_cast<int>(m_tab->tables.size())) {
        const auto& table = m_tab->tables[m_tab->currentTableIndex];
        for (int row = firstRow; row <= lastRow && row < static_cast<int>(table.entries.size()); ++row) {
            const auto& entry = table.entries[row];
            RowCache cache;
            fillCacheRow(cache, entry.key, entry.originalKey, entry.value, m_tab->version);
            m_displayCache[row] = cache;
        }
    }
}

QVariant GXTTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    
    if (orientation == Qt::Vertical) {
        return QString::number(section + 1);
    }
    
    if (section < 0 || section >= ColumnCount) {
        return QVariant();
    }
    
    static const QStringList whmHeaders = {"Hash", "值"};
    static const QStringList datHeaders = {"Hash", "值"};
    static const QStringList gxtHeaders = {"键", "值"};
    
    if (!m_tab) {
        return gxtHeaders.value(section, QString());
    }
    
    const QStringList& headers = (m_tab->isWHM || m_tab->isDAT) 
        ? (m_tab->isDAT ? datHeaders : whmHeaders) 
        : gxtHeaders;
    
    return headers.value(section, QString());
}

Qt::ItemFlags GXTTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    auto flags = QAbstractTableModel::flags(index);
    // 始终允许选择，但在只读模式下不允许编辑
    flags |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return m_editable ? (flags | Qt::ItemIsEditable) : flags;
}

bool GXTTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || !m_tab || role != Qt::EditRole || !m_editable) return false;
    
    const int row = index.row();
    const int col = index.column();
    QString newValueStr = value.toString();
    std::string newValue;
    try {
        newValue = newValueStr.toStdString();
    } catch (const std::exception& e) {
        qWarning() << "String conversion failed in setData:" << e.what();
        return false;
    }
    
    // GTA III/VC 键名格式验证（编辑键列时）
    if (col == KeyColumn && (m_tab->version == GXTVersion::GTA_III || m_tab->version == GXTVersion::GTA_VC)) {
        QString keyText = newValueStr.trimmed();
        // 检查长度
        if (keyText.length() > 7) {
            qWarning() << "GTA III/VC 键名长度超过7个字符: " << keyText;
            return false;
        }
        // 检查字符范围
        QRegularExpression validKeyRegex("^[0-9A-Z_]+$");
        if (!validKeyRegex.match(keyText).hasMatch()) {
            qWarning() << "GTA III/VC 键名包含非法字符: " << keyText;
            return false;
        }
    }
    
    bool success = false;
    QString oldKey, oldValue;
    QString actualNewKey, actualNewValue;
    
    try {
        if (m_tab->isWHM) {
            if (row >= 0 && row < static_cast<int>(m_tab->whmEntries.size()) && 
                !m_tab->whmEntries.empty() && col == ValueColumn) {
                auto& entry = m_tab->whmEntries[row];
                if (entry.text != newValue) {
                    oldKey = "0x" + QString("%1").arg(entry.hash, 8, 16, QChar('0')).toUpper();
                    oldValue = QString::fromStdString(entry.text);
                    entry.text = newValue;
                    actualNewKey = oldKey;
                    actualNewValue = QString::fromStdString(entry.text);
                    success = true;
                }
            }
        } else if (m_tab->isDAT) {
            if (row >= 0 && row < static_cast<int>(m_tab->datEntries.size()) && 
                !m_tab->datEntries.empty() && col == ValueColumn) {
                auto& entry = m_tab->datEntries[row];
                if (entry.text != newValue) {
                    oldKey = "0x" + QString("%1").arg(entry.hash, 8, 16, QChar('0')).toUpper();
                    oldValue = QString::fromStdString(entry.text);
                    entry.text = newValue;
                    actualNewKey = oldKey;
                    actualNewValue = QString::fromStdString(entry.text);
                    success = true;
                }
            }
        } else if (!m_tab->originalHasTABL && m_tab->tables.empty()) {
            // 无表文件，使用noTablEntries
            if (row >= 0 && row < static_cast<int>(m_tab->noTablEntries.size())) {
                auto& entry = m_tab->noTablEntries[row];
                if (col == KeyColumn && entry.key != newValue) {
                    oldKey = QString::fromStdString(entry.key);
                    oldValue = QString::fromStdString(entry.value);
                    entry.key = newValue;
                    entry.originalKey = newValue;
                    actualNewKey = QString::fromStdString(entry.key);
                    actualNewValue = oldValue;
                    success = true;
                } else if (col == ValueColumn && entry.value != newValue) {
                    oldKey = QString::fromStdString(entry.key);
                    oldValue = QString::fromStdString(entry.value);
                    entry.value = newValue;
                    actualNewKey = oldKey;
                    actualNewValue = QString::fromStdString(entry.value);
                    success = true;
                }
            }
        } else {
            if (m_tab->currentTableIndex >= 0 && m_tab->currentTableIndex < static_cast<int>(m_tab->tables.size())) {
                auto& table = m_tab->tables[m_tab->currentTableIndex];
                if (row < static_cast<int>(table.entries.size())) {
                    auto& entry = table.entries[row];
                    if (col == KeyColumn && entry.key != newValue) {
                        oldKey = QString::fromStdString(entry.key);
                        oldValue = QString::fromStdString(entry.value);
                        
                        QString debugOldKey = oldKey;
                        QString debugNewKey = QString::fromStdString(newValue);
                        if (debugNewKey.contains("FF_WARN", Qt::CaseInsensitive) || debugOldKey.contains("FF_WARN", Qt::CaseInsensitive)) {
                            qInfo() << "[setData] 修改键: row=" << row << ", 旧键=" << debugOldKey << ", 新键=" << debugNewKey;
                        }
                        
                        QString inputKey = QString::fromStdString(newValue);
                        QString processedKey = inputKey;
                        
                        if (m_tab->version == GXTVersion::GTA_III) {
                            processedKey = inputKey;
                            qDebug() << "[Hash转换] III版本: 输入=" << inputKey << " -> 保持原样=" << processedKey;
                        } else if (m_tab->version == GXTVersion::GTA_VC) {
                            processedKey = inputKey;
                            qDebug() << "[Hash转换] VC版本: 输入=" << inputKey << " -> 保持原样=" << processedKey;
                        } else if (m_tab->version == GXTVersion::GTA_SA) {
                            uint32_t hash = 0;
                            if (findSATHash(inputKey, hash)) {
                                processedKey = "0x" + QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                                qDebug() << "[Hash转换] SA版本从映射表: 输入=" << inputKey << " -> Hash=" << processedKey;
                            } else {
                                std::string upperKey = inputKey.toUpper().toStdString();
                                hash = CKeyGen::GetUppercaseKey(upperKey.c_str());
                                processedKey = "0x" + QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                                qDebug() << "[Hash转换] SA版本计算: 输入=" << inputKey << " -> Hash=" << processedKey;
                            }
                        } else if (m_tab->version == GXTVersion::GTA_IV) {
                            uint32_t hash = 0;
                            if (findIVTHash(inputKey, hash)) {
                                processedKey = "0x" + QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                                qDebug() << "[Hash转换] IV版本从映射表: 输入=" << inputKey << " -> Hash=" << processedKey;
                            } else {
                                std::string str = inputKey.toLower().toStdString();
                                hash = GXTEditor::calculateGTA4Hash(str);
                                processedKey = "0x" + QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                                qDebug() << "[Hash转换] IV版本计算: 输入=" << inputKey << " -> Hash=" << processedKey;
                            }
                        } else if (m_tab->version == GXTVersion::GXT2) {
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
                            processedKey = "0x" + QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                            qDebug() << "[Hash转换] GXT2版本: 输入=" << inputKey << " -> Hash=" << processedKey;
                        } else {
                            bool isHex = false;
                            uint32_t hexValue = inputKey.toUInt(&isHex, 16);
                            if (isHex && inputKey.length() == 8) {
                                processedKey = "0x" + inputKey.toUpper();
                                qDebug() << "[Hash转换] Hex格式: 输入=" << inputKey << " -> 保持=" << processedKey;
                            } else {
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
                                processedKey = "0x" + QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                                qDebug() << "[Hash转换] 默认JOAAT: 输入=" << inputKey << " -> Hash=" << processedKey;
                            }
                        }
                        
                        // 保存原始键值用于显示，但实际存储转换后的hash（GTA_III和GTA_VC除外）
                        entry.originalKey = newValue;
                        if (m_tab->version == GXTVersion::GTA_III || m_tab->version == GXTVersion::GTA_VC) {
                            entry.key = processedKey.toStdString();
                        } else {
                            entry.key = processedKey.toStdString();
                        }
                        actualNewKey = QString::fromStdString(entry.key);
                        actualNewValue = oldValue;
                        success = true;
                    } else if (col == ValueColumn && entry.value != newValue) {
                        oldKey = QString::fromStdString(entry.key);
                        oldValue = QString::fromStdString(entry.value);
                        entry.value = newValue;
                        actualNewKey = oldKey;
                        actualNewValue = QString::fromStdString(entry.value);
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
        m_displayCacheValid = false;
        m_displayCache.clear();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        emit dataModified();
        emit cellEdited(row, col, oldKey, oldValue, actualNewKey, actualNewValue);
    }
    return success;
}