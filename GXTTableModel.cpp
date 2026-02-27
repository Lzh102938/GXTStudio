#include "GXTTableModel.h"
#include "GXTParser.h"
#include "GXTEditor.h"
#include <QStringConverter>
#include <QFile>
#include <QTextStream>
#include <QDir>

// 初始化静态成员变量
QMap<uint32_t, QString> GXTTableModel::s_satKeyMap;
QMap<uint32_t, QString> GXTTableModel::s_ivtKeyMap;

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
    beginResetModel();
    m_tab = tab;
    m_cachedRowCount = -1;
    m_displayCacheValid = false;
    // 总是从tab获取最新的currentTableIndex，确保与标签页状态同步
    m_currentTableIndex = tab ? tab->currentTableIndex : -1;
    m_cachedVersion = tab ? static_cast<int>(tab->version) : -1;
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

void GXTTableModel::loadSATKeyMap()
{
    // 如果已经加载过，直接返回
    if (!s_satKeyMap.isEmpty()) {
        return;
    }
    
    // 构建文件路径
    QString keylistPath = QDir::current().filePath("keylist/SATKEY.lst");
    
    // 检查文件是否存在
    if (!QFile::exists(keylistPath)) {
        qWarning() << "SATKEY.lst文件不存在:" << keylistPath;
        return;
    }
    
    // 打开文件
    QFile file(keylistPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开SATKEY.lst文件:" << keylistPath;
        return;
    }
    
    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    
    int loadedCount = 0;
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        
        // 跳过空行和注释
        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }
        
        // 按制表符分割
        QStringList parts = line.split('\t');
        if (parts.size() != 2) {
            continue; // 格式不正确
        }
        
        QString keyStr = parts[0].trimmed();
        QString hashStr = parts[1].trimmed();
        
        // 解析hash（移除0x前缀并转换为uint32）
        bool ok;
        uint32_t hash = hashStr.toUInt(&ok, 16); // 16进制转换
        if (!ok) {
            qWarning() << "解析hash失败:" << hashStr;
            continue;
        }
        
        // 添加到映射
        s_satKeyMap[hash] = keyStr;
        loadedCount++;
    }
    
    file.close();
    
    qDebug() << "成功加载" << loadedCount << "条SATKEY映射";
}

void GXTTableModel::loadIVTKeyMap()
{
    // 如果已经加载过，直接返回
    if (!s_ivtKeyMap.isEmpty()) {
        qInfo() << "IVTKEY映射表已加载，大小: " << s_ivtKeyMap.size();
        return;
    }

    qInfo() << "开始加载IVTKEY映射表...";

    // 构建文件路径
    QString keylistPath = QDir::current().filePath("keylist/IVTKEY.lst");

    // 检查文件是否存在
    if (!QFile::exists(keylistPath)) {
        qWarning() << "IVTKEY.lst文件不存在:" << keylistPath;
        return;
    }

    // 打开文件
    QFile file(keylistPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开IVTKEY.lst文件:" << keylistPath;
        return;
    }

    qInfo() << "已打开IVTKEY.lst文件:" << keylistPath;

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    
    int loadedCount = 0;
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        // 跳过空行和注释
        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        // 按制表符分割
        QStringList parts = line.split('\t');
        if (parts.size() != 2) {
            continue; // 格式不正确
        }

        QString keyStr = parts[0].trimmed();
        QString hashStr = parts[1].trimmed();

        // 解析hash（移除0x前缀并转换为uint32）
        bool ok;
        uint32_t hash = hashStr.toUInt(&ok, 16); // 16进制转换
        if (!ok) {
            qWarning() << "解析hash失败:" << hashStr;
            continue;
        }

        // 添加到映射
        s_ivtKeyMap[hash] = keyStr;
        loadedCount++;

        // 记录前5条加载的数据
        if (loadedCount <= 5) {
            qInfo() << "加载: " << keyStr << " -> 0x" << QString::number(hash, 16);
        }
    }

    file.close();

    qInfo() << "IVTKEY映射表加载完成，共加载 " << loadedCount << " 条记录";
}

bool GXTTableModel::findSATKey(uint32_t hash, QString& outKey)
{
    auto it = s_satKeyMap.find(hash);
    if (it != s_satKeyMap.end()) {
        outKey = it.value();
        return true;
    }
    return false;
}

bool GXTTableModel::findIVTKey(uint32_t hash, QString& outKey)
{
    auto it = s_ivtKeyMap.find(hash);
    if (it != s_ivtKeyMap.end()) {
        outKey = it.value();
        return true;
    }
    return false;
}

bool GXTTableModel::findSATHash(const QString& key, uint32_t& outHash)
{
    // 遍历映射表，查找匹配的key
    for (auto it = s_satKeyMap.begin(); it != s_satKeyMap.end(); ++it) {
        if (it.value() == key) {
            outHash = it.key();
            return true;
        }
    }
    return false;
}

bool GXTTableModel::findIVTHash(const QString& key, uint32_t& outHash)
{
    qInfo() << "查找IVTKEY映射，输入键: " << key << ", 映射表大小: " << s_ivtKeyMap.size();

    // 如果映射表为空，先尝试加载
    if (s_ivtKeyMap.isEmpty()) {
        qWarning() << "IVTKEY映射表为空，尝试加载";
        loadIVTKeyMap();
        qWarning() << "加载后IVTKEY映射表大小: " << s_ivtKeyMap.size();
    }

    // 遍历映射表，查找匹配的key（不区分大小写）
    for (auto it = s_ivtKeyMap.begin(); it != s_ivtKeyMap.end(); ++it) {
        if (it.value().compare(key, Qt::CaseInsensitive) == 0) {
            outHash = it.key();
            qInfo() << "找到匹配: " << key << " -> 0x" << QString::number(outHash, 16);
            return true;
        }
    }
    qWarning() << "未找到匹配: " << key;
    return false;
}

bool GXTTableModel::isSATKeyMapEmpty()
{
    return s_satKeyMap.isEmpty();
}

bool GXTTableModel::isIVTKeyMapEmpty()
{
    return s_ivtKeyMap.isEmpty();
}

void GXTTableModel::resetModel()
{
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
    m_displayCacheValid = false;  // 【性能优化】清除显示缓存
    endResetModel();
}

void GXTTableModel::forceDataReset()
{
    beginResetModel();
    // 清除所有缓存
    m_cachedRowCount = -1;
    m_displayCacheValid = false;  // 【性能优化】清除显示缓存
    
    // 从tab重新获取currentTableIndex，确保状态同步
    if (m_tab) {
        m_currentTableIndex = m_tab->currentTableIndex;
        m_cachedVersion = static_cast<int>(m_tab->version);
        
        // 根据最新的tab状态重新计算行数
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
        m_cachedVersion = -1;
    }
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
        // 检查缓存是否需要重建
        if (!m_displayCacheValid) {
            buildDisplayCache();
        }
        
        // 快速行边界检查
        if (row < 0 || row >= m_displayCache.size()) return QVariant();
        
        const RowCache& cache = m_displayCache[row];
        return (col == KeyColumn) ? cache.key : cache.value;
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

// 【性能优化】格式化哈希键
QString GXTTableModel::formatHashKey(uint32_t hash) const
{
    return "0x" + QString::number(hash, 16).toUpper().rightJustified(8, '0');
}

// 【性能优化】格式化GXT键
QString GXTTableModel::formatKey(const std::string& key, const std::string& originalKey, GXTVersion version) const
{
    // GTA III 和 VC: 直接返回原始键
    if (version == GXTVersion::GTA_III || version == GXTVersion::GTA_VC) {
        return QString::fromStdString(key);
    }
    
    // 优先使用原始键值
    if (!originalKey.empty()) {
        return QString::fromStdString(originalKey);
    }
    
    QString keyStr = QString::fromStdString(key);
    
    // GTA SA: 尝试SATKEY映射
    if (version == GXTVersion::GTA_SA && !s_satKeyMap.isEmpty()) {
        bool ok;
        uint32_t hash = keyStr.toUInt(&ok, 16);
        if (ok) {
            auto it = s_satKeyMap.find(hash);
            if (it != s_satKeyMap.end()) {
                return it.value();
            }
        }
    }
    
    // GTA IV: 尝试IVTKEY映射
    if (version == GXTVersion::GTA_IV && !s_ivtKeyMap.isEmpty()) {
        bool ok;
        uint32_t hash = keyStr.toUInt(&ok, 16);
        if (ok) {
            auto it = s_ivtKeyMap.find(hash);
            if (it != s_ivtKeyMap.end()) {
                return it.value();
            }
        }
    }
    
    // 添加0x前缀（如果是8位hex）
    if (keyStr.length() == 8) {
        bool allHex = true;
        for (int i = 0; i < 8; ++i) {
            QChar c = keyStr[i].toLower();
            if (!c.isDigit() && (c < 'a' || c > 'f')) {
                allHex = false;
                break;
            }
        }
        if (allHex) {
            return "0x" + keyStr;
        }
    }
    
    return keyStr;
}

// 【性能优化】构建显示缓存
void GXTTableModel::buildDisplayCache() const
{
    m_displayCache.clear();
    
    if (!m_tab) {
        m_displayCacheValid = true;
        return;
    }
    
    int rowCount = 0;
    
    // 计算行数并预分配
    if (m_tab->isWHM) {
        rowCount = static_cast<int>(m_tab->whmEntries.size());
    } else if (m_tab->isDAT) {
        rowCount = static_cast<int>(m_tab->datEntries.size());
    } else if (m_tab->currentTableIndex >= 0 && m_tab->currentTableIndex < static_cast<int>(m_tab->tables.size())) {
        rowCount = static_cast<int>(m_tab->tables[m_tab->currentTableIndex].entries.size());
    }
    
    m_displayCache.reserve(rowCount);
    
    // WHM 格式
    if (m_tab->isWHM) {
        for (const auto& entry : m_tab->whmEntries) {
            RowCache cache;
            cache.key = formatHashKey(entry.hash);
            cache.value = QString::fromStdString(entry.text);
            cache.valid = true;
            m_displayCache.append(cache);
        }
    }
    // DAT 格式
    else if (m_tab->isDAT) {
        for (const auto& entry : m_tab->datEntries) {
            RowCache cache;
            cache.key = formatHashKey(entry.hash);
            cache.value = QString::fromStdString(entry.text);
            cache.valid = true;
            m_displayCache.append(cache);
        }
    }
    // GXT 格式
    else if (m_tab->currentTableIndex >= 0 && m_tab->currentTableIndex < static_cast<int>(m_tab->tables.size())) {
        const auto& table = m_tab->tables[m_tab->currentTableIndex];
        for (const auto& entry : table.entries) {
            RowCache cache;
            cache.key = formatKey(entry.key, entry.originalKey, m_tab->version);
            cache.value = QString::fromStdString(entry.value);
            cache.valid = true;
            m_displayCache.append(cache);
        }
    }
    
    m_displayCacheValid = true;
    m_cachedRowCount = rowCount;
}

QVariant GXTTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();
    static const QStringList whmHeaders = {"Hash", "值"};
    static const QStringList datHeaders = {"Hash", "值"};  // DAT和WHM使用相同的表头
    static const QStringList gxtHeaders = {"键", "值"};
    const QStringList& headers = (m_tab && (m_tab->isWHM || m_tab->isDAT)) ? (m_tab->isDAT ? datHeaders : whmHeaders) : gxtHeaders;
    return (section < ColumnCount) ? headers[section] : QVariant();
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
                        
                        // 输入字符串后，立即进行hash计算或映射转换
                        // 表面显示明文，但实际存储hash值
                        QString inputKey = QString::fromStdString(newValue);
                        QString processedKey = inputKey;
                        
                        // 根据版本进行不同的处理
                        if (m_tab->version == GXTVersion::GTA_III) {
                            // GTA III: 直接保存字符串键名，不进行hash计算
                            processedKey = inputKey;
                            qDebug() << "[Hash转换] III版本: 输入=" << inputKey << " -> 保持原样=" << processedKey;
                        } else if (m_tab->version == GXTVersion::GTA_VC) {
                            // GTA VC: 直接保存字符串键名，不进行hash计算
                            processedKey = inputKey;
                            qDebug() << "[Hash转换] VC版本: 输入=" << inputKey << " -> 保持原样=" << processedKey;
                        } else if (m_tab->version == GXTVersion::GTA_SA) {
                            // GTA SA: 使用CKeyGen计算hash
                            std::string upperKey = inputKey.toUpper().toStdString();
                            uint32_t hash = CKeyGen::GetUppercaseKey(upperKey.c_str());
                            processedKey = QString("%1").arg(hash, 8, 16, QChar('0')).toUpper();
                            qDebug() << "[Hash转换] SA版本: 输入=" << inputKey << " -> Hash=0x" << processedKey;
                        } else if (m_tab->version == GXTVersion::GTA_IV) {
                            // GTA IV: 使用GTA4 hash算法
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
                        
                        // 保存原始键值用于显示，但实际存储转换后的hash（GTA_III和GTA_VC除外）
                        entry.originalKey = newValue;  // 保存原始输入用于显示
                        if (m_tab->version == GXTVersion::GTA_III || m_tab->version == GXTVersion::GTA_VC) {
                            // GTA III和VC: 实际存储字符串键名
                            entry.key = processedKey.toStdString();  // 存储原始字符串
                        } else {
                            // 其他版本: 存储hash值
                            entry.key = processedKey.toStdString();  // 实际存储hash值
                        }
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