#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QWidget>
#include "CharTableData.h"

class CharTableWidget;
class QTabWidget;

// 字符表管理器 - 统一管理CharTable文件的打开、切换、保存和导出
class CharTableManager : public QObject
{
    Q_OBJECT

public:
    explicit CharTableManager(QTabWidget* tabWidget, QObject* parent = nullptr);
    ~CharTableManager();

    // 打开字符表文件
    bool openCharTable(const QString& filePath);
    
    // 保存当前字符表文件
    bool saveCurrentCharTable();
    
    // 导出当前字符表
    bool exportCurrentCharTable();
    
    // 检查是否为字符表文件
    static bool isCharTableFile(const QString& filePath);
    
    // 获取当前字符表文件名
    QString currentCharTableFileName() const;
    
    // 检查当前标签页是否为字符表
    bool isCurrentTabCharTable() const;

signals:
    void charTableOpened(const QString& fileName);
    void charTableSaved(const QString& fileName);
    void charTableExported(const QString& fileName);
    void error(const QString& message);

private slots:
    void onTabChanged(int index);

private:
    // 创建字符表标签页
    int createCharTableTab(const QString& fileName, const CharTableData& data);
    
    // 获取字符表数据
    CharTableData* getCharTableData(int tabIndex);
    
    // 获取字符表控件
    CharTableWidget* getCharTableWidget(int tabIndex);

private:
    QTabWidget* m_tabWidget;
    QVector<CharTableData> m_charTableData;  // 存储所有打开的字符表数据
    QVector<int> m_tabIndexMap;              // 标签页索引到字符表数据的映射
};