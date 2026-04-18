#pragma once

#include <QPixmap>
#include <QSize>
#include <QVector>
#include <QDateTime>

class BackgroundCache {
private:
    struct CacheLevel {
        QSize size;
        QPixmap pixmap;
        qint64 timestamp;
        int accessCount;
    };
    
    static const int MAX_CACHE_LEVELS = 5;
    static const int SIZE_TOLERANCE = 50;
    
    QVector<CacheLevel> m_cacheLevels;
    QPixmap m_originalPixmap;
    Qt::AspectRatioMode m_aspectRatioMode;
    
public:
    void setOriginalPixmap(const QPixmap& pixmap) {
        m_originalPixmap = pixmap;
        m_cacheLevels.clear();
    }
    
    QPixmap getScaledPixmap(const QSize& targetSize, Qt::AspectRatioMode mode, Qt::TransformationMode transformMode) {
        if (m_originalPixmap.isNull()) {
            return QPixmap();
        }
        
        m_aspectRatioMode = mode;
        
        for (auto& level : m_cacheLevels) {
            if (qAbs(level.size.width() - targetSize.width()) < SIZE_TOLERANCE &&
                qAbs(level.size.height() - targetSize.height()) < SIZE_TOLERANCE) {
                level.accessCount++;
                level.timestamp = QDateTime::currentMSecsSinceEpoch();
                return level.pixmap;
            }
        }
        
        if (m_cacheLevels.size() >= MAX_CACHE_LEVELS) {
            int leastUsedIndex = 0;
            int minAccessCount = m_cacheLevels[0].accessCount;
            qint64 oldestTimestamp = m_cacheLevels[0].timestamp;
            
            for (int i = 1; i < m_cacheLevels.size(); ++i) {
                if (m_cacheLevels[i].accessCount < minAccessCount ||
                    (m_cacheLevels[i].accessCount == minAccessCount && m_cacheLevels[i].timestamp < oldestTimestamp)) {
                    leastUsedIndex = i;
                    minAccessCount = m_cacheLevels[i].accessCount;
                    oldestTimestamp = m_cacheLevels[i].timestamp;
                }
            }
            
            m_cacheLevels.remove(leastUsedIndex);
        }
        
        QPixmap scaled = m_originalPixmap.scaled(targetSize, mode, transformMode);
        
        CacheLevel newLevel;
        newLevel.size = targetSize;
        newLevel.pixmap = scaled;
        newLevel.timestamp = QDateTime::currentMSecsSinceEpoch();
        newLevel.accessCount = 1;
        
        m_cacheLevels.append(newLevel);
        
        return scaled;
    }
    
    void clear() {
        m_cacheLevels.clear();
        m_originalPixmap = QPixmap();
    }
    
    int getCacheLevelCount() const {
        return m_cacheLevels.size();
    }
    
    bool hasOriginalPixmap() const {
        return !m_originalPixmap.isNull();
    }
};
