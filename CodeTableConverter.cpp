#include "CodeTableConverter.h"
#include "GXTStudio.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

CodeTableConverter::CodeTableConverter(QObject* parent)
    : QObject(parent)
    , m_state(CodeTableState::NotMounted)
    , m_reverseMapValid(false)
{
}

CodeTableConverter::~CodeTableConverter()
{
    unmountTable();
}

bool CodeTableConverter::mountTable(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开码表文件:" << filePath;
        return false;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);  // 强制使用UTF-8编码

    m_conversionMap.clear();
    m_tableName = QFileInfo(filePath).fileName();

    int lineNumber = 0;
    while (!in.atEnd()) {
        lineNumber++;
        QString line = in.readLine().trimmed();

        // 跳过空行和注释
        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        // 解析格式：中文字符 + 制表符/等号 + 十六进制Unicode
        // 支持制表符或等号作为分隔符
        QStringList parts;
        if (line.contains('\t')) {
            parts = line.split('\t', Qt::SkipEmptyParts);
        } else if (line.contains('=')) {
            parts = line.split('=', Qt::SkipEmptyParts);
        } else {
            qWarning() << "码表文件格式错误，第" << lineNumber << "行: 缺少分隔符（制表符或等号）:" << line;
            continue;
        }

        if (parts.size() != 2) {
            qWarning() << "码表文件格式错误，第" << lineNumber << "行:" << line;
            continue;
        }

        QString chinese = parts[0].trimmed();
        QString hexStr = parts[1].trimmed();
        QChar unicodeChar = hexToUnicode(hexStr);

        if (unicodeChar.isNull()) {
            qWarning() << "无效的Unicode编码，第" << lineNumber << "行:" << hexStr;
            continue;
        }

        m_conversionMap[chinese] = unicodeChar;
    }

    file.close();

    if (m_conversionMap.isEmpty()) {
        qWarning() << "码表文件为空或格式错误";
        unmountTable();
        return false;
    }

    m_state = CodeTableState::Original;
    m_reverseMapValid = false;

    qDebug() << "成功挂载码表:" << m_tableName << "共" << m_conversionMap.size() << "个字符";
    return true;
}

void CodeTableConverter::unmountTable()
{
    m_conversionMap.clear();
    m_reverseMap.clear();
    m_tableName.clear();
    m_state = CodeTableState::NotMounted;
    m_reverseMapValid = false;

    qDebug() << "已卸载码表";
}

bool CodeTableConverter::convertForward(GXTStudio* viewer)
{
    if (!isTableMounted()) {
        qWarning() << "未挂载码表，无法执行转换";
        return false;
    }

    if (!viewer) {
        qWarning() << "无效的viewer指针";
        return false;
    }

    FileTab* currentTab = viewer->getCurrentTab();
    if (!currentTab) {
        qWarning() << "未打开任何文件";
        return false;
    }

    // 检查是否为WHM文件
    if (currentTab->isWHM) {
        qWarning() << "WHM文件不支持码表转换";
        return false;
    }

    // 遍历所有表进行转换
    int totalConvertedCount = 0;

    for (size_t tableIndex = 0; tableIndex < currentTab->tables.size(); ++tableIndex) {
        GXTTabl& table = currentTab->tables[tableIndex];
        int tableConvertedCount = 0;

        for (size_t i = 0; i < table.entries.size(); ++i) {
            GXTEntry& entry = table.entries[i];

            QString originalText = QString::fromStdString(entry.value);
            QString convertedText;
            bool converted = false;

            for (int j = 0; j < originalText.length(); ) {
                QString currentChar = originalText[j];

                // 检查是否在映射表中
                if (m_conversionMap.contains(currentChar)) {
                    convertedText.append(m_conversionMap[currentChar]);
                    converted = true;
                    j += currentChar.length();
                } else {
                    convertedText.append(originalText[j]);
                    j++;
                }
            }

            if (converted) {
                entry.value = convertedText.toStdString();
                tableConvertedCount++;
            }
        }

        totalConvertedCount += tableConvertedCount;
        qDebug() << "表" << QString::fromStdString(table.name)
                  << "转换了" << tableConvertedCount << "个条目";
    }

    // 更新表格视图以反映更改
    if (currentTab->entryTableView && currentTab->entryTableView->model()) {
        currentTab->entryTableView->viewport()->update();
    }

    m_state = CodeTableState::Converted;
    qDebug() << "正向转换完成，共转换" << totalConvertedCount << "个单元格";

    return true;
}

bool CodeTableConverter::convertReverse(GXTStudio* viewer)
{
    if (!isTableMounted()) {
        qWarning() << "未挂载码表，无法执行反向转换";
        return false;
    }

    if (!viewer) {
        qWarning() << "无效的viewer指针";
        return false;
    }

    // 确保反向映射表已构建
    if (!m_reverseMapValid) {
        buildReverseMap();
    }

    FileTab* currentTab = viewer->getCurrentTab();
    if (!currentTab) {
        qWarning() << "未打开任何文件";
        return false;
    }

    // 检查是否为WHM文件
    if (currentTab->isWHM) {
        qWarning() << "WHM文件不支持码表转换";
        return false;
    }

    // 遍历所有表进行转换
    int totalConvertedCount = 0;

    for (size_t tableIndex = 0; tableIndex < currentTab->tables.size(); ++tableIndex) {
        GXTTabl& table = currentTab->tables[tableIndex];
        int tableConvertedCount = 0;

        for (size_t i = 0; i < table.entries.size(); ++i) {
            GXTEntry& entry = table.entries[i];

            QString convertedText;
            bool converted = false;

            QString originalText = QString::fromStdString(entry.value);

            for (int j = 0; j < originalText.length(); ++j) {
                QChar currentChar = originalText[j];

                // 检查是否在反向映射表中
                if (m_reverseMap.contains(currentChar)) {
                    convertedText.append(m_reverseMap[currentChar]);
                    converted = true;
                } else {
                    convertedText.append(currentChar);
                }
            }

            if (converted) {
                entry.value = convertedText.toStdString();
                tableConvertedCount++;
            }
        }

        totalConvertedCount += tableConvertedCount;
        qDebug() << "表" << QString::fromStdString(table.name)
                  << "转换了" << tableConvertedCount << "个条目";
    }

    // 更新表格视图以反映更改
    if (currentTab->entryTableView && currentTab->entryTableView->model()) {
        currentTab->entryTableView->viewport()->update();
    }

    m_state = CodeTableState::Original;
    qDebug() << "反向转换完成，共转换" << totalConvertedCount << "个单元格";

    return true;
}

bool CodeTableConverter::convert(GXTStudio* viewer)
{
    if (!viewer) {
        qWarning() << "无效的viewer指针";
        return false;
    }

    if (m_state == CodeTableState::NotMounted) {
        qWarning() << "未挂载码表，无法执行转换";
        return false;
    }

    if (m_state == CodeTableState::Original) {
        return convertForward(viewer);
    } else if (m_state == CodeTableState::Converted) {
        return convertReverse(viewer);
    }

    return false;
}

QMap<QString, QChar> CodeTableConverter::readCharacterMapping(const QString& filePath)
{
    QMap<QString, QChar> mapping;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开码表文件:" << filePath;
        return mapping;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    int lineNumber = 0;
    while (!in.atEnd()) {
        lineNumber++;
        QString line = in.readLine().trimmed();

        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        // 支持制表符或等号作为分隔符
        QStringList parts;
        if (line.contains('\t')) {
            parts = line.split('\t', Qt::SkipEmptyParts);
        } else if (line.contains('=')) {
            parts = line.split('=', Qt::SkipEmptyParts);
        } else {
            qWarning() << "码表文件格式错误，第" << lineNumber << "行: 缺少分隔符（制表符或等号）:" << line;
            continue;
        }

        if (parts.size() != 2) {
            qWarning() << "码表文件格式错误，第" << lineNumber << "行:" << line;
            continue;
        }

        QString chinese = parts[0].trimmed();
        QString hexStr = parts[1].trimmed();
        QChar unicodeChar = hexToUnicode(hexStr);

        if (!unicodeChar.isNull()) {
            mapping[chinese] = unicodeChar;
        }
    }

    file.close();

    qDebug() << "读取码表完成，共" << mapping.size() << "个字符";
    return mapping;
}

QChar CodeTableConverter::hexToUnicode(const QString& text)
{
    QString hexStr = text.trimmed();

    // 移除0x或0X前缀
    if (hexStr.startsWith("0x", Qt::CaseInsensitive)) {
        hexStr = hexStr.mid(2);
    }

    bool ok = false;
    uint codePoint = hexStr.toUInt(&ok, 16);

    if (!ok) {
        return QChar();
    }

    // 检查是否为有效的Unicode字符
    if (codePoint > 0x10FFFF) {
        return QChar();
    }

    // 如果是BMP字符（<= 0xFFFF），直接使用
    if (codePoint <= 0xFFFF) {
        return QChar(static_cast<ushort>(codePoint));
    }

    // 如果是补充字符，返回空字符串（QChar不支持）
    qWarning() << "不支持的Unicode补充字符:" << hexStr;
    return QChar();
}

void CodeTableConverter::buildReverseMap()
{
    m_reverseMap.clear();

    for (auto it = m_conversionMap.begin(); it != m_conversionMap.end(); ++it) {
        m_reverseMap[it.value()] = it.key();
    }

    m_reverseMapValid = true;
    qDebug() << "反向映射表构建完成，共" << m_reverseMap.size() << "个字符";
}
