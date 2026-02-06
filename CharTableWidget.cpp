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
    
    // 连接 Qt 自带的光标位置变化信号
    connect(this, &QPlainTextEdit::cursorPositionChanged, [this]() {
        emitCursorPosition();
    });
    
    // 连接文本变化信号，强制格式化
    connect(this, &QPlainTextEdit::textChanged, [this]() {
        reformatText();
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
    emit cursorPositionChanged(line, column);
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
        
        // 检查是否是ASCII字符 (0-127)
        if (inputChar.unicode() < 128) {
            if (m_hintLabel) {
                m_hintLabel->setText("不允许输入ASCII字符");
                m_hintLabel->setStyleSheet("color: red; font-size: 12px;");
                m_hintLabel->repaint();
            }
            event->accept();
            return;
        }
        
        // 检查当前字符数
        QString text = toPlainText();
        text.remove('\n');
        int currentChars = text.length();
        
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
    
    // 如果输入了有效字符，延迟清除错误提示
    if (!event->text().isEmpty() && event->text()[0].isPrint()) {
        QChar inputChar = event->text()[0];
        if (!m_existingChars.contains(inputChar) && m_hintLabel) {
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
    
    QString text = toPlainText();
    text.remove('\n');
    int currentChars = text.length();
    
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
    QString text = toPlainText();
    for (const QChar& ch : text) {
        if (ch != '\n') {
            m_existingChars.insert(ch);
        }
    }
}

void CharTableWidget::reformatText()
{
    if (m_formatting) return;  // 防止递归
    m_formatting = true;
    
    QString text = toPlainText();
    // 移除所有换行符，合并成一行
    text.remove('\n');
    
    // 检测并移除重复字符（只保留第一个出现的）
    QString deduped;
    deduped.reserve(text.length());
    QSet<QChar> seen;
    bool hasDuplicate = false;
    QChar dupChar;
    
    for (const QChar& ch : text) {
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
    
    // 重新按每64字符分行
    QString formatted;
    for (int i = 0; i < text.length(); ++i) {
        formatted.append(text[i]);
        if ((i + 1) % COLS_PER_LINE == 0 && i < text.length() - 1) {
            formatted.append('\n');
        }
    }
    
    // 获取当前光标位置
    QTextCursor cursor = textCursor();
    int oldPos = cursor.position();
    
    // 只有在文本确实变化时才更新
    if (formatted != toPlainText()) {
        setPlainText(formatted);
        
        // 恢复光标位置（尽量保持在原位置）
        cursor.setPosition(qMin(oldPos, formatted.length()));
        setTextCursor(cursor);
    }
    
    // 重建字符集合
    rebuildCharSet();
    
    // 更新提示
    updateHint();
    
    m_formatting = false;
}

void CharTableWidget::setData(const CharTableData& data)
{
    m_data = data;
    
    if (m_data.rows <= 0 || m_data.cols <= 0 || m_data.cells.isEmpty()) {
        setPlainText(QString());
        return;
    }
    
    // 收集有效字符
    QVector<uint16_t> characters;
    characters.reserve(m_data.cells.size());
    for (uint16_t ch : m_data.cells) {
        if (ch != 0x0000 && ch != 0x0020 && ch != 0x0009 && ch != 0x000A && ch != 0x000D) {
            characters.append(ch);
        }
    }
    
    // 排序
    std::sort(characters.begin(), characters.end());
    
    // 构建文本
    QString text;
    for (int i = 0; i < characters.size(); ++i) {
        text.append(QChar(characters[i]));
        if ((i + 1) % COLS_PER_LINE == 0 && i < characters.size() - 1) {
            text.append('\n');
        }
    }
    
    // 使用光标插入文本，保持格式
    setPlainText(text);
    
    // 设置固定行高
    QFontMetrics fm(font());
    int lineHeight = fm.height();
    
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block);
        QTextBlockFormat format;
        format.setLineHeight(lineHeight, QTextBlockFormat::FixedHeight);
        format.setTopMargin(0);
        format.setBottomMargin(0);
        cursor.setBlockFormat(format);
    }
    
    // 重建字符集合
    rebuildCharSet();
    
    // 更新提示
    updateHint();
}
