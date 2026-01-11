#include "CharTableWidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QDebug>
#include <QPaintEvent>
#include <QRegion>
#include <QPainterPath>

CharTableWidget::CharTableWidget(QWidget* parent)
    : QWidget(parent)
{
    // 启用双缓冲减少闪烁
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAutoFillBackground(true);

    m_charFont = QFont("Segoe UI", 18);
    m_charFont.setWeight(QFont::Normal);
}

void CharTableWidget::setData(const CharTableData& data)
{
    m_data = data;
    // Choose a font size proportional to cell size
    int fs = qMax(10, m_cellSize / 2);
    m_charFont.setPointSize(fs);
    m_scaledFont = m_charFont;
    m_useScaledFont = false;
    updateGeometry();
    update();
}

void CharTableWidget::setCellSize(int size)
{
    if (size < 12) size = 12;
    if (size > 256) size = 256;
    m_cellSize = size;
    int fs = qMax(10, m_cellSize / 2);
    m_charFont.setPointSize(fs);
    m_scaledFont = m_charFont;
    m_useScaledFont = false;
    updateGeometry();
    update();
}

void CharTableWidget::drawCellBackground(QPainter& p, const QRect& cellRect)
{
    p.fillRect(cellRect, Qt::white);
    p.drawRect(cellRect);
}

void CharTableWidget::drawCellCharacter(QPainter& p, uint16_t ch, const QRect& cellRect)
{
    // 过滤空白字符：空字符、空格、制表符、换行符、回车符
    if (ch == 0x0000 || ch == 0x0020 || ch == 0x0009 || ch == 0x000A || ch == 0x000D) {
        return;
    }

    QString s;
    s.append(QChar((ushort)ch));

    // 使用缓存的字体
    if (m_useScaledFont) {
        p.setFont(m_scaledFont);
    } else {
        p.setFont(m_charFont);
    }

    p.setPen(QColor(34,34,34));
    p.drawText(cellRect, Qt::AlignCenter, s);
}

void CharTableWidget::paintEvent(QPaintEvent* ev)
{
    QPainter p(this);

    // 优化1：启用高质量抗锯齿
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 优化2：使用剪裁区域，只绘制可见部分
    QRegion visibleRegion = ev->region();
    if (visibleRegion.isEmpty()) {
        visibleRegion = rect();
    }

    // 背景
    p.fillRect(rect(), (m_data.type == CharTableData::GTA_VC) ? QColor("#f3f6fb") : Qt::white);

    if (m_data.rows <= 0 || m_data.cols <= 0) return;

    int w = m_cellSize;
    int h = m_cellSize;

    // 优化3：批量绘制网格背景
    QPen gridPen(QColor(200,200,200));
    gridPen.setWidth(1);
    p.setPen(gridPen);

    // 优化4：只计算可见区域的行列范围
    QRect visibleRect = visibleRegion.boundingRect();
    int startCol = qMax(0, visibleRect.left() / w);
    int endCol = qMin(m_data.cols, (visibleRect.right() + w - 1) / w + 1);
    int startRow = qMax(0, visibleRect.top() / h);
    int endRow = qMin(m_data.rows, (visibleRect.bottom() + h - 1) / h + 1);

    // 优化5：预计算是否需要缩放字体
    QFontMetrics fm(m_charFont);
    QString sampleText = QChar(0x4E00); // 使用常用中文字符作为样本
    QSizeF sampleSize = fm.size(Qt::TextSingleLine, sampleText);

    bool needsScaling = (sampleSize.width() > w - 8 || sampleSize.height() > h - 8);
    if (needsScaling != m_useScaledFont) {
        qreal scale = qMin((w - 8) / sampleSize.width(), (h - 8) / sampleSize.height());
        int newFs = qMax(8, int(m_charFont.pointSizeF() * scale));
        m_scaledFont = m_charFont;
        m_scaledFont.setPointSize(newFs);
        m_useScaledFont = true;
    }

    // 批量绘制可见区域的单元格
    for (int r = startRow; r < endRow; ++r) {
        for (int c = startCol; c < endCol; ++c) {
            int x = c * w;
            int y = r * h;
            QRect cellRect(x, y, w, h);

            // 绘制单元格背景
            drawCellBackground(p, cellRect);

            // 获取字符并绘制
            int idx = r * m_data.cols + c;
            if (idx >= 0 && idx < m_data.cells.size()) {
                uint16_t ch = m_data.cells[idx];
                drawCellCharacter(p, ch, cellRect);
            }
        }
    }
}

QSize CharTableWidget::minimumSizeHint() const
{
    if (m_data.rows <= 0 || m_data.cols <= 0) return QSize(200,200);
    return QSize(m_data.cols * m_cellSize, m_data.rows * m_cellSize);
}
