#pragma once

#include <QWidget>
#include <QPixmap>
#include <QCache>
#include <QPoint>
#include "CharTableParser.h"

class CharTableWidget : public QWidget {
    Q_OBJECT
public:
    explicit CharTableWidget(QWidget* parent = nullptr);
    ~CharTableWidget();
    void setData(const CharTableData& data);
    void setCellSize(int size);
    int cellSize() const { return m_cellSize; }
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    // 字符缓存相关
    struct CacheKey {
        uint16_t charCode;
        int cellSize;
        bool operator==(const CacheKey& other) const {
            return charCode == other.charCode && cellSize == other.cellSize;
        }
    };

    // 声明友元函数
    friend uint qHash(const CacheKey& key, uint seed);

    QPixmap* getOrCreateCharPixmap(uint16_t charCode);
    void copySelectedCells();
    QPoint getCellFromPos(const QPoint& pos);
    void normalizeSelection();
    void clearSelection();

    CharTableData m_data;
    int m_cellSize = 48;
    QFont m_charFont;
    QFont m_scaledFont;
    bool m_useScaledFont = false;

    // 极致优化：轻量级字符缓存系统（只缓存最近使用的字符）
    QCache<CacheKey, QPixmap> m_charCache;

    // 选择相关
    bool m_isSelecting = false;
    QPoint m_selectionStart = QPoint(-1, -1);
    QPoint m_selectionEnd = QPoint(-1, -1);
};
