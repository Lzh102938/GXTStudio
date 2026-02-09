#pragma once

#include <QWidget>
#include <QPainter>
#include <QFont>
#include <QFontDatabase>
#include <QMenu>
#include <QWidgetAction>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QByteArray>

// 文本渲染窗格 - 用于仿真游戏字母的实际效果
class TextRenderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TextRenderWidget(QWidget* parent = nullptr);
    ~TextRenderWidget();

    // 设置要渲染的文本
    void setText(const QString& text);
    
    // 获取当前渲染的文本
    QString text() const { return m_text; }
    
    // 设置GTA版本
    void setGtaVersion(int version);
    int gtaVersion() const { return m_gtaVersion; }

    // 清空文本
    void clear();

private slots:
    void onOutlineEnabledChanged(int state);
    void onShadowEnabledChanged(int state);
    void onFontSizeChanged(int size);
    void showContextMenu(const QPoint& pos);
    void exportToImage();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupUI();
    void loadViceCityFont();
    void loadSystemFallbackFont(QFontDatabase& db);
    
    // 绘制渐变背景
    void drawGradientBackground(QPainter* painter);
    
    // 解析并绘制带颜色标记的文本
    void drawColoredText(QPainter* painter);
    
    // 判断是否为隐藏标记
    bool isHiddenToken(const QString& token);
    
    // 移除隐藏标记，保留可见文本
    QString removeHiddenTokens(const QString& input);
    
    // 分割文本和标记
    QStringList splitTextAndTokens(const QString& input);

    // 更新缓存的PNG图片
    void updateCachedPNG();

    // 检查字符是否需要使用备用字体
    bool needsFallbackFont(const QChar& ch) const;

    // 【性能优化】解析文本片段
    void parseTextFragments(const QString& text);

    // 【性能优化】触发延迟PNG缓存更新
    void schedulePNGCacheUpdate();

private slots:
    // 【性能优化】延迟PNG缓存更新槽函数
    void onPNGCacheUpdateTimeout();

private:
    // 【性能优化】文本片段结构
    struct TextFragment {
        QString text;        // 文本内容
        QColor color;        // 颜色
        bool isColorToken;   // 是否为颜色标记
        bool isHiddenToken;  // 是否为隐藏标记
    };

    // 【性能优化】行片段结构
    struct LineFragments {
        QVector<TextFragment> fragments;  // 行内的所有片段
        int width;                         // 行总宽度
    };

    QString m_text;

    // 字体
    QFont m_mainFont;        // 主字体（根据GTA版本变化）
    QFont m_fallbackFont;    // 备用字体（中文字体）
    int m_mainFontId;
    QString m_mainFontFamily;  // 主字体族名称
    bool m_mainFontLoaded;
    
    // 设置
    bool m_outlineEnabled;
    bool m_shadowEnabled;
    int m_fontSize;
    
    // 渐变设置
    QColor m_gradientMiddleColor;
    QColor m_gradientEdgeColor;
    
    // GTA版本（0=GTA III, 1=VC, 2=SA, 3=LCS, 4=GTA IV）
    int m_gtaVersion;

    // 拖拽相关
    QPoint m_dragStartPos;
    bool m_isDragging;

    // 缓存的PNG图片数据
    QByteArray m_cachedPNGData;
    bool m_pngCacheValid;

    // 【性能优化】文本解析缓存
    QVector<LineFragments> m_parsedLinesCache;  // 解析后的行片段缓存
    int m_cachedLineHeight;                       // 缓存的行高度
    double m_cachedFontScale;                    // 缓存的字体缩放比例
    bool m_parseCacheValid;                      // 解析缓存是否有效

    // 【性能优化】延迟PNG更新定时器
    QTimer* m_pngCacheUpdateTimer;  // PNG缓存更新定时器
};
