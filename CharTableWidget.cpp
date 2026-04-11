#include "CharTableWidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QScrollBar>
#include <QTextBlock>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QTimer>
#include <algorithm>
#include <QMimeData>
#include <QInputMethodEvent>
#include <QTextCharFormat>
#include <QTextCursor>

CharTableWidget::CharTableWidget(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setUndoRedoEnabled(true);
    setCenterOnScroll(false);
    
    QFont font("Microsoft YaHei", 11);
    font.setStyleHint(QFont::SansSerif);
    setFont(font);
    
    QFontMetrics fm(font);
    m_cellSize = fm.horizontalAdvance('W');
    
    setViewportMargins(0, 0, 0, 0);
    document()->setDocumentMargin(0);
    
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    option.setUseDesignMetrics(false);
    document()->setDefaultTextOption(option);
    
    m_reformatTimer = new QTimer(this);
    m_reformatTimer->setSingleShot(true);
    m_reformatTimer->setInterval(100);
    connect(m_reformatTimer, &QTimer::timeout, this, &CharTableWidget::reformatText);
    
    m_cursorPosTimer = new QTimer(this);
    m_cursorPosTimer->setSingleShot(true);
    m_cursorPosTimer->setInterval(100);
    connect(m_cursorPosTimer, &QTimer::timeout, this, &CharTableWidget::emitCursorPosition);
    
    m_highlightTimer = new QTimer(this);
    m_highlightTimer->setSingleShot(true);
    m_highlightTimer->setInterval(3000);
    connect(m_highlightTimer, &QTimer::timeout, this, &CharTableWidget::clearHighlight);
    
    updateStyle();
    
    connect(this, &QPlainTextEdit::cursorPositionChanged, [this]() {
        m_cursorPosTimer->start();
    });
    
    connect(this, &QPlainTextEdit::textChanged, [this]() {
        m_textDirty = true;
        m_reformatTimer->start();
        emit textModified();
    });
}

void CharTableWidget::setTextColor(const QColor& color)
{
    if (m_textColor != color) {
        m_textColor = color;
        updateStyle();
    }
}

void CharTableWidget::updateStyle()
{
    setStyleSheet(QString(R"(
        QPlainTextEdit {
            background-color: rgba(255, 255, 255, 0.5);
            border: 1px solid rgba(200, 200, 200, 0.5);
            border-radius: 6px;
            selection-background-color: rgba(33, 150, 243, 0.3);
            selection-color: %1;
            padding: 4px;
            color: %1;
        }
        QPlainTextEdit:focus {
            border: 1px solid rgba(33, 150, 243, 0.6);
        }
    )").arg(m_textColor.name()));
    
    QPalette pal = palette();
    pal.setColor(QPalette::Text, m_textColor);
    setPalette(pal);
}

void CharTableWidget::getCursorPosition(int& line, int& column) const
{
    QTextCursor cursor = textCursor();
    line = cursor.blockNumber() + 1;  // 从1开始
    column = cursor.positionInBlock() + 1;  // 从1开始
}

void CharTableWidget::emitCursorPosition()
{
    int line, column;
    getCursorPosition(line, column);
    
    // 只有位置真正变化时才发出信号（节流）
    if (line != m_lastLine || column != m_lastColumn) {
        m_lastLine = line;
        m_lastColumn = column;
        emit cursorPositionChanged(line, column);
    }
}

void CharTableWidget::keyPressEvent(QKeyEvent* event)
{
    // 禁止回车键
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        event->accept();
        return;
    }
    
    // 检查是否是输入字符（排除功能键）
    if (!event->text().isEmpty() && event->text()[0].isPrint()) {
        QChar inputChar = event->text()[0];
        
        // 快速检查ASCII字符 (0-127)
        if (inputChar.unicode() < 128) {
            if (m_hintLabel) {
                m_hintLabel->setText("不允许输入ASCII字符");
                m_hintLabel->setStyleSheet("color: red; font-size: 12px;");
                m_hintLabel->update();
            }
            event->accept();
            return;
        }
        
        // 使用缓存的字符集合进行快速判断
        int currentChars = m_existingChars.size();
        
        // 如果已达到最大字符数，禁止输入
        if (currentChars >= MAX_CHARS) {
            updateHint();
            emit maxCharsReached();
            event->accept();
            return;
        }
        
        // 检查是否输入重复字符
        if (m_existingChars.contains(inputChar)) {
            if (m_hintLabel) {
                m_hintLabel->setText(QString("字符 '%1' 已存在，不允许重复").arg(inputChar));
                m_hintLabel->setStyleSheet("color: red; font-size: 12px;");
                m_hintLabel->update();
            }
            emit duplicateChar(inputChar);
            event->accept();
            return;
        }
    }
    
    // 其他按键正常处理
    QPlainTextEdit::keyPressEvent(event);
    
    // 如果输入了有效字符，延迟清除错误提示（只有当字符未被拒绝时）
    if (!event->text().isEmpty() && event->text()[0].isPrint()) {
        QChar inputChar = event->text()[0];
        if (inputChar.unicode() >= 128 && !m_existingChars.contains(inputChar) && m_hintLabel) {
            QTimer::singleShot(2000, this, [this]() {
                updateHint();
            });
        }
    }
}

void CharTableWidget::mousePressEvent(QMouseEvent* event)
{
    QPlainTextEdit::mousePressEvent(event);
    emitCursorPosition();
}

void CharTableWidget::setHintLabel(QLabel* label)
{
    m_hintLabel = label;
}

void CharTableWidget::updateHint()
{
    if (!m_hintLabel) return;
    
    // 使用缓存的字符集合大小，避免获取文本
    int currentChars = m_existingChars.size();
    
    if (currentChars >= MAX_CHARS) {
        m_hintLabel->setText(QString("字符数最多 %1 x %2 (已达上限)").arg(COLS_PER_LINE).arg(MAX_ROWS));
        m_hintLabel->setStyleSheet("color: red; font-size: 12px;");
        m_maxReached = true;
    } else {
        m_hintLabel->clear();
        m_hintLabel->setStyleSheet("color: #666; font-size: 12px;");
        m_maxReached = false;
    }
}

void CharTableWidget::insertFromMimeData(const QMimeData* source)
{
    if (!source) return;
    QString text = source->text();
    if (text.isEmpty()) return;

    QString filtered;
    filtered.reserve(text.length());
    bool asciiRemoved = false;

    for (QChar ch : text) {
        if (ch == '\r') continue;
        if (ch == '\n') {
            filtered.append('\n');
            continue;
        }
        if (ch.unicode() < 128) {
            asciiRemoved = true;
            continue; // 跳过 ASCII
        }
        if (m_existingChars.contains(ch)) {
            continue; // 跳过已存在字符，避免重复
        }
        filtered.append(ch);
        m_existingChars.insert(ch);
        if (m_existingChars.size() >= MAX_CHARS) break;
    }

    if (filtered.isEmpty()) {
        if (asciiRemoved && m_hintLabel) {
            m_hintLabel->setText("粘贴内容包含非法或重复字符");
            m_hintLabel->setStyleSheet("color: red; font-size: 12px;");
            QTimer::singleShot(2000, this, [this]() { updateHint(); });
        }
        return;
    }

    // 以纯文本方式插入，避免富文本干扰
    QPlainTextEdit::insertPlainText(filtered);
    m_textDirty = true;
    m_reformatTimer->start();
}

void CharTableWidget::inputMethodEvent(QInputMethodEvent* event)
{
    if (!event) {
        QPlainTextEdit::inputMethodEvent(event);
        return;
    }

    QString commit = event->commitString();
    if (!commit.isEmpty()) {
        QString filtered;
        filtered.reserve(commit.length());
        for (QChar ch : commit) {
            if (ch.unicode() < 128) continue;
            if (m_existingChars.contains(ch)) continue;
            filtered.append(ch);
            m_existingChars.insert(ch);
            if (m_existingChars.size() >= MAX_CHARS) break;
        }

        if (!filtered.isEmpty()) {
            QPlainTextEdit::insertPlainText(filtered);
            m_textDirty = true;
            m_reformatTimer->start();
        }
        event->accept();
        return;
    }

    // 没有提交字符串时，交给基类处理（例如预编辑）
    QPlainTextEdit::inputMethodEvent(event);
}

void CharTableWidget::sortCharsInWidget()
{
    // 获取当前所有有效字符，去掉换行与 ASCII
    QString text = toPlainText();
    QVector<uint16_t> chars;
    chars.reserve(text.length());
    for (QChar ch : text) {
        if (ch == '\n' || ch.unicode() == 0) continue;
        if (ch.unicode() < 128) continue;
        chars.append(ch.unicode());
        if (chars.size() >= MAX_CHARS) break;
    }

    if (chars.empty()) return;

    std::sort(chars.begin(), chars.end());

    // 去重（保留一个）
    int write = 0;
    for (int i = 0; i < chars.size(); ++i) {
        if (write == 0 || chars[i] != chars[write - 1]) {
            chars[write++] = chars[i];
        }
    }
    chars.resize(write);

    // 构建格式化文本
    int charCount = chars.size();
    QString formatted;
    formatted.reserve(charCount + (charCount / COLS_PER_LINE) + 2);
    for (int i = 0; i < charCount; ++i) {
        formatted.append(QChar(chars[i]));
        if ((i + 1) % COLS_PER_LINE == 0 && i < charCount - 1) formatted.append('\n');
    }

    // 直接更新文本并刷新缓存
    setPlainText(formatted);
    m_cachedText = formatted;
    m_textDirty = false;

    // 更新字符集合与提示
    m_existingChars.clear();
    m_existingChars.reserve(chars.size());
    for (uint16_t ch : chars) m_existingChars.insert(QChar(ch));
    updateHint();
}

void CharTableWidget::rebuildCharSet()
{
    m_existingChars.clear();
    
    // 如果缓存有效，直接使用缓存，避免获取文本
    QString text = m_textDirty ? toPlainText() : m_cachedText;
    if (text.isEmpty()) return;
    
    m_existingChars.reserve(text.length());  // 预分配空间
    
    // 使用迭代器而非索引，减少计算
    for (auto it = text.constBegin(); it != text.constEnd(); ++it) {
        if (*it == '\n') continue;
        if (it->unicode() < 128) continue; // 跳过 ASCII
        m_existingChars.insert(*it);
    }
    
    m_cachedText = text;
    m_textDirty = false;
}

void CharTableWidget::reformatText()
{
    if (m_formatting) return;
    m_formatting = true;
    
    QString text = m_textDirty ? toPlainText() : m_cachedText;
    
    if (text.isEmpty()) {
        m_formatting = false;
        return;
    }
    
    int textLen = text.length();
    int processLimit = qMin(textLen, MAX_CHARS);
    
    QVector<uint16_t> chars;
    chars.reserve(processLimit);
    QSet<QChar> seen;
    seen.reserve(processLimit);
    
    bool hasDuplicate = false;
    QChar dupChar;
    
    for (int i = 0; i < textLen && chars.size() < MAX_CHARS; ++i) {
        const QChar& ch = text[i];
        if (ch == '\n') continue;
        if (ch.unicode() < 128) continue;
        if (seen.contains(ch)) {
            if (!hasDuplicate) {
                hasDuplicate = true;
                dupChar = ch;
            }
            continue;
        }
        seen.insert(ch);
        chars.append(ch.unicode());
    }
    
    if (hasDuplicate && m_hintLabel) {
        m_hintLabel->setText(QString("字符 '%1' 已存在，不允许重复").arg(dupChar));
        m_hintLabel->setStyleSheet("color: red; font-size: 12px;");
        emit duplicateChar(dupChar);
        QTimer::singleShot(2000, this, [this]() { updateHint(); });
    }
    
    int len = chars.size();
    if (len == 0) {
        m_formatting = false;
        return;
    }
    
    std::sort(chars.begin(), chars.end());
    
    m_charsToHighlight.clear();
    
    if (!m_cachedText.isEmpty()) {
        QSet<uint16_t> oldCharSet;
        QString oldText = m_cachedText;
        for (const QChar& ch : oldText) {
            if (ch == '\n' || ch.unicode() == 0 || ch.unicode() < 128) continue;
            oldCharSet.insert(ch.unicode());
        }
        
        for (uint16_t ch : chars) {
            if (!oldCharSet.contains(ch)) {
                m_charsToHighlight.insert(QChar(ch));
            }
        }
    }
    
    int lineCount = (len - 1) / COLS_PER_LINE + 1;
    QString formatted;
    formatted.reserve(len + lineCount - 1);
    
    for (int i = 0; i < len; ++i) {
        formatted.append(QChar(chars[i]));
        if ((i + 1) % COLS_PER_LINE == 0 && i < len - 1) {
            formatted.append('\n');
        }
    }
    
    if (formatted != text) {
        QTextCursor cursor = textCursor();
        int savedPos = cursor.position();
        
        cursor.beginEditBlock();
        cursor.select(QTextCursor::Document);
        cursor.insertText(formatted);
        cursor.endEditBlock();
        
        m_cachedText = formatted;
        m_textDirty = false;
        
        cursor.setPosition(qMin(savedPos, formatted.length()));
        setTextCursor(cursor);
        
        if (!m_charsToHighlight.isEmpty()) {
            applyHighlight();
            m_highlightTimer->start();
        }
    } else {
        m_cachedText = formatted;
        m_textDirty = false;
    }
    
    m_existingChars = std::move(seen);
    updateHint();
    
    m_formatting = false;
}

void CharTableWidget::applyHighlight()
{
    if (m_charsToHighlight.isEmpty()) return;
    
    QTextCursor cursor = textCursor();
    int savedPos = cursor.position();
    
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    
    QTextCharFormat defaultFormat;
    QTextCharFormat highlightFormat;
    highlightFormat.setBackground(QColor("#3399ff"));
    highlightFormat.setForeground(Qt::white);
    
    QString text = toPlainText();
    int textLen = text.length();
    
    for (int i = 0; i < textLen; ++i) {
        QChar ch = text[i];
        if (ch == '\n') continue;
        if (m_charsToHighlight.contains(ch)) {
            cursor.setPosition(i);
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
            cursor.setCharFormat(highlightFormat);
        }
    }
    
    cursor.endEditBlock();
    
    cursor.setPosition(qMin(savedPos, textLen));
    setTextCursor(cursor);
}

void CharTableWidget::clearHighlight()
{
    m_charsToHighlight.clear();
    
    QTextCursor cursor = textCursor();
    int savedPos = cursor.position();
    
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    
    QTextCharFormat defaultFormat;
    cursor.setCharFormat(defaultFormat);
    
    cursor.endEditBlock();
    
    cursor.setPosition(qMin(savedPos, toPlainText().length()));
    setTextCursor(cursor);
}

void CharTableWidget::setData(const CharTableData& data)
{
    m_data = data;
    
    clearUndoStack();
    
    if (m_data.rows <= 0 || m_data.cols <= 0 || m_data.cells.isEmpty()) {
        setPlainText(QString());
        m_cachedText.clear();
        m_textDirty = false;
        m_existingChars.clear();
        return;
    }
    
    const int charCount = m_data.cells.size();
    int validCount = 0;
    
    const uint16_t* cells = m_data.cells.constData();
    for (int i = 0; i < charCount; ++i) {
        uint16_t ch = cells[i];
        if (ch != 0x0000 && ch != 0x0020 && ch != 0x0009 && ch != 0x000A && ch != 0x000D) {
            ++validCount;
        }
    }
    
    if (validCount == 0) {
        setPlainText(QString());
        m_cachedText.clear();
        m_textDirty = false;
        m_existingChars.clear();
        return;
    }
    
    QVector<uint16_t> characters;
    characters.reserve(validCount);
    for (int i = 0; i < charCount; ++i) {
        uint16_t ch = cells[i];
        if (ch != 0x0000 && ch != 0x0020 && ch != 0x0009 && ch != 0x000A && ch != 0x000D) {
            characters.append(ch);
        }
    }
    
    int lineCount = (validCount - 1) / COLS_PER_LINE + 1;
    int totalLength = validCount + (lineCount - 1);
    
    QString text;
    text.reserve(totalLength);
    
    for (int i = 0; i < validCount; ++i) {
        text.append(QChar(characters[i]));
        if ((i + 1) % COLS_PER_LINE == 0 && i < validCount - 1) {
            text.append('\n');
        }
    }
    
    setPlainText(text);
    m_cachedText = text;
    m_textDirty = false;
    
    m_existingChars.clear();
    m_existingChars.reserve(validCount);
    for (int i = 0; i < validCount; ++i) {
        m_existingChars.insert(QChar(characters[i]));
    }
    
    updateHint();
}

void CharTableWidget::updateDataFromText()
{
    // 从当前文本提取有效字符
    QString text = toPlainText();
    QVector<uint16_t> validChars;
    
    // 提取所有非空字符（跳过换行符）
    for (QChar ch : text) {
        if (ch == '\n' || ch.unicode() == 0) continue;
        if (ch.unicode() < 128) continue; // 跳过 ASCII
        validChars.append(ch.unicode());
    }
    
    // 确保不超出最大字符数限制
    if (validChars.size() > MAX_CHARS) {
        validChars.resize(MAX_CHARS);
    }
    
    // 更新数据
    m_data.cells.fill(0); // 先清空所有数据
    
    // 按行列顺序填充
    for (int i = 0; i < validChars.size(); ++i) {
        if (i < m_data.cells.size()) {
            m_data.cells[i] = validChars[i];
        }
    }
    
    // 更新字符集合
    rebuildCharSet();
    
    // 更新提示
    updateHint();
}