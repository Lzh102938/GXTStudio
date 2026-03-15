#include "TextRenderWidget.h"
#include <QApplication>
#include <QPainterPath>
#include <QDebug>
#include <QDir>
#include <QFontMetrics>
#include <QMenu>
#include <QWidgetAction>
#include <QRegularExpression>
#include <QFileDialog>
#include <QImage>
#include <QPixmap>
#include <QDrag>
#include <QMimeData>
#include <QMessageBox>
#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QUrl>
#include <QDateTime>
#include <QTimer>

// 静态定义：控制备用字体信息只打印一次
bool TextRenderWidget::s_fallbackLogged = false;

// 获取对应版本的颜色
static QColor getColorForToken(const QString& token, int gtaVersion) {
    // 定义颜色映射 - 所有颜色都设置90%不透明度(alpha=230/255 = 90%)
    static const QMap<QString, QColor> gta3Colors = {
        {"~b~", QColor(125, 163, 237, 255)},      // blue
        {"~g~", QColor(69, 147, 111, 255)},      // green
        {"~h~", QColor(255, 255, 255, 255)},  // white (highlight)
        {"~p~", QColor(163, 107, 244, 255)},    // purple
        {"~r~", QColor(110, 42, 17, 255)},      // red
        {"~w~", QColor(230, 230, 230, 255)},  // light gray - reset to default color
        {"~y~", QColor(204, 190, 103, 255)},    // yellow
        {"~a~", QColor(255, 165, 0, 255)},    // area text (orange)
    };

    static const QMap<QString, QColor> vcColors = {
        {"~b~", QColor(0, 0, 255, 255)},      // blue
        {"~g~", QColor(230, 136, 203, 255)},  // hot pink
        {"~h~", QColor(255, 255, 255, 255)},  // highlight (white)
        {"~p~", QColor(168, 110, 252, 255)},    // purple
        {"~r~", QColor(230, 136, 203, 255)},  // hot pink
        {"~w~", QColor(230, 230, 230, 255)},  // light gray - reset to default color
        {"~y~", QColor(255, 227, 79, 255)},    // yellow
        {"~o~", QColor(255, 192, 203, 255)},  // pink
        {"~q~", QColor(199, 144, 203, 255)},  // plum pink
        {"~x~", QColor(173, 216, 230, 255)},  // light blue
        {"~t~", QColor(86, 212, 146, 255)},      // green
        {"~l~", QColor(0, 0, 0, 255)},        // black text
    };

    static const QMap<QString, QColor> saColors = {
        {"~b~", QColor(0, 0, 255, 255)},      // blue
        {"~g~", QColor(0, 255, 0, 255)},      // green
        {"~h~", QColor(255, 255, 255, 255)},  // white (highlight)
        {"~p~", QColor(128, 0, 128, 255)},    // purple
        {"~r~", QColor(255, 0, 0, 255)},      // red
        {"~s~", QColor(255, 255, 255, 255)},  // reset color to standard (white)
        {"~w~", QColor(230, 230, 230, 255)},  // light gray - reset to default color
        {"~x~", QColor(0, 0, 255, 255)},      // cross icon (blue for cross)
        {"~y~", QColor(255, 255, 0, 255)},    // yellow
        {"~t~", QColor(0, 255, 0, 255)},      // triangle icon (green)
        {"~o~", QColor(255, 165, 0, 255)},    // circle icon (orange)
        {"~q~", QColor(255, 181, 197, 255)},  // square icon (plum pink)
        {"~l~", QColor(0, 0, 0, 255)},        // black text
    };

    static const QMap<QString, QColor> lcsColors = {
        {"~b~", QColor(0, 0, 255, 255)},      // blue
        {"~g~", QColor(0, 255, 0, 255)},      // green
        {"~h~", QColor(255, 255, 255, 255)},  // white (highlight)
        {"~p~", QColor(128, 0, 128, 255)},    // purple
        {"~r~", QColor(255, 0, 0, 255)},      // red
        {"~w~", QColor(230, 230, 230, 255)},  // light gray - reset to default color
        {"~x~", QColor(173, 216, 230, 255)},  // light blue
        {"~y~", QColor(255, 255, 0, 255)},    // yellow
        {"~o~", QColor(255, 192, 203, 255)},  // pink
        {"~q~", QColor(255, 181, 197, 255)},  // plum pink
        {"~l~", QColor(0, 0, 0, 255)},        // black text
    };

    static const QMap<QString, QColor> gta4Colors = {
        {"~b~", QColor(51, 105, 114, 255)},      // blue
        {"~g~", QColor(87, 124, 88, 255)},      // green
        {"~s~", QColor(255, 255, 255, 255)},  // white text
        {"~p~", QColor(152, 111, 158, 255)},    // purple
        {"~r~", QColor(138, 62, 62, 255)},      // red
        {"~w~", QColor(255, 255, 255, 255)},  // light gray - reset to default color
        {"~x~", QColor(173, 216, 230, 255)},  // light blue
        {"~y~", QColor(215, 197, 121, 255)},    // yellow
        {"~l~", QColor(0, 0, 0, 255)},        // black text
    };
    
    // 根据版本选择颜色映射
    switch (gtaVersion) {
        case 0: // GTA III
            return gta3Colors.value(token, QColor(230, 230, 230, 255)); // 默认浅灰色(90%不透明度)
        case 1: // Vice City
            return vcColors.value(token, QColor(230, 230, 230, 255)); // 默认浅灰色(90%不透明度)
        case 2: // San Andreas
            return saColors.value(token, QColor(230, 230, 230, 255)); // 默认浅灰色(90%不透明度)
        case 3: // Liberty City Stories
            return lcsColors.value(token, QColor(230, 230, 230, 255)); // 默认浅灰色(90%不透明度)
        case 4: // GTA IV
            return gta4Colors.value(token, QColor(230, 230, 230, 255)); // 默认浅灰色(90%不透明度)
        default:
            return QColor(230, 230, 230, 230); // 默认浅灰色90%不透明度
    }
}

TextRenderWidget::TextRenderWidget(QWidget* parent)
    : QWidget(parent)
    , m_text("")
    , m_mainFontId(-1)
    , m_mainFontFamily("")
    , m_mainFontLoaded(false)
    , m_outlineEnabled(true)
    , m_shadowEnabled(false)
    , m_fontSize(16)  // 进一步减小默认字体大小
    , m_gradientMiddleColor(128, 128, 128)  // 灰色
    , m_gradientEdgeColor(255, 255, 255)     // 白色
    , m_gtaVersion(2) // 默认设置为SA版本
    , m_dragStartPos(0, 0)
    , m_isDragging(false)
    , m_pngCacheValid(false)
    , m_cachedLineHeight(0)
    , m_cachedFontScale(1.0)
    , m_parseCacheValid(false)
{
    // 启用硬件加速和优化渲染
    setAttribute(Qt::WA_TranslucentBackground); // 启用透明背景
    setAttribute(Qt::WA_NoSystemBackground); // 禁用系统背景，手动绘制
    setAttribute(Qt::WA_NativeWindow); // 使用原生窗口，启用GPU硬件加速
    setMinimumHeight(40);  // 降低最小高度
    setMaximumHeight(100);  // 增加最大高度，支持更大字号
    setMinimumWidth(400);
    setAutoFillBackground(false);

    // 【性能优化】初始化PNG缓存更新定时器（延迟500ms更新）
    m_pngCacheUpdateTimer = new QTimer(this);
    m_pngCacheUpdateTimer->setSingleShot(true);
    m_pngCacheUpdateTimer->setInterval(500);  // 500ms延迟
    connect(m_pngCacheUpdateTimer, &QTimer::timeout, this, &TextRenderWidget::onPNGCacheUpdateTimeout);

    loadViceCityFont();
    setupUI();
}

TextRenderWidget::~TextRenderWidget()
{
}

void TextRenderWidget::setupUI()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);
    
    // 设置现代化灰色背景和圆角矩形样式
    setStyleSheet(R"(
        TextRenderWidget {
            background-color: #f0f0f0;
            border: 1px solid #d0d0d0;
            border-radius: 8px;
            padding: 2px;
        }
    )");
    
    // 启用右键菜单
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &TextRenderWidget::showContextMenu);

    // 启用拖拽
    setAcceptDrops(true);
}

void TextRenderWidget::showContextMenu(const QPoint& pos)
{
    QMenu contextMenu(this);
    
    // 设置现代化菜单样式
    contextMenu.setStyleSheet(R"(
        QMenu {
            background-color: #ffffff;
            border: 1px solid #cccccc;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 16px;
            border-radius: 4px;
            font-size: 12px;
            color: #333333;
            background-color: transparent;
        }
        QMenu::item:selected {
            background-color: #f0f0f0;
        }
        QMenu::separator {
            height: 1px;
            background: #e0e0e0;
            margin: 4px 0;
        }
        QMenu::indicator {
            background-color: transparent;
        }
        QMenu::left-arrow, QMenu::right-arrow {
            background-color: transparent;
        }
        QMenu::scroller {
            background-color: transparent;
        }
        QWidget {
            background-color: #ffffff;
        }
        QCheckBox {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 12px;
            color: #495057;
            padding: 4px 8px;
            background-color: transparent;
        }
        QLabel {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 12px;
            color: #495057;
            padding: 4px 8px;
            background-color: transparent;
        }
        QSpinBox {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 12px;
            border: 1px solid #ced4da;
            border-radius: 4px;
            padding: 3px 6px;
            background-color: white;
            min-width: 60px;
        }
        QSpinBox:focus {
            border: 1px solid #80bdff;
            outline: none;
        }
    )");
    
    // 创建自定义控件 - 更现代化的布局
    QWidgetAction* outlineAction = new QWidgetAction(&contextMenu);
    QCheckBox* outlineCheckBox = new QCheckBox("描边");
    outlineCheckBox->setChecked(m_outlineEnabled);
    connect(outlineCheckBox, &QCheckBox::stateChanged, this, &TextRenderWidget::onOutlineEnabledChanged);
    outlineAction->setDefaultWidget(outlineCheckBox);
    
    QWidgetAction* shadowAction = new QWidgetAction(&contextMenu);
    QCheckBox* shadowCheckBox = new QCheckBox("阴影");
    shadowCheckBox->setChecked(m_shadowEnabled);
    connect(shadowCheckBox, &QCheckBox::stateChanged, this, &TextRenderWidget::onShadowEnabledChanged);
    shadowAction->setDefaultWidget(shadowCheckBox);
    
    QWidgetAction* sizeAction = new QWidgetAction(&contextMenu);
    QWidget* sizeWidget = new QWidget();
    QHBoxLayout* sizeLayout = new QHBoxLayout(sizeWidget);
    sizeLayout->setContentsMargins(8, 2, 8, 2);
    sizeLayout->setSpacing(6);
    
    QLabel* sizeLabel = new QLabel("大小:");
    sizeLabel->setStyleSheet("font-family: 'Microsoft YaHei', 'Segoe UI'; font-size: 12px; color: #495057;");
    
    QSpinBox* sizeSpinBox = new QSpinBox();
    sizeSpinBox->setRange(8, 48);  // 扩大字体范围
    sizeSpinBox->setValue(m_fontSize);
    sizeSpinBox->setMinimumWidth(50);
    connect(sizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &TextRenderWidget::onFontSizeChanged);
    
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(sizeSpinBox);
    sizeAction->setDefaultWidget(sizeWidget);
    
    // 添加动作到菜单
    contextMenu.addAction(outlineAction);
    contextMenu.addAction(shadowAction);
    contextMenu.addSeparator();
    contextMenu.addAction(sizeAction);
    contextMenu.addSeparator();

    // 添加导出图片选项
    QAction* exportAction = contextMenu.addAction("导出图片");
    connect(exportAction, &QAction::triggered, this, &TextRenderWidget::exportToImage);

    // 显示菜单
    contextMenu.exec(mapToGlobal(pos));
}

void TextRenderWidget::loadViceCityFont()
{
    QFontDatabase db;
    QString mainFontPath;
    QString fallbackFontPath;

    // 根据GTA版本选择字体
    if (m_gtaVersion == 0) {
        // GTA III 使用 Arial Black 和微软雅黑粗体，全部斜体
        // GTA III 默认字号为15
        m_fontSize = 15;
            qDebug() << "GTA III version selected, loading fonts:";
        // 主字体直接使用系统Arial Black，斜体
            m_mainFont = QFont("Arial Black", m_fontSize);
        m_mainFont.setBold(true);
        m_mainFont.setItalic(true); // 斜体
        m_mainFontLoaded = true;
        m_mainFontFamily = "Arial Black";
        // 备用字体直接使用系统微软雅黑粗体，斜体
        m_fallbackFont = QFont("Microsoft YaHei", m_fontSize);
        m_fallbackFont.setBold(true);
        m_fallbackFont.setItalic(true); // 斜体
        return; // GTA III直接返回，不继续执行
    } else if (m_gtaVersion == 4) {
        // GTA IV 使用 WHM 使用 alte-din-1451-mittelschrift.regular.ttf 和微软雅黑粗体
        mainFontPath = QApplication::applicationDirPath() + "/font/alte-din-1451-mittelschrift.regular.ttf";
        fallbackFontPath.clear(); // 使用系统微软雅黑粗体
    } else if (m_gtaVersion == 2) {
        // GTA SA 使用 FuturaLT-Bold.ttf 和微软简综艺.ttf
        mainFontPath = QApplication::applicationDirPath() + "/font/FuturaLT-Bold.ttf";
        fallbackFontPath = QApplication::applicationDirPath() + "/font/微软简综艺.ttf";
    } else {
        // 其他版本使用 ViceCitySans.otf
        mainFontPath = QApplication::applicationDirPath() + "/font/ViceCitySans.otf";
        fallbackFontPath.clear(); // 使用系统默认中文字体
    }

    // 加载主字体
    m_mainFontId = db.addApplicationFont(mainFontPath);

    if (m_mainFontId != -1) {
        QStringList families = db.applicationFontFamilies(m_mainFontId);
        if (!families.isEmpty()) {
            m_mainFont.setFamily(families.first());
            m_mainFont.setPointSize(m_fontSize);
            m_mainFontLoaded = true;
            m_mainFontFamily = families.first(); // 保存字体族名称
            qDebug() << "Main font loaded successfully:" << families.first() << "from:" << mainFontPath;
        } else {
            m_mainFontLoaded = false;
        }
    } else {
        m_mainFontLoaded = false;
    }

    // 加载备用字体
    if (!fallbackFontPath.isEmpty()) {

        // 使用QFontDatabase加载字体
        int fallbackFontId = db.addApplicationFont(fallbackFontPath);

        if (fallbackFontId != -1) {
            QStringList families = db.applicationFontFamilies(fallbackFontId);

            // 尝试多个可能的字体族名称
            QStringList possibleNames = {
                "微软简综艺",
                "DFZongYi",
                "DFZongYi-Bd-80-Win-GB",
                "DFZongYi Bd",
                "DFZongYi-Bd",
                "ZongYi",
                "DF ZongYi"
            };

            bool fontMatched = false;
            QString usedFamily;

            // 优先使用QFontDatabase返回的字体族
            if (!families.isEmpty() && !families.first().isEmpty()) {
                usedFamily = families.first();
                m_fallbackFont = QFont(usedFamily, m_fontSize);
                QFontInfo info(m_fallbackFont);

                // 检查是否真正使用了该字体
                if (info.family() != "MS Sans Serif" && info.family() != "Arial") {
                    fontMatched = true;
                }
            }

            // 如果失败，尝试其他可能的名称
            if (!fontMatched) {

                // 获取系统所有字体族
                QStringList systemFamilies = db.families();

                for (const QString& testName : possibleNames) {

                    // 检查系统中是否存在这个字体
                    bool foundInSystem = false;
                    for (const QString& sysFamily : systemFamilies) {
                        if (sysFamily.compare(testName, Qt::CaseInsensitive) == 0) {
                            foundInSystem = true;
                            usedFamily = sysFamily;
                            break;
                        }
                    }

                    if (foundInSystem) {
                        m_fallbackFont = QFont(usedFamily, m_fontSize);
                        QFontInfo info(m_fallbackFont);

                        if (info.family() != "MS Sans Serif" && info.family() != "Arial") {
                            fontMatched = true;
                            break;
                        }
                    }
                }
            }

            // 如果所有方法都失败，回退到系统字体
            if (!fontMatched) {
                loadSystemFallbackFont(db);
            } else {
                // 验证最终字体
                QFontInfo fontInfo(m_fallbackFont);
            }
        } else {
            loadSystemFallbackFont(db);
        }
    } else {
        loadSystemFallbackFont(db);
    }

    // 最终验证
    QFontInfo finalFontInfo(m_fallbackFont);
}

void TextRenderWidget::setGtaVersion(int version)
{
    if (m_gtaVersion != version) {
        m_gtaVersion = version;
        loadViceCityFont();
        m_parseCacheValid = false;
        m_pngCacheValid = false;
        schedulePNGCacheUpdate();
        update();
    }
}

void TextRenderWidget::loadSystemFallbackFont(QFontDatabase& db)
{
    // 直接使用微软雅黑作为系统备用字体（确保使用粗体）

    QStringList families = db.families();
    QString yaHeiFamily;
    for (const QString& family : families) {
        if (family.contains("Microsoft YaHei", Qt::CaseInsensitive)) {
            yaHeiFamily = family;
            break;
        }
    }

    if (!yaHeiFamily.isEmpty()) {
        m_fallbackFont = QFont(yaHeiFamily, m_fontSize);
        // 微软雅黑使用粗体，以匹配艺术字体的外观
        m_fallbackFont.setBold(true);
        QFontInfo info(m_fallbackFont);
    } else {
        // 如果找不到微软雅黑，尝试其他中文字体（不包括宋体）
        qDebug() << "Microsoft YaHei not found, trying alternatives...";
        bool foundFallback = false;
        QStringList fallbackCandidates = {"SimHei", "Microsoft YaHei UI", "黑体", "PingFang SC", "Microsoft JhengHei"};
        for (const QString& candidate : fallbackCandidates) {
            if (families.contains(candidate, Qt::CaseInsensitive)) {
                m_fallbackFont = QFont(candidate, m_fontSize);
                m_fallbackFont.setBold(true); // 备选字体也使用粗体
                foundFallback = true;
                break;
            }
        }
        if (!foundFallback) {
            // 最后的备选：使用系统默认无衬线字体
            m_fallbackFont = QFont("Arial", m_fontSize);
            m_fallbackFont.setStyleHint(QFont::SansSerif);
            m_fallbackFont.setBold(true); // Arial也使用粗体
            qDebug() << "No Chinese font found, using Arial Bold";
        }
    }
}

bool TextRenderWidget::needsFallbackFont(const QChar& ch) const
{
    // 如果主字体未加载，总是使用备用字体
    if (!m_mainFontLoaded) {
        return true;
    }

    // GTA SA 使用 Twentieth-Century-Bold.ttf 和其他版本的 ViceCitySans.otf 都只支持ASCII字符
    // 对于非ASCII字符（unicode >= 256），使用备用字体（中文等）
    bool needsFallback = (ch.unicode() >= 256);
    if (needsFallback) {
        if (!s_fallbackLogged) {
            qDebug() << "Fallback font family:" << m_fallbackFont.family();
            s_fallbackLogged = true;
        }
    }
    return needsFallback;
}

void TextRenderWidget::setText(const QString& text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;

    // 【性能优化】标记缓存无效
    m_parseCacheValid = false;
    m_pngCacheValid = false;

    // 【性能优化】使用延迟PNG更新，避免频繁生成PNG
    schedulePNGCacheUpdate();

    // 直接触发重绘，不再显示预览标记
    update(); // 触发重绘
}

void TextRenderWidget::clear()
{
    setText("");
}

void TextRenderWidget::onOutlineEnabledChanged(int state)
{
    m_outlineEnabled = (state == Qt::Checked);
    m_pngCacheValid = false;
    schedulePNGCacheUpdate();
    update();
}

void TextRenderWidget::onShadowEnabledChanged(int state)
{
    m_shadowEnabled = (state == Qt::Checked);
    m_pngCacheValid = false;
    // 阴影不影响字符缓存，无需清除
    schedulePNGCacheUpdate();
    update();
}

void TextRenderWidget::onFontSizeChanged(int size)
{
    m_fontSize = size;
    m_mainFont.setPointSize(m_fontSize);
    m_fallbackFont.setPointSize(m_fontSize);
    m_parseCacheValid = false;
    m_pngCacheValid = false;
    schedulePNGCacheUpdate();
    update();
}

void TextRenderWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 绘制渐变背景
    drawGradientBackground(&painter);
    
    // 绘制文字
    if (!m_text.isEmpty()) {
        drawColoredText(&painter);
    }
}

void TextRenderWidget::drawGradientBackground(QPainter* painter)
{
    QRect rect = this->rect();
    
    // 背景完全透明
    painter->fillRect(rect, Qt::transparent);
    
    // 绘制圆角边框（半透明）
    painter->setPen(QPen(QColor(208, 208, 208, 128), 1));  // #d0d0d0 半透明
    painter->setBrush(Qt::NoBrush);
    
    // 使用抗锯齿绘制圆角矩形
    QPainterPath path;
    path.addRoundedRect(rect, 8, 8);  // 8像素圆角
    painter->drawPath(path);
}

// 判断是否为隐藏标记（不显示在文本中）
bool TextRenderWidget::isHiddenToken(const QString& token) {
    // 根据用户提供的表，这些标记不会显示在文本中
    static const QStringList hiddenTokens = {
        "~1~", "~k~", "~n~", "~z~", "~<~", "~>~", "~f~", 
        "~d~", "~u~", "~c~", "~j~", "~m~", "~v~", 
        "~a~", "~l~"  // ~a~ area text, ~l~ black text
    };
    
    return hiddenTokens.contains(token.toLower());
}

void TextRenderWidget::drawColoredText(QPainter* painter)
{
    if (m_text.isEmpty()) {
        return;
    }

    QRect rect = this->rect();
    QRect textRect = rect.adjusted(10, 6, -10, -6);

    if (!m_parseCacheValid) {
        parseTextFragments(m_text);
    }

    QFont currentFont = m_mainFontLoaded ? m_mainFont : m_fallbackFont;
    QFontMetrics fm(currentFont);
    int totalTextHeight = m_parsedLinesCache.size() * m_cachedLineHeight;

    int availableHeight = textRect.height();
    int maxLines = m_parsedLinesCache.size();
    if (totalTextHeight > availableHeight) {
        maxLines = availableHeight / m_cachedLineHeight;
        totalTextHeight = maxLines * m_cachedLineHeight;
    }

    int startY = textRect.top() + (availableHeight - totalTextHeight) / 2;

    QColor currentColor(230, 230, 230, 255);

    for (int i = 0; i < maxLines; ++i) {
        const LineFragments& line = m_parsedLinesCache[i];
        int xPos = textRect.left();
        int yPos = startY + i * m_cachedLineHeight;
        // 修复baseline计算：使用行高的一半加上ascent的一半，确保文字垂直居中
        int baseline = yPos + m_cachedLineHeight - fm.descent();

        for (const TextFragment& fragment : line.fragments) {
            if (fragment.isHiddenToken) {
                continue;
            }

            if (fragment.isColorToken) {
                currentColor = fragment.color;
            } else {
                QString textToDraw = fragment.text;
                
                int charPos = 0;
                while (charPos < textToDraw.length()) {
                    QFont charFont = currentFont;
                    int start = charPos;
                    
                    QChar firstChar = textToDraw[charPos];
                    bool useFallback = needsFallbackFont(firstChar);
                    if (useFallback) {
                        charFont = m_fallbackFont;
                    }
                    
                    while (charPos < textToDraw.length()) {
                        QChar ch = textToDraw[charPos];
                        if (needsFallbackFont(ch) != useFallback) {
                            break;
                        }
                        charPos++;
                    }
                    
                    QString segment = textToDraw.mid(start, charPos - start);
                    QFontMetrics fmChar(charFont);
                    
                    if (m_shadowEnabled) {
                        QColor shadowColor(0, 0, 0, 120);
                        painter->setFont(charFont);
                        painter->setPen(shadowColor);
                        painter->drawText(xPos + 2, baseline, segment);
                    }
                    
                    if (m_outlineEnabled) {
                        QPainterPath path;
                        path.addText(xPos, baseline, charFont, segment);
                        
                        QPen outlinePen(QColor(0, 0, 0, 255), 2.5);
                        outlinePen.setJoinStyle(Qt::RoundJoin);
                        outlinePen.setCapStyle(Qt::RoundCap);
                        
                        painter->strokePath(path, outlinePen);
                        painter->fillPath(path, currentColor);
                    } else {
                        painter->setFont(charFont);
                        painter->setPen(currentColor);
                        painter->drawText(xPos, baseline, segment);
                    }
                    
                    xPos += fmChar.horizontalAdvance(segment);
                }
            }
        }
    }
}

// 移除隐藏标记，保留可见文本
QString TextRenderWidget::removeHiddenTokens(const QString& input) {
    QString result = input;
    QRegularExpression colorRegex("~[a-zA-Z0-9]~");
    
    int pos = 0;
    QRegularExpressionMatch match;
    while ((match = colorRegex.match(result, pos)).hasMatch()) {
        QString token = match.captured(0).toLower();
        // 检查是否是隐藏标记
        if (isHiddenToken(token)) {
            // 移除隐藏标记
            result.remove(match.capturedStart(0), match.capturedLength(0));
            // 不增加pos，因为字符串长度已改变
            } else {
            // 非隐藏标记，继续移动位置
            pos = match.capturedEnd(0);
        }
    }
    
    return result;
}

// 分割文本和标记
QStringList TextRenderWidget::splitTextAndTokens(const QString& input) {
    QStringList parts;
    QRegularExpression colorRegex("~[a-zA-Z0-9]~");
    
    int lastPos = 0;
    int pos = 0;
    QRegularExpressionMatch match;
    while ((match = colorRegex.match(input, pos)).hasMatch()) {
        // 添加颜色标记前的文本
        if (match.capturedStart(0) > lastPos) {
            parts << input.mid(lastPos, match.capturedStart(0) - lastPos);
        }
        // 添加颜色标记
        parts << match.captured(0);
        pos = match.capturedEnd(0);
        lastPos = pos;
    }
    
    // 添加最后剩余的文本
    if (lastPos < input.length()) {
        parts << input.mid(lastPos);
    }
    
    return parts;
}

// 【性能优化】快速文本分割（状态机替代正则表达式，性能提升约3-5倍）
QStringList TextRenderWidget::splitTextAndTokensFast(const QString& input) {
    QStringList parts;
    int len = input.length();
    int i = 0;
    int lastPos = 0;
    
    while (i < len) {
        // 检测颜色标记格式 ~X~ （X为单个字符）
        if (input[i] == '~' && i + 2 < len && input[i + 2] == '~') {
            QChar midChar = input[i + 1];
            // 检查是否为有效标记字符（字母或数字）
            if (midChar.isLetterOrNumber()) {
                // 先添加标记前的文本
                if (i > lastPos) {
                    parts << input.mid(lastPos, i - lastPos);
                }
                // 添加标记
                parts << input.mid(i, 3);
                i += 3;
                lastPos = i;
                continue;
            }
        }
        i++;
    }
    
    // 添加最后剩余的文本
    if (lastPos < len) {
        parts << input.mid(lastPos);
    }
    
    return parts;
}

void TextRenderWidget::exportToImage()
{
    if (m_text.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有可导出的内容");
        return;
    }

    // 让用户选择保存位置
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "导出图片",
        QDir::homePath() + "/text_render.png",
        "PNG 图片 (*.png);;JPEG 图片 (*.jpg);;BMP 图片 (*.bmp)"
    );

    if (fileName.isEmpty()) {
        return;
    }

    // 【性能优化】停止延迟更新定时器（如果正在运行），立即更新PNG缓存
    if (m_pngCacheUpdateTimer->isActive()) {
        m_pngCacheUpdateTimer->stop();
    }

    // 确保PNG缓存已生成
    if (!m_pngCacheValid) {
        updateCachedPNG();
    }

    // 从缓存获取图片
    QImage image = QImage::fromData(m_cachedPNGData, "PNG");

    // 保存图片
    bool success = image.save(fileName);

    if (success) {
        QMessageBox::information(this, "成功", QString("图片已成功导出到:\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, "错误", "保存图片失败");
    }
}

void TextRenderWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
        m_isDragging = false;
    }
    QWidget::mousePressEvent(event);
}

void TextRenderWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        return;
    }

    if ((event->pos() - m_dragStartPos).manhattanLength() < 10) {
        return;
    }

    m_isDragging = true;

    // 创建拖拽对象
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData();

    // 【性能优化】停止延迟更新定时器（如果正在运行），立即更新PNG缓存
    if (m_pngCacheUpdateTimer->isActive()) {
        m_pngCacheUpdateTimer->stop();
    }

    // 确保PNG缓存已生成
    if (!m_pngCacheValid) {
        updateCachedPNG();
    }

    // 从缓存创建图片用于显示
    QImage image = QImage::fromData(m_cachedPNGData, "PNG");

    // 设置拖拽数据 - 同时提供图片和文件两种格式
    // 1. 图片数据格式 - 供QQ等应用程序使用
    mimeData->setData("image/png", m_cachedPNGData);
    mimeData->setImageData(image);

    // 2. 创建临时文件供桌面保存使用
    QString tempPath = QDir::tempPath() + "/gxt_text_render_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz") + ".png";
    bool saved = image.save(tempPath, "PNG");

    if (saved) {
        // 使用 setUrls() 设置文件列表 - 桌面会优先使用这个
        QList<QUrl> urls;
        urls << QUrl::fromLocalFile(tempPath);
        mimeData->setUrls(urls);
    }

    drag->setMimeData(mimeData);
    drag->setPixmap(QPixmap::fromImage(image));
    drag->setHotSpot(event->pos());

    // 执行拖拽 - 强制使用CopyAction
    Qt::DropAction result = drag->exec(Qt::CopyAction, Qt::CopyAction);

    // 清理临时文件（延迟删除，让接收方有时间复制）
    QTimer::singleShot(1000, [tempPath]() {
        QFile::remove(tempPath);
    });
}

void TextRenderWidget::dragEnterEvent(QDragEnterEvent* event)
{
    // 接受图片和文本类型的拖拽
    if (event->mimeData()->hasImage() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void TextRenderWidget::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasText()) {
        // 如果拖入的是文本，设置为当前文本
        QString droppedText = event->mimeData()->text();
        setText(droppedText);
        event->acceptProposedAction();
    } else if (event->mimeData()->hasImage()) {
        // 如果拖入的是图片，提取文本（如果可能）
        QImage image = qvariant_cast<QImage>(event->mimeData()->imageData());
        if (!image.isNull()) {
            // 这里可以添加OCR功能来从图片中提取文本
            // 目前暂不实现
        }
        event->acceptProposedAction();
    }
}

void TextRenderWidget::updateCachedPNG()
{
    if (!m_parseCacheValid) {
        parseTextFragments(m_text);
    }

    QFont currentFont = m_mainFontLoaded ? m_mainFont : m_fallbackFont;
    QFontMetrics fm(currentFont);

    int maxWidth = 0;
    int totalHeight = m_parsedLinesCache.size() * m_cachedLineHeight;

    for (const LineFragments& line : m_parsedLinesCache) {
        if (line.width > maxWidth) {
            maxWidth = line.width;
        }
    }

    const int margin = 8;
    QSize imageSize(maxWidth + margin * 2, totalHeight + margin * 2);

    if (maxWidth == 0 || totalHeight == 0) {
        imageSize = this->size();
    }

    QImage image(imageSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setFont(currentFont);

    if (!m_text.isEmpty()) {
        QColor currentColor(255, 255, 255, 255);
        int yPos = margin;

        for (const LineFragments& lineFrags : m_parsedLinesCache) {
            int xPos = margin;
            // 修复baseline计算：使用行高减去descent，确保文字垂直居中
            int baseline = yPos + m_cachedLineHeight - fm.descent();

            for (const TextFragment& fragment : lineFrags.fragments) {
                if (fragment.isHiddenToken) {
                    continue;
                }

                if (fragment.isColorToken) {
                    currentColor = fragment.color;
                } else {
                    QString textToDraw = fragment.text;
                    
                    int charPos = 0;
                    while (charPos < textToDraw.length()) {
                        QFont charFont = currentFont;
                        int start = charPos;
                        
                        QChar firstChar = textToDraw[charPos];
                        bool useFallback = needsFallbackFont(firstChar);
                        if (useFallback) {
                            charFont = m_fallbackFont;
                        }
                        
                        while (charPos < textToDraw.length()) {
                            QChar ch = textToDraw[charPos];
                            if (needsFallbackFont(ch) != useFallback) {
                                break;
                            }
                            charPos++;
                        }
                        
                        QString segment = textToDraw.mid(start, charPos - start);
                        QFontMetrics fmChar(charFont);
                        
                        if (m_shadowEnabled) {
                            QColor shadowColor(0, 0, 0, 120);
                            painter.setFont(charFont);
                            painter.setPen(shadowColor);
                            painter.drawText(xPos + 2, baseline, segment);
                        }
                        
                        if (m_outlineEnabled) {
                            QPainterPath path;
                            path.addText(xPos, baseline, charFont, segment);
                            
                            QPen outlinePen(QColor(0, 0, 0, 255), 2.5);
                            outlinePen.setJoinStyle(Qt::RoundJoin);
                            outlinePen.setCapStyle(Qt::RoundCap);
                            
                            painter.strokePath(path, outlinePen);
                            painter.fillPath(path, currentColor);
                        } else {
                            painter.setFont(charFont);
                            painter.setPen(currentColor);
                            painter.drawText(xPos, baseline, segment);
                        }
                        
                        xPos += fmChar.horizontalAdvance(segment);
                    }
                }
            }

            yPos += m_cachedLineHeight;
        }
    }

    painter.end();

    m_cachedPNGData.clear();
    QBuffer buffer(&m_cachedPNGData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    m_pngCacheValid = true;
}

// 【性能优化】解析文本片段
void TextRenderWidget::parseTextFragments(const QString& text)
{
    m_parsedLinesCache.clear();

    QFont currentFont = m_mainFontLoaded ? m_mainFont : m_fallbackFont;
    QFontMetrics fm(currentFont);
    QFontMetrics fm_fallback(m_fallbackFont);

    // 计算行高度和字体缩放比例
    m_cachedLineHeight = qMax(fm.height(), fm_fallback.height());
    if (m_mainFontLoaded) {
        m_cachedFontScale = static_cast<double>(fm.height()) / static_cast<double>(fm_fallback.height());
    } else {
        m_cachedFontScale = 1.0;
    }

    // 分割文本为行
    QStringList lines = text.split('\n');
    m_parsedLinesCache.reserve(lines.size());

    // 解析每一行
    for (const QString& lineStr : lines) {
        LineFragments lineFrags;
        lineFrags.width = 0;
        lineFrags.fragments.reserve(10);  // 预分配，减少重新分配

        QStringList parts = splitTextAndTokensFast(lineStr);  // 使用快速分割

        for (const QString& part : parts) {
            TextFragment fragment;

            if (part.startsWith("~") && part.endsWith("~") && part.length() == 3) {
                // 这是一个颜色标记
                QString token = part.toLower();
                fragment.isColorToken = true;
                fragment.isHiddenToken = isHiddenToken(token);
                fragment.color = getColorForToken(token, m_gtaVersion);
                fragment.text = part;
            } else {
                // 普通文本
                fragment.isColorToken = false;
                fragment.isHiddenToken = false;
                fragment.color = QColor(230, 230, 230, 255); // 默认浅灰色（~w~颜色）
                fragment.text = part;

                // 计算文本宽度（预计算）
                int textWidth = 0;
                for (const QChar& ch : part) {
                    QFont charFont = needsFallbackFont(ch) ? m_fallbackFont : currentFont;
                    QFontMetrics fm_char(charFont);
                    textWidth += fm_char.horizontalAdvance(ch);
                }
                lineFrags.width += textWidth;
            }

            lineFrags.fragments.append(fragment);
        }

        m_parsedLinesCache.append(lineFrags);
    }

    m_parseCacheValid = true;
}

// 【性能优化】触发延迟PNG缓存更新
void TextRenderWidget::schedulePNGCacheUpdate()
{
    // 如果已经有一个定时器在运行，重新启动它（取消之前的更新）
    if (m_pngCacheUpdateTimer->isActive()) {
        m_pngCacheUpdateTimer->stop();
    }
    // 启动定时器，延迟500ms更新
    m_pngCacheUpdateTimer->start();
}

// 【性能优化】延迟PNG缓存更新槽函数
void TextRenderWidget::onPNGCacheUpdateTimeout()
{
    // 如果PNG缓存无效，更新它
    if (!m_pngCacheValid) {
        updateCachedPNG();
    }
}