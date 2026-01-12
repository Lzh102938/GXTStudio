#include "CharTableWidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QDebug>
#include <QPaintEvent>
#include <QRegion>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>

// CacheKey 的哈希函数实现
uint qHash(const CharTableWidget::CacheKey& key, uint seed) {
    return qHash(key.charCode, seed) ^ qHash(key.cellSize, seed);
}

CharTableWidget::CharTableWidget(QWidget* parent)
    : QWidget(parent)
{
    // 极致优化：不使用双缓冲，减少内存占用
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    // 设置焦点策略
    setFocusPolicy(Qt::StrongFocus);

    // 使用更轻量的字体
    m_charFont = QFont("Arial", 18);
    m_charFont.setWeight(QFont::Normal);
    m_charFont.setStyleHint(QFont::SansSerif, QFont::PreferBitmap);

    // 极致优化：只缓存最近使用的500个字符，大幅减少内存占用
    m_charCache.setMaxCost(500);
}

CharTableWidget::~CharTableWidget()
{
    m_charCache.clear();
}

void CharTableWidget::setData(const CharTableData& data)
{
    m_data = data;
    int fs = qMax(10, m_cellSize / 2);
    m_charFont.setPointSize(fs);

    // 预计算字体缩放
    QFontMetrics fm(m_charFont);
    QString sampleText = QChar(0x4E00);
    QSizeF sampleSize = fm.size(Qt::TextSingleLine, sampleText);
    bool needsScaling = (sampleSize.width() > m_cellSize - 6 || sampleSize.height() > m_cellSize - 6);
    if (needsScaling) {
        qreal scale = qMin((m_cellSize - 6) / sampleSize.width(), (m_cellSize - 6) / sampleSize.height());
        int newFs = qMax(8, int(m_charFont.pointSizeF() * scale));
        m_scaledFont = m_charFont;
        m_scaledFont.setPointSize(newFs);
        m_useScaledFont = true;
    } else {
        m_scaledFont = m_charFont;
        m_useScaledFont = false;
    }

    clearSelection();
    m_charCache.clear();
    updateGeometry();
    update();
}

void CharTableWidget::setCellSize(int size)
{
    if (size < 12) size = 12;
    if (size > 256) size = 256;
    if (m_cellSize == size) return;

    m_cellSize = size;
    int fs = qMax(10, m_cellSize / 2);
    m_charFont.setPointSize(fs);

    // 预计算字体缩放
    QFontMetrics fm(m_charFont);
    QString sampleText = QChar(0x4E00);
    QSizeF sampleSize = fm.size(Qt::TextSingleLine, sampleText);
    bool needsScaling = (sampleSize.width() > m_cellSize - 6 || sampleSize.height() > m_cellSize - 6);
    if (needsScaling) {
        qreal scale = qMin((m_cellSize - 6) / sampleSize.width(), (m_cellSize - 6) / sampleSize.height());
        int newFs = qMax(8, int(m_charFont.pointSizeF() * scale));
        m_scaledFont = m_charFont;
        m_scaledFont.setPointSize(newFs);
        m_useScaledFont = true;
    } else {
        m_scaledFont = m_charFont;
        m_useScaledFont = false;
    }

    updateGeometry();
    update();
}

QPixmap* CharTableWidget::getOrCreateCharPixmap(uint16_t charCode)
{
    // 过滤空白字符
    if (charCode == 0x0000 || charCode == 0x0020 || charCode == 0x0009 ||
        charCode == 0x000A || charCode == 0x000D) {
        return nullptr;
    }

    // 创建缓存键
    CacheKey key{charCode, m_cellSize};

    // 检查缓存
    if (m_charCache.contains(key)) {
        return m_charCache.object(key);
    }

    // 创建新的字符位图（尽量小）
    int padding = 2;
    int pixmapSize = m_cellSize - padding * 2;
    QPixmap* pixmap = new QPixmap(pixmapSize, pixmapSize);
    pixmap->fill(Qt::transparent);

    QPainter p(pixmap);
    p.setRenderHint(QPainter::TextAntialiasing, false); // 关闭抗锯齿提升性能

    // 设置字体
    if (m_useScaledFont) {
        p.setFont(m_scaledFont);
    } else {
        p.setFont(m_charFont);
    }

    // 绘制字符
    p.setPen(QColor(34, 34, 34));
    QRect textRect(0, 0, pixmapSize, pixmapSize);
    QString s = QChar((ushort)charCode);
    p.drawText(textRect, Qt::AlignCenter, s);

    // 添加到缓存（cost=1，每个字符占用相同的缓存空间）
    m_charCache.insert(key, pixmap, 1);

    return pixmap;
}

void CharTableWidget::paintEvent(QPaintEvent* ev)
{
    // 提前返回检查
    if (m_data.rows <= 0 || m_data.cols <= 0) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setRenderHint(QPainter::TextAntialiasing, false);
        p.fillRect(rect(), (m_data.type == CharTableData::GTA_VC) ? QColor("#f3f6fb") : Qt::white);
        return;
    }

    const int w = m_cellSize;
    const int h = m_cellSize;

    // 极致优化：直接绘制到屏幕，关闭所有抗锯齿
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::TextAntialiasing, false);

    // 绘制背景
    p.fillRect(rect(), (m_data.type == CharTableData::GTA_VC) ? QColor("#f3f6fb") : Qt::white);

    // 计算可见区域（只绘制可见部分）
    QRect visibleRect = ev->region().boundingRect();
    if (visibleRect.isEmpty()) {
        visibleRect = rect();
    }

    // 精确计算可见区域的行列范围
    int startCol = qMax(0, visibleRect.left() / w);
    int endCol = qMin(m_data.cols, (visibleRect.right() + w - 1) / w + 1);
    int startRow = qMax(0, visibleRect.top() / h);
    int endRow = qMin(m_data.rows, (visibleRect.bottom() + h - 1) / h + 1);

    // 批量绘制网格线（一次性绘制所有线条）
    QPen gridPen(QColor(220, 220, 220));
    gridPen.setWidth(1);
    p.setPen(gridPen);

    // 绘制可见区域的垂直网格线
    const int topY = startRow * h;
    const int bottomY = endRow * h;
    for (int c = startCol; c <= endCol; ++c) {
        const int x = c * w;
        p.drawLine(x, topY, x, bottomY);
    }

    // 绘制可见区域的水平网格线
    const int leftX = startCol * w;
    const int rightX = endCol * w;
    for (int r = startRow; r <= endRow; ++r) {
        const int y = r * h;
        p.drawLine(leftX, y, rightX, y);
    }

    // 批量绘制字符和背景
    const int padding = 2;
    const bool isSelected = (m_selectionStart.x() != -1 && m_selectionEnd.x() != -1);
    int selStartRow = isSelected ? qMin(m_selectionStart.y(), m_selectionEnd.y()) : -1;
    int selEndRow = isSelected ? qMax(m_selectionStart.y(), m_selectionEnd.y()) : -1;
    int selStartCol = isSelected ? qMin(m_selectionStart.x(), m_selectionEnd.x()) : -1;
    int selEndCol = isSelected ? qMax(m_selectionStart.x(), m_selectionEnd.x()) : -1;

    for (int r = startRow; r < endRow; ++r) {
        const int y = r * h;
        for (int c = startCol; c < endCol; ++c) {
            const int x = c * w;
            const bool cellSelected = isSelected && r >= selStartRow && r <= selEndRow &&
                                   c >= selStartCol && c <= selEndCol;

            // 绘制单元格背景
            p.fillRect(x, y, w, h, cellSelected ? QColor(51, 153, 255) : Qt::white);
            p.drawRect(x, y, w, h);

            // 绘制字符
            const int idx = r * m_data.cols + c;
            if (idx >= 0 && idx < m_data.cells.size()) {
                const uint16_t ch = m_data.cells[idx];
                QPixmap* pixmap = getOrCreateCharPixmap(ch);
                if (pixmap) {
                    p.drawPixmap(x + padding, y + padding, *pixmap);
                }
            }
        }
    }
}

QSize CharTableWidget::minimumSizeHint() const
{
    if (m_data.rows <= 0 || m_data.cols <= 0) return QSize(200,200);
    return QSize(m_data.cols * m_cellSize, m_data.rows * m_cellSize);
}

void CharTableWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_isSelecting = true;
        m_selectionStart = getCellFromPos(event->pos());
        m_selectionEnd = m_selectionStart;
        update();
    }
}

void CharTableWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isSelecting) {
        m_selectionEnd = getCellFromPos(event->pos());
        update();
    }
}

void CharTableWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        normalizeSelection();
    }
}

void CharTableWidget::keyPressEvent(QKeyEvent* event)
{
    // Ctrl+C 复制选中的字符
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_C) {
        copySelectedCells();
        return;
    }
    QWidget::keyPressEvent(event);
}

void CharTableWidget::copySelectedCells()
{
    if (m_selectionStart.x() == -1 || m_selectionEnd.x() == -1) return;

    QString copiedText;
    normalizeSelection();

    int startRow = qMin(m_selectionStart.y(), m_selectionEnd.y());
    int endRow = qMax(m_selectionStart.y(), m_selectionEnd.y());
    int startCol = qMin(m_selectionStart.x(), m_selectionEnd.x());
    int endCol = qMax(m_selectionStart.x(), m_selectionEnd.x());

    for (int r = startRow; r <= endRow; ++r) {
        for (int c = startCol; c <= endCol; ++c) {
            int idx = r * m_data.cols + c;
            if (idx >= 0 && idx < m_data.cells.size()) {
                uint16_t ch = m_data.cells[idx];
                // 过滤空白字符
                if (ch != 0x0000 && ch != 0x0020 && ch != 0x0009 && ch != 0x000A && ch != 0x000D) {
                    copiedText.append(QChar((ushort)ch));
                }
            }
        }
    }

    if (!copiedText.isEmpty()) {
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(copiedText);
    }
}

QPoint CharTableWidget::getCellFromPos(const QPoint& pos)
{
    if (m_data.rows <= 0 || m_data.cols <= 0) return QPoint(-1, -1);
    int col = pos.x() / m_cellSize;
    int row = pos.y() / m_cellSize;
    // 确保在有效范围内
    if (col < 0) col = 0;
    if (col >= m_data.cols) col = m_data.cols - 1;
    if (row < 0) row = 0;
    if (row >= m_data.rows) row = m_data.rows - 1;
    return QPoint(col, row);
}

void CharTableWidget::normalizeSelection()
{
    // 确保选择的范围有效
    if (m_selectionStart.x() == -1 || m_selectionEnd.x() == -1) return;
    
    // 确保在有效范围内
    m_selectionStart.setX(qBound(0, m_selectionStart.x(), m_data.cols - 1));
    m_selectionStart.setY(qBound(0, m_selectionStart.y(), m_data.rows - 1));
    m_selectionEnd.setX(qBound(0, m_selectionEnd.x(), m_data.cols - 1));
    m_selectionEnd.setY(qBound(0, m_selectionEnd.y(), m_data.rows - 1));
}

void CharTableWidget::clearSelection()
{
    m_selectionStart = QPoint(-1, -1);
    m_selectionEnd = QPoint(-1, -1);
    m_isSelecting = false;
    update();
}
