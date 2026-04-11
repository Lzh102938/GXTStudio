#pragma once

#include <QPixmap>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QtCore/QCache>

namespace TextColor {
    constexpr const char* DEFAULT_COLOR = "#495057";
    constexpr const char* DARK_TEXT_COLOR = "#495057";
    constexpr const char* LIGHT_TEXT_COLOR = "#ffffff";
}

class TextColorCalculator {
public:
    void updateBackground(const QPixmap& pixmap, Qt::AspectRatioMode aspectRatioMode, qreal opacity) {
        m_backgroundImage = pixmap.toImage();
        m_aspectRatioMode = aspectRatioMode;
        m_opacity = opacity;
        m_cachedTargetSize = QSize();

        m_colorCache.clear();
        m_gridCache.clear();
    }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    QColor calculateColor(const QPoint& pos, const QRect& targetRect) {
        if (!m_enabled || m_backgroundImage.isNull()) {
            return m_defaultColor;
        }

        qint64 cacheKey = (static_cast<qint64>(pos.x()) << 32) | static_cast<qint64>(pos.y());
        if (auto* cachedColor = m_colorCache.object(cacheKey)) {
            return *cachedColor;
        }

        int gridX = pos.x() / GRID_SIZE;
        int gridY = pos.y() / GRID_SIZE;
        qint64 gridKey = (static_cast<qint64>(gridX) << 32) | static_cast<qint64>(gridY);

        QColor* gridColor = m_gridCache.object(gridKey);
        if (gridColor && targetRect.size() == m_cachedTargetSize) {
            QColor* cachedColor = new QColor(*gridColor);
            m_colorCache.insert(cacheKey, cachedColor);
            return *gridColor;
        }

        QColor result = calculateColorInternal(pos, targetRect);

        QColor* cachedColor = new QColor(result);
        m_colorCache.insert(cacheKey, cachedColor);

        QColor* newGridColor = new QColor(result);
        m_gridCache.insert(gridKey, newGridColor);

        return result;
    }

    QColor calculateColorForRegion(const QRect& region, const QRect& targetRect) {
        if (!m_enabled || m_backgroundImage.isNull()) {
            return m_defaultColor;
        }
        return calculateColor(region.center(), targetRect);
    }

    void clearCache() {
        m_colorCache.clear();
        m_gridCache.clear();
    }

    int getColorCacheSize() const { return m_colorCache.size(); }
    int getGridCacheSize() const { return m_gridCache.size(); }

    void setDefaultColor(const QColor& color) { m_defaultColor = color; }
    void setDarkTextColor(const QColor& color) { m_darkTextColor = color; }
    void setLightTextColor(const QColor& color) { m_lightTextColor = color; }

private:
    QColor calculateColorInternal(const QPoint& pos, const QRect& targetRect) {
        if (targetRect.size() != m_cachedTargetSize) {
            m_cachedTargetSize = targetRect.size();
            m_cachedScaledImage = m_backgroundImage.scaled(
                targetRect.size(),
                m_aspectRatioMode,
                Qt::SmoothTransformation
            );
            m_cachedImgX = targetRect.x() + (targetRect.width() - m_cachedScaledImage.width()) / 2;
            m_cachedImgY = targetRect.y() + (targetRect.height() - m_cachedScaledImage.height()) / 2;
        }

        int pixelX = pos.x() - m_cachedImgX;
        int pixelY = pos.y() - m_cachedImgY;

        if (pixelX < 0 || pixelX >= m_cachedScaledImage.width() ||
            pixelY < 0 || pixelY >= m_cachedScaledImage.height()) {
            return m_defaultColor;
        }

        QColor bgColor = m_cachedScaledImage.pixelColor(pixelX, pixelY);

        int luminanceInt = (76 * bgColor.red() + 150 * bgColor.green() + 29 * bgColor.blue()) >> 8;

        int alpha = bgColor.alpha();
        if (alpha == 255) {
            return (luminanceInt > 127) ? m_darkTextColor : m_lightTextColor;
        } else if (alpha == 0) {
            return (242 > 127) ? m_darkTextColor : m_lightTextColor;
        } else {
            int opacityInt = static_cast<int>(m_opacity * 255);
            int mixedLuminance = (luminanceInt * alpha * opacityInt + 242 * (255 - alpha * opacityInt)) >> 16;
            return (mixedLuminance > 127) ? m_darkTextColor : m_lightTextColor;
        }
    }

    static const int GRID_SIZE = 32;

    QImage m_backgroundImage;
    Qt::AspectRatioMode m_aspectRatioMode = Qt::KeepAspectRatioByExpanding;
    qreal m_opacity = 1.0;
    bool m_enabled = false;

    QImage m_cachedScaledImage;
    QSize m_cachedTargetSize;
    int m_cachedImgX = 0;
    int m_cachedImgY = 0;

    mutable QCache<qint64, QColor> m_colorCache;
    mutable QCache<qint64, QColor> m_gridCache;

    QColor m_defaultColor = QColor(TextColor::DEFAULT_COLOR);
    QColor m_darkTextColor = QColor(TextColor::DARK_TEXT_COLOR);
    QColor m_lightTextColor = QColor(TextColor::LIGHT_TEXT_COLOR);
};
