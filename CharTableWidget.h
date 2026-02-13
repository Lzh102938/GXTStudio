#pragma once

#include <QPlainTextEdit>
#include <QSet>
#include "CharTableParser.h"

class QLabel;

class CharTableWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CharTableWidget(QWidget* parent = nullptr);
    
    // 加载字符表数据
    void setData(const CharTableData& data);
    
    // 获取当前数据
    CharTableData data() const { return m_data; }
    
    // 从当前文本更新数据
    void updateDataFromText();
    
    // 获取当前行列位置（从1开始，像记事本一样）
    void getCursorPosition(int& line, int& column) const;
    
    // 设置提示标签
    void setHintLabel(QLabel* label);

signals:
    // 光标位置变化信号
    void cursorPositionChanged(int line, int column);
    // 字符数超限信号
    void maxCharsReached();
    // 重复字符信号
    void duplicateChar(QChar ch);
    // 文本被修改信号
    void textModified();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    CharTableData m_data;
    int m_cellSize = 0;
    static const int COLS_PER_LINE = 64;
    static const int MAX_ROWS = 64;
    static const int MAX_CHARS = COLS_PER_LINE * MAX_ROWS;  // 64x64 = 4096
    bool m_formatting = false;  // 防止递归
    bool m_maxReached = false;  // 是否已达到最大字符数
    QLabel* m_hintLabel = nullptr;  // 提示标签
    QSet<QChar> m_existingChars;  // 已存在的字符集合，用于查重
    QString m_cachedText;  // 缓存的纯文本
    bool m_textDirty = true;  // 文本是否需要重新缓存
    QTimer* m_reformatTimer;  // 延迟格式化定时器
    QTimer* m_cursorPosTimer;  // 光标位置信号节流定时器
    int m_lastLine = -1;  // 上次发出的行号
    int m_lastColumn = -1;  // 上次发出的列号
    
    void emitCursorPosition();
    void reformatText();  // 重新格式化文本，确保每行64字符
    void updateHint();  // 更新提示信息
    void rebuildCharSet();  // 重建字符集合
};