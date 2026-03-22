#pragma once

#include <QTableWidgetItem>
#include <QList>
#include <QMutex>
#include <QMutexLocker>

/**
 * @brief 表格项缓存池 - 单例模式
 * 
 * 用于缓存 QTableWidgetItem 对象，减少频繁创建/删除的开销
 * 最大缓存 1000 个对象
 */
class ItemPool {
public:
    static ItemPool& instance();
    
    // 获取一个 QTableWidgetItem（从缓存或新建）
    QTableWidgetItem* getItem();
    
    // 归还一个 QTableWidgetItem 到缓存池
    void returnItem(QTableWidgetItem* item);
    
    // 清空缓存池
    void clear();

private:
    ItemPool() = default;
    ~ItemPool();
    
    // 禁止拷贝
    ItemPool(const ItemPool&) = delete;
    ItemPool& operator=(const ItemPool&) = delete;
    
    QList<QTableWidgetItem*> m_pool;
    QMutex m_mutex;
};