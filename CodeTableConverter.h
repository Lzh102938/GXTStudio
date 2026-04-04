#pragma once

#include <QString>
#include <QMap>
#include <QObject>
#include <memory>

class GXTStudio;

/**
 * @brief 码表转换状态枚举
 */
enum class CodeTableState {
    NotMounted,      // 未挂载码表
    Original,        // 原始状态
    Converted        // 已转换状态
};

/**
 * @brief 码表转换器类
 * 
 * 负责码表的挂载、正向转换（码表字符 -> Unicode）和反向转换（Unicode -> 码表字符）
 */
class CodeTableConverter : public QObject
{
    Q_OBJECT

public:
    explicit CodeTableConverter(QObject* parent = nullptr);
    ~CodeTableConverter();

    /**
     * @brief 挂载码表文件
     * @param filePath 码表文件路径
     * @return 是否挂载成功
     */
    bool mountTable(const QString& filePath);

    /**
     * @brief 卸载当前挂载的码表
     */
    void unmountTable();

    /**
     * @brief 检查是否已挂载码表
     * @return 是否已挂载
     */
    bool isTableMounted() const { return !m_conversionMap.isEmpty(); }

    /**
     * @brief 获取当前码表状态
     * @return 码表状态
     */
    CodeTableState getState() const { return m_state; }

    /**
     * @brief 设置码表状态
     * @param state 码表状态
     */
    void setState(CodeTableState state) { m_state = state; }

    /**
     * @brief 获取当前挂载的码表文件名
     * @return 码表文件名
     */
    QString getTableName() const { return m_tableName; }

    /**
     * @brief 正向转换：码表字符 -> Unicode字符
     * @param viewer GXTStudio主窗口指针
     * @return 是否转换成功
     */
    bool convertForward(GXTStudio* viewer);

    /**
     * @brief 反向转换：Unicode字符 -> 码表字符
     * @param viewer GXTStudio主窗口指针
     * @return 是否转换成功
     */
    bool convertReverse(GXTStudio* viewer);

    /**
     * @brief 执行转换（根据当前状态自动选择正向或反向）
     * @param viewer GXTStudio主窗口指针
     * @return 是否转换成功
     */
    bool convert(GXTStudio* viewer);

    /**
     * @brief 读取字符映射表
     * @param filePath 码表文件路径
     * @return 字符映射表（中文 -> Unicode字符）
     */
    static QMap<QString, QChar> readCharacterMapping(const QString& filePath);

    /**
     * @brief 将十六进制字符串转换为Unicode字符
     * @param text 十六进制字符串（支持0x前缀或不带前缀）
     * @return Unicode字符
     */
    static QChar hexToUnicode(const QString& text);

private:
    /**
     * @brief 构建反向映射表（性能优化：缓存）
     */
    void buildReverseMap();
    QMap<QString, QChar> m_conversionMap;  // 正向映射表：中文 -> Unicode字符
    QMap<QChar, QString> m_reverseMap;     // 反向映射表：Unicode字符 -> 中文（缓存）
    QString m_tableName;                 // 当前挂载的码表文件名
    CodeTableState m_state;               // 当前码表状态
    bool m_reverseMapValid;              // 反向映射表是否有效
};
