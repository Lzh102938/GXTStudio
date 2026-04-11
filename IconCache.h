#pragma once

#include <QIcon>
#include <QHash>
#include <QFile>
#include <QCoreApplication>

class IconCache {
public:
    static IconCache& instance() {
        static IconCache inst;
        return inst;
    }
    
    QIcon getIcon(const QString& relativePath) {
        QString fullPath = QCoreApplication::applicationDirPath() + "/" + relativePath;
        if (!m_cache.contains(fullPath)) {
            if (QFile::exists(fullPath)) {
                m_cache[fullPath] = QIcon(fullPath);
            } else {
                m_cache[fullPath] = QIcon();
            }
        }
        return m_cache[fullPath];
    }
    
    QIcon getIconFullPath(const QString& fullPath) {
        if (!m_cache.contains(fullPath)) {
            if (QFile::exists(fullPath)) {
                m_cache[fullPath] = QIcon(fullPath);
            } else {
                m_cache[fullPath] = QIcon();
            }
        }
        return m_cache[fullPath];
    }
    
    void clear() { m_cache.clear(); }
    int size() const { return m_cache.size(); }
    
private:
    IconCache() = default;
    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;
    
    QHash<QString, QIcon> m_cache;
};
