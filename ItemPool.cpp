#include "ItemPool.h"

ItemPool& ItemPool::instance()
{
    static ItemPool pool;
    return pool;
}

ItemPool::~ItemPool()
{
    clear();
}

QTableWidgetItem* ItemPool::getItem()
{
    QMutexLocker locker(&m_mutex);
    if (!m_pool.isEmpty()) {
        return m_pool.takeLast();
    }
    return new QTableWidgetItem();
}

void ItemPool::returnItem(QTableWidgetItem* item)
{
    if (!item) return;
    
    QMutexLocker locker(&m_mutex);
    // 清理项目数据
    item->setText("");
    item->setToolTip("");
    item->setData(Qt::UserRole, QVariant());
    
    if (m_pool.size() < 1000) { // 限制缓存池大小
        m_pool.append(item);
    } else {
        delete item;
    }
}

void ItemPool::clear()
{
    QMutexLocker locker(&m_mutex);
    qDeleteAll(m_pool);
    m_pool.clear();
}