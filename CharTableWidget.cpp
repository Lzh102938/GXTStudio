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

CharTableWidget::CharTableWidget(QWidget* parent)
    : QPlainTextEdit(parent)
{
    // 基础设置 - 优化性能
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setUndoRedoEnabled(false);
    setCenterOnScroll(false);
    
    // 设置微软雅黑字体
    QFont font("Microsoft YaHei", 11);
    font.setStyleHint(QFont::SansSerif);
    setFont(font);
    
    // 计算单元格大小
    QFontMetrics fm(font);
    m_cellSize = fm.horizontalAdvance('W');
    
    // 移除所有边距
    setViewportMargins(0, 0, 0, 0);
    document()->setDocumentMargin(0);
    
    // 优化文本选项
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    option.setUseDesignMetrics(false);  // 使用整数度量，更快
    document()->setDefaultTextOption(option);
    
    // 创建延迟格式化定时器（防抖）
    m_reformatTimer = new QTimer(this);
    m_reformatTimer->setSingleShot(true);
    m_reformatTimer->setInterval(50);  // 50ms延迟
    connect(m_reformatTimer, &QTimer::timeout, this, &CharTableWidget::reformatText);
    
    // 创建光标位置信号节流定时器
    m_cursorPosTimer = new QTimer(this);
    m_cursorPosTimer->setSingleShot(true);
    m_cursorPosTimer->setInterval(100);  // 100ms节流
    connect(m_cursorPosTimer, &QTimer::timeout, this, &CharTableWidget::emitCursorPosition);
    
    // 使用默认绘制，不自定义 paintEvent 以利用 Qt 的优化
    
    // 设置样式
    setStyleSheet(R"(
        QPlainTextEdit {
            background-color: white;
            border: 1px solid #ccc;
            selection-background-color: #3399ff;
            padding: 0px;
        }
    )");
    
    // 连接 Qt 自带的光标位置变化信号（使用节流）
    connect(this, &QPlainTextEdit::cursorPositionChanged, [this]() {
        m_cursorPosTimer->start();  // 重置定时器，实现节流
    });
    
    // 连接文本变化信号，使用延迟格式化
    connect(this, &QPlainTextEdit::textChanged, [this]() {
        m_textDirty = true;  // 标记文本缓存失效
        m_reformatTimer->start();  // 重置定时器，实现防抖
        emit textModified();  // 发出文本修改信号
    });
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
                m_hintLabel->repaint();
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
                m_hintLabel->repaint();  // 强制刷新
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

void CharTableWidget::rebuildCharSet()
{
    m_existingChars.clear();
    
    // 如果缓存有效，直接使用缓存，避免获取文本
    QString text = m_textDirty ? toPlainText() : m_cachedText;
    if (text.isEmpty()) return;
    
    m_existingChars.reserve(text.length());  // 预分配空间
    
    // 使用迭代器而非索引，减少计算
    for (auto it = text.constBegin(); it != text.constEnd(); ++it) {
        if (*it != '\n') {
            m_existingChars.insert(*it);
        }
    }
    
    m_cachedText = text;
    m_textDirty = false;
}

void CharTableWidget::reformatText()
{
    if (m_formatting) return;  // 防止递归
    m_formatting = true;
    
    // 使用缓存文本，避免重复获取
    QString text = m_textDirty ? toPlainText() : m_cachedText;
    
    // 快速检查：如果文本为空或没有换行，直接返回
    if (text.isEmpty() || !text.contains('\n')) {
        // 只需要检查字符限制
        int chars = text.length();
        if (chars > MAX_CHARS) {
            text = text.left(MAX_CHARS);
            setPlainText(text);
            emit maxCharsReached();
        }
        m_formatting = false;
        return;
    }
    
    // 移除所有换行符，合并成一行
    text.remove('\n');
    
    // 检测并移除重复字符（只保留第一个出现的）
    QString deduped;
    deduped.reserve(qMin(text.length(), MAX_CHARS));
    QSet<QChar> seen;
    bool hasDuplicate = false;
    QChar dupChar;
    
    // 限制循环次数，只处理前MAX_CHARS个字符
    int processLimit = qMin(text.length(), MAX_CHARS);
    for (int i = 0; i < processLimit; ++i) {
        const QChar& ch = text[i];
        if (seen.contains(ch)) {
            hasDuplicate = true;
            dupChar = ch;
            continue;  // 跳过重复字符
        }
        seen.insert(ch);
        deduped.append(ch);
    }
    
    // 显示重复字符提示
    if (hasDuplicate && m_hintLabel) {
        m_hintLabel->setText(QString("字符 '%1' 已存在，不允许重复").arg(dupChar));
        m_hintLabel->setStyleSheet("color: red; font-size: 12px;");
        emit duplicateChar(dupChar);
        QTimer::singleShot(2000, this, [this]() {
            updateHint();
        });
    }
    
    text = deduped;
    
    // 限制字符数不超过 64x64
    if (text.length() > MAX_CHARS) {
        text = text.left(MAX_CHARS);
        emit maxCharsReached();
    }
    
    // 重新按每64字符分行 - 使用预分配
    int len = text.length();
    int newLines = (len - 1) / COLS_PER_LINE;
    QString formatted;
    formatted.reserve(len + newLines);  // 预分配空间
    
    for (int i = 0; i < len; ++i) {
        formatted.append(text[i]);
        if ((i + 1) % COLS_PER_LINE == 0 && i < len - 1) {
            formatted.append('\n');
        }
    }
    
    // 获取当前光标位置
    QTextCursor cursor = textCursor();
    int oldPos = cursor.position();
    
    // 只有在文本确实变化时才更新（使用缓存的旧文本比较）
    QString oldText = m_cachedText.isEmpty() ? toPlainText() : m_cachedText;
    if (formatted != oldText) {
        setPlainText(formatted);
        m_cachedText = formatted;  // 更新缓存
        m_textDirty = false;
        
        // 恢复光标位置（尽量保持在原位置）
        cursor.setPosition(qMin(oldPos, formatted.length()));
        setTextCursor(cursor);
    }
    
    // 重建字符集合（复用seen集合，避免重新遍历）
    m_existingChars = std::move(seen);
    
    // 更新提示（使用缓存的文本）
    m_cachedText = formatted;
    m_textDirty = false;
    updateHint();
    
    m_formatting = false;
}

void CharTableWidget::setData(const CharTableData& data)
{
    m_data = data;
    
    if (m_data.rows <= 0 || m_data.cols <= 0 || m_data.cells.isEmpty()) {
        setPlainText(QString());
        m_cachedText.clear();
        m_textDirty = false;
        m_existingChars.clear();
        return;
    }
    
    // 收集有效字符（使用预分配和快速过滤）
    QVector<uint16_t> characters;
    characters.reserve(m_data.cells.size());
    
    // 使用const引用避免拷贝
    const QVector<uint16_t>& cells = m_data.cells;
    for (uint16_t ch : cells) {
        // 快速过滤：使用位运算判断
        if (ch != 0x0000 && ch != 0x0020 && ch != 0x0009 && ch != 0x000A && ch != 0x000D) {
            characters.append(ch);
        }
    }
    
    // 如果没有有效字符，直接返回
    if (characters.isEmpty()) {
        setPlainText(QString());
        m_cachedText.clear();
        m_textDirty = false;
        m_existingChars.clear();
        return;
    }
    
    // 排序
    std::sort(characters.begin(), characters.end());
    
    // 预计算需要的换行符数量
    int charCount = characters.size();
    int lineCount = (charCount - 1) / COLS_PER_LINE + 1;
    int totalLength = charCount + (lineCount - 1);  // 文本 + 换行符
    
    // 构建文本（预分配空间）
    QString text;
    text.reserve(totalLength);
    
    for (int i = 0; i < charCount; ++i) {
        text.append(QChar(characters[i]));
        if ((i + 1) % COLS_PER_LINE == 0 && i < charCount - 1) {
            text.append('\n');
        }
    }
    
    // 使用光标插入文本，保持格式
    setPlainText(text);
    m_cachedText = text;  // 更新缓存
    m_textDirty = false;
    
    // 设置固定行高（批量设置，使用更高效的方法）
    QFontMetrics fm(font());
    int lineHeight = fm.height();
    QTextBlockFormat format;
    format.setLineHeight(lineHeight, QTextBlockFormat::FixedHeight);
    format.setTopMargin(0);
    format.setBottomMargin(0);
    
    QTextCursor cursor(document());
    cursor.beginEditBlock();  // 开始批量编辑
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        QTextCursor blockCursor(block);
        blockCursor.setBlockFormat(format);
    }
    cursor.endEditBlock();  // 结束批量编辑
    
    // 重建字符集合（使用已排序的characters）
    m_existingChars.clear();
    m_existingChars.reserve(charCount);
    for (uint16_t ch : characters) {
        m_existingChars.insert(QChar(ch));
    }
    
    // 更新提示
    updateHint();
}

void CharTableWidget::updateDataFromText()
{
    // 从当前文本提取有效字符
    QString text = toPlainText();
    QVector<uint16_t> validChars;
    
    // 提取所有非空字符（跳过换行符）
    for (QChar ch : text) {
        if (ch != '\n' && ch.unicode() != 0) {
            validChars.append(ch.unicode());
        }
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