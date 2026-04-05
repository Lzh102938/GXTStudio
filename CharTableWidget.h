#pragma once

#include <QPlainTextEdit>
#include <QSet>
#include "CharTableParser.h"

class QLabel;

class CharTableWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CharTableWidget(QWidget* parent = nullptr);
    
    void setData(const CharTableData& data);
    
    CharTableData data() const { return m_data; }
    
    void updateDataFromText();
    
    void getCursorPosition(int& line, int& column) const;
    
    void setHintLabel(QLabel* label);
    void sortCharsInWidget();
    void clearHighlight();
    void clearUndoStack() { document()->setUndoRedoEnabled(false); document()->setUndoRedoEnabled(true); }
    
    void setTextColor(const QColor& color);
    QColor textColor() const { return m_textColor; }

signals:
    void cursorPositionChanged(int line, int column);
    void maxCharsReached();
    void duplicateChar(QChar ch);
    void textModified();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void insertFromMimeData(const QMimeData* source) override;
    void inputMethodEvent(QInputMethodEvent* event) override;

private:
    CharTableData m_data;
    int m_cellSize = 0;
    static const int COLS_PER_LINE = 64;
    static const int MAX_ROWS = 64;
    static const int MAX_CHARS = COLS_PER_LINE * MAX_ROWS;
    bool m_formatting = false;
    bool m_maxReached = false;
    QLabel* m_hintLabel = nullptr;
    QSet<QChar> m_existingChars;
    QString m_cachedText;
    bool m_textDirty = true;
    QTimer* m_reformatTimer;
    QTimer* m_cursorPosTimer;
    QTimer* m_highlightTimer;
    int m_lastLine = -1;
    int m_lastColumn = -1;
    QSet<QChar> m_charsToHighlight;
    QColor m_textColor = QColor("#333333");
    
    void emitCursorPosition();
    void reformatText();
    void updateHint();
    void rebuildCharSet();
    void applyHighlight();
    void updateStyle();
};