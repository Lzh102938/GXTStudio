#pragma once

#include <QWidget>
#include "CharTableParser.h"

class CharTableWidget : public QWidget {
    Q_OBJECT
public:
    explicit CharTableWidget(QWidget* parent = nullptr);
    void setData(const CharTableData& data);
    void setCellSize(int size);
    int cellSize() const { return m_cellSize; }
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    void drawCellBackground(QPainter& p, const QRect& cellRect);
    void drawCellCharacter(QPainter& p, uint16_t ch, const QRect& cellRect);

    CharTableData m_data;
    int m_cellSize = 48;
    QFont m_charFont;
    QFont m_scaledFont;
    bool m_useScaledFont = false;
};
