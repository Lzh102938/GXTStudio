#include "GXTStudio.h"
#include "TextRenderWidget.h"
#include "AboutDialog.h"
#include "MultiThreadProgressDialog.h"
#include "GXTTableModel.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QHeaderView>
#include <QInputDialog>
#include <QTextStream>
#include <QClipboard>
#include <QScrollBar>
#include <QSlider>
#include <QElapsedTimer>
#include <QStyleFactory>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDateTime>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QToolButton>
#include <QLabel>
#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QPainter>
#include <QMessageBox>
#include <QRegularExpression>
#include <QItemSelection>
#include <QStringConverter>
#include <windows.h>
#include <fstream>

// 简单的事件过滤器类，用于处理父容器resize事件
class ResizeEventFilter : public QObject {
public:
    explicit ResizeEventFilter(QObject* parent = nullptr) : QObject(parent), m_button(nullptr), m_listWidget(nullptr) {}

    void setButton(QToolButton* button) { m_button = button; }
    void setListWidget(QListWidget* listWidget) { m_listWidget = listWidget; }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Resize && m_button && m_listWidget) {
            QWidget* panel = qobject_cast<QWidget*>(obj);
            if (panel) {
                // 获取列表控件在父容器中的几何位置
                QRect listRect = m_listWidget->geometry();

                // 按钮右对齐列表控件右侧，下移到底部留白区域
                // 按钮大小为40x40，在底部45px的留白区域中，与右侧文本渲染控件中间对齐
                int buttonX = listRect.right() - 40;  // 右对齐列表控件右侧
                int buttonY = listRect.bottom() + 5;  // 下移到底部留白，与文本渲染控件中间对齐

                m_button->move(buttonX, buttonY);
            }
        }
        return false;
    }

private:
    QToolButton* m_button;
    QListWidget* m_listWidget;
};

// 字符串转换函数
std::string qstring_to_ansi(const QString& qstr) {
    if (qstr.isEmpty()) return "";

    std::wstring wstr = qstr.toStdWString();
    int size = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";

    std::string result(size, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), &result[0], size, nullptr, nullptr);
    return result;
}

// 标准化键（移除x、0x、0X前缀）
std::string normalizeKeyStdString(const std::string& key) {
    std::string normalized = key;
    // 移除0x或0X前缀
    if (normalized.length() >= 2 && (normalized.substr(0, 2) == "0x" || normalized.substr(0, 2) == "0X")) {
        normalized = normalized.substr(2);
    }
    // 移除x前缀（兼容旧格式）
    else if (normalized.length() >= 1 && normalized[0] == 'x') {
        normalized = normalized.substr(1);
    }
    return normalized;
}

GXTStudio::GXTStudio(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_statusLabel(nullptr)
    , m_menuBar(nullptr)
    , m_toolBar(nullptr)
    , m_statusBar(nullptr)
    , m_openAction(nullptr)
    , m_saveAction(nullptr)
    , m_saveAsAction(nullptr)
    , m_exportAction(nullptr)
    , m_closeAction(nullptr)
    , m_exitAction(nullptr)
    , m_findAction(nullptr)
    , m_replaceAction(nullptr)
    , m_readOnlyAction(nullptr)
    , m_increaseFontAction(nullptr)
    , m_decreaseFontAction(nullptr)
    , m_aboutAction(nullptr)
    , m_codeTableAction(nullptr)
    , m_smartTranslateAction(nullptr)
    , m_configTranslateAction(nullptr)
    , m_executeTranslateAction(nullptr)
    , m_setBackgroundAction(nullptr)
    , m_clearBackgroundAction(nullptr)
    , m_smartTranslator(nullptr)
    , m_smartTranslatorThread(nullptr)
    , m_translateProgressDialog(nullptr)
    , m_cancelOption(MultiThreadProgressDialog::NoCancel)
    , m_smartTranslateButton(nullptr)
    , m_codeTableButton(nullptr)
    , m_mountCodeTableAction(nullptr)
    , m_convertCodeTableAction(nullptr)
    , m_unmountCodeTableAction(nullptr)
    , m_codeTableConverter(nullptr)
    , m_currentTabIndex(-1)
    , m_isReadOnly(true)
    , m_fontSize(11)  // 表格字体大小
    , m_parseThread(nullptr)
    , m_parseWorker(nullptr)
    , m_saveThread(nullptr)
    , m_saveWorker(nullptr)
    , m_saveProgressBar(nullptr)
    , m_autoSaveButton(nullptr)
    , m_autoSaveTimer(nullptr)
    , m_autoSaveEnabled(false)
    , m_autoSaveFutureWatcher(nullptr)
    , m_cleanupTimer(nullptr)
    , m_resizeDebouncer(nullptr)
    , m_backgroundOpacity(1.0)  // 默认透明度100%（完全不透明）
    , m_backgroundEnabled(false)  // 默认禁用背景
    , m_backgroundAspectRatioMode(Qt::KeepAspectRatioByExpanding)  // 填充模式：保持比例填充，可能裁剪边缘
{
    ui.setupUi(this);
    
    // 设置全局字体为微软雅黑，启用字体平滑
    QFont globalFont = this->font();
    globalFont.setFamily("Microsoft YaHei");
    globalFont.setHintingPreference(QFont::PreferFullHinting);
    globalFont.setStyleStrategy(QFont::PreferAntialias);
    this->setFont(globalFont);
    
    // 修复黑色块闪烁的设置
    setAttribute(Qt::WA_OpaquePaintEvent, false); // 禁用不透明绘制
    setAttribute(Qt::WA_NoSystemBackground, false); // 启用系统背景
    setAttribute(Qt::WA_TranslucentBackground, false); // 禁用半透明背景
    setAttribute(Qt::WA_StyledBackground, true); // 启用样式背景
    
    // 启用双缓冲防止闪烁
    setAttribute(Qt::WA_PaintOnScreen, false); // 不在屏幕上直接绘制
    setAttribute(Qt::WA_DontCreateNativeAncestors, true); // 防止本地祖先干扰
    setAttribute(Qt::WA_DeleteOnClose, false); // 防止删除时闪烁
    
    // 设置自动填充背景
    setAutoFillBackground(true);
    
    // 启用鼠标跟踪减少重绘
    setMouseTracking(false); // 关闭鼠标跟踪减少重绘频率
    
    // 设置窗口图标和样式
    setWindowTitle("GXTStudio - GTAmod中文组");
    setMinimumSize(800, 600);
    
    // 检测屏幕分辨率并设置窗口尺寸
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QSize screenSize = screen->size();
        int screenWidth = screenSize.width();
        int screenHeight = screenSize.height();
        
        int windowWidth, windowHeight;
        
        if (screenWidth == 1366 && screenHeight == 768) {
            windowWidth = 960;
            windowHeight = 618;
        } else if (screenWidth == 1920 && screenHeight == 1080) {
            windowWidth = 1349;
            windowHeight = 868;
        } else if (screenWidth == 3840 && screenHeight == 2160) {
            windowWidth = 2698;
            windowHeight = 1736;
        } else {
            // 默认尺寸
            windowWidth = 1200;
            windowHeight = 800;
        }
        
        resize(windowWidth, windowHeight);
    } else {
        // 无法获取屏幕信息时使用默认尺寸
        resize(1200, 800);
    }
    
    // 设置接受拖拽
    setAcceptDrops(true);
    
    // 初始化UI
    setupUI();
    setupMenus();
    setupToolBars();
    setupStatusBar();
    setupCentralWidget();
    
    // 设置异步解析线程
    setupParseThread();
    
    // 设置异步保存线程
    setupSaveThread();
    
    // 设置智能翻译功能
    setupTranslationProgress();
    
    // 初始化码表转换器
    m_codeTableConverter = new CodeTableConverter(this);
    
    // 初始化版本感知保存系统
    initializeSaveStrategies();
    
    // 设置布局优化
    setupResizeOptimizations();
    
    connectSignals();
    
    // 预计算表格度量
    precomputeTableMetrics();
    
    // 加载设置
    QSettings settings;
    m_fontSize = settings.value("fontSize", 11).toInt();  // 表格默认字体大小
    m_isReadOnly = settings.value("readOnly", true).toBool();
    m_autoSaveEnabled = settings.value("autoSaveEnabled", false).toBool();  // 加载自动保存设置

    // 更新自动保存按钮状态
    if (m_autoSaveButton) {
        if (m_autoSaveEnabled) {
            m_autoSaveButton->setText(QString(QChar(0xf205)));  // 开启状态图标
            m_autoSaveButton->setToolTip("自动保存（开启）");
            // 启用状态使用蓝色 - 紧凑设计
            m_autoSaveButton->setStyleSheet(QString(R"(
                QToolButton {
                    border: none;
                    background: transparent;
                    color: #4a90e2;
                    padding: 0;
                    border-radius: 3px;
                }
                QToolButton:hover {
                    background: rgba(74,144,226,0.1);
                    color: #3a80d2;
                }
                QToolButton:pressed {
                    background: rgba(74,144,226,0.2);
                }
            )"));
        } else {
            m_autoSaveButton->setText(QString(QChar(0xf204)));  // 关闭状态图标
            m_autoSaveButton->setToolTip("自动保存（关闭）");
            // 关闭状态使用黑色 - 紧凑设计
            m_autoSaveButton->setStyleSheet(QString(R"(
                QToolButton {
                    border: none;
                    background: transparent;
                    color: #333333;
                    padding: 0;
                    border-radius: 3px;
                }
                QToolButton:hover {
                    background: rgba(0,0,0,0.05);
                    color: #1a73e8;
                }
                QToolButton:pressed {
                    background: rgba(0,0,0,0.1);
                }
            )"));
        }
    }

    // 更新状态
    updateActions();
    updateStatusBar();
    
    // 加载SATKEY映射
    GXTTableModel::loadSATKeyMap();
    
    // 加载IVTKEY映射
    GXTTableModel::loadIVTKeyMap();
    
    showLogMessage("GXTStudio 已启动");
}

GXTStudio::~GXTStudio()
{
    // 停止所有定时器，防止在对象销毁后触发
    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
    }
    if (m_resizeDebouncer) {
        m_resizeDebouncer->stop();
    }

    // 保存设置
    QSettings settings;
    settings.setValue("fontSize", m_fontSize);
    settings.setValue("readOnly", m_isReadOnly);

    // 停止翻译线程
    if (m_codeTableConverter) {
        delete m_codeTableConverter;
        m_codeTableConverter = nullptr;
    }
    
    if (m_smartTranslatorThread) {
        qWarning() << "开始停止翻译线程...";
        
        // 首先取消翻译 - 使用非阻塞方式避免死锁
        if (m_smartTranslator) {
            QMetaObject::invokeMethod(m_smartTranslator, "cancelTranslation", Qt::QueuedConnection);
            // 给一些时间让取消操作完成
            QThread::msleep(100);
        }
        
        // 更优雅的线程终止
        m_smartTranslatorThread->requestInterruption();
        m_smartTranslatorThread->quit();
        
        // 等待线程结束，但增加超时时间
        if (!m_smartTranslatorThread->wait(5000)) {
            qWarning() << "翻译线程未能在5秒内正常结束，强制终止";
            m_smartTranslatorThread->terminate();
            m_smartTranslatorThread->wait(2000);
        }
        
        delete m_smartTranslatorThread;
        m_smartTranslatorThread = nullptr;
        m_smartTranslator = nullptr;
        qWarning() << "翻译线程已完全停止";
    }
    
    // 停止解析线程
    if (m_parseThread) {
        m_parseThread->quit();
        m_parseThread->wait(3000);
        delete m_parseThread;
        m_parseThread = nullptr;
    }

    // 停止保存线程
    if (m_saveThread) {
        m_saveThread->quit();
        m_saveThread->wait(3000);
        delete m_saveThread;
        m_saveThread = nullptr;
        m_saveWorker = nullptr;
    }

    // 清理资源
    m_fileTabs.clear();
}

// 格式化键值显示 - 用于导出txt时显示明文或添加0x前缀
QString GXTStudio::formatKeyForDisplay(const QString& key)
{
    // 如果key是8位hex字符，添加0x前缀
    if (key.length() == 8) {
        bool allHex = true;
        for (const QChar& c : key) {
            if (!c.isDigit() && (c.toLower() < 'a' || c.toLower() > 'f')) {
                allHex = false;
                break;
            }
        }
        if (allHex) {
            return "0x" + key;
        }
    }
    // 否则直接返回key（明文）
    return key;
}

// UI辅助方法 - 优化样式代码复用
QString GXTStudio::getCardStyle(const QString& color) const
{
    return QString(R"(
        QWidget {
            background-color: white;
            border-radius: 10px;
            border: 1px solid #e0e0e0;
            padding: 15px;
        }
        QWidget:hover {
            border: 2px solid %1;
            padding: 14px;
        }
    )").arg(color);
}

QString GXTStudio::getButtonStyle(const QString& color, bool darker) const
{
    QString bgColor = darker ? QColor(color).darker(120).name() : color;
    QString pressedColor = darker ? QColor(color).darker(150).name() : bgColor;
    return QString(R"(
        QToolButton {
            background-color: %1;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 12px 16px;
            font-size: 14px;
            font-weight: bold;
            text-align: left;
        }
        QToolButton:hover {
            background-color: %2;
        }
        QToolButton:pressed {
            background-color: %3;
        }
    )").arg(color).arg(bgColor).arg(pressedColor);
}

QWidget* GXTStudio::createWelcomeTab()
{
    QWidget* defaultTab = new QWidget();
    defaultTab->setStyleSheet("background: transparent;");
    QVBoxLayout* mainLayout = new QVBoxLayout(defaultTab);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // 创建滚动区域
    QScrollArea* scrollArea = new QScrollArea(defaultTab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background: transparent;");
    scrollArea->viewport()->setStyleSheet("background: transparent;");
    
    // 防画面撕裂设置 - 优化绘制属性，避免背景重绘
    scrollArea->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, true);   // 启用不透明绘制，减少重绘区域
    scrollArea->viewport()->setAttribute(Qt::WA_NoSystemBackground, true); // 禁用系统背景绘制
    scrollArea->setAutoFillBackground(false);                              // 禁用自动填充背景，避免双重绘制
    scrollArea->viewport()->setAutoFillBackground(false);
    
    // 平滑滚动设置 - 适中的滚动步长
    QScrollBar* vScrollBar = scrollArea->verticalScrollBar();
    vScrollBar->setSingleStep(30);  // 适中的单步滚动距离，平衡平滑度和响应性
    vScrollBar->setPageStep(100);   // 设置页面滚动距离

    // 创建内容容器
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background: transparent;");
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(20);
    layout->setAlignment(Qt::AlignTop);
    
    // 标题区域
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel();
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setText(QString("<span style=\"font-family:'Font Awesome 7 Free Solid'; font-size:36px; vertical-align: middle;\">%1</span> <span style=\"font-size:36px; font-weight:bold; color:#2E86AB; vertical-align: middle;\">GXTStudio</span>").arg(QString(FA::QGamepadModern)));
    titleLabel->setTextFormat(Qt::RichText);
    titleLabel->setStyleSheet("margin: 10px 0;");
    titleLayout->addWidget(titleLabel);
    layout->addLayout(titleLayout);
    
    // 副标题
    QLabel* subtitleLabel = new QLabel("专业的 GTA 游戏文本文件编辑器", this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet("font-size: 18px; color: #555; margin-bottom: 20px; font-style: italic;");
    layout->addWidget(subtitleLabel);
    
    // 创建功能卡片区域
    QGridLayout* featuresGrid = new QGridLayout();
    featuresGrid->setSpacing(15);
    featuresGrid->setColumnStretch(0, 1);
    featuresGrid->setColumnStretch(1, 1);
    
    // 创建卡片的lambda函数
    auto createCard = [this](const QString& icon, const QString& title, const QString& content, const QString& color) {
        QWidget* card = new QWidget();
        card->setStyleSheet(getCardStyle(color));
        
        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(15, 15, 15, 15);
        
        // 图标和标题
        QHBoxLayout* headerLayout = new QHBoxLayout();
        QLabel* iconLabel = new QLabel();
        iconLabel->setText(QString("<span style=\"font-family:'Font Awesome 7 Free Solid'; font-size:18px;\">%1</span>").arg(icon));
        iconLabel->setTextFormat(Qt::RichText);
        iconLabel->setStyleSheet(QString("color: %1;").arg(color));
        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 16px;").arg(color));
        
        headerLayout->addWidget(iconLabel);
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        cardLayout->addLayout(headerLayout);
        
        // 内容
        QLabel* contentLabel = new QLabel(content);
        contentLabel->setWordWrap(true);
        contentLabel->setStyleSheet("color: #333; font-size: 14px; margin-top: 10px;");
        cardLayout->addWidget(contentLabel);
        
        return card;
    };
    
    // 功能卡片
    featuresGrid->addWidget(createCard(QString(FA::QWrench), "编辑功能", 
        "完整的文本编辑功能，支持添加、修改、删除条目，支持多格式GXT文件编辑", "#2E86AB"), 0, 0);
    featuresGrid->addWidget(createCard(QString(FA::QSearchMagnifyingGlass), "搜索过滤", 
        "强大的搜索和过滤功能，支持正则表达式，快速定位所需内容", "#27ae60"), 0, 1);
    featuresGrid->addWidget(createCard(QString(FA::QPalette), "界面设计", 
        "现代化界面设计，操作简单直观，支持字体大小调整和只读/编辑模式切换", "#e74c3c"), 1, 0);
    featuresGrid->addWidget(createCard(QString(FA::QBullseye), "性能优化", 
        "优化的性能表现，轻松处理大型文件，支持多线程操作", "#f39c12"), 1, 1);
    
    layout->addLayout(featuresGrid);
    
    // 快速操作标题
    auto createTitle = [](const QString& icon, const QString& text) {
        QLabel* label = new QLabel();
        label->setTextInteractionFlags(Qt::NoTextInteraction);
        label->setText(QString("<span style=\"font-family:'Font Awesome 7 Free Solid'; font-size:20px; vertical-align: middle;\">%1</span> <span style=\"font-size:20px; font-weight:bold; color:#333; vertical-align: middle;\">%2</span>").arg(icon).arg(text));
        label->setTextFormat(Qt::RichText);
        label->setStyleSheet("margin: 20px 0 10px 0;");
        return label;
    };
    
    layout->addWidget(createTitle(QString(FA::QBoltLightning), "快速操作"));
    
    // 操作按钮网格
    QGridLayout* actionsGrid = new QGridLayout();
    actionsGrid->setSpacing(10);
    
    // 创建操作按钮
    auto createActionButton = [this](const QString& text, const QString& icon, const QString& tooltip,
                                     void (GXTStudio::*slot)(), const QString& color) {
        QToolButton* button = new QToolButton();
        button->setStyleSheet(getButtonStyle(color, true));
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(16, 16));
        
        // 绘制图标
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setFont(FA::solidFont(16));
        painter.setPen(Qt::white);
        painter.drawText(QRect(0, 0, 16, 16), Qt::AlignCenter, icon);
        button->setIcon(QIcon(pixmap));
        
        button->setToolTip(tooltip);
        
        if (slot) {
            connect(button, &QToolButton::clicked, this, slot);
        } else {
            button->setEnabled(false);
            button->setToolTip("此功能正在开发中...");
        }
        
        return button;
    };
    
    actionsGrid->addWidget(createActionButton("打开文件", QString(FA::QFolderOpen), "打开GXT文件", &GXTStudio::openFile, "#3498db"), 0, 0);
    actionsGrid->addWidget(createActionButton("拖拽文件", QString(FA::QUpload), "拖拽文件到窗口", nullptr, "#9b59b6"), 0, 1);
    actionsGrid->addWidget(createActionButton("翻译配置", QString(FA::QCog), "配置智能翻译选项", &GXTStudio::onConfigTranslate, "#e67e22"), 1, 0);
    actionsGrid->addWidget(createActionButton("关于", QString(FA::QInfoCircle), "查看关于信息", &GXTStudio::showAbout, "#1abc9c"), 1, 1);
    
    layout->addLayout(actionsGrid);
    
    // 文件格式支持区域
    layout->addWidget(createTitle(QString(FA::QFileAlt), "支持的文件格式"));
    
    // 格式支持列表
    auto createFormatItem = [](const QString& format) {
        QLabel* label = new QLabel();
        label->setText(QString("<span style=\"font-family:'Font Awesome 7 Free Solid'; font-size:14px; vertical-align: middle; color:#666;\">%1</span> <span style=\"font-size:14px; color:#444; padding: 8px 12px; vertical-align: middle;\">%2</span>").arg(QString(FA::QGlobe)).arg(format));
        label->setTextFormat(Qt::RichText);
        label->setStyleSheet("padding: 8px 12px; background-color: #f8f9fa; border-radius: 6px; margin: 3px 0;");
        return label;
    };
    
    QStringList formats = {"GTA III, Vice City, San Andreas", "GTA IV", "GTA V (GXT2)", "WHM Files", "DAT Files"};
    for (const QString& format : formats) {
        layout->addWidget(createFormatItem(format));
    }
    
    // 快捷键区域
    layout->addWidget(createTitle(QString(FA::QKeyboard), "常用快捷键"));
    
    // 快捷键列表
    QTableWidget* shortcutTable = new QTableWidget();
    shortcutTable->setColumnCount(2);
    shortcutTable->setHorizontalHeaderLabels({"快捷键", "功能"});
    shortcutTable->horizontalHeader()->setStretchLastSection(true);
    shortcutTable->setStyleSheet(R"(
        QTableWidget {
            border: 1px solid #ddd;
            border-radius: 8px;
            gridline-color: #eee;
        }
        QTableWidget::item {
            padding: 8px;
        }
        QHeaderView::section {
            background-color: #2E86AB;
            color: white;
            padding: 8px;
            border: none;
        }
    )");
    
    QStringList shortcuts = {
        "Ctrl+O 打开文件",
        "Ctrl+S 保存文件", 
        "Ctrl+F 查找文本",
        "Ctrl+H 替换文本",
        "Ctrl+E 编辑单元格",
        "Ctrl++/- 调整字体大小",
        "F1 帮助",
        "F5 刷新"
    };
    
    shortcutTable->setRowCount(shortcuts.size());
    for (int i = 0; i < shortcuts.size(); ++i) {
        QStringList parts = shortcuts[i].split(" ", Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            QString key = parts[0] + " " + parts[1]; // e.g. "Ctrl+O"
            QStringList funcParts = parts.mid(2); // remaining parts
            QString func = funcParts.join(" ");
            
            QTableWidgetItem* keyItem = new QTableWidgetItem(key);
            keyItem->setTextAlignment(Qt::AlignCenter);
            keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
            
            QTableWidgetItem* funcItem = new QTableWidgetItem(func);
            funcItem->setFlags(funcItem->flags() & ~Qt::ItemIsEditable);
            
            shortcutTable->setItem(i, 0, keyItem);
            shortcutTable->setItem(i, 1, funcItem);
        }
    }
    
    shortcutTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    layout->addWidget(shortcutTable);
    
    // 统计信息区域
    QHBoxLayout* statsLayout = new QHBoxLayout();
    
    auto createStatBox = [](const QString& icon, const QString& title, const QString& value, const QString& color = "#3498db") {
        QWidget* box = new QWidget();
        box->setStyleSheet(QString(R"(
            QWidget {
                background-color: %1;
                color: white;
                border-radius: 8px;
                padding: 12px;
            }
        )").arg(color));
        
        QVBoxLayout* boxLayout = new QVBoxLayout(box);
        boxLayout->setAlignment(Qt::AlignCenter);
        
        QLabel* iconLabel = new QLabel();
        iconLabel->setText(QString("<span style=\"font-family:'Font Awesome 7 Free Solid'; font-size:20px;\">%1</span>").arg(icon));
        iconLabel->setTextFormat(Qt::RichText); // 确保使用富文本格式
        iconLabel->setAlignment(Qt::AlignCenter);
        
        QLabel* valueLabel = new QLabel(value);
        valueLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
        valueLabel->setAlignment(Qt::AlignCenter);
        
        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-size: 12px;");
        titleLabel->setAlignment(Qt::AlignCenter);
        
        boxLayout->addWidget(iconLabel);
        boxLayout->addWidget(valueLabel);
        boxLayout->addWidget(titleLabel);
        
        return box;
    };
    
    statsLayout->addWidget(createStatBox(QString(FA::QFileCode), "格式", "5种", "#3498db"));
    statsLayout->addWidget(createStatBox(QString(FA::QTable), "表格", "无限", "#2ecc71"));
    statsLayout->addWidget(createStatBox(QString(FA::QDatabase), "条目", "无限制", "#9b59b6"));
    statsLayout->addWidget(createStatBox(QString(FA::QLanguage), "语言", "多国", "#f39c12"));
    
    layout->addLayout(statsLayout);
    
    // 底部信息
    QFrame* lineBottom = new QFrame();
    lineBottom->setFrameShape(QFrame::HLine);
    lineBottom->setFrameShadow(QFrame::Sunken);
    lineBottom->setStyleSheet("background-color: #ddd; max-height: 1px; margin: 10px 0;");
    layout->addWidget(lineBottom);
    
    QLabel* footerLabel = new QLabel();
    footerLabel->setText(QString(R"(
        <div style="text-align: center; padding: 15px; background-color: #f0f0f0; border-radius: 8px;">
            <p><strong><span style="font-family:'Font Awesome 7 Free Solid';">%1</span> 开发团队：</strong>GTAmod中文组</p>
            <p><strong><span style="font-family:'Font Awesome 7 Free Solid';">%2</span> 技术支持：</strong>基于 Qt 框架开发</p>
            <p><strong><span style="font-family:'Font Awesome 7 Free Solid';>%3</span> 版本：</strong>v1.0 | 持续更新中...</p>
        </div>
    )").arg(QString(FA::QBuilding)).arg(QString(FA::QCodeBranch)).arg(QString(FA::QTag)));
    footerLabel->setTextFormat(Qt::RichText);
    layout->addWidget(footerLabel);
    
    // 添加弹性空间
    layout->addStretch();
    
    // 设置滚动区域
    scrollArea->setWidget(contentWidget);
    scrollArea->setWidgetResizable(true);
    mainLayout->addWidget(scrollArea);
    
    return defaultTab;
}

void GXTStudio::setupUI()
{
    // 创建标签页容器
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    
    // 添加欢迎标签页
    m_tabWidget->addTab(createWelcomeTab(), "欢迎使用");
    setCentralWidget(m_tabWidget);
}

void GXTStudio::setupMenus()
{
    // 菜单已通过UI文件创建，这里获取指针
    m_menuBar = ui.menuBar;

    // 获取动作指针
    m_openAction = ui.actionOpen;
    m_saveAction = ui.actionSave;
    m_saveAsAction = ui.actionSaveAs;
    m_exportAction = ui.actionExport;
    m_closeAction = ui.actionClose;
    m_exitAction = ui.actionExit;
    m_findAction = ui.actionFind;
    m_replaceAction = ui.actionReplace;
    m_readOnlyAction = ui.actionReadOnly;
    m_increaseFontAction = ui.actionIncreaseFont;
    m_decreaseFontAction = ui.actionDecreaseFont;
    m_aboutAction = ui.actionAbout;
    m_codeTableAction = ui.actionCodeTable;
    m_smartTranslateAction = ui.actionSmartTranslate;

    // 设置只读模式初始状态
    m_readOnlyAction->setChecked(m_isReadOnly);

    // 添加背景设置菜单到视图菜单
    if (ui.menuView) {
        ui.menuView->addSeparator();
        m_setBackgroundAction = new QAction("设置背景图片...", this);
        m_clearBackgroundAction = new QAction("清除背景", this);
        m_clearBackgroundAction->setEnabled(false);  // 初始状态禁用
        ui.menuView->addAction(m_setBackgroundAction);
        ui.menuView->addAction(m_clearBackgroundAction);
    }
}

void GXTStudio::setupToolBars()
{
    m_toolBar = ui.mainToolBar;
    
    // 添加常用工具栏按钮
    m_toolBar->addAction(m_openAction);
    m_toolBar->addAction(m_saveAction);
    m_toolBar->addAction(m_saveAsAction);
    m_toolBar->addSeparator();

    // 创建码表转换按钮和下拉菜单
    m_codeTableButton = new QToolButton(this);
    m_codeTableButton->setText("码表转换(&C)");
    m_codeTableButton->setToolTip("码表转换功能");
    m_codeTableButton->setPopupMode(QToolButton::MenuButtonPopup);
    
    // 创建下拉菜单
    QMenu* codeTableMenu = new QMenu(this);
    
    // 挂载码表动作
    m_mountCodeTableAction = new QAction("挂载码表(&M)", this);
    m_mountCodeTableAction->setToolTip("选择并挂载码表文件");
    connect(m_mountCodeTableAction, &QAction::triggered, this, &GXTStudio::onMountCodeTable);
    codeTableMenu->addAction(m_mountCodeTableAction);
    
    // 执行转换动作
    m_convertCodeTableAction = new QAction("执行转换(&C)", this);
    m_convertCodeTableAction->setToolTip("执行字符转换");
    m_convertCodeTableAction->setEnabled(false); // 初始禁用，需要先挂载码表
    connect(m_convertCodeTableAction, &QAction::triggered, this, &GXTStudio::onConvertCodeTable);
    codeTableMenu->addAction(m_convertCodeTableAction);
    
    // 卸载码表动作
    m_unmountCodeTableAction = new QAction("卸载码表(&U)", this);
    m_unmountCodeTableAction->setToolTip("卸载当前挂载的码表");
    m_unmountCodeTableAction->setEnabled(false); // 初始禁用
    connect(m_unmountCodeTableAction, &QAction::triggered, this, &GXTStudio::onUnmountCodeTable);
    codeTableMenu->addAction(m_unmountCodeTableAction);
    
    m_codeTableButton->setMenu(codeTableMenu);

    // 初始化时连接到挂载码表（因为刚创建时肯定没有挂载码表）
    connect(m_codeTableButton, &QToolButton::clicked, this, &GXTStudio::onMountCodeTable);
    updateCodeTableButtonText(); // 初始化按钮文本

    // 根据只读模式设置按钮状态
    m_codeTableButton->setEnabled(!m_isReadOnly);

    m_toolBar->addWidget(m_codeTableButton);
    m_toolBar->addSeparator();
    
    // 创建智能翻译按钮和下拉菜单
    m_smartTranslateButton = new QToolButton(this);
    m_smartTranslateButton->setText("智能翻译(&T)");
    m_smartTranslateButton->setToolTip("智能翻译功能");
    m_smartTranslateButton->setPopupMode(QToolButton::MenuButtonPopup);
    
    // 创建下拉菜单
    QMenu* translateMenu = new QMenu(this);
    
    // 配置密钥动作
    m_configTranslateAction = new QAction("配置密钥(&C)", this);
    m_configTranslateAction->setToolTip("配置翻译API密钥和提示词");
    connect(m_configTranslateAction, &QAction::triggered, this, &GXTStudio::onConfigTranslate);
    translateMenu->addAction(m_configTranslateAction);
    
    // 执行翻译动作
    m_executeTranslateAction = new QAction("执行翻译(&T)", this);
    m_executeTranslateAction->setToolTip("执行智能翻译");
    m_executeTranslateAction->setEnabled(false); // 初始禁用，需要先配置密钥
    connect(m_executeTranslateAction, &QAction::triggered, this, &GXTStudio::onExecuteTranslate);
    translateMenu->addAction(m_executeTranslateAction);
    
    m_smartTranslateButton->setMenu(translateMenu);
    
    // 检查是否已配置API密钥来决定默认动作
    QSettings settings;
    QString apiKey = settings.value("Translate/apiKey", "").toString();
    if (apiKey.trimmed().isEmpty()) {
        // 未配置密钥，默认动作是配置密钥
        connect(m_smartTranslateButton, &QToolButton::clicked, this, &GXTStudio::onConfigTranslate);
    } else {
        // 已配置密钥，默认动作是执行翻译
        m_executeTranslateAction->setEnabled(true);
        connect(m_smartTranslateButton, &QToolButton::clicked, this, &GXTStudio::onExecuteTranslate);
    }
    
    m_toolBar->addWidget(m_smartTranslateButton);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_readOnlyAction);
}

void GXTStudio::setupStatusBar()
{
    m_statusBar = ui.statusBar;

    // 创建状态标签，确保支持中文显示
    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setTextFormat(Qt::PlainText);
    QFont font = m_statusLabel->font();
    font.setFamily("Microsoft YaHei, SimSun, Arial"); // 优先使用中文字体
    font.setPointSize(12);
    m_statusLabel->setFont(font);

    // 创建保存进度条（位于状态栏中间，用于保存和自动保存）
    m_saveProgressBar = new QProgressBar(this);
    m_saveProgressBar->setRange(0, 100);
    m_saveProgressBar->setValue(0);
    m_saveProgressBar->setTextVisible(true);
    m_saveProgressBar->setMaximumWidth(250);
    m_saveProgressBar->setMinimumWidth(200);
    m_saveProgressBar->setMinimumHeight(22);
    m_saveProgressBar->setMaximumHeight(22);
    
    // 美化样式 - 使用更现代的扁平设计
    m_saveProgressBar->setStyleSheet(
        "QProgressBar {"
        "  border: none;"
        "  border-radius: 3px;"
        "  background-color: #e8e8e8;"
        "  text-align: center;"
        "  font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;"
        "  font-size: 12px;"
        "  color: #000000;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #4a90e2;"
        "  border-radius: 3px;"
        "}"
    );
    m_saveProgressBar->hide();  // 默认隐藏

    // 统一字体
    QFont smallFont = font;
    smallFont.setPointSize(12);

    // ========== 自动保存区域 ==========
    // 创建自动保存按钮 - 使用更紧凑的设计
    m_autoSaveButton = new QToolButton(this);
    m_autoSaveButton->setText(QString(QChar(0xf204)));  // 关闭状态图标 U+f204
    m_autoSaveButton->setFont(FA::solidFont(14));
    m_autoSaveButton->setToolTip("自动保存（关闭）");
    m_autoSaveButton->setFixedSize(26, 26);
    m_autoSaveButton->setStyleSheet(QString(R"(
        QToolButton {
            border: none;
            background: transparent;
            color: #333333;
            padding: 0;
            border-radius: 3px;
        }
        QToolButton:hover {
            background: rgba(0,0,0,0.05);
            color: #1a73e8;
        }
        QToolButton:pressed {
            background: rgba(0,0,0,0.1);
        }
    )"));
    connect(m_autoSaveButton, &QToolButton::clicked, this, &GXTStudio::onAutoSaveToggle);

    // 创建版本标签
    QLabel* versionLabel = new QLabel("v1.0", this);
    versionLabel->setFont(smallFont);
    versionLabel->setStyleSheet("color: #000000; margin-left: 8px;");

    // 设置状态栏样式 - 统一背景，消除分割线
    m_statusBar->setStyleSheet(R"(
        QStatusBar {
            background: #f8f9fa;
            border-top: 1px solid #e0e0e0;
        }
        QStatusBar::item {
            border: none;
        }
    )");
    m_statusBar->setContentsMargins(8, 0, 8, 0);

    // 左侧容器：状态标签
    QWidget* leftContainer = new QWidget(this);
    leftContainer->setStyleSheet("background: transparent;");
    QHBoxLayout* leftLayout = new QHBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    m_statusLabel->setStyleSheet("color: #000000; font-size: 12px; padding: 0;");
    leftLayout->addWidget(m_statusLabel);
    leftLayout->addStretch();
    
    // 中间容器：保存进度条
    QWidget* centerContainer = new QWidget(this);
    centerContainer->setStyleSheet("background: transparent;");
    QHBoxLayout* centerLayout = new QHBoxLayout(centerContainer);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);
    centerLayout->addStretch();
    centerLayout->addWidget(m_saveProgressBar);
    centerLayout->addStretch();
    
    // 右侧容器：自动保存按钮 + 版本号
    QWidget* rightContainer = new QWidget(this);
    rightContainer->setStyleSheet("background: transparent;");
    QHBoxLayout* rightLayout = new QHBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);
    rightLayout->addStretch();
    rightLayout->addWidget(m_autoSaveButton);
    rightLayout->addWidget(versionLabel);
    
    // 添加到状态栏
    m_statusBar->addWidget(leftContainer, 1);    // 左侧可伸缩
    m_statusBar->addWidget(centerContainer, 0);  // 中间固定
    m_statusBar->addWidget(rightContainer, 0);   // 右侧固定

    // 创建自动保存定时器
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(AUTOSAVE_DELAY);  // 5秒延迟
    connect(m_autoSaveTimer, &QTimer::timeout, this, &GXTStudio::onAutoSaveTimer);
}

void GXTStudio::setupCentralWidget()
{
    // 主界面已通过setupUI设置
}

void GXTStudio::keyPressEvent(QKeyEvent* event)
{
    // 处理Ctrl+C快捷键复制
    if (event->modifiers() & Qt::ControlModifier && event->key() == Qt::Key_C) {
        FileTab* currentTab = getCurrentTab();
        if (currentTab && currentTab->entryTableView) {
            // 检查是否有选中的项目
            QModelIndexList selectedIndexes = currentTab->entryTableView->selectionModel()->selectedIndexes();
            if (!selectedIndexes.isEmpty()) {
                // 分析选中的列
                QSet<int> selectedColumns;
                for (const QModelIndex& index : selectedIndexes) {
                    selectedColumns.insert(index.column());
                }
                
                // 根据选中的列决定复制方式
                if (selectedColumns.size() == 1) {
                    if (selectedColumns.contains(0)) {
                        // 只选中了键列
                        copySelectedKeys();
                    } else {
                        // 只选中了值列
                        copySelectedValues();
                    }
                } else {
                    // 选中了多列，复制键值对
                    copySelectedKeyValues();
                }
                return; // 已处理，不调用父类方法
            }
        }
    }
    
    // 处理Ctrl+E快捷键编辑
    if (event->modifiers() & Qt::ControlModifier && event->key() == Qt::Key_E) {
        if (!m_isReadOnly) {
            editCellAsDialog();
            return; // 已处理，不调用父类方法
        }
    }
    
    // 其他按键交给父类处理
    QMainWindow::keyPressEvent(event);
}

void GXTStudio::connectSignals()
{
    // 菜单动作连接
    connect(m_openAction, &QAction::triggered, this, &GXTStudio::openFile);
    connect(m_saveAction, &QAction::triggered, this, &GXTStudio::saveFile);
    connect(m_saveAsAction, &QAction::triggered, this, &GXTStudio::saveAsFile);
    connect(m_exportAction, &QAction::triggered, this, &GXTStudio::exportFile);
    connect(m_closeAction, &QAction::triggered, this, &GXTStudio::closeFile);
    connect(m_exitAction, &QAction::triggered, this, &GXTStudio::exitApp);
    
    connect(m_findAction, &QAction::triggered, this, &GXTStudio::findText);
    connect(m_replaceAction, &QAction::triggered, this, &GXTStudio::replaceText);
    connect(m_codeTableAction, &QAction::triggered, this, &GXTStudio::onCodeTableConvert);
    connect(m_smartTranslateAction, &QAction::triggered, this, &GXTStudio::onSmartTranslate);
    connect(m_readOnlyAction, &QAction::toggled, this, &GXTStudio::toggleReadOnly);
    
    connect(m_increaseFontAction, &QAction::triggered, this, &GXTStudio::increaseFont);
    connect(m_decreaseFontAction, &QAction::triggered, this, &GXTStudio::decreaseFont);
    connect(m_aboutAction, &QAction::triggered, this, &GXTStudio::showAbout);

    // 背景设置菜单连接
    if (m_setBackgroundAction) {
        connect(m_setBackgroundAction, &QAction::triggered, this, &GXTStudio::onSetBackground);
    }
    if (m_clearBackgroundAction) {
        connect(m_clearBackgroundAction, &QAction::triggered, this, &GXTStudio::onClearBackground);
    }

    // 标签页信号连接
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &GXTStudio::onTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &GXTStudio::closeTab);
    
    // 【关键修复】QTabWidget 没有 tabMoved 信号，需要通过 QTabBar 监听
    connect(m_tabWidget->tabBar(), &QTabBar::tabMoved, this, &GXTStudio::onTabMoved);
    
    // 简化内存管理 - Qt自动处理大部分内存
}

void GXTStudio::setupParseThread()
{
    showLogMessage("正在设置解析线程...");

    // 创建解析线程和工作对象
    m_parseThread = new QThread(this);
    m_parseWorker = new ParseWorker();

    showLogMessage("解析线程和工作对象已创建");

    // 移动工作对象到子线程
    m_parseWorker->moveToThread(m_parseThread);

    // 连接信号槽
    connect(m_parseWorker, &ParseWorker::parseCompleted,
            this, &GXTStudio::onParseCompleted, Qt::QueuedConnection);

    showLogMessage("信号槽连接完成");

    // 启动线程
    m_parseThread->start();

    showLogMessage("解析线程已启动");
}

void GXTStudio::setupSaveThread()
{
    // 创建保存线程和工作对象
    m_saveThread = new QThread(this);
    m_saveWorker = new SaveWorker(this);  // 传入所有者指针

    // 移动工作对象到子线程
    m_saveWorker->moveToThread(m_saveThread);

    // 注意：信号槽连接在每次保存时动态设置，避免自动保存和手动保存互相干扰

    // 启动线程
    m_saveThread->start();
}

// 性能优化方法已简化

void GXTStudio::setupResizeOptimizations()
{
    // 创建防抖定时器 - 增加延迟以大幅减少计算频率
    m_resizeDebouncer = new QTimer(this);
    m_resizeDebouncer->setSingleShot(true);
    m_resizeDebouncer->setInterval(400); // 增加到400ms，大幅减少触发频率
    connect(m_resizeDebouncer, &QTimer::timeout, this, &GXTStudio::performDelayedResize);
    m_lastWindowSize = size();
    
    // 创建缓存清理定时器
    m_cleanupTimer = new QTimer(this);
    m_cleanupTimer->setInterval(60000); // 60秒清理一次，减少频率
    connect(m_cleanupTimer, &QTimer::timeout, this, &GXTStudio::cleanupDelegatesCache);
    m_cleanupTimer->start();
}

// 极致内存优化的缓存管理
void GXTStudio::cleanupListCache(FileTab* tab)
{
    if (!tab) return;
    
    // 释放缓存内存
    tab->listCache.cachedDisplayTexts.clear();
    tab->listCache.cachedTableNames.clear();
    tab->listCache.cachedCounts.clear();
    tab->listCache.cachedMainFlags.clear();
    tab->listCache.cachedDisplayTexts.shrink_to_fit();
    tab->listCache.cachedTableNames.shrink_to_fit();
    tab->listCache.cachedCounts.shrink_to_fit();
    tab->listCache.cachedMainFlags.shrink_to_fit();
    
    tab->listCache.lastTableCount = 0;
    tab->listCache.needsRebuild = true;
}

// 缓存预热方法已删除

void GXTStudio::performDelayedResize()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableView) return;
    
    // 只有在真正需要时才调整列宽
    if (!currentTab->cache.layoutNeedsUpdate) return;
    
    QHeaderView* header = currentTab->entryTableView->horizontalHeader();
    if (!header) return;
    
    // 缓存当前宽度，避免重复计算
    static int lastTotalWidth = -1;
    int totalWidth = currentTab->entryTableView->viewport()->width();
    
    // 避免对相同宽度重复计算
    if (abs(lastTotalWidth - totalWidth) < 10) { // 增加阈值到10px
        currentTab->cache.layoutNeedsUpdate = false;
        return;
    }
    
    if (totalWidth > 0) {
        // 使用辅助函数计算键列宽度：始终保持显示10个字符的宽度
        int keyWidth = calculateKeyColumnWidth();
        
        int valueWidth = totalWidth - keyWidth - 2;
        if (valueWidth < 300) {
            valueWidth = 300;
            keyWidth = totalWidth - valueWidth - 2;
        }
        
        // 批量调整列宽，减少重绘次数
        header->blockSignals(true);
        header->resizeSection(0, keyWidth);
        header->resizeSection(1, valueWidth);
        header->blockSignals(false);
        
        lastTotalWidth = totalWidth;
    }
    
    currentTab->cache.layoutNeedsUpdate = false;
}

void GXTStudio::cleanupDelegatesCache()
{
    // 遍历所有标签页的委托并清理缓存
    int totalCleared = 0;
    for (auto& tab : m_fileTabs) {
        if (tab.entryTableView) {
            auto* delegate = qobject_cast<OptimizedItemDelegate*>(tab.entryTableView->itemDelegate());
            if (delegate) {
                // 获取清理前的缓存大小
                int cacheSize = delegate->getCacheSize();
                delegate->clearCache();
                totalCleared += cacheSize;
                
                // 强制垃圾回收
                if (delegate->getCacheSize() == 0) {
                    tab.entryTableView->viewport()->update();
                }
            }
        }
        
        // 同时清理列表缓存
        cleanupListCache(&tab);
    }
    
    // 如果清理了大量缓存，触发系统垃圾回收
    if (totalCleared > 500) {
        // 触发Qt的事件循环清理
        QCoreApplication::processEvents();
    }
}

// 预加载方法已删除 - Qt模型视图架构已优化

// preloadVisibleArea 函数已定义在前面，删除重复定义

// 文件操作槽函数
void GXTStudio::openFile()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
        "打开GXT文件", "", 
        "GXT文件 (*.gxt *.gxt2 *.dat *.whm);;所有文件 (*.*)");
    
    if (!fileNames.isEmpty()) {
        for (const QString& fileName : fileNames) {
            startAsyncParse(fileName);
        }
    }
}

void GXTStudio::saveFile()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) {
        return;
    }
    
    if (currentTab->filePath.isEmpty()) {
        saveAsFile();
        return;
    }
    
    // 如果是字符表文件，直接保存
    if (currentTab->isCharTable && currentTab->charTableWidget) {
        const CharTableData& data = currentTab->charTableWidget->data();
        bool success = false;

        if (data.type == CharTableData::GTA_VC) {
            success = CharTableParser::saveGtaVc(currentTab->filePath, data);
        } else if (data.type == CharTableData::GTA_IV) {
            success = CharTableParser::saveGtaIv(currentTab->filePath, data);
        }

        if (success) {
            currentTab->isModified = false;
            showLogMessage("字符表已保存: " + currentTab->filePath);
            QMessageBox::information(this, "保存成功", "字符表已成功保存！");
        } else {
            QMessageBox::critical(this, "保存失败", "无法保存字符表文件");
        }
        return;
    }
    
    // 使用 QtConcurrent 完全异步保存（包括数据复制）
    // 捕获所有需要的数据，不传递指针
    QString filePath = currentTab->filePath;
    QString fileName = currentTab->fileName;
    GXTVersion version = currentTab->version;
    bool isWHM = currentTab->isWHM;
    bool isDAT = currentTab->isDAT;
    bool isWHMRSC = currentTab->isWHMRSC;
    
    // 使用状态栏进度条
    m_saveProgressBar->setFormat("正在保存 %p%");
    m_saveProgressBar->show();
    m_saveProgressBar->setValue(10);
    
    showLogMessage("正在后台保存: " + fileName);
    
    // 在后台线程中执行数据复制和保存
        QFuture<SaveResult> future = QtConcurrent::run([this, filePath, fileName, version, isWHM, isDAT, isWHMRSC]() -> SaveResult {
        SaveResult result;
        result.filePath = filePath;
        result.fileName = fileName;
        result.isNewPath = false;
        result.isAutoSave = false;
        result.success = false;
        
        QElapsedTimer timer;
        timer.start();
        
        // 在主线程中获取数据副本（线程安全）
        AutoSaveData saveData;
        saveData.filePath = filePath;
        saveData.version = version;
        saveData.isWHM = isWHM;
        saveData.isWHMRSC = isWHMRSC;
        saveData.isDAT = isDAT;

        QMetaObject::invokeMethod(this, [this, &saveData]() {
            FileTab* tab = getCurrentTab();
            if (tab && tab->filePath == saveData.filePath) {
                if (saveData.isWHM) {
                    saveData.whmEntries = tab->whmEntries;
                } else if (saveData.isDAT) {
                    saveData.datEntries = tab->datEntries;
                } else {
                    saveData.tables = tab->tables;
                }
            }
        }, Qt::BlockingQueuedConnection);

        // 执行保存
        bool success = false;
        try {
            GXTEditor editor;
            if (isWHMRSC) {
                // WHM/RSC 格式 - 使用 parser 的保存方法
                GXTParser parser;
                success = parser.saveWHMRSC(saveData.filePath.toStdString(), saveData.whmEntries);
            } else if (isWHM) {
                success = editor.saveAsWHM(saveData.filePath.toStdString(), saveData.whmEntries);
            } else if (isDAT) {
                success = editor.saveAsDAT(saveData.filePath.toStdString(), saveData.datEntries);
            } else {
                // GXT/GXT2 文件 - 使用批量添加提高性能
                editor.setVersion(saveData.version);
                for (const auto& table : saveData.tables) {
                    if (!editor.addTable(table.name)) continue;
                    size_t tableIndex = editor.getTableCount() - 1;
                    // 批量添加条目（跳过重复检查，O(n) 复杂度）
                    std::vector<std::pair<std::string, std::string>> entriesBatch;
                    entriesBatch.reserve(table.entries.size());
                    for (const auto& entry : table.entries) {
                        entriesBatch.emplace_back(entry.key, entry.value);
                    }
                    editor.addEntriesBatch(tableIndex, entriesBatch);
                }
                success = editor.saveToFile(saveData.filePath.toStdString());
            }
        } catch (...) {
            success = false;
        }
        
        result.success = success;
        result.elapsedMs = timer.elapsed();
        
        if (success) {
            QFileInfo fi(filePath);
            result.bytesWritten = fi.size();
        }
        
        return result;
    });
    
    // 使用 watcher 监听完成
    auto* watcher = new QFutureWatcher<SaveResult>(this);
    connect(watcher, &QFutureWatcher<SaveResult>::finished, this, [this, watcher]() {
        SaveResult result = watcher->result();
        
        // 隐藏状态栏进度条
        if (m_saveProgressBar) {
            m_saveProgressBar->hide();
            m_saveProgressBar->setValue(0);
        }
        
        // 处理结果
        onSaveCompleted(result);
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
}

void GXTStudio::saveAsFile()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) return;
    
    // 根据当前文件类型确定默认扩展名和过滤器
    QString defaultExt;
    QString filter;
    QString defaultPath;
    
    QFileInfo currentFileInfo(currentTab->filePath);
    QString currentDir = currentFileInfo.absolutePath();
    QString baseName = currentFileInfo.completeBaseName();
    
    if (currentTab->isWHM) {
        defaultExt = ".whm";
        filter = "WHM文件 (*.whm)";
        defaultPath = currentDir + "/" + baseName + "_copy" + defaultExt;
    } else if (currentTab->isDAT) {
        defaultExt = ".dat";
        filter = "DAT文件 (*.dat)";
        defaultPath = currentDir + "/" + baseName + "_copy" + defaultExt;
    } else if (currentTab->version != GXTVersion::UNKNOWN) {
        // GXT/GXT2文件，根据版本确定扩展名
        switch (currentTab->version) {
            case GXTVersion::GXT2:
                defaultExt = ".gxt2";
                filter = "GXT2文件 (*.gxt2)";
                break;
            case GXTVersion::GTA_IV:
                defaultExt = ".gxt";
                filter = "GTA IV GXT文件 (*.gxt)";
                break;
            case GXTVersion::GTA_SA:
                defaultExt = ".gxt";
                filter = "GTA SA GXT文件 (*.gxt)";
                break;
            case GXTVersion::GTA_VC:
                defaultExt = ".gxt";
                filter = "GTA VC GXT文件 (*.gxt)";
                break;
            case GXTVersion::GTA_III:
                defaultExt = ".gxt";
                filter = "GTA III GXT文件 (*.gxt)";
                break;
            default:
                defaultExt = ".gxt";
                filter = "GXT文件 (*.gxt)";
                break;
        }
    } else {
        // 未知版本，默认为文本格式
        defaultExt = ".txt";
        filter = "文本文件 (*.txt)";
        defaultPath = currentDir + "/" + baseName + "_copy" + defaultExt;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "另存为", defaultPath, filter);
    
    if (!fileName.isEmpty()) {
        // 验证扩展名是否匹配预期格式
        QFileInfo saveFileInfo(fileName);
        QString selectedExt = saveFileInfo.suffix().toLower();
        QString expectedExt = defaultExt.mid(1); // 去掉点号
        
        if (selectedExt != expectedExt) {
            // 如果用户手动修改了扩展名，自动添加正确的扩展名
            fileName = saveFileInfo.absolutePath() + "/" + saveFileInfo.completeBaseName() + defaultExt;
        }
        
        // 更新标签页的文件路径信息（另存为新路径）
        currentTab->filePath = fileName;
        currentTab->fileName = QFileInfo(fileName).fileName();
        
        // 使用 QtConcurrent 完全异步保存
        QString newFilePath = fileName;
        QString newFileName = QFileInfo(fileName).fileName();
        GXTVersion version = currentTab->version;
        bool isWHM = currentTab->isWHM;
        bool isWHMRSC = currentTab->isWHMRSC;
        bool isDAT = currentTab->isDAT;

        // 使用状态栏进度条
        m_saveProgressBar->setFormat("正在另存为 %p%");
        m_saveProgressBar->show();
        m_saveProgressBar->setValue(10);

        showLogMessage("正在后台另存为: " + newFileName);

        // 在后台线程中执行
        QFuture<SaveResult> future = QtConcurrent::run([this, newFilePath, newFileName, version, isWHM, isDAT, isWHMRSC]() -> SaveResult {
            SaveResult result;
            result.filePath = newFilePath;
            result.fileName = newFileName;
            result.isNewPath = true;
            result.isAutoSave = false;
            result.success = false;
            
            QElapsedTimer timer;
            timer.start();
            
            // 在主线程中获取数据副本
            AutoSaveData saveData;
            saveData.filePath = newFilePath;
            saveData.version = version;
            saveData.isWHM = isWHM;
            saveData.isWHMRSC = isWHMRSC;
            saveData.isDAT = isDAT;

            QMetaObject::invokeMethod(this, [this, &saveData]() {
                FileTab* tab = getCurrentTab();
                if (tab) {
                    if (saveData.isWHM) {
                        saveData.whmEntries = tab->whmEntries;
                    } else if (saveData.isDAT) {
                        saveData.datEntries = tab->datEntries;
                    } else {
                        saveData.tables = tab->tables;
                    }
                }
            }, Qt::BlockingQueuedConnection);
            
            // 执行保存
            bool success = false;
            try {
                GXTEditor editor;
                if (isWHMRSC) {
                    // WHM/RSC 格式 - 使用 parser 的保存方法
                    GXTParser parser;
                    success = parser.saveWHMRSC(saveData.filePath.toStdString(), saveData.whmEntries);
                } else if (isWHM) {
                    success = editor.saveAsWHM(saveData.filePath.toStdString(), saveData.whmEntries);
                } else if (isDAT) {
                    success = editor.saveAsDAT(saveData.filePath.toStdString(), saveData.datEntries);
                } else {
                    // GXT/GXT2 文件 - 使用批量添加提高性能
                    editor.setVersion(saveData.version);
                    for (const auto& table : saveData.tables) {
                        if (!editor.addTable(table.name)) continue;
                        size_t tableIndex = editor.getTableCount() - 1;
                        // 批量添加条目（跳过重复检查，O(n) 复杂度）
                        std::vector<std::pair<std::string, std::string>> entriesBatch;
                        entriesBatch.reserve(table.entries.size());
                        for (const auto& entry : table.entries) {
                            entriesBatch.emplace_back(entry.key, entry.value);
                        }
                        editor.addEntriesBatch(tableIndex, entriesBatch);
                    }
                    success = editor.saveToFile(saveData.filePath.toStdString());
                }
            } catch (...) {
                success = false;
            }
            
            result.success = success;
            result.elapsedMs = timer.elapsed();
            
            if (success) {
                QFileInfo fi(newFilePath);
                result.bytesWritten = fi.size();
            }
            
            return result;
        });
        
        // 使用 watcher 监听完成
        auto* watcher = new QFutureWatcher<SaveResult>(this);
        connect(watcher, &QFutureWatcher<SaveResult>::finished, this, [this, watcher]() {
            SaveResult result = watcher->result();
            
            // 隐藏状态栏进度条
            if (m_saveProgressBar) {
                m_saveProgressBar->hide();
                m_saveProgressBar->setValue(0);
            }
            
            // 处理结果
            onSaveCompleted(result);
            watcher->deleteLater();
        });
        
        watcher->setFuture(future);
    }
}

void GXTStudio::onVersionChanged(const QModelIndex& selectedIndex)
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) return;
    
    QString text = selectedIndex.data(Qt::DisplayRole).toString();
    
    // 设置版本信息到textRenderWidget
    if (currentTab->textRenderWidget) {
        // 将GXTVersion枚举转换为数字版本 (0=GTA III, 1=VC, 2=SA, 3=LCS, 4=GTA IV)
        int versionNum = 0;
        switch (currentTab->version) {
            case GXTVersion::GTA_III:
                versionNum = 0;
                break;
            case GXTVersion::GTA_VC:
                versionNum = 1;
                break;
            case GXTVersion::GTA_SA:
                versionNum = 2;
                break;
            case GXTVersion::GTA_IV:
                versionNum = 4;
                break;
            case GXTVersion::GXT2:
            default:
                versionNum = 2; // 默认为SA版本
                break;
        }
        currentTab->textRenderWidget->setGtaVersion(versionNum);
        currentTab->textRenderWidget->setText(text);
    }
}

void GXTStudio::exportFile()
{
    exportCurrentFile();
}

void GXTStudio::exportCurrentFile()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) return;
    
    QFileInfo fileInfo(currentTab->filePath);
    QString baseName = fileInfo.completeBaseName();
    QString exportPath = fileInfo.absolutePath() + "/" + baseName + "_Export.txt";
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出整个文件", exportPath,
        "文本文件 (*.txt);;所有文件 (*.*)");
    
    if (!fileName.isEmpty()) {
        // 创建同名文件夹
        QFileInfo exportFileInfo(fileName);
        QString folderPath = exportFileInfo.absolutePath() + "/" + baseName;
        QDir dir;
        if (!dir.exists(folderPath)) {
            dir.mkpath(folderPath);
        }
        
        GXTParser parser;
        parser.setLogCallback([this](const std::string& msg) {
            showLogMessage(QString::fromStdString(msg));
        });
        
        if (currentTab->isWHM) {
            // WHM文件导出 - 导出到汇总txt和文件夹中的各个文件
            QFile allFile(fileName);
            if (allFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream allStream(&allFile);
                allStream.setEncoding(QStringConverter::Utf8);
                
                for (size_t i = 0; i < currentTab->whmEntries.size(); ++i) {
                    // 安全检查
                    if (i >= currentTab->whmEntries.size() || currentTab->whmEntries.empty()) break;
                    
                    const auto& entry = currentTab->whmEntries[i];
                    QString hashStr = QString("0x%1").arg(entry.hash, 8, 16, QChar('0')).toUpper();
                    QString line = QString("%1=%2").arg(hashStr, QString::fromStdString(entry.text));
                    allStream << line << Qt::endl;
                    
                    // 同时保存到文件夹中的单个文件
                    QString individualFile = folderPath + "/" + hashStr + ".txt";
                    QFile indFile(individualFile);
                    if (indFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream indStream(&indFile);
                        indStream.setEncoding(QStringConverter::Utf8);
                        indStream << QString::fromStdString(entry.text);
                        indFile.close();
                    }
                }
                allFile.close();
            }
            showLogMessage("WHM文件已导出: " + fileName + " 及分文件夹");
        } else {
            // GXT/GXT2文件导出
            QFile allFile(fileName);
            if (allFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream allStream(&allFile);
                allStream.setEncoding(QStringConverter::Utf8);
                
                // 导出所有表到汇总文件
                for (const auto& table : currentTab->tables) {
                    if (currentTab->tables.size() > 1 || table.name != "MAIN") {
                        allStream << QString("[%1]").arg(QString::fromStdString(table.name)) << Qt::endl;
                    }
                    
                    for (const auto& entry : table.entries) {
                        QString keyStr;
                        // 优先使用原始键值
                        if (!entry.originalKey.empty()) {
                            keyStr = QString::fromStdString(entry.originalKey);
                        } else {
                            // 尝试使用SATKEY/IVTKEY映射
                            QString entryKey = QString::fromStdString(entry.key);
                            bool foundInLst = false;
                            
                            if (currentTab->version == GXTVersion::GTA_SA && !GXTTableModel::isSATKeyMapEmpty()) {
                                bool ok;
                                uint32_t hash = entryKey.toUInt(&ok, 16);
                                if (ok) {
                                    QString lstKey;
                                    if (GXTTableModel::findSATKey(hash, lstKey)) {
                                        keyStr = lstKey;
                                        foundInLst = true;
                                    }
                                }
                            } else if (currentTab->version == GXTVersion::GTA_IV && !GXTTableModel::isIVTKeyMapEmpty()) {
                                bool ok;
                                uint32_t hash = entryKey.toUInt(&ok, 16);
                                if (ok) {
                                    QString lstKey;
                                    if (GXTTableModel::findIVTKey(hash, lstKey)) {
                                        keyStr = lstKey;
                                        foundInLst = true;
                                    }
                                }
                            }
                            
                            if (!foundInLst) {
                                keyStr = GXTStudio::formatKeyForDisplay(entryKey);
                            }
                        }
                        QString line = QString("%1=%2").arg(keyStr, QString::fromStdString(entry.value));
                        allStream << line << Qt::endl;
                    }
                    
                    if (currentTab->tables.size() > 1 || table.name != "MAIN") {
                        allStream << Qt::endl; // 表之间空行
                    }
                }
                allFile.close();
                
                // 导出各个表到文件夹
                for (const auto& table : currentTab->tables) {
                    QString tableFile = folderPath + "/" + QString::fromStdString(table.name) + ".txt";
                    QFile file(tableFile);
                    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream stream(&file);
                        stream.setEncoding(QStringConverter::Utf8);
                        
                        if (currentTab->tables.size() > 1 || table.name != "MAIN") {
                            stream << QString("[%1]").arg(QString::fromStdString(table.name)) << Qt::endl;
                        }
                        
                        for (const auto& entry : table.entries) {
                            QString keyStr;
                            // 优先使用原始键值
                            if (!entry.originalKey.empty()) {
                                keyStr = QString::fromStdString(entry.originalKey);
                            } else {
                                // 尝试使用SATKEY/IVTKEY映射
                                QString entryKey = QString::fromStdString(entry.key);
                                bool foundInLst = false;
                                
                                if (currentTab->version == GXTVersion::GTA_SA && !GXTTableModel::isSATKeyMapEmpty()) {
                                    bool ok;
                                    uint32_t hash = entryKey.toUInt(&ok, 16);
                                    if (ok) {
                                        QString lstKey;
                                        if (GXTTableModel::findSATKey(hash, lstKey)) {
                                            keyStr = lstKey;
                                            foundInLst = true;
                                        }
                                    }
                                } else if (currentTab->version == GXTVersion::GTA_IV && !GXTTableModel::isIVTKeyMapEmpty()) {
                                    bool ok;
                                    uint32_t hash = entryKey.toUInt(&ok, 16);
                                    if (ok) {
                                        QString lstKey;
                                        if (GXTTableModel::findIVTKey(hash, lstKey)) {
                                            keyStr = lstKey;
                                            foundInLst = true;
                                        }
                                    }
                                }
                                
                                if (!foundInLst) {
                                    keyStr = GXTStudio::formatKeyForDisplay(entryKey);
                                }
                            }
                            QString line = QString("%1=%2").arg(keyStr, QString::fromStdString(entry.value));
                            stream << line << Qt::endl;
                        }
                        file.close();
                    }
                }
            }
            showLogMessage("GXT文件已导出: " + fileName + " 及分文件夹");
        }
    }
}

void GXTStudio::exportSelectedTable()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->tableList) return;
    
    // 获取当前选中的表
    int currentRow = currentTab->tableList->currentRow();
    if (currentRow < 0) return;
    
    QListWidgetItem* currentItem = currentTab->tableList->currentItem();
    if (!currentItem) return;
    
    int tableIndex = currentItem->data(Qt::UserRole).toInt();
    if (tableIndex < 0 || tableIndex >= static_cast<int>(currentTab->tables.size())) return;
    
    const auto& table = currentTab->tables[tableIndex];
    QString tableName = QString::fromStdString(table.name);
    
    QFileInfo fileInfo(currentTab->filePath);
    QString defaultPath = fileInfo.absolutePath() + "/" + tableName + ".txt";
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出表: " + tableName, defaultPath,
        "文本文件 (*.txt);;所有文件 (*.*)");
    
    if (!fileName.isEmpty()) {
        exportTableToFolder(tableIndex, fileName);
    }
}

void GXTStudio::exportTableToFolder(int tableIndex, const QString& filePath)
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || tableIndex < 0 || tableIndex >= static_cast<int>(currentTab->tables.size())) return;
    
    const auto& table = currentTab->tables[tableIndex];
    QString tableName = QString::fromStdString(table.name);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        
        if (currentTab->tables.size() > 1 || tableName != "MAIN") {
            stream << QString("[%1]").arg(tableName) << Qt::endl;
        }
        
        for (const auto& entry : table.entries) {
            QString keyStr;
            // 优先使用原始键值
            if (!entry.originalKey.empty()) {
                keyStr = QString::fromStdString(entry.originalKey);
            } else {
                // 尝试使用SATKEY/IVTKEY映射
                QString entryKey = QString::fromStdString(entry.key);
                bool foundInLst = false;
                
                if (currentTab->version == GXTVersion::GTA_SA && !GXTTableModel::isSATKeyMapEmpty()) {
                    bool ok;
                    uint32_t hash = entryKey.toUInt(&ok, 16);
                    if (ok) {
                        QString lstKey;
                        if (GXTTableModel::findSATKey(hash, lstKey)) {
                            keyStr = lstKey;
                            foundInLst = true;
                        }
                    }
                } else if (currentTab->version == GXTVersion::GTA_IV && !GXTTableModel::isIVTKeyMapEmpty()) {
                    bool ok;
                    uint32_t hash = entryKey.toUInt(&ok, 16);
                    if (ok) {
                        QString lstKey;
                        if (GXTTableModel::findIVTKey(hash, lstKey)) {
                            keyStr = lstKey;
                            foundInLst = true;
                        }
                    }
                }
                
                if (!foundInLst) {
                    keyStr = GXTStudio::formatKeyForDisplay(entryKey);
                }
            }
            QString line = QString("%1=%2").arg(keyStr, QString::fromStdString(entry.value));
            stream << line << Qt::endl;
        }
        file.close();
        
        showLogMessage(QString("表 '%1' 已导出: %2").arg(tableName, filePath));
    } else {
        QMessageBox::warning(this, "导出失败", "无法创建导出文件: " + filePath);
    }
}

void GXTStudio::showTableContextMenu(const QPoint& pos)
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->tableList || currentTab->isWHM) return;

    // 获取点击的项目
    QListWidgetItem* item = currentTab->tableList->itemAt(pos);
    if (!item) return;

    // 获取表索引
    int tableIndex = item->data(Qt::UserRole).toInt();
    if (tableIndex < 0 || tableIndex >= static_cast<int>(currentTab->tables.size())) return;

    QString tableName = QString::fromStdString(currentTab->tables[tableIndex].name);

    // 创建右键菜单
    QMenu* contextMenu = new QMenu(this);

    // 添加重命名动作
    QAction* renameTableAction = contextMenu->addAction("重命名表");
    contextMenu->addSeparator();

    // 添加导出动作
    QAction* exportTableAction = contextMenu->addAction(QString("导出表 '%1'").arg(tableName));

    // 连接信号
    connect(renameTableAction, &QAction::triggered, this, [this, tableIndex]() {
        renameTable(tableIndex);
    });
    connect(exportTableAction, &QAction::triggered, this, [this, tableIndex]() {
        exportSelectedTable();
    });

    // 显示菜单
    contextMenu->exec(currentTab->tableList->viewport()->mapToGlobal(pos));
    delete contextMenu;
}

void GXTStudio::closeFile()
{
    if (m_currentTabIndex >= 0) {
        closeTab(m_currentTabIndex);
    }
}

void GXTStudio::exitApp()
{
    close();
}

// 编辑操作槽函数
void GXTStudio::findText()
{
    FileTab* tab = getCurrentTab();
    if (!tab || !tab->searchEdit) return;
    
    // 聚焦到搜索框
    tab->searchEdit->setFocus();
    tab->searchEdit->selectAll();
}

void GXTStudio::replaceText()
{
    // 简化的替换功能
    QMessageBox::information(this, "替换", "替换功能正在开发中...");
}

void GXTStudio::toggleReadOnly(bool readOnly)
{
    m_isReadOnly = readOnly;

    // 更新所有标签页中表格的编辑触发器和模型可编辑性
    for (auto& tab : m_fileTabs) {
        if (tab.tableList) {
            tab.tableList->setEditTriggers(QAbstractItemView::DoubleClicked);
        }
        if (tab.entryTableView) {
            // 在只读模式下仍然允许选择，但不触发编辑
            if (m_isReadOnly) {
                tab.entryTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
                // 设置选择行为为行选择，允许用户选择整个行
                tab.entryTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
                tab.entryTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
            } else {
                tab.entryTableView->setEditTriggers(QAbstractItemView::DoubleClicked);
                tab.entryTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
                tab.entryTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
            }
        }
        if (tab.entryTableModel) {
            tab.entryTableModel->setEditable(!m_isReadOnly);
        }
        // 更新添加新表按钮状态
        if (tab.addTableButton) {
            tab.addTableButton->setEnabled(!m_isReadOnly && !tab.isWHM && !tab.isDAT);
        }
        // 更新添加条目按钮状态
        if (tab.addEntryButton) {
            tab.addEntryButton->setEnabled(!m_isReadOnly);
        }
    }
    
    // 更新菜单项文本和动作状态 - 保持文本固定为"只读模式"
    m_readOnlyAction->setText("只读模式");
    updateActions(); // 【修复】更新保存按钮等动作的状态
    
    // 【修复】更新码表转换按钮和下拉菜单状态
    if (m_codeTableButton) {
        bool enabled = !m_isReadOnly;
        m_codeTableButton->setEnabled(enabled);
        
        // 更新下拉菜单中的动作状态
        if (m_mountCodeTableAction) {
            m_mountCodeTableAction->setEnabled(enabled);
        }
        if (m_convertCodeTableAction) {
            // 转换菜单项只有在已挂载码表且非只读模式下才启用
            bool canConvert = enabled && m_codeTableConverter && m_codeTableConverter->isTableMounted();
            m_convertCodeTableAction->setEnabled(canConvert);
        }
        if (m_unmountCodeTableAction) {
            // 卸载菜单项只有在已挂载码表且非只读模式下才启用
            bool canUnmount = enabled && m_codeTableConverter && m_codeTableConverter->isTableMounted();
            m_unmountCodeTableAction->setEnabled(canUnmount);
        }
        
        // 更新按钮文本
        updateCodeTableButtonText();
    }
    
    // 更新状态栏
    if (m_statusLabel) {
        m_statusLabel->setText(QString("状态: %1").arg(m_isReadOnly ? "只读" : "编辑"));
    }
}

// 视图操作槽函数
void GXTStudio::increaseFont()
{
    m_fontSize = qMin(m_fontSize + 1, 16);  // 使用更小的步长和合理的最大值
    updateFontSizes();
    showLogMessage(QString("字体大小: %1").arg(m_fontSize));
}

void GXTStudio::decreaseFont()
{
    m_fontSize = qMax(m_fontSize - 1, 8);   // 使用更小的步长和合理的最小值
    updateFontSizes();
    showLogMessage(QString("字体大小: %1").arg(m_fontSize));
}

// 帮助槽函数
void GXTStudio::showAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void GXTStudio::showSaveSuccessDialog(const SaveResult& result)
{
    // 创建自定义对话框
    QDialog dialog(this);
    dialog.setWindowTitle("保存成功");
    dialog.setWindowIcon(QIcon());
    dialog.setModal(true);
    dialog.setMinimumWidth(480);
    dialog.setMaximumWidth(480);

    // 设置现代蓝白风格 - 与主界面一致
    dialog.setStyleSheet(
        "QDialog { background-color: #ffffff; border-radius: 8px; }"
        "QLabel { color: #2c3e50; }"
        "QLabel#titleLabel { font-size: 16px; font-weight: bold; color: #3498db; }"
        "QLabel#labelKey { font-size: 11px; font-weight: bold; color: #5f6368; }"
        "QLabel#labelValue { font-size: 12px; color: #202124; }"
        "QLabel#labelFileName { font-size: 13px; font-weight: 500; color: #2c3e50; }"
        "QLabel#labelHighlight { font-size: 12px; color: #3498db; font-weight: 500; }"
        "QLabel#labelPath { font-size: 11px; color: #5f6368; font-family: 'Segoe UI', 'Microsoft YaHei', Consolas, monospace; background-color: #f8f9fa; padding: 5px 8px; border-radius: 4px; border: 1px solid #e9ecef; }"
        "QGroupBox { border: 1px solid #e9ecef; background-color: #f8f9fa; border-radius: 6px; margin-top: 0px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QPushButton#mainButton { background-color: #3498db; color: white; border: none; border-radius: 6px; padding: 6px 20px; font-weight: 500; font-size: 12px; min-width: 70px; }"
        "QPushButton#mainButton:hover { background-color: #2980b9; }"
        "QPushButton#mainButton:pressed { background-color: #2471a3; }"
        "QPushButton#actionButton { background-color: #ffffff; color: #5f6368; border: 1px solid #dadce0; border-radius: 4px; padding: 4px 8px; font-size: 11px; min-width: 28px; }"
        "QPushButton#actionButton:hover { background-color: #f8f9fa; border-color: #bdc1c6; }"
        "QLineEdit { background-color: #ffffff; border: 1px solid #dadce0; border-radius: 4px; padding: 4px 6px; }"
        "QLineEdit:focus { border: 1px solid #3498db; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 16, 16, 12);
    mainLayout->setSpacing(8);

    // 标题行 - 带图标和成功徽章
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(8);

    QString iconText = QString(FA::QCheck);
    QLabel* iconLabel = new QLabel(iconText);
    QFont iconFont(FA::solidFontFamily());
    iconFont.setPointSize(16);
    iconLabel->setFont(iconFont);
    iconLabel->setStyleSheet("color: #3498db;");
    titleLayout->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel("文件保存成功");
    titleLabel->setObjectName("titleLabel");
    titleLabel->setStyleSheet("color: #3498db;");
    titleLayout->addWidget(titleLabel);

    titleLayout->addStretch();
    mainLayout->addLayout(titleLayout);

    // 文件信息卡片
    QGroupBox* infoGroup = new QGroupBox();
    infoGroup->setFlat(true);

    QGridLayout* gridLayout = new QGridLayout(infoGroup);
    gridLayout->setSpacing(6);
    gridLayout->setContentsMargins(10, 10, 10, 10);

    // 格式化文件大小并准备标题子布局（类型徽章 + 名称）
    double sizeInBytes = static_cast<double>(result.bytesWritten);
    QString sizeText;
    if (sizeInBytes <= 0) {
        sizeText = "0 B";
    } else if (sizeInBytes < 1024.0) {
        sizeText = QString("%1 B").arg(static_cast<int>(sizeInBytes));
    } else if (sizeInBytes < 1024.0 * 1024.0) {
        sizeText = QString("%1 KB").arg(sizeInBytes / 1024.0, 0, 'f', 1);
    } else {
        sizeText = QString("%1 MB").arg(sizeInBytes / (1024.0 * 1024.0), 0, 'f', 1);
    }

    // 耗时文本
    QString timeText = QString("%1 ms").arg(result.elapsedMs);

    // 行0: 文件名（大字体显示）
    QLabel* fileNameLabel = new QLabel("文件名:");
    fileNameLabel->setObjectName("labelKey");
    gridLayout->addWidget(fileNameLabel, 0, 0);

    QLabel* fileNameValue = new QLabel(QFileInfo(result.filePath).fileName());
    fileNameValue->setObjectName("labelFileName");
    gridLayout->addWidget(fileNameValue, 0, 1);

    // 行1: 文件路径
    QLabel* pathLabel = new QLabel("路径:");
    pathLabel->setObjectName("labelKey");
    gridLayout->addWidget(pathLabel, 1, 0);

    QLineEdit* pathEdit = new QLineEdit(result.filePath);
    pathEdit->setObjectName("labelPath");
    pathEdit->setReadOnly(true);
    pathEdit->setFixedHeight(28);
    pathEdit->setCursor(Qt::IBeamCursor);
    gridLayout->addWidget(pathEdit, 1, 1);

    // 名称行布局和类型徽章（file type badge）
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->setSpacing(6);
    QLabel* typeBadge = new QLabel(QFileInfo(result.filePath).suffix().toUpper());
    typeBadge->setFixedHeight(20);
    typeBadge->setAlignment(Qt::AlignCenter);
    typeBadge->setStyleSheet("background-color: #e3f2fd; color: #1976d2; border-radius: 4px; padding: 2px 6px; font-size: 10px; font-weight: 500;");
    nameLayout->addWidget(typeBadge);
    nameLayout->addStretch();

    gridLayout->addLayout(nameLayout, 2, 1);

    // 行3: 操作按钮
    QHBoxLayout* buttonRowLayout = new QHBoxLayout();
    buttonRowLayout->setSpacing(3);
    buttonRowLayout->addStretch();

    // 快捷按钮组
    QPushButton* copyPathBtn = new QPushButton();
    copyPathBtn->setObjectName("actionButton");
    copyPathBtn->setFixedSize(55, 22);
    copyPathBtn->setToolTip("复制路径");
    copyPathBtn->setText(QString("%1").arg(FA::QClipboardList));
    QFont copyFont(FA::solidFontFamily());
    copyFont.setPointSize(9);
    copyPathBtn->setFont(copyFont);
    connect(copyPathBtn, &QPushButton::clicked, [result]() {
        QApplication::clipboard()->setText(result.filePath);
    });
    buttonRowLayout->addWidget(copyPathBtn);

    QPushButton* openFolderBtn = new QPushButton();
    openFolderBtn->setObjectName("actionButton");
    openFolderBtn->setFixedSize(65, 22);
    openFolderBtn->setToolTip("打开文件夹");
    openFolderBtn->setText(QString("%1").arg(FA::QFolder));
    openFolderBtn->setFont(copyFont);
    connect(openFolderBtn, &QPushButton::clicked, [result]() {
        QFileInfo fi(result.filePath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
    });
    buttonRowLayout->addWidget(openFolderBtn);

    QPushButton* copyNameBtn = new QPushButton();
    copyNameBtn->setObjectName("actionButton");
    copyNameBtn->setFixedSize(65, 22);
    copyNameBtn->setToolTip("复制文件名");
    copyNameBtn->setText(QString("%1").arg(FA::QFileAlt));
    copyNameBtn->setFont(copyFont);
    connect(copyNameBtn, &QPushButton::clicked, [result]() {
        QFileInfo fi(result.filePath);
        QApplication::clipboard()->setText(fi.fileName());
    });
    buttonRowLayout->addWidget(copyNameBtn);

    gridLayout->addLayout(buttonRowLayout, 3, 0, 1, 2);

    // 行4: 文件大小和耗时（紧凑布局）
    QLabel* statsLabel = new QLabel("统计:");
    statsLabel->setObjectName("labelKey");
    gridLayout->addWidget(statsLabel, 4, 0);

    QHBoxLayout* statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(12);

    QLabel* sizeValue = new QLabel(QString("大小: %1").arg(sizeText));
    sizeValue->setObjectName("labelHighlight");
    sizeValue->setStyleSheet("color: #3498db;");
    statsLayout->addWidget(sizeValue);

    QLabel* timeValue = new QLabel(QString("耗时: %1").arg(timeText));
    timeValue->setObjectName("labelHighlight");
    timeValue->setStyleSheet("color: #17a2b8;");
    statsLayout->addWidget(timeValue);

    if (result.isNewPath) {
        QLabel* newLabel = new QLabel("新保存");
        newLabel->setStyleSheet("background-color: #e3f2fd; color: #1976d2; border-radius: 4px; padding: 2px 6px; font-size: 10px; font-weight: 500;");
        statsLayout->addWidget(newLabel);
    }

    statsLayout->addStretch();
    gridLayout->addLayout(statsLayout, 4, 1);

    mainLayout->addWidget(infoGroup);

    // 底部按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* okButton = new QPushButton("确定");
    okButton->setObjectName("mainButton");
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setDefault(true);
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(okButton);

    mainLayout->addLayout(buttonLayout);

    dialog.exec();
}

// 表和条目操作槽函数
void GXTStudio::onTableSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED(previous);
    
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) {
        return;
    }
    
    if (!currentTab->tableList) {
        return;
    }
    
    if (currentTab->isWHM) {
        return;
    }
    
    if (!current) {
        return;
    }
    
    // 获取当前选中的行 - 使用QListWidget的row方法
    int selectedRow = currentTab->tableList->row(current);
    
    if (selectedRow < 0) {
        return;
    }
    
    // 验证行号的有效性
    if (selectedRow >= currentTab->tableList->count()) {
        return;
    }
    
    // 使用统一的表格切换函数 - 直接使用行号作为索引
    switchToTable(selectedRow);
}

void GXTStudio::switchToTable(int newTableIndex)
{
    FileTab* tab = getCurrentTab();
    if (!tab) {
        return;
    }
    
    if (tab->isWHM) {
        return;
    }
    
    if (newTableIndex < 0 || newTableIndex >= static_cast<int>(tab->tables.size())) {
        return;
    }
    
    // 【关键修复】立即更新当前表格索引
    tab->currentTableIndex = newTableIndex;
    
    // 【关键修复】立即重新设置模型，确保模型指向正确的表格索引
    if (tab->entryTableModel) {
        tab->entryTableModel->setTab(tab);
    }
    
    // 不检查是否已切换到相同表格，总是更新模型以确保数据正确显示
    if (tab->entryTableView) {
        // 禁用重绘以减少闪烁
        tab->entryTableView->setUpdatesEnabled(false);
        
        // 清除委托缓存
        if (auto* delegate = qobject_cast<OptimizedItemDelegate*>(tab->entryTableView->itemDelegate())) {
            delegate->clearCache();
        }
        
        // 【关键修复】确保模型正确
        if (tab->entryTableView->model() != tab->entryTableModel) {
            tab->entryTableView->setModel(tab->entryTableModel);
        }
        
        // 清除视口
        tab->entryTableView->viewport()->update();
    }
    
    // 同步更新侧边栏选择
    if (tab->tableList) {
        // 阻塞信号防止递归调用
        const bool wasBlocked = tab->tableList->blockSignals(true);
        tab->tableList->setCurrentRow(newTableIndex);
        tab->tableList->blockSignals(wasBlocked);
    }

    // 【性能优化】减少延迟从50ms到10ms，加快响应速度
    // 【关键修复】捕获索引而不是指针，避免标签页移动后指针失效
    int currentTabIndex = m_currentTabIndex;
    QTimer::singleShot(10, this, [this, currentTabIndex, newTableIndex]() {
        // 【关键修复】每次都通过索引重新获取指针，避免指针失效
        FileTab* tab = (currentTabIndex >= 0 && currentTabIndex < static_cast<int>(m_fileTabs.size()))
                         ? &m_fileTabs[currentTabIndex] : nullptr;

        // 验证标签页和索引仍然有效
        if (!tab || currentTabIndex != m_currentTabIndex) return;
        if (newTableIndex < 0 || newTableIndex >= static_cast<int>(tab->tables.size())) return;

        try {
            if (tab->entryTableModel) {
                // 【关键修复】强制重置模型以清除所有缓存
                tab->entryTableModel->forceDataReset();

                // 重新启用表格视图更新
                if (tab->entryTableView) {
                    tab->entryTableView->setUpdatesEnabled(true);

                    // 【关键修复】确保模型正确
                    if (tab->entryTableView->model() != tab->entryTableModel) {
                        tab->entryTableView->setModel(tab->entryTableModel);
                    }

                    tab->entryTableView->viewport()->update();
                    tab->entryTableView->update();

                    // 【关键修复】强制立即重绘，确保内容立即显示
                    tab->entryTableView->viewport()->repaint();
                    tab->entryTableView->repaint();

                    // 【性能优化】立即处理事件，确保UI立即更新显示
                    QCoreApplication::processEvents();
                }

                updateStatusBar();

                // 记录切换日志
                QString newTableName = QString::fromStdString(tab->tables[newTableIndex].name);
                showLogMessage("切换到表格: " + newTableName + " (条目数: " +
                               QString::number(tab->tables[newTableIndex].entries.size()) + ")");
            }
        } catch (...) {
            qWarning() << "Exception occurred during table switching";
            // 确保视图更新被重新启用
            if (tab && tab->entryTableView) {
                tab->entryTableView->setUpdatesEnabled(true);
            }
        }
    });
}

// 处理添加表格的请求
void GXTStudio::onAddTableClicked(const QModelIndex& index)
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || currentTab->isWHM || currentTab->isDAT) return;
    
    // 获取当前表名并建议一个新表名
    QString baseName = "新表";
    if (index.isValid()) {
        QString currentName = index.data(Qt::UserRole + 1).toString();
        if (!currentName.isEmpty()) {
            baseName = currentName + "_副本";
        }
    }
    
    // 创建输入对话框
    bool ok;
    QString newTableName = QInputDialog::getText(this, "添加新表", "请输入新表名:", 
                                               QLineEdit::Normal, baseName, &ok);
    
    if (ok && !newTableName.trimmed().isEmpty()) {
        newTableName = newTableName.trimmed();
        
        // 检查表名是否已存在
        for (const auto& table : currentTab->tables) {
            if (QString::fromStdString(table.name) == newTableName) {
                QMessageBox::warning(this, "错误", "表名已存在，请使用不同的名称。");
                return;
            }
        }
        
        // 创建新表
        GXTTabl newTable;
        newTable.name = newTableName.toStdString();
        newTable.entries.clear(); // 空表
        
        // 添加到表格列表
        currentTab->tables.push_back(newTable);
        
        // 更新UI列表
        updateTableList();
        
        // 切换到新表
        switchToTable(static_cast<int>(currentTab->tables.size()) - 1);
        
        showLogMessage(QString("已添加新表: %1").arg(newTableName));
    }
}

// 处理编辑表名的请求
void GXTStudio::onEditTableName(const QModelIndex& index)
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || currentTab->isWHM || currentTab->isDAT) return;
    
    if (!index.isValid()) return;
    
    // 设置列表项可编辑
    if (currentTab->tableList) {
        QListWidgetItem* item = currentTab->tableList->item(index.row());
        if (item) {
            currentTab->tableList->editItem(item);
        }
    }
}

// 处理表名变更
void GXTStudio::onTableNameChanged(const QModelIndex& index, const QString& newName)
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || currentTab->isWHM) return;
    
    if (!index.isValid() || newName.trimmed().isEmpty()) return;
    
    QString trimmedName = newName.trimmed();
    int tableIndex = index.row();
    
    // 检查表名是否已存在（排除当前表）
    for (size_t i = 0; i < currentTab->tables.size(); ++i) {
        if (i != static_cast<size_t>(tableIndex) && 
            QString::fromStdString(currentTab->tables[i].name) == trimmedName) {
            QMessageBox::warning(this, "错误", "表名已存在，请使用不同的名称。");
            updateTableList(); // 恢复原始名称
            return;
        }
    }
    
    // 更新表名
    if (tableIndex >= 0 && tableIndex < static_cast<int>(currentTab->tables.size())) {
        currentTab->tables[tableIndex].name = trimmedName.toStdString();
        showLogMessage(QString("表名已更新为: %1").arg(trimmedName));
    }
}




void GXTStudio::onSearchTextChanged()
{
    FileTab* tab = getCurrentTab();
    if (!tab || !tab->searchEdit) return;
    
    QString searchText = tab->searchEdit->text();
    if (searchText != m_searchState.searchText) {
        // 使用简单的防抖搜索
        QTimer::singleShot(300, this, [this, searchText]() {
            if (searchText == getCurrentTab()->searchEdit->text()) {
                performSearch(searchText);
            }
        });
    }
}

void GXTStudio::performSearch(const QString& searchText)
{
    FileTab* tab = getCurrentTab();
    if (!tab) return;
    
    // 获取搜索选项
    bool caseSensitive = tab->caseSensitiveButton ? tab->caseSensitiveButton->isChecked() : false;
    bool useRegex = tab->regexButton ? tab->regexButton->isChecked() : false;
    
    // 清除之前的高亮
    clearSearchHighlight();
    
    if (searchText.isEmpty()) {
        m_searchState.clear();
        showLogMessage("搜索已清空");
        return;
    }
    
    // 编译正则表达式（如果需要）
    QRegularExpression regex;
    if (useRegex) {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!caseSensitive) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        regex.setPattern(searchText);
        regex.setPatternOptions(options);
        
        if (!regex.isValid()) {
            showLogMessage(QString("正则表达式错误: %1").arg(regex.errorString()));
            m_searchState.clear();
            return;
        }
    }
    
    // 更新搜索状态
    m_searchState.searchText = searchText;
    m_searchState.currentTableIndex = 0;
    m_searchState.currentEntryIndex = -1;
    m_searchState.allMatches.clear();
    m_searchState.caseSensitive = caseSensitive;
    m_searchState.useRegex = useRegex;
    
    Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    
    // 在所有表中搜索匹配项
    if (tab->isWHM) {
        // WHM文件搜索
        for (size_t i = 0; i < tab->whmEntries.size(); ++i) {
            // 安全检查
            if (i >= tab->whmEntries.size() || tab->whmEntries.empty()) break;
            
            const auto& entry = tab->whmEntries[i];
            QString hashStr = "0x" + QString::number(entry.hash, 16).toUpper().rightJustified(8, '0');
            QString text = hashStr + " " + QString::fromStdString(entry.text);
            
            bool matched = false;
            if (useRegex) {
                matched = regex.match(text).hasMatch();
            } else {
                matched = text.contains(searchText, cs);
            }
            
            if (matched) {
                m_searchState.allMatches.emplace_back(0, static_cast<int>(i));
            }
        }
    } else {
        // GXT文件搜索
        for (size_t tableIdx = 0; tableIdx < tab->tables.size(); ++tableIdx) {
            const auto& table = tab->tables[tableIdx];
            for (size_t entryIdx = 0; entryIdx < table.entries.size(); ++entryIdx) {
                const auto& entry = table.entries[entryIdx];
                QString text = QString::fromStdString(entry.key) + " " + QString::fromStdString(entry.value);
                
                bool matched = false;
                if (useRegex) {
                    matched = regex.match(text).hasMatch();
                } else {
                    matched = text.contains(searchText, cs);
                }
                
                if (matched) {
                    m_searchState.allMatches.emplace_back(static_cast<int>(tableIdx), static_cast<int>(entryIdx));
                }
            }
        }
    }
    
    m_searchState.isValid = !m_searchState.allMatches.empty();
    
    if (m_searchState.isValid) {
        QString modeText = useRegex ? " (正则表达式)" : "";
        if (caseSensitive && !useRegex) {
            modeText = " (区分大小写)";
        }
        showLogMessage(QString("找到 %1 个匹配项%2").arg(m_searchState.allMatches.size()).arg(modeText));
        // 跳转到第一个匹配项
        jumpToMatch(0);
    } else {
        showLogMessage("未找到匹配项");
    }
}

void GXTStudio::onSearchNext()
{
    FileTab* tab = getCurrentTab();
    if (!tab) return;
    
    // 如果没有搜索状态或搜索框为空，尝试从搜索框获取文本进行搜索
    if ((!m_searchState.isValid || m_searchState.allMatches.empty()) && 
        tab->searchEdit && !tab->searchEdit->text().isEmpty()) {
        performSearch(tab->searchEdit->text());
        return;
    }
    
    if (!m_searchState.isValid || m_searchState.allMatches.empty()) {
        return;
    }
    
    // 获取当前选中单元格的位置
    int currentTableRow = -1;
    int currentTableIndex = tab->currentTableIndex;
    
    if (tab->entryTableView && tab->entryTableView->selectionModel()) {
        QModelIndex currentIndex = tab->entryTableView->selectionModel()->currentIndex();
        if (currentIndex.isValid()) {
            currentTableRow = currentIndex.row();
        }
    }
    
    // 从当前选中位置开始查找下一个匹配项
    int startIndex = m_searchState.currentEntryIndex;
    
    // 如果有当前选中的行，尝试从该行附近开始搜索
    if (currentTableRow >= 0) {
        // 在当前表中查找从currentTableRow开始的匹配项
        for (int i = 0; i < static_cast<int>(m_searchState.allMatches.size()); ++i) {
            int searchIndex = (startIndex + 1 + i) % m_searchState.allMatches.size();
            const auto& match = m_searchState.allMatches[searchIndex];
            
            // 如果匹配项在当前表中且行号大于当前行，这就是下一个
            if (match.first == currentTableIndex && match.second > currentTableRow) {
                jumpToMatch(searchIndex);
                return;
            }
        }
        
        // 如果当前表中没有找到更大的行号，从当前表中下一行开始循环搜索
        for (int i = 0; i < static_cast<int>(m_searchState.allMatches.size()); ++i) {
            int searchIndex = (startIndex + 1 + i) % m_searchState.allMatches.size();
            const auto& match = m_searchState.allMatches[searchIndex];
            
            // 找到第一个匹配项（包括当前表中行号小于当前行的）
            jumpToMatch(searchIndex);
            return;
        }
    } else {
        // 没有选中行，使用原来的逻辑
        int nextIndex = m_searchState.currentEntryIndex + 1;
        if (nextIndex >= static_cast<int>(m_searchState.allMatches.size())) {
            nextIndex = 0; // 循环到第一个
            showLogMessage("已搜索到末尾，从第一个匹配项重新开始");
        }
        jumpToMatch(nextIndex);
    }
}

void GXTStudio::onSearchPrevious()
{
    FileTab* tab = getCurrentTab();
    if (!tab) return;
    
    // 如果没有搜索状态或搜索框为空，尝试从搜索框获取文本进行搜索
    if ((!m_searchState.isValid || m_searchState.allMatches.empty()) && 
        tab->searchEdit && !tab->searchEdit->text().isEmpty()) {
        performSearch(tab->searchEdit->text());
        if (m_searchState.isValid) {
            // 如果是新搜索，跳到最后一个匹配项
            jumpToMatch(static_cast<int>(m_searchState.allMatches.size()) - 1);
        }
        return;
    }
    
    if (!m_searchState.isValid || m_searchState.allMatches.empty()) {
        return;
    }
    
    // 获取当前选中单元格的位置
    int currentTableRow = -1;
    int currentTableIndex = tab->currentTableIndex;
    
    if (tab->entryTableView && tab->entryTableView->selectionModel()) {
        QModelIndex currentIndex = tab->entryTableView->selectionModel()->currentIndex();
        if (currentIndex.isValid()) {
            currentTableRow = currentIndex.row();
        }
    }
    
    // 从当前选中位置开始查找上一个匹配项
    int startIndex = m_searchState.currentEntryIndex;
    
    // 如果有当前选中的行，尝试从该行附近开始搜索
    if (currentTableRow >= 0) {
        // 在当前表中查找从currentTableRow开始向上的匹配项
        for (int i = 0; i < static_cast<int>(m_searchState.allMatches.size()); ++i) {
            int searchIndex = (startIndex - 1 - i + m_searchState.allMatches.size()) % m_searchState.allMatches.size();
            const auto& match = m_searchState.allMatches[searchIndex];
            
            // 如果匹配项在当前表中且行号小于当前行，这就是上一个
            if (match.first == currentTableIndex && match.second < currentTableRow) {
                jumpToMatch(searchIndex);
                return;
            }
        }
        
        // 如果当前表中没有找到更小的行号，从当前表中上一行开始循环搜索
        for (int i = 0; i < static_cast<int>(m_searchState.allMatches.size()); ++i) {
            int searchIndex = (startIndex - 1 - i + m_searchState.allMatches.size()) % m_searchState.allMatches.size();
            const auto& match = m_searchState.allMatches[searchIndex];
            
            // 找到最后一个匹配项（包括当前表中行号大于当前行的）
            jumpToMatch(searchIndex);
            return;
        }
    } else {
        // 没有选中行，使用原来的逻辑
        int prevIndex = m_searchState.currentEntryIndex - 1;
        if (prevIndex < 0) {
            prevIndex = static_cast<int>(m_searchState.allMatches.size()) - 1; // 循环到最后一个
            showLogMessage("已搜索到开头，从最后一个匹配项重新开始");
        }
        jumpToMatch(prevIndex);
    }
}

void GXTStudio::jumpToMatch(int matchIndex)
{
    FileTab* tab = getCurrentTab();
    if (!tab || matchIndex < 0 || matchIndex >= static_cast<int>(m_searchState.allMatches.size())) {
        return;
    }
    
    const auto& match = m_searchState.allMatches[matchIndex];
    int targetTableIndex = match.first;
    int targetEntryIndex = match.second;
    
    // 如果需要切换表
    if (!tab->isWHM && targetTableIndex != tab->currentTableIndex) {
        switchToTable(targetTableIndex);
    }
    
    // 高亮显示匹配的条目
    highlightMatch(targetEntryIndex);
    
    // 更新当前匹配位置
    m_searchState.currentEntryIndex = matchIndex;
    
    // 显示当前匹配信息
    QString tableName;
    QString keyText;
    QString valueText;
    
    if (tab->isWHM) {
        tableName = "WHM Entries";
        // 安全检查
        if (targetEntryIndex < 0 || targetEntryIndex >= static_cast<int>(tab->whmEntries.size()) || 
            tab->whmEntries.empty()) {
            showLogMessage("错误：无效的条目索引");
            return;
        }
        
        const auto& entry = tab->whmEntries[targetEntryIndex];
        keyText = "0x" + QString::number(entry.hash, 16).toUpper().rightJustified(8, '0');
        valueText = QString::fromStdString(entry.text);
    } else {
        tableName = QString::fromStdString(tab->tables[targetTableIndex].name);
        const auto& entry = tab->tables[targetTableIndex].entries[targetEntryIndex];
        keyText = QString::fromStdString(entry.key);
        valueText = QString::fromStdString(entry.value);
    }
    
    showLogMessage(QString("匹配 %1/%2: 表[%3] %4 = %5")
                   .arg(matchIndex + 1)
                   .arg(m_searchState.allMatches.size())
                   .arg(tableName)
                   .arg(keyText)
                   .arg(valueText.left(50) + (valueText.length() > 50 ? "..." : "")));
}

void GXTStudio::highlightMatch(int entryIndex)
{
    FileTab* tab = getCurrentTab();
    if (!tab || !tab->entryTableView || !tab->entryTableModel) return;
    
    // 清除之前的高亮
    clearSearchHighlight();
    
    // 选择并滚动到匹配的条目
    QModelIndex modelIndex = tab->entryTableModel->index(entryIndex, 0);
    if (modelIndex.isValid()) {
        tab->entryTableView->selectRow(entryIndex);
        tab->entryTableView->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);
        
        // 使用统一的高亮委托设置搜索文本
        auto* delegate = qobject_cast<OptimizedItemDelegate*>(tab->entryTableView->itemDelegate());
        if (delegate) {
            delegate->setSearchText(m_searchState.searchText, m_searchState.useRegex, m_searchState.caseSensitive);
            tab->entryTableView->viewport()->update(); // 仅更新视口
        }
    }
}

void GXTStudio::clearSearchHighlight()
{
    FileTab* tab = getCurrentTab();
    if (!tab || !tab->entryTableView) return;
    
    // 使用单例委托清除高亮
    auto* delegate = qobject_cast<OptimizedItemDelegate*>(tab->entryTableView->itemDelegate());
    if (delegate) {
        delegate->setSearchText(QString());
        delegate->clearCache(); // 清理缓存
        tab->entryTableView->viewport()->update(); // 仅更新视口，不重绘整个表格
    }
}

void GXTStudio::onCodeTableConvert()
{
    // 旧版功能已弃用，改用新的码表转换下拉按钮
    onMountCodeTable();
}

void GXTStudio::onMountCodeTable()
{
    // 检查是否为只读模式
    if (m_isReadOnly) {
        QMessageBox::warning(this, "只读模式", "当前为只读模式，无法使用码表转换功能。\n请先关闭只读模式：编辑 → 只读模式");
        return;
    }

    // 打开文件选择对话框
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择码表文件",
        "",
        "文本文件 (*.txt);;所有文件 (*.*)"
    );

    if (filePath.isEmpty()) {
        return; // 用户取消了选择
    }

    // 挂载码表
    if (m_codeTableConverter->mountTable(filePath)) {
        // 挂载成功
        bool enabled = !m_isReadOnly;
        m_convertCodeTableAction->setEnabled(enabled);
        m_unmountCodeTableAction->setEnabled(enabled);

        // 更新按钮的信号连接：从挂载切换到转换
        disconnect(m_codeTableButton, &QToolButton::clicked, this, &GXTStudio::onMountCodeTable);
        connect(m_codeTableButton, &QToolButton::clicked, this, &GXTStudio::onConvertCodeTable);

        // 更新按钮文本为执行转换
        updateCodeTableButtonText();

        // 在状态栏显示挂载信息
        showLogMessage(QString("已挂载码表: %1").arg(m_codeTableConverter->getTableName()));
    } else {
        // 挂载失败
        QMessageBox::warning(this, "挂载失败", "码表文件格式不正确或为空");
    }
}

void GXTStudio::onConvertCodeTable()
{
    // 检查是否为只读模式
    if (m_isReadOnly) {
        QMessageBox::warning(this, "只读模式", "当前为只读模式，无法执行码表转换。\n请先关闭只读模式：编辑 → 只读模式");
        return;
    }

    if (!m_codeTableConverter->isTableMounted()) {
        QMessageBox::information(this, "提示", "请先挂载码表文件");
        return;
    }

    // 记录转换前的状态
    CodeTableState previousState = m_codeTableConverter->getState();
    QString conversionText;

    // 执行转换
    if (m_codeTableConverter->convert(this)) {
        // 转换成功
        CodeTableState currentState = m_codeTableConverter->getState();

        // 根据状态变化确定转换类型
        if (previousState == CodeTableState::Original && currentState == CodeTableState::Converted) {
            conversionText = "正向转换 (码表字符 -> Unicode)";
        } else if (previousState == CodeTableState::Converted && currentState == CodeTableState::Original) {
            conversionText = "反向转换 (Unicode -> 码表字符)";
        } else {
            // 状态未变化或其他情况
            conversionText = "转换完成";
        }

        showLogMessage(QString("已完成%1，点击可继续反向转换").arg(conversionText));

        // 更新按钮文本
        updateCodeTableButtonText();
    }
}

void GXTStudio::onUnmountCodeTable()
{
    if (!m_codeTableConverter->isTableMounted()) {
        QMessageBox::information(this, "提示", "当前未挂载任何码表");
        return;
    }

    // 确认对话框
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认卸载",
        QString("确定要卸载码表 '%1' 吗？").arg(m_codeTableConverter->getTableName()),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        // 卸载码表
        m_codeTableConverter->unmountTable();
        
        // 更新按钮状态
        m_convertCodeTableAction->setEnabled(false);
        m_unmountCodeTableAction->setEnabled(false);
        
        // 更新按钮的信号连接：从转换切换回挂载
        disconnect(m_codeTableButton, &QToolButton::clicked, this, &GXTStudio::onConvertCodeTable);
        connect(m_codeTableButton, &QToolButton::clicked, this, &GXTStudio::onMountCodeTable);
        
        // 更新按钮文本
        updateCodeTableButtonText();
        
        // 在状态栏显示卸载信息
        showLogMessage("已卸载码表");
    }
}

void GXTStudio::updateCodeTableButtonText()
{
    if (!m_codeTableButton || !m_codeTableConverter) {
        return;
    }

    if (!m_codeTableConverter->isTableMounted()) {
        // 未挂载码表，显示"挂载码表"
        m_codeTableButton->setText("挂载码表(&C)");
    } else {
        // 已挂载码表，根据当前状态显示相应的文本
        CodeTableState state = m_codeTableConverter->getState();
        if (state == CodeTableState::Original) {
            m_codeTableButton->setText("码表→Unicode(&C)");
        } else if (state == CodeTableState::Converted) {
            m_codeTableButton->setText("Unicode→码表(&C)");
        }
    }
}

void GXTStudio::onSmartTranslate()
{
    // 检查是否已配置API密钥
    QSettings settings;
    QString apiKey = settings.value("Translate/apiKey", "").toString();
    if (apiKey.trimmed().isEmpty()) {
        onConfigTranslate();
    } else {
        onExecuteTranslate();
    }
}

void GXTStudio::addNewEntry()
{
    if (m_isReadOnly) return;
    
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) return;
    
    bool ok;
    QString key = QInputDialog::getText(this, "添加条目", "键名:", QLineEdit::Normal, "", &ok);
    if (!ok || key.isEmpty()) return;
    
    QString value = QInputDialog::getText(this, "添加条目", "值:", QLineEdit::Normal, "", &ok);
    if (!ok) return;
    
    int tableIndex = currentTab->currentTableIndex;
    if (tableIndex >= 0 && tableIndex < static_cast<int>(currentTab->tables.size())) {
        auto& table = currentTab->tables[tableIndex];
        
        // 检查键是否已存在
        for (const auto& entry : table.entries) {
            if (entry.key == key.toStdString()) {
                QMessageBox::warning(this, "添加失败", "无法添加条目，键名已存在");
                return;
            }
        }
        
        // 添加新条目
        GXTEntry newEntry;
        newEntry.key = key.toStdString();
        newEntry.value = value.toStdString();
        table.entries.push_back(newEntry);

        currentTab->isModified = true;
        updateEntryTable();
        updateWindowTitle();
        showLogMessage("已添加新条目: " + key);

        // 触发自动保存
        if (m_autoSaveEnabled && m_autoSaveTimer) {
            m_autoSaveTimer->stop();
            m_autoSaveTimer->start();
        }
    }
}

void GXTStudio::deleteEntry()
{
    if (m_isReadOnly) return;
    
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableView || !currentTab->entryTableModel) return;
    
    int currentRow = currentTab->entryTableView->currentIndex().row();
    if (currentRow >= 0) {
        if (QMessageBox::question(this, "确认删除",
            "确定要删除选中的条目吗？",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            
            int tableIndex = currentTab->currentTableIndex;
            if (tableIndex >= 0 && tableIndex < static_cast<int>(currentTab->tables.size())) {
                auto& table = currentTab->tables[tableIndex];
                if (currentRow < static_cast<int>(table.entries.size())) {
                    table.entries.erase(table.entries.begin() + currentRow);
                    currentTab->isModified = true;
                    updateEntryTable();
                    updateWindowTitle();
                    showLogMessage("已删除条目");

                    // 触发自动保存
                    if (m_autoSaveEnabled && m_autoSaveTimer) {
                        m_autoSaveTimer->stop();
                        m_autoSaveTimer->start();
                    }
                }
            }
        }
    }
}

void GXTStudio::deleteSelectedRows()
{
    if (m_isReadOnly) return;
    
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableView || !currentTab->entryTableModel) return;
    
    // 获取选中的行（去重）
    QSet<int> selectedRowsSet;
    QModelIndexList selectedIndexes = currentTab->entryTableView->selectionModel()->selectedIndexes();
    for (const QModelIndex& index : selectedIndexes) {
        selectedRowsSet.insert(index.row());
    }
    
    if (selectedRowsSet.isEmpty()) return;
    
    // 按行号从大到小排序，以便从后往前删除
    QList<int> selectedRows = selectedRowsSet.values();
    std::sort(selectedRows.begin(), selectedRows.end(), std::greater<int>());
    
    // 确认删除
    int count = selectedRows.size();
    if (QMessageBox::question(this, "确认删除",
        QString("确定要删除选中的 %1 个条目吗？").arg(count),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        
        int tableIndex = currentTab->currentTableIndex;
        if (tableIndex >= 0 && tableIndex < static_cast<int>(currentTab->tables.size())) {
            auto& table = currentTab->tables[tableIndex];
            
            // 从后往前删除，避免索引变化
            for (int row : selectedRows) {
                if (row < static_cast<int>(table.entries.size())) {
                    table.entries.erase(table.entries.begin() + row);
                }
            }
            
            currentTab->isModified = true;
            updateEntryTable();
            updateWindowTitle();
            showLogMessage(QString("已删除 %1 个条目").arg(count));

            // 触发自动保存
            if (m_autoSaveEnabled && m_autoSaveTimer) {
                m_autoSaveTimer->stop();
                m_autoSaveTimer->start();
            }
        }
    }
}

void GXTStudio::addNewTable()
{
    if (m_isReadOnly) {
        QMessageBox::information(this, "提示", "当前处于只读模式，无法添加新表。请先切换到编辑模式。");
        return;
    }
    
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) {
        QMessageBox::warning(this, "错误", "未找到当前标签页");
        return;
    }
    
    // 生成默认表名
    QString defaultName = QString("NewTable%1").arg(currentTab->tables.size() + 1);
    for (int i = 1; i <= 1000; i++) {
        QString testName = QString("NewTable%1").arg(i);
        bool exists = false;
        for (const auto& table : currentTab->tables) {
            if (table.name == testName.toStdString()) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            defaultName = testName;
            break;
        }
    }
    
    // 创建自定义对话框
    QDialog dialog(this);
    dialog.setWindowTitle("添加新表");
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);
    
    // 标题标签
    QLabel* titleLabel = new QLabel("创建新表格", &dialog);
    titleLabel->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: #2c3e50;"
    );
    layout->addWidget(titleLabel);
    
    // 表名输入区域
    QLabel* nameLabel = new QLabel("表名称:", &dialog);
    nameLabel->setStyleSheet(
        "font-size: 13px; color: #5a6c7d; margin-top: 8px;"
    );
    layout->addWidget(nameLabel);
    
    QLineEdit* nameEdit = new QLineEdit(defaultName, &dialog);
    nameEdit->setStyleSheet(R"(
        QLineEdit {
            border: 2px solid #dcdfe6;
            border-radius: 4px;
            padding: 8px 12px;
            font-size: 13px;
            color: #303133;
            background-color: white;
        }
        QLineEdit:focus {
            border-color: #409eff;
            outline: none;
        }
    )");
    nameEdit->selectAll();
    layout->addWidget(nameEdit);
    
    // 提示信息
    QLabel* hintLabel = new QLabel("提示：表名不能为空，且不能与现有表名重复", &dialog);
    hintLabel->setStyleSheet(
        "font-size: 11px; color: #909399; margin-bottom: 8px;"
    );
    layout->addWidget(hintLabel);
    
    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();
    
    QPushButton* cancelButton = new QPushButton("取消", &dialog);
    cancelButton->setMinimumWidth(90);
    cancelButton->setMinimumHeight(32);
    cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: white;
            color: #606266;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #f5f7fa;
            border-color: #c0c4cc;
            color: #409eff;
        }
        QPushButton:pressed {
            background-color: #e6e8eb;
        }
    )");
    buttonLayout->addWidget(cancelButton);
    
    QPushButton* okButton = new QPushButton("确定", &dialog);
    okButton->setMinimumWidth(90);
    okButton->setMinimumHeight(32);
    okButton->setDefault(true);
    okButton->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #409eff, stop:1 #66b1ff);
            color: white;
            border: 1px solid #3a8ee6;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #66b1ff, stop:1 #409eff);
            border-color: #337ecc;
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a8ee6, stop:1 #337ecc);
        }
    )");
    buttonLayout->addWidget(okButton);
    
    layout->addLayout(buttonLayout);
    
    // 自动聚焦到输入框
    nameEdit->setFocus();
    
    // 连接信号
    bool accepted = false;
    connect(okButton, &QPushButton::clicked, [&]() {
        QString tableName = nameEdit->text().trimmed();
        
        if (tableName.isEmpty()) {
            QMessageBox::warning(&dialog, "输入错误", "表名不能为空！");
            nameEdit->setFocus();
            return;
        }
        
        // 检查表名是否已存在
        for (const auto& table : currentTab->tables) {
            if (table.name == tableName.toStdString()) {
                QMessageBox::warning(&dialog, "输入错误", 
                    QString("表名 '%1' 已存在，请使用其他名称！").arg(tableName));
                nameEdit->selectAll();
                nameEdit->setFocus();
                return;
            }
        }
        
        accepted = true;
        dialog.accept();
    });
    
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(nameEdit, &QLineEdit::returnPressed, okButton, &QPushButton::click);
    
    // 显示对话框
    if (dialog.exec() != QDialog::Accepted || !accepted) {
        return;
    }
    
    QString tableName = nameEdit->text().trimmed();
    
    // 添加新表
    GXTTabl newTable;
    newTable.name = tableName.toStdString();
    currentTab->tables.push_back(newTable);
    
    currentTab->isModified = true;
    updateTableList();
    updateEntryTable();
    updateWindowTitle();
    
    // 切换到新添加的表
    switchToTable(currentTab->tables.size() - 1);

    showLogMessage("已添加新表: " + tableName);

    // 显示成功提示
    QMessageBox::information(this, "成功", QString("表 '%1' 已成功创建！").arg(tableName));

    // 触发自动保存
    if (m_autoSaveEnabled && m_autoSaveTimer) {
        m_autoSaveTimer->stop();
        m_autoSaveTimer->start();
    }
}

void GXTStudio::onTableDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;

    // 获取表索引
    int tableIndex = item->data(Qt::UserRole).toInt();

    // 双击时重命名表
    renameTable(tableIndex);
}

void GXTStudio::renameTable(int tableIndex)
{
    if (m_isReadOnly) {
        QMessageBox::information(this, "提示", "当前处于只读模式，无法重命名表。请先切换到编辑模式。");
        return;
    }

    FileTab* currentTab = getCurrentTab();
    if (!currentTab) return;

    if (tableIndex < 0 || tableIndex >= static_cast<int>(currentTab->tables.size())) {
        QMessageBox::warning(this, "错误", "无效的表索引");
        return;
    }

    // 获取当前表名
    QString currentTableName = QString::fromStdString(currentTab->tables[tableIndex].name);

    // 创建自定义对话框
    QDialog dialog(this);
    dialog.setWindowTitle("重命名表");
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setMinimumWidth(400);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    // 标题标签
    QLabel* titleLabel = new QLabel("重命名表格", &dialog);
    titleLabel->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: #2c3e50;"
    );
    layout->addWidget(titleLabel);

    // 原表名提示
    QLabel* oldNameLabel = new QLabel(QString("原表名: %1").arg(currentTableName), &dialog);
    oldNameLabel->setStyleSheet(
        "font-size: 13px; color: #909399; margin-top: 8px;"
    );
    layout->addWidget(oldNameLabel);

    // 新表名输入区域
    QLabel* nameLabel = new QLabel("新表名:", &dialog);
    nameLabel->setStyleSheet(
        "font-size: 13px; color: #5a6c7d; margin-top: 8px;"
    );
    layout->addWidget(nameLabel);

    QLineEdit* nameEdit = new QLineEdit(currentTableName, &dialog);
    nameEdit->setStyleSheet(R"(
        QLineEdit {
            border: 2px solid #dcdfe6;
            border-radius: 4px;
            padding: 8px 12px;
            font-size: 13px;
            color: #303133;
            background-color: white;
        }
        QLineEdit:focus {
            border-color: #409eff;
            outline: none;
        }
    )");
    nameEdit->selectAll();
    layout->addWidget(nameEdit);

    // 提示信息
    QLabel* hintLabel = new QLabel("提示：表名不能为空，且不能与现有表名重复", &dialog);
    hintLabel->setStyleSheet(
        "font-size: 11px; color: #909399; margin-bottom: 8px;"
    );
    layout->addWidget(hintLabel);

    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();

    QPushButton* cancelButton = new QPushButton("取消", &dialog);
    cancelButton->setMinimumWidth(90);
    cancelButton->setMinimumHeight(32);
    cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: white;
            color: #606266;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #f5f7fa;
            border-color: #c0c4cc;
            color: #409eff;
        }
        QPushButton:pressed {
            background-color: #e6e8eb;
        }
    )");
    buttonLayout->addWidget(cancelButton);

    QPushButton* okButton = new QPushButton("确定", &dialog);
    okButton->setMinimumWidth(90);
    okButton->setMinimumHeight(32);
    okButton->setDefault(true);
    okButton->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #409eff, stop:1 #66b1ff);
            color: white;
            border: 1px solid #3a8ee6;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #66b1ff, stop:1 #409eff);
            border-color: #337ecc;
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a8ee6, stop:1 #337ecc);
        }
    )");
    buttonLayout->addWidget(okButton);

    layout->addLayout(buttonLayout);

    // 自动聚焦到输入框
    nameEdit->setFocus();

    // 连接信号
    bool accepted = false;
    connect(okButton, &QPushButton::clicked, [&]() {
        QString newTableName = nameEdit->text().trimmed();

        if (newTableName.isEmpty()) {
            QMessageBox::warning(&dialog, "输入错误", "表名不能为空！");
            nameEdit->setFocus();
            return;
        }

        // 检查表名是否已存在
        for (int i = 0; i < static_cast<int>(currentTab->tables.size()); i++) {
            if (i != tableIndex && currentTab->tables[i].name == newTableName.toStdString()) {
                QMessageBox::warning(&dialog, "输入错误",
                    QString("表名 '%1' 已存在，请使用其他名称！").arg(newTableName));
                nameEdit->selectAll();
                nameEdit->setFocus();
                return;
            }
        }

        accepted = true;
        dialog.accept();
    });

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(nameEdit, &QLineEdit::returnPressed, okButton, &QPushButton::click);

    // 显示对话框
    if (dialog.exec() != QDialog::Accepted || !accepted) {
        return;
    }

    QString newTableName = nameEdit->text().trimmed();

    // 重命名表
    currentTab->tables[tableIndex].name = newTableName.toStdString();
    currentTab->isModified = true;

    updateTableList();
    updateEntryTable();
    updateWindowTitle();

    // 重新选中该表
    QListWidgetItem* newItem = currentTab->tableList->item(tableIndex);
    if (newItem) {
        currentTab->tableList->setCurrentItem(newItem);
    }

    showLogMessage("已重命名表: " + currentTableName + " -> " + newTableName);

    // 显示成功提示
    QMessageBox::information(this, "成功", QString("表 '%1' 已成功重命名为 '%2'！").arg(currentTableName, newTableName));

    // 触发自动保存
    if (m_autoSaveEnabled && m_autoSaveTimer) {
        m_autoSaveTimer->stop();
        m_autoSaveTimer->start();
    }
}

void GXTStudio::addEntryFromInputs(QLineEdit* keyEdit, QLineEdit* valueEdit)
{
    if (m_isReadOnly) {
        QMessageBox::information(this, "提示", "当前处于只读模式，无法添加条目。请先切换到编辑模式。");
        return;
    }

    FileTab* currentTab = getCurrentTab();
    if (!currentTab || currentTab->isWHM) {
        QMessageBox::warning(this, "错误", "当前文件格式不支持添加条目");
        return;
    }

    if (currentTab->currentTableIndex < 0 || currentTab->currentTableIndex >= static_cast<int>(currentTab->tables.size())) {
        QMessageBox::warning(this, "错误", "请先选择一个表格");
        return;
    }

    QString keyText = keyEdit->text().trimmed();
    QString valueText = valueEdit->text();

    if (keyText.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "键不能为空！");
        keyEdit->setFocus();
        return;
    }

    qInfo() << "开始添加条目，输入键: " << keyText << ", 值: " << valueText;

    // 检查键是否已存在
    GXTTabl& currentTable = currentTab->tables[currentTab->currentTableIndex];

    // 标准化输入的键用于比较
    std::string normalizedInputKey = normalizeKeyStdString(keyText.toStdString());
    qInfo() << "标准化后的键: " << QString::fromStdString(normalizedInputKey);
    
    // 判断输入的是hash还是原始字符串（只检查是否有0x前缀）
    bool isHashFormat = keyText.startsWith("0x", Qt::CaseInsensitive);
    
    // 如果输入的不是hash格式，需要计算hash进行比较
    std::string compareKey = normalizedInputKey;
    if (!isHashFormat) {
        // 只有GTA_SA和GTA_IV版本才进行hash转换，其他版本使用明文
        if (currentTab->version == GXTVersion::GTA_IV || currentTab->version == GXTVersion::GTA_SA) {
            uint32_t inputHash;
            // 尝试在IVTKEY映射中反向查找（GTA IV）
            if (currentTab->version == GXTVersion::GTA_IV && GXTTableModel::findIVTHash(keyText, inputHash)) {
                compareKey = QString("%1").arg(inputHash, 8, 16, QChar('0')).toStdString();
                qInfo() << "从IVTKEY映射中找到: " << keyText << " -> 0x" << QString::number(inputHash, 16);
            }
            // 尝试在SATKEY映射中反向查找（GTA SA）
            else if (currentTab->version == GXTVersion::GTA_SA && GXTTableModel::findSATHash(keyText, inputHash)) {
                compareKey = QString("%1").arg(inputHash, 8, 16, QChar('0')).toStdString();
                qInfo() << "从SATKEY映射中找到: " << keyText << " -> 0x" << QString::number(inputHash, 16);
            }
            // 否则计算对应的哈希值
            else {
                qWarning() << "lst映射中未找到: " << keyText << "，准备计算hash";
                if (currentTab->version == GXTVersion::GTA_IV) {
                    // 确保先转换为小写再计算
                    std::string lowerKey = keyText.toLower().toStdString();
                    qInfo() << "转换为小写: " << QString::fromStdString(lowerKey);
                    inputHash = GXTEditor::calculateGTA4Hash(lowerKey);
                    qWarning() << "计算GTA4 hash: " << keyText << " (小写: " << QString::fromStdString(lowerKey) << ") -> 0x" << QString::number(inputHash, 16);
                } else if (currentTab->version == GXTVersion::GTA_SA) {
                    // GTA SA使用CKeyGen计算hash（内部会转为大写）
                    std::string upperKey = keyText.toUpper().toStdString();
                    qInfo() << "转换为大写: " << QString::fromStdString(upperKey);
                    inputHash = CKeyGen::GetUppercaseKey(upperKey.c_str());
                    qWarning() << "计算SA hash: " << keyText << " (大写: " << QString::fromStdString(upperKey) << ") -> 0x" << QString::number(inputHash, 16);
                }
                compareKey = QString("%1").arg(inputHash, 8, 16, QChar('0')).toStdString();
                qWarning() << "最终使用compareKey: " << QString::fromStdString(compareKey);
            }
        } else {
            // GTA_III、GTA_VC、GXT2等版本使用明文
            compareKey = normalizedInputKey;
        }
    }
    
    for (const auto& entry : currentTable.entries) {
        if (QString::fromStdString(entry.key).toLower() == QString::fromStdString(compareKey).toLower()) {
            QMessageBox::warning(this, "输入错误", QString("键 '%1' 已存在，请使用不同的键！").arg(keyText));
            keyEdit->selectAll();
            keyEdit->setFocus();
            return;
        }
    }

    // 添加新条目
    GXTEntry newEntry;
    newEntry.key = compareKey;  // 使用计算/查找后的hash值（用于保存）
    newEntry.value = valueText.toStdString();
    newEntry.originalKey = keyText.toStdString();  // 保存原始输入（用于显示）
    currentTable.entries.push_back(newEntry);

    // 更新UI
    currentTab->isModified = true;
    updateEntryTable();
    updateWindowTitle();

    // 清空输入框
    keyEdit->clear();
    valueEdit->clear();
    keyEdit->setFocus();

    showLogMessage("已添加条目: " + keyText + " = " + valueText);

    // 触发自动保存
    if (m_autoSaveEnabled && m_autoSaveTimer) {
        m_autoSaveTimer->stop();
        m_autoSaveTimer->start();
    }

    // 滚动到新添加的条目
    if (currentTab->entryTableView) {
        int newRow = static_cast<int>(currentTable.entries.size()) - 1;
        currentTab->entryTableView->scrollToBottom();
        QModelIndex newIndex = currentTab->entryTableModel->index(newRow, 0);
        currentTab->entryTableView->selectRow(newRow);
    }
}


void GXTStudio::deleteTable()
{
    if (m_isReadOnly) return;
    
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->tableList) return;
    
    // 获取当前选中的行
    QListWidgetItem* currentItem = currentTab->tableList->currentItem();
    if (!currentItem) return;
    
    QString itemText = currentItem->text();
    // 从文本中提取表名（去掉后面的条目数部分）
    QStringList parts = itemText.split("  ");
    QString tableName = parts.isEmpty() ? "Unknown" : parts[0];
    int tableIndex = currentItem->data(Qt::UserRole).toInt();
    
    if (QMessageBox::question(this, "确认删除", 
        QString("确定要删除表 '%1' 吗？\n这将删除表中的所有条目！").arg(tableName), 
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        
        if (tableIndex >= 0 && tableIndex < static_cast<int>(currentTab->tables.size())) {
            currentTab->tables.erase(currentTab->tables.begin() + tableIndex);
            currentTab->currentTableIndex = 0; // 重置到第一个表
            currentTab->isModified = true;
            updateTableList();
            updateEntryTable();
            updateWindowTitle();
            showLogMessage("已删除表: " + tableName);

            // 触发自动保存
            if (m_autoSaveEnabled && m_autoSaveTimer) {
                m_autoSaveTimer->stop();
                m_autoSaveTimer->start();
            }
        }
    }
}

void GXTStudio::onDeleteTableFromList(const QModelIndex& index)
{
    if (m_isReadOnly) return;
    
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) return;
    
    // 获取表格索引
    int tableIndex = index.data(Qt::UserRole).toInt();
    QString tableName = index.data(Qt::UserRole + 1).toString();
    
    // 确认删除
    if (QMessageBox::question(this, "确认删除", 
        QString("确定要删除表 '%1' 吗？\n这将删除表中的所有条目！").arg(tableName), 
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        
        if (!currentTab->isWHM && tableIndex >= 0 && tableIndex < static_cast<int>(currentTab->tables.size())) {
            currentTab->tables.erase(currentTab->tables.begin() + tableIndex);
            currentTab->currentTableIndex = 0; // 重置到第一个表
            currentTab->isModified = true;
            currentTab->listCache.needsRebuild = true; // 标记缓存需要重建
            updateTableList();
            updateEntryTable();
            updateWindowTitle();
            showLogMessage("已删除表: " + tableName);
        } else if (currentTab->isWHM) {
            QMessageBox::warning(this, "无法删除", "WHM 文件不能删除表格！");
        }
    }
}

void GXTStudio::showContextMenu(const QPoint& pos)
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableView) return;

    // 获取选中的项目
    QModelIndexList selectedIndexes = currentTab->entryTableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    // 创建右键菜单
    QMenu* contextMenu = new QMenu(this);

    // 添加编辑菜单项（仅在非只读模式下显示）
    if (!m_isReadOnly) {
        QAction* editAction = contextMenu->addAction("编辑为");
        editAction->setShortcut(QKeySequence("Ctrl+E"));
        connect(editAction, &QAction::triggered, this, &GXTStudio::editCellAsDialog);

        // 添加删除菜单项
        QAction* deleteAction = contextMenu->addAction("删除选定行");
        deleteAction->setShortcut(QKeySequence("Delete"));
        connect(deleteAction, &QAction::triggered, this, &GXTStudio::deleteSelectedRows);

        contextMenu->addSeparator();

        // 添加翻译菜单项（始终显示，点击时检查是否配置API密钥）
        QAction* translateAction = contextMenu->addAction("🌐 翻译选定行");
        translateAction->setShortcut(QKeySequence("Ctrl+T"));
        connect(translateAction, &QAction::triggered, this, &GXTStudio::translateSelectedRows);
        contextMenu->addSeparator();
    }

    // 添加菜单项
    QAction* copyKeyAction = contextMenu->addAction("复制键/Hash");
    QAction* copyValueAction = contextMenu->addAction("复制值");
    QAction* copyKeyValueAction = contextMenu->addAction("复制键值对");

    // 连接信号
    connect(copyKeyAction, &QAction::triggered, this, &GXTStudio::copySelectedKeys);
    connect(copyValueAction, &QAction::triggered, this, &GXTStudio::copySelectedValues);
    connect(copyKeyValueAction, &QAction::triggered, this, &GXTStudio::copySelectedKeyValues);

    // 显示菜单
    contextMenu->exec(currentTab->entryTableView->viewport()->mapToGlobal(pos));
    delete contextMenu;
}

void GXTStudio::copySelectedKeys()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableView || !currentTab->entryTableModel) return;
    
    QModelIndexList selectedIndexes = currentTab->entryTableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;
    
    QStringList keys;
    QSet<int> processedRows; // 避免重复处理同一行的不同列
    
    for (const QModelIndex& index : selectedIndexes) {
        int row = index.row();
        if (processedRows.contains(row)) continue;
        
        QModelIndex keyIndex = currentTab->entryTableModel->index(row, 0);
        if (keyIndex.isValid()) {
            QString key = currentTab->entryTableModel->data(keyIndex, Qt::DisplayRole).toString();
            keys << key;
            processedRows.insert(row);
        }
    }
    
    if (!keys.isEmpty()) {
        QApplication::clipboard()->setText(keys.join("\n"));
        showLogMessage(QString("已复制 %1 个键/Hash").arg(keys.size()));
    }
}

void GXTStudio::copySelectedValues()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableView || !currentTab->entryTableModel) return;
    
    QModelIndexList selectedIndexes = currentTab->entryTableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;
    
    QStringList values;
    QSet<int> processedRows; // 避免重复处理同一行的不同列
    
    for (const QModelIndex& index : selectedIndexes) {
        int row = index.row();
        if (processedRows.contains(row)) continue;
        
        QModelIndex valueIndex = currentTab->entryTableModel->index(row, 1);
        if (valueIndex.isValid()) {
            QString value = currentTab->entryTableModel->data(valueIndex, Qt::DisplayRole).toString();
            values << value;
            processedRows.insert(row);
        }
    }
    
    if (!values.isEmpty()) {
        QApplication::clipboard()->setText(values.join("\n"));
        showLogMessage(QString("已复制 %1 个值").arg(values.size()));
    }
}

void GXTStudio::copySelectedKeyValues()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableView || !currentTab->entryTableModel) return;
    
    QModelIndexList selectedIndexes = currentTab->entryTableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;
    
    QStringList keyValues;
    QSet<int> processedRows; // 避免重复处理同一行的不同列
    
    for (const QModelIndex& index : selectedIndexes) {
        int row = index.row();
        if (processedRows.contains(row)) continue;
        
        QModelIndex keyIndex = currentTab->entryTableModel->index(row, 0);
        QModelIndex valueIndex = currentTab->entryTableModel->index(row, 1);
        
        if (keyIndex.isValid() && valueIndex.isValid()) {
            QString key = currentTab->entryTableModel->data(keyIndex, Qt::DisplayRole).toString();
            QString value = currentTab->entryTableModel->data(valueIndex, Qt::DisplayRole).toString();
            keyValues << QString("%1=%2").arg(key, value);
            processedRows.insert(row);
        }
    }
    
    if (!keyValues.isEmpty()) {
        QApplication::clipboard()->setText(keyValues.join("\n"));
        showLogMessage(QString("已复制 %1 个键值对").arg(keyValues.size()));
    }
}

// 翻译选定行槽函数
void GXTStudio::translateSelectedRows()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableView || !currentTab->entryTableModel) return;

    // 检查是否已配置API密钥
    if (!m_smartTranslator || !m_smartTranslator->isReady()) {
        QMessageBox::warning(this, "警告", "翻译功能未配置，请先配置API密钥。");
        onConfigTranslate();
        return;
    }

    if (m_isReadOnly) {
        QMessageBox::warning(this, "警告", "当前为只读模式，无法执行翻译。请先切换到编辑模式。");
        return;
    }

    // 获取选中的行（只处理值列）
    QModelIndexList selectedIndexes = currentTab->entryTableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要翻译的行。");
        return;
    }

    // 收集选中的行（去重）
    QSet<int> selectedRows;
    for (const QModelIndex& index : selectedIndexes) {
        selectedRows.insert(index.row());
    }

    if (selectedRows.isEmpty()) {
        QMessageBox::information(this, "提示", "请选择要翻译的行。");
        return;
    }

    // 构建翻译任务列表
    QList<TranslateTask> tasks;
    const int MAX_SAFE_TASKS = SmartTranslator::MAX_SAFE_TASKS;
    const int MAX_QLIST_ITEMS = SmartTranslator::MAX_QLIST_SAFE_ITEMS;

    for (int row : selectedRows) {
        if (tasks.size() >= MAX_SAFE_TASKS) {
            QMessageBox::warning(this, "任务限制",
                QString("已达到最大任务数量限制: %1").arg(MAX_SAFE_TASKS));
            break;
        }

        // 获取键和值
        QModelIndex keyIndex = currentTab->entryTableModel->index(row, 0);
        QModelIndex valueIndex = currentTab->entryTableModel->index(row, 1);

        if (keyIndex.isValid() && valueIndex.isValid()) {
            QString key = currentTab->entryTableModel->data(keyIndex, Qt::DisplayRole).toString();
            QString value = currentTab->entryTableModel->data(valueIndex, Qt::DisplayRole).toString();

            // 跳过空值
            if (value.trimmed().isEmpty()) {
                continue;
            }

            // 检查值长度
            if (value.length() > 10000) {
                qWarning() << "行" << row << "的文本过长，将被跳过。";
                continue;
            }

            // TranslateTask 的 row 字段设置为实际的表格行号
            tasks.append(TranslateTask(key, value, row));
        }
    }

    if (tasks.isEmpty()) {
        QMessageBox::information(this, "提示", "没有有效的文本需要翻译。");
        return;
    }

    // 确认翻译
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认翻译",
        QString("即将翻译 %1 行文本。\n\n是否继续？").arg(tasks.size()),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    // 设置翻译进度对话框
    if (!m_translateProgressDialog) {
        m_translateProgressDialog = new MultiThreadProgressDialog(this);
    }
    m_translateProgressDialog->reset();
    m_translateProgressDialog->setTotalThreads(1);
    m_translateProgressDialog->setStartTime(QDateTime::currentMSecsSinceEpoch());
    m_translateProgressDialog->setWindowTitle("翻译选定行");
    m_translateProgressDialog->show();

    // 开始翻译
    showLogMessage(QString("开始翻译 %1 行文本").arg(tasks.size()));
    m_smartTranslator->translateTexts(tasks);
}

// 编辑操作槽函数
void GXTStudio::editCellAsDialog()
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableView || !currentTab->entryTableModel) return;
    
    // 获取当前选中的单元格
    QModelIndex currentIndex = currentTab->entryTableView->selectionModel()->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::information(this, "提示", "请先选择要编辑的单元格");
        return;
    }
    
    int row = currentIndex.row();
    int column = currentIndex.column();
    
    openEditDialog(row, column);
}

void GXTStudio::openEditDialog(int row, int column)
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->entryTableModel) return;
    
    if (row < 0 || row >= currentTab->entryTableModel->rowCount() ||
        column < 0 || column >= currentTab->entryTableModel->columnCount()) {
        return;
    }
    
    // 获取当前键和值
    QModelIndex keyIndex = currentTab->entryTableModel->index(row, 0);
    QModelIndex valueIndex = currentTab->entryTableModel->index(row, 1);
    
    QString currentKey = currentTab->entryTableModel->data(keyIndex, Qt::EditRole).toString();
    QString currentValue = currentTab->entryTableModel->data(valueIndex, Qt::EditRole).toString();
    
    // 创建编辑对话框
    QDialog editDialog(this);
    editDialog.setWindowTitle("编辑条目");
    editDialog.setModal(true);
    editDialog.resize(500, 300);
    
    QVBoxLayout* layout = new QVBoxLayout(&editDialog);
    
    // 键编辑
    QHBoxLayout* keyLayout = new QHBoxLayout();
    QLabel* keyLabel = new QLabel("键/Hash:");
    QLineEdit* keyEdit = new QLineEdit(currentKey);
    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(keyEdit);
    layout->addLayout(keyLayout);
    
    // 值编辑
    QHBoxLayout* valueLayout = new QHBoxLayout();
    QLabel* valueLabel = new QLabel("值:");
    QTextEdit* valueEdit = new QTextEdit(currentValue);
    valueEdit->setAcceptRichText(false);
    valueEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    valueEdit->setWordWrapMode(QTextOption::WordWrap);
    valueEdit->setMaximumHeight(200);
    valueLayout->addWidget(valueLabel);
    layout->addWidget(valueEdit);
    
    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* okButton = new QPushButton("确定");
    QPushButton* cancelButton = new QPushButton("取消");
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    // 连接信号
    connect(okButton, &QPushButton::clicked, &editDialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &editDialog, &QDialog::reject);
    
    // 设置焦点和选择
    if (column == 0) {
        keyEdit->setFocus();
        keyEdit->selectAll();
    } else {
        valueEdit->setFocus();
        valueEdit->selectAll();
    }
    
    // 显示对话框
    if (editDialog.exec() == QDialog::Accepted) {
        QString newKey = keyEdit->text().trimmed();
        QString newValue = valueEdit->toPlainText();
        
        // 验证键不为空
        if (newKey.isEmpty()) {
            QMessageBox::warning(this, "错误", "键/Hash不能为空");
            return;
        }
        
        // 如果键发生了变化，检查是否重复
        if (newKey != currentKey) {
            for (int i = 0; i < currentTab->entryTableModel->rowCount(); ++i) {
                if (i != row) {
                    QModelIndex checkKeyIndex = currentTab->entryTableModel->index(i, 0);
                    QString existingKey = currentTab->entryTableModel->data(checkKeyIndex, Qt::EditRole).toString();
                    if (existingKey == newKey) {
                        QMessageBox::warning(this, "错误", "键/Hash已存在，不能重复");
                        return;
                    }
                }
            }
        }
        
        // 应用更改
        currentTab->entryTableModel->setData(keyIndex, newKey, Qt::EditRole);
        currentTab->entryTableModel->setData(valueIndex, newValue, Qt::EditRole);
        
        // 标记为已修改
        currentTab->isModified = true;
        updateWindowTitle();
        
        // 重新选中编辑的行
        currentTab->entryTableView->selectRow(row);
        
        showLogMessage("条目已编辑");
    }
}

// 标签页操作槽函数
void GXTStudio::onTabChanged(int index)
{
    if (m_currentTabIndex == index) return;

    // 先保存前一个标签页索引，用于清理
    int previousTabIndex = m_currentTabIndex;

    // 清理前一个标签页的资源
    if (previousTabIndex >= 0 && previousTabIndex < static_cast<int>(m_fileTabs.size())) {
        FileTab* previousTab = &m_fileTabs[previousTabIndex];
        if (previousTab && previousTab != getCurrentTab()) {
            // 暂停前一个标签页的视图更新（但保持可用状态）
            if (previousTab->entryTableView) {
                previousTab->entryTableView->setUpdatesEnabled(false);
            }
            if (previousTab->tableList) {
                previousTab->tableList->setUpdatesEnabled(false);
            }
        }
    }

    // 更新索引
    m_currentTabIndex = index;

    // 获取新的当前标签页
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) {
        // 如果没有有效的当前标签页，恢复前一个标签页的状态
        if (previousTabIndex >= 0 && previousTabIndex < static_cast<int>(m_fileTabs.size())) {
            FileTab* previousTab = &m_fileTabs[previousTabIndex];
            if (previousTab) {
                if (previousTab->entryTableView) {
                    previousTab->entryTableView->setUpdatesEnabled(true);
                }
                if (previousTab->tableList) {
                    previousTab->tableList->setUpdatesEnabled(true);
                }
            }
        }
        return;
    }

    // 【关键修复】立即重新设置模型的tab指针，确保模型指向正确的FileTab
    if (currentTab->entryTableModel && currentTab->entryTableView) {
        // 强制重新设置模型，清除所有缓存
        currentTab->entryTableModel->setTab(currentTab);
    }

    // 暂停所有视图更新以防止闪烁和内存问题
    if (currentTab->entryTableView) {
        currentTab->entryTableView->setUpdatesEnabled(false);
    }
    if (currentTab->tableList) {
        currentTab->tableList->setUpdatesEnabled(false);
    }

    // 清理委托缓存
    if (currentTab->entryTableView) {
        auto* delegate = qobject_cast<OptimizedItemDelegate*>(currentTab->entryTableView->itemDelegate());
        if (delegate) {
            delegate->clearCache();
        }
    }

    // 确保视图可见
    if (currentTab->entryTableView) {
        currentTab->entryTableView->setVisible(true);
        currentTab->entryTableView->show();
    }
    if (currentTab->tableList) {
        currentTab->tableList->setVisible(true);
        currentTab->tableList->show();
    }

    // 【关键修复】捕获索引而不是指针，避免标签页移动后指针失效
    int currentTabIndex = m_currentTabIndex;

    // 【性能优化】减少延迟从50ms到10ms，加快标签页切换响应速度
    QTimer::singleShot(10, this, [this, currentTabIndex, previousTabIndex]() {
        // 【关键修复】每次都通过索引重新获取指针，避免指针失效
        FileTab* currentTab = (currentTabIndex >= 0 && currentTabIndex < static_cast<int>(m_fileTabs.size()))
                              ? &m_fileTabs[currentTabIndex] : nullptr;

        // 确保当前标签页仍然有效
        if (!currentTab || currentTabIndex != m_currentTabIndex) {
            return;
        }

        try {
            // 【关键修复】再次强制重置模型，确保数据完全同步
            if (currentTab->entryTableModel) {
                currentTab->entryTableModel->forceDataReset();
            }

            // 【关键修复】确保表格视图的模型已正确设置
            if (currentTab->entryTableView && currentTab->entryTableModel) {
                QAbstractItemModel* currentModel = currentTab->entryTableView->model();
                if (currentModel != currentTab->entryTableModel) {
                    // 如果模型不匹配，重新设置
                    currentTab->entryTableView->setModel(currentTab->entryTableModel);
                }
            }

            // 恢复视图更新
            if (currentTab->entryTableView) {
                currentTab->entryTableView->setUpdatesEnabled(true);
            }
            if (currentTab->tableList) {
                currentTab->tableList->setUpdatesEnabled(true);
            }

            // 【关键修复】强制刷新视口，确保内容立即显示
            if (currentTab->entryTableView) {
                currentTab->entryTableView->viewport()->update();
                currentTab->entryTableView->update();
            }

            // 更新表格列表，这会设置正确的 currentTableIndex 并更新模型
            // 注意：CharTable 没有 tableList，跳过 updateTableList
            if (!currentTab->isCharTable) {
                updateTableList();
            }

            // 不再单独调用 updateEntryTable()，因为 updateTableList 已经更新了模型
            updateWindowTitle();
            updateStatusBar();

            // 【关键修复】强制更新表格视图，确保数据立即显示
            if (currentTab->entryTableView) {
                currentTab->entryTableView->viewport()->repaint();
                currentTab->entryTableView->repaint();
            }

            // 如果启用了背景，更新搜索UI颜色以匹配当前背景
            if (m_backgroundEnabled) {
                updateSearchUIColors(*currentTab);
            }

            // 触发一次内存清理
            QTimer::singleShot(100, [this]() {
                cleanupDelegatesCache();
                QCoreApplication::processEvents();
            });
        } catch (...) {
            qWarning() << "Exception occurred during tab change";
            // 确保视图更新被重新启用
            if (currentTab && currentTab->entryTableView) {
                currentTab->entryTableView->setUpdatesEnabled(true);
            }
            if (currentTab && currentTab->tableList) {
                currentTab->tableList->setUpdatesEnabled(true);
            }
        }
    });
}

void GXTStudio::closeTab(int index)
{
    // 检查标签页索引是否有效
    if (index < 0 || index >= m_tabWidget->count()) {
        return;
    }

    // 检查是否为字符表标签页（不在 m_fileTabs 中）
    if (index >= static_cast<int>(m_fileTabs.size())) {
        // 直接关闭字符表标签页
        QWidget* widget = m_tabWidget->widget(index);
        m_tabWidget->removeTab(index);
        if (widget) {
            widget->deleteLater();
        }
        return;
    }

    // 处理普通 FileTab 标签页
    FileTab& tab = m_fileTabs[index];
    
    // 检查是否有未保存的更改
    if (tab.isModified) {
        int ret = QMessageBox::question(this, "保存更改",
            QString("文件 '%1' 有未保存的更改。\n是否保存？").arg(tab.fileName),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        
        if (ret == QMessageBox::Save) {
            // TODO: 实现 GXTParser 的 saveToFile 方法
            // 暂时直接关闭
        } else if (ret == QMessageBox::Cancel) {
            return;
        }
    }
    
    // 快速关闭：先移除UI，立即更新界面
    m_tabWidget->removeTab(index);
    
    // 立即更新当前标签页索引，保证界面响应
    if (m_fileTabs.empty()) {
        m_currentTabIndex = -1;
    } else {
        if (index < m_currentTabIndex) {
            m_currentTabIndex--;
        }
        else if (index == m_currentTabIndex) {
            m_currentTabIndex = qMin(m_currentTabIndex, static_cast<int>(m_fileTabs.size()) - 1);
        }
    }
    
    // 立即设置当前标签页
    if (m_currentTabIndex >= 0 && m_currentTabIndex < static_cast<int>(m_fileTabs.size())) {
        m_tabWidget->setCurrentIndex(m_currentTabIndex);
    }
    
    // 立即更新界面状态
    updateActions();
    updateWindowTitle();
    showLogMessage("已关闭标签页: " + tab.fileName);
    
    // 异步清理资源，避免阻塞界面
    FileTab& closingTab = m_fileTabs[index];
    
    // 立即断开信号连接，防止回调
    if (closingTab.entryTableView) {
        closingTab.entryTableView->disconnect();
        // 安全断开滚动条连接
        QScrollBar* scrollBar = closingTab.entryTableView->verticalScrollBar();
        if (scrollBar) {
            scrollBar->disconnect();
        }
    }
    if (closingTab.tableList) {
        closingTab.tableList->disconnect();
    }
    
    // 使用定时器延迟清理重资源，避免阻塞UI
    QTimer::singleShot(0, this, [this, closingTab]() mutable {
        // 清理滚动定时器
        if (closingTab.cache.scrollTimer) {
            closingTab.cache.scrollTimer->stop();
            closingTab.cache.scrollTimer->deleteLater();
            closingTab.cache.scrollTimer = nullptr;
        }
        
        // 清理进度对话框
        if (closingTab.progressDialog) {
            closingTab.progressDialog->close();
            closingTab.progressDialog->deleteLater();
            closingTab.progressDialog = nullptr;
        }
        
        // 快速清理表格：删除模型和视图
        if (closingTab.entryTableModel) {
            closingTab.entryTableModel->deleteLater();
            closingTab.entryTableModel = nullptr;
        }
        if (closingTab.entryTableView) {
            closingTab.entryTableView->deleteLater();
            closingTab.entryTableView = nullptr;
        }
        
        if (closingTab.tableList) {
            closingTab.tableList->clear();
            closingTab.tableList->clear();
            closingTab.tableList->deleteLater();
            closingTab.tableList = nullptr;
        }
        
        // 清理数据结构
        closingTab.tables.clear();
        closingTab.whmEntries.clear();
        closingTab.cache = {};
    });
    
    // 最后移除数据
    m_fileTabs.erase(m_fileTabs.begin() + index);
}

// 处理标签页拖拽移动
void GXTStudio::onTabMoved(int from, int to)
{
    // 【关键修复】标签页移动的完整解决方案
    // 核心问题：QTabWidget 的 widget 顺序与 m_fileTabs 顺序不一致
    // 解决方案：同步重新排序 widget 和 m_fileTabs

    if (from < 0 || from >= static_cast<int>(m_fileTabs.size()) ||
        to < 0 || to >= static_cast<int>(m_fileTabs.size())) {
        return;
    }

    if (from == to) {
        return;
    }

    // 记录是否移动了当前标签页
    bool currentTabMoved = (from == m_currentTabIndex);

    // 【关键修复】在删除 widget 之前，先保存标签页标题
    QString tabTitle = m_tabWidget->tabText(from);

    // 【关键修复】获取需要移动的 widget
    QWidget* movedWidget = m_tabWidget->widget(from);

    // 【关键修复】阻塞 QTabWidget 的信号，防止在移动过程中触发 currentChanged
    m_tabWidget->blockSignals(true);

    // 移除旧位置的 widget 和数据
    m_tabWidget->removeTab(from);

    // 【关键修复】在 m_fileTabs 中移动元素
    FileTab temp = std::move(m_fileTabs[from]);
    m_fileTabs.erase(m_fileTabs.begin() + from);

    // 【关键修复】重新插入到目标位置
    // 因为已经移除了 from 位置的元素，需要调整插入位置
    int insertPos = (to > from) ? (to - 1) : to;
    m_fileTabs.insert(m_fileTabs.begin() + insertPos, std::move(temp));

    // 【关键修复】将 widget 重新插入到 QTabWidget 的目标位置
    // insertTab 的正确参数顺序：index, widget, label
    m_tabWidget->insertTab(to, movedWidget, tabTitle);

    // 【关键修复】解除信号阻塞
    m_tabWidget->blockSignals(false);

    // 【关键修复】更新当前标签页索引
    if (currentTabMoved) {
        m_currentTabIndex = to;
    } else {
        // 如果当前标签页没有被移动，需要根据移动方向调整索引
        if (from < m_currentTabIndex && to >= m_currentTabIndex) {
            m_currentTabIndex--;
        } else if (from > m_currentTabIndex && to <= m_currentTabIndex) {
            m_currentTabIndex++;
        }
    }

    // 验证索引有效性
    if (m_currentTabIndex < 0) {
        m_currentTabIndex = 0;
    } else if (m_currentTabIndex >= static_cast<int>(m_fileTabs.size())) {
        m_currentTabIndex = static_cast<int>(m_fileTabs.size()) - 1;
    }

    // 【关键修复】强制刷新所有标签页的模型连接
    for (auto& tab : m_fileTabs) {
        if (tab.entryTableModel) {
            tab.entryTableModel->setTab(&tab);
        }
    }

    // 【关键修复】只更新UI状态，不触发标签页切换
    // 因为我们已经手动重新排序了widgets，不需要再次切换
    updateWindowTitle();
    updateStatusBar();
    updateActions();

    // 【关键修复】强制刷新当前标签页的UI
    FileTab* currentTab = getCurrentTab();
    if (currentTab) {
        if (currentTab->entryTableView && currentTab->entryTableModel) {
            // 确保模型正确设置
            if (currentTab->entryTableView->model() != currentTab->entryTableModel) {
                currentTab->entryTableView->setModel(currentTab->entryTableModel);
            }

            // 强制重置模型
            currentTab->entryTableModel->forceDataReset();

            // 清除委托缓存
            if (auto* delegate = qobject_cast<OptimizedItemDelegate*>(currentTab->entryTableView->itemDelegate())) {
                delegate->clearCache();
            }

            // 确保更新启用
            currentTab->entryTableView->setUpdatesEnabled(true);
            currentTab->entryTableView->viewport()->setUpdatesEnabled(true);

            // 强制重绘
            currentTab->entryTableView->viewport()->update();
            currentTab->entryTableView->viewport()->repaint();
        }

        // 刷新左侧表格列表
        if (currentTab->tableList) {
            currentTab->tableList->setUpdatesEnabled(true);
            currentTab->tableList->viewport()->setUpdatesEnabled(true);
            updateTableList();
        }

        // 立即处理所有待处理事件
        QCoreApplication::processEvents();
    }

    showLogMessage(QString("标签页已从位置 %1 移动到 %2").arg(from).arg(to));
}

// 更新方法
void GXTStudio::updateTableList()
{
    FileTab* tab = getCurrentTab();
    if (!tab || !tab->tableList) {
        showLogMessage("updateTableList: tab或tableList为空");
        return;
    }

    showLogMessage(QString("updateTableList: 文件=%1, WHM=%2, 表数量=%3")
                  .arg(tab->fileName)
                  .arg(tab->isWHM)
                  .arg(tab->isWHM ? 1 : tab->tables.size()));

    // 极致性能优化：智能缓存系统
    const size_t currentTableCount = tab->isWHM ? 1 : tab->tables.size();
    
    // 检查是否需要重建缓存
    bool needsRebuild = tab->listCache.needsRebuild || 
                       tab->listCache.lastTableCount != currentTableCount;
    
    if (needsRebuild) {
        // 预分配内存，避免重复分配
        tab->listCache.cachedDisplayTexts.clear();
        tab->listCache.cachedTableNames.clear();
        tab->listCache.cachedCounts.clear();
        tab->listCache.cachedMainFlags.clear();
        
        tab->listCache.cachedDisplayTexts.reserve(currentTableCount);
        tab->listCache.cachedTableNames.reserve(currentTableCount);
        tab->listCache.cachedCounts.reserve(currentTableCount);
        tab->listCache.cachedMainFlags.reserve(currentTableCount);
        
        // 批量计算所有数据
        if (tab->isWHM) {
            tab->listCache.cachedTableNames.push_back("WHM Entries");
            tab->listCache.cachedCounts.push_back(QString::number(tab->whmEntries.size()));
            // 只显示表名，隐藏条目数
            tab->listCache.cachedDisplayTexts.push_back("WHM Entries");
            tab->listCache.cachedMainFlags.push_back(true);
        } else {
            for (size_t i = 0; i < tab->tables.size(); ++i) {
                const auto& table = tab->tables[i];
                QString tableName = QString::fromStdString(table.name);
                QString countText = QString::number(table.entries.size());
                
                tab->listCache.cachedTableNames.push_back(tableName);
                tab->listCache.cachedCounts.push_back(countText);
                // 只显示表名，隐藏条目数
                tab->listCache.cachedDisplayTexts.push_back(tableName);
                tab->listCache.cachedMainFlags.push_back(table.name == "MAIN");
            }
        }
        
        tab->listCache.lastTableCount = currentTableCount;
        tab->listCache.needsRebuild = false;
    }
    
    // 极致渲染优化
    tab->tableList->setUpdatesEnabled(false);
    tab->tableList->blockSignals(true);
    
    // 智能更新：只在需要时清空
    if (tab->tableList->count() != static_cast<int>(currentTableCount)) {
        tab->tableList->clear();
        
        // 设置高性能委托（总是设置以确保删除图标显示）
        auto* delegate = new OptimizedListItemDelegate(tab->tableList);
        tab->tableList->setItemDelegate(delegate);
        
        // 连接删除表格信号
        connect(delegate, &OptimizedListItemDelegate::deleteTableRequested,
                this, &GXTStudio::onDeleteTableFromList);
        
        // 启用虚拟滚动和批量布局优化
        tab->tableList->setUniformItemSizes(true);
        tab->tableList->setLayoutMode(QListView::Batched);
        tab->tableList->setBatchSize(20); // 批量处理20个项目
        
        // 禁用不必要的动画以提升性能
        tab->tableList->setMovement(QListView::Static);
        tab->tableList->setResizeMode(QListView::Fixed);
        
        // 启用滚动优化
        tab->tableList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        tab->tableList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        
        // 批量创建项目（预分配内存）
        tab->tableList->setUpdatesEnabled(false);
        for (size_t i = 0; i < currentTableCount; ++i) {
            QListWidgetItem* item = new QListWidgetItem();
            item->setData(Qt::UserRole, static_cast<int>(i));
            item->setData(Qt::DisplayRole, tab->listCache.cachedDisplayTexts[i]);
            item->setData(Qt::UserRole + 1, tab->listCache.cachedTableNames[i]);
            item->setData(Qt::UserRole + 2, tab->listCache.cachedCounts[i]);
            item->setData(Qt::UserRole + 3, static_cast<bool>(tab->listCache.cachedMainFlags[i]));
            item->setSizeHint(QSize(tab->tableList->width() - 10, 36)); // 增加宽度以确保删除图标可见
            
            tab->tableList->addItem(item);
        }
    } else {
        // 增量更新：只更新数据，不重建项目
        for (int i = 0; i < tab->tableList->count(); ++i) {
            QListWidgetItem* item = tab->tableList->item(i);
            if (item) {
                item->setData(Qt::DisplayRole, tab->listCache.cachedDisplayTexts[i]);
                item->setData(Qt::UserRole + 1, tab->listCache.cachedTableNames[i]);
                item->setData(Qt::UserRole + 2, tab->listCache.cachedCounts[i]);
                item->setData(Qt::UserRole + 3, static_cast<bool>(tab->listCache.cachedMainFlags[i]));
            }
        }
    }
    
    // 智能选择逻辑（使用缓存数据）
    int targetIndex = 0;
    if (!tab->isWHM) {
        auto mainIt = std::find(tab->listCache.cachedMainFlags.begin(), 
                              tab->listCache.cachedMainFlags.end(), true);
        if (mainIt != tab->listCache.cachedMainFlags.end()) {
            targetIndex = std::distance(tab->listCache.cachedMainFlags.begin(), mainIt);
        } else if (tab->currentTableIndex >= 0 && tab->currentTableIndex < tab->tableList->count()) {
            targetIndex = tab->currentTableIndex;
        }
    }
    
    // 恢复更新
    tab->tableList->setUpdatesEnabled(true);
    tab->tableList->blockSignals(false);
    
    if (targetIndex >= 0 && targetIndex < tab->tableList->count()) {
        // 阻塞信号，避免触发 onTableSelectionChanged 导致重复更新
        const bool wasBlocked = tab->tableList->blockSignals(true);
        tab->currentTableIndex = targetIndex;
        tab->tableList->setCurrentRow(targetIndex);
        tab->tableList->blockSignals(wasBlocked);

        // 强制触发模型更新，确保在切换标签页后表格能正确显示数据
        if (tab->entryTableModel) {
            tab->entryTableModel->forceDataReset();
            // 强制刷新视图以确保数据正确显示
            if (tab->entryTableView) {
                tab->entryTableView->viewport()->update();
                tab->entryTableView->update();
            }
        }
    }

    // 恢复视图更新并刷新状态栏
    if (tab->entryTableView) {
        tab->entryTableView->setUpdatesEnabled(true);
        tab->entryTableView->viewport()->update();
    }
    
    // 根据文件类型禁用/启用"添加表"按钮
    // DAT和WHM文件不允许添加表
    if (tab->addTableButton) {
        tab->addTableButton->setEnabled(!tab->isWHM && !tab->isDAT);
    }
    
    updateStatusBar();
}

void GXTStudio::updateEntryTable()
{
    FileTab* tab = getCurrentTab();
    if (!tab) {
        return;
    }
    
    // 确保表格视图和模型存在
    if (!tab->entryTableView || !tab->entryTableModel) {
        return;
    }
    
    // 如果正在解析，跳过更新
    if (tab->isParsing) {
        return;
    }
    
    // 【关键修复】首先确保模型的tab指针正确
    tab->entryTableModel->setTab(tab);
    
    // 禁用更新以防止闪烁
    tab->entryTableView->setUpdatesEnabled(false);
    
    // 【关键修复】强制刷新模型以确保数据同步
    tab->entryTableModel->forceDataReset();
    
    // 清除委托缓存以确保正确的渲染
    auto* delegate = qobject_cast<OptimizedItemDelegate*>(tab->entryTableView->itemDelegate());
    if (delegate) {
        delegate->clearCache();
    }
    
    // 【关键修复】确保表格视图的模型正确
    if (tab->entryTableView->model() != tab->entryTableModel) {
        tab->entryTableView->setModel(tab->entryTableModel);
    }
    
    // 清除视口缓存
    tab->entryTableView->viewport()->update();
    
    // 重新启用更新
    tab->entryTableView->setUpdatesEnabled(true);
    
    // 【关键修复】强制立即重绘，确保内容立即显示
    tab->entryTableView->viewport()->repaint();
    tab->entryTableView->repaint();
    
    // 强制完整刷新
    tab->entryTableView->update();
}

void GXTStudio::updateWindowTitle()
{
    QString title = "GXTStudio - GTAmod中文组";
    
    FileTab* currentTab = getCurrentTab();
    if (currentTab) {
        title += " - " + currentTab->fileName;
        if (currentTab->isModified) {
            title += " *";
        }
    }
    
    setWindowTitle(title);
}

void GXTStudio::updateStatusBar()
{
    // 检查状态标签是否仍然有效（对象可能已经被销毁）
    if (!m_statusLabel) {
        return;
    }

    FileTab* currentTab = getCurrentTab();
    if (currentTab) {
        QString status = currentTab->fileName;

        // 显示文件版本信息
        if (currentTab->isWHM) {
            status += " | WHM";
        } else if (currentTab->isDAT) {
            status += " | DAT";
        } else if (currentTab->isCharTable) {
            // 字符表文件显示 CHAR_VC 或 CHAR_IV
            if (currentTab->charTableData.type == CharTableData::GTA_VC) {
                status += " | CHAR_VC";
            } else if (currentTab->charTableData.type == CharTableData::GTA_IV) {
                status += " | CHAR_IV";
            } else {
                status += " | CHAR";
            }
        } else {
            status += " | " + getVersionName(currentTab->version);
        }

        // 显示条目数量（DAT和WHM都显示条目数）
        if (currentTab->isWHM && !currentTab->whmEntries.empty()) {
            status += " | 条目: " + QString::number(currentTab->whmEntries.size());
        } else if (currentTab->isDAT && !currentTab->datEntries.empty()) {
            status += " | 条目: " + QString::number(currentTab->datEntries.size());
        } else {
            int tableIndex = currentTab->currentTableIndex;
            if (tableIndex >= 0 && tableIndex < static_cast<int>(currentTab->tables.size())) {
                const auto& table = currentTab->tables[tableIndex];
                status += " | 表: " + QString::fromStdString(table.name);
                status += " | 条目: " + QString::number(table.entries.size());
            }
        }

        if (currentTab->isModified) {
            status += " | 已修改";
        }

        if (m_isReadOnly) {
            status += " | 只读";
        }

        m_statusLabel->setText(status);
    } else {
        m_statusLabel->setText("就绪");
    }
}

void GXTStudio::updateActions()
{
    FileTab* currentTab = getCurrentTab();
    bool hasFile = (currentTab != nullptr);
    
    m_saveAction->setEnabled(hasFile && !m_isReadOnly);
    m_saveAsAction->setEnabled(hasFile);
    m_exportAction->setEnabled(hasFile);
    m_closeAction->setEnabled(hasFile);
    m_findAction->setEnabled(hasFile);
    m_replaceAction->setEnabled(hasFile && !m_isReadOnly);
    
    // 【修复】更新添加新表按钮状态（确保在只读模式下按钮是禁用的）
    if (currentTab && currentTab->addTableButton) {
        currentTab->addTableButton->setEnabled(!m_isReadOnly && !currentTab->isWHM && !currentTab->isDAT);
    }
    
    // 【修复】更新码表转换按钮和下拉菜单状态
    if (m_codeTableButton) {
        bool enabled = !m_isReadOnly;
        m_codeTableButton->setEnabled(enabled);
        
        // 更新下拉菜单中的动作状态
        if (m_mountCodeTableAction) {
            m_mountCodeTableAction->setEnabled(enabled);
        }
        if (m_convertCodeTableAction) {
            // 转换菜单项只有在已挂载码表且非只读模式下才启用
            bool canConvert = enabled && m_codeTableConverter && m_codeTableConverter->isTableMounted();
            m_convertCodeTableAction->setEnabled(canConvert);
        }
        if (m_unmountCodeTableAction) {
            // 卸载菜单项只有在已挂载码表且非只读模式下才启用
            bool canUnmount = enabled && m_codeTableConverter && m_codeTableConverter->isTableMounted();
            m_unmountCodeTableAction->setEnabled(canUnmount);
        }
        
        // 更新按钮文本
        updateCodeTableButtonText();
    }
}

// 以下函数是头文件中声明但未实现的槽函数，添加空实现以修复链接错误
void GXTStudio::onTranslationFinished(const QList<TranslateResult>& results)
{
    Q_UNUSED(results)
    // 实现将在翻译功能完成后添加
}

void GXTStudio::startAsyncParse(const QString& filePath)
{
    showLogMessage(QString("startAsyncParse 被调用 - 文件: %1").arg(filePath));

    // 检查解析工作对象是否有效
    if (!m_parseWorker) {
        showLogMessage("错误: m_parseWorker 为空，无法启动解析");
        return;
    }

    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    // 判断文件类型：完全区分DAT和WHM
    bool isWHM = (suffix == "whm");
    bool isWHMRSC = false;  // 压缩的HTML文档格式
    bool isDAT = (suffix == "dat");

    // 对于 .whm 文件，检测是否为 RSC 格式（压缩的 HTML 文档）
    if (isWHM) {
        // 默认使用简单 WHM 格式
        isWHMRSC = false;

        // 尝试读取文件头部来检测格式
        FILE* f = nullptr;
        fopen_s(&f, filePath.toLocal8Bit().constData(), "rb");
        if (f) {
            // 读取前4字节检查magic
            char magic[4];
            fread(magic, 1, 4, f);

            // 检查是否为 RSC 格式（前3字节为 "RSC"）
            if (magic[0] == 'R' && magic[1] == 'S' && magic[2] == 'C') {
                qDebug() << "检测到 RSC 格式";
                isWHMRSC = true;
            } else {
                // 读取第一个 uint32 作为条目数
                rewind(f);
                uint32_t count = 0;
                fread(&count, 1, sizeof(count), f);

                // 获取文件大小
                fseek(f, 0, SEEK_END);
                long fileSize = ftell(f);
                fclose(f);

                // 简单 WHM 格式检查：
                // 条目数应该在合理范围内（1-100000）
                // 如果条目数异常，可能是 RSC 格式
                if (count == 0 || count > 100000) {
                    qDebug() << "文件格式检测：条目数=" << count << ", 可能是 RSC 格式";
                    isWHMRSC = true;
                }
            }

            if (f) fclose(f);
        }
    }

    // 如果是 .dat 文件，尝试判断是否为字符表文件
    // 字符表特征：
    // 1. GTA_VC: wm_vcchs.dat，固定大小 0x10000 * 2 = 131072 字节
    // 2. GTA_IV: char_table.dat 或类似名称，大小不固定（UTF-16LE字符序列）
    if (isDAT) {
        qint64 fsize = fileInfo.size();
        QString baseName = fileInfo.baseName().toLower();

        // 检查是否为字符表文件（通过文件名模式或大小）
        bool isCharTable = false;
        if (fsize == 0x10000 * 2) {
            // GTA_VC 字符表标准大小
            isCharTable = true;
        } else if (baseName.contains("char_table") || baseName.contains("wm_vcchs") ||
                   baseName.contains("chs") || baseName.contains("char")) {
            // GTA_IV 或其他字符表（通过文件名识别）
            isCharTable = true;
        }

        if (isCharTable) {
            CharTableData data;
            int tabIndex = -1;
            CharTableWidget* charTableWidget = nullptr;
            
            if (CharTableParser::loadGtaVc(filePath, data)) {
                charTableWidget = createCharTableTab(fileInfo.fileName(), data);
                if (charTableWidget) {
                    QWidget* parentPage = charTableWidget->property("parentPage").value<QWidget*>();
                    if (parentPage) {
                        tabIndex = m_tabWidget->addTab(parentPage, fileInfo.fileName());
                    }
                }
            } else if (CharTableParser::loadGtaIv(filePath, data)) {
                charTableWidget = createCharTableTab(fileInfo.fileName(), data);
                if (charTableWidget) {
                    QWidget* parentPage = charTableWidget->property("parentPage").value<QWidget*>();
                    if (parentPage) {
                        tabIndex = m_tabWidget->addTab(parentPage, fileInfo.fileName());
                    }
                }
            }
            
            // 若都失败，则继续当作普通 DAT 解析
            if (tabIndex >= 0 && charTableWidget) {
                // 创建FileTab结构体并添加到m_fileTabs
                FileTab newTab;
                newTab.filePath = filePath;
                newTab.fileName = fileInfo.fileName();
                newTab.isCharTable = true;
                newTab.isWHM = false;
                newTab.isDAT = false;
                newTab.isModified = false;
                newTab.version = GXTVersion::UNKNOWN;
                newTab.charTableData = data;
                newTab.charTableWidget = charTableWidget;
                
                // 确保m_fileTabs和m_tabWidget的索引一致
                // tabIndex是m_tabWidget->addTab返回的索引
                if (tabIndex <= static_cast<int>(m_fileTabs.size())) {
                    m_fileTabs.insert(m_fileTabs.begin() + tabIndex, newTab);
                } else {
                    m_fileTabs.push_back(newTab);
                }
                m_currentTabIndex = tabIndex;
                
                // 自动切换到新打开的字符表标签页
                // 阻塞信号避免onTabChanged干扰，然后手动更新UI
                m_tabWidget->blockSignals(true);
                m_tabWidget->setCurrentIndex(tabIndex);
                m_tabWidget->blockSignals(false);
                
                // 手动更新状态栏和标题
                updateStatusBar();
                updateWindowTitle();
                updateActions();
                
                return;
            }
        }
    }

    // 创建解析任务
    ParseTask task;
    task.filePath = filePath;
    task.fileName = fileInfo.fileName();
    task.narrowPath = filePath.toLocal8Bit().toStdString();
    task.isWHM = isWHM;
    task.isWHMRSC = isWHMRSC;
    task.isDAT = isDAT;
    task.tabIndex = static_cast<int>(m_fileTabs.size());

    showLogMessage(QString("创建解析任务 - 索引: %1, WHM: %2, DAT: %3").arg(task.tabIndex).arg(task.isWHM).arg(task.isDAT));

    // 异步调用解析方法
    bool success = QMetaObject::invokeMethod(m_parseWorker, "parseFile",
                                            Qt::QueuedConnection,
                                            Q_ARG(ParseTask, task));

    if (!success) {
        showLogMessage("错误: 调用 parseFile 方法失败");
    } else {
        showLogMessage("解析任务已提交到线程");
    }
}

void GXTStudio::onParseCompleted(const ParseResult& result)
{
    showLogMessage(QString("onParseCompleted 被调用 - 文件: %1, 成功: %2, 错误信息: %3, 表数量: %4, WHM条目: %5, DAT条目: %6")
                  .arg(result.fileName)
                  .arg(result.success)
                  .arg(result.errorMessage)
                  .arg(result.tables.size())
                  .arg(result.isWHM ? result.whmEntries.size() : 0)
                  .arg(result.isDAT ? result.datEntries.size() : 0));

    try {
        // 验证结果
        if (!result.success) {
            throw std::runtime_error(QString("解析失败: %1").arg(result.errorMessage).toStdString());
        }

        if (result.tabIndex < 0 || result.tabIndex > static_cast<int>(m_fileTabs.size())) {
            throw std::runtime_error("解析结果索引无效");
        }

        // 创建新的标签页结构
        FileTab tab;
        tab.fileName = result.fileName;
        tab.filePath = result.filePath;
        tab.tables = result.tables;
        tab.whmEntries = result.whmEntries;
        tab.datEntries = result.datEntries;  // 设置DAT条目
        tab.isWHM = result.isWHM;
        tab.isDAT = result.isDAT;  // 设置DAT标志
        tab.version = result.version;  // 设置版本信息

        // 插入到标签页列表
        if (result.tabIndex == static_cast<int>(m_fileTabs.size())) {
            m_fileTabs.push_back(tab);
        } else {
            if (result.tabIndex < 0) {
                throw std::runtime_error("标签页索引无效");
            }
            m_fileTabs.insert(m_fileTabs.begin() + result.tabIndex, tab);
        }

        // 诊断日志：确保版本信息被正确保存
        showLogMessage(QString("版本信息已保存到FileTab - 版本: %1 (枚举值: %2), WHM: %3, DAT: %4")
                      .arg(getVersionString(tab.version))
                      .arg(static_cast<int>(tab.version))
                      .arg(tab.isWHM ? "是" : "否")
                      .arg(tab.isDAT ? "是" : "否"));

        // 计算解析时间
        qint64 parseTime = result.parseTime.elapsed();

        // 先设置当前标签页索引，这样updateTableList才能正常工作
        m_currentTabIndex = result.tabIndex;
        
        // 创建实际的标签页内容和UI（内部会调用updateTableList）
        // 注意：这里传递m_fileTabs中元素的引用，而不是局部变量tab
        createTabContent(m_fileTabs[result.tabIndex], result.tabIndex);

        // 切换到该标签页
        m_tabWidget->setCurrentIndex(result.tabIndex);

        showLogMessage(QString("成功加载 %1 (耗时: %2ms)").arg(tab.fileName).arg(parseTime));

        // 【性能优化】移除不必要的100ms延迟，立即显示内容
        // 立即确保右侧表格显示第一个表的内容
        const auto& fileTab = m_fileTabs[result.tabIndex];
        
        if (!fileTab.isWHM && !fileTab.isDAT && !fileTab.tables.empty()) {
            // GXT文件：立即切换到第一个表
            switchToTable(0);
        } else if (fileTab.isWHM && !fileTab.whmEntries.empty()) {
            // WHM文件：立即更新表格
            updateEntryTable();
        } else if (fileTab.isDAT && !fileTab.datEntries.empty()) {
            // DAT文件：立即更新表格（DAT和WHM使用相同的表格显示）
            updateEntryTable();
        }
        
        // 立即处理事件，确保UI立即更新
        QCoreApplication::processEvents();
        
        // 【修复】更新动作状态，确保保存按钮和添加表按钮状态正确
        updateActions();

    } catch (const std::exception& e) {
        qCritical() << "处理解析结果时发生错误:" << e.what();
        showLogMessage(QString("处理解析结果时发生错误: %1").arg(e.what()));
    } catch (...) {
        qCritical() << "处理解析结果时发生未知错误";
        showLogMessage("处理解析结果时发生未知错误");
    }
}

std::string GXTStudio::getVersionName(GXTVersion version)
{
    switch (version) {
        case GXTVersion::GTA_III: return "GTA III";
        case GXTVersion::GTA_VC: return "GTA Vice City";
        case GXTVersion::GTA_SA: return "GTA San Andreas";
        case GXTVersion::GTA_IV: return "GTA IV";
        case GXTVersion::GXT2: return "GXT2";
        default: return "Unknown";
    }
}

void GXTStudio::updateFontSizes()
{
    // 创建表格专用字体
    QFont tableFont;
    tableFont.setFamily("Microsoft YaHei");
    tableFont.setPointSize(m_fontSize);
    tableFont.setHintingPreference(QFont::PreferFullHinting);
    tableFont.setStyleStrategy(QFont::PreferAntialias);
    
    // 创建左表专用字体（稍小一些，保持比例协调）
    QFont leftTableFont = tableFont;
    leftTableFont.setPointSize(qMax(m_fontSize - 1, 8));  // 左表字体稍小但可调节
    
    // 只更新表格相关的字体，不影响其他UI元素
    for (auto& tab : m_fileTabs) {
        if (tab.entryTableView) {
            // 右侧主表格使用标准字体
            tab.entryTableView->setFont(tableFont);
            tab.entryTableView->verticalHeader()->setDefaultSectionSize(m_fontSize + 10);  // 增加行高适应更大字体
            tab.cache.layoutNeedsUpdate = true;
        }
        
        if (tab.tableList) {
            // 左侧表列表使用稍小的字体
            tab.tableList->setFont(leftTableFont);
        }
    }
    
    // 延迟布局更新
    if (m_resizeDebouncer) {
        m_resizeDebouncer->start(50);
    }
}

// 工具方法
FileTab* GXTStudio::getCurrentTab()
{
    if (m_currentTabIndex >= 0 && m_currentTabIndex < static_cast<int>(m_fileTabs.size())) {
        return &m_fileTabs[m_currentTabIndex];
    }
    
    return nullptr;
}

FileTab* GXTStudio::getTab(int index)
{
    if (index >= 0 && index < static_cast<int>(m_fileTabs.size())) {
        return &m_fileTabs[index];
    }
    return nullptr;
}

void GXTStudio::addFileTab(const QString& filePath)
{
    // 现在使用异步解析，这个方法为了兼容性保留
    startAsyncParse(filePath);
}

void GXTStudio::removeFileTab(int index)
{
    if (index >= 0 && index < static_cast<int>(m_fileTabs.size())) {
        m_fileTabs.erase(m_fileTabs.begin() + index);
    }
}

void GXTStudio::createTabContent(FileTab& tab, int tabIndex)
{
    // 创建新的标签页widget
    QWidget* tabWidget = new QWidget();
    
    // 创建主布局 - 使用更紧凑的边距
    QVBoxLayout* mainLayout = new QVBoxLayout(tabWidget);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // 创建搜索栏容器
    QWidget* searchContainer = new QWidget();
    searchContainer->setMaximumHeight(40);
    searchContainer->setMinimumHeight(36);
    QHBoxLayout* searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(6, 2, 6, 2);
    searchLayout->setSpacing(6);
    
    // 创建搜索框容器（用于放置搜索框和图标按钮）
    QWidget* searchWrapper = new QWidget();
    QHBoxLayout* wrapperLayout = new QHBoxLayout(searchWrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->setSpacing(4);
    
    // 获取搜索区域应该使用的颜色（使用现有判断颜色的方法）
    QPoint searchPos = tabWidget->mapTo(this, QPoint(100, 20));
    QColor searchTextColor = getTextColorForPosition(searchPos);
    QString searchTextColorName = searchTextColor.name();
    
    // 创建搜索框
    QLineEdit* searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("搜索文本或哈希值...");
    searchEdit->setMinimumHeight(32);
    
    // 设置搜索框样式和字体 - 使用自动判断的颜色
    QFont searchFont("Microsoft YaHei", m_fontSize);
    searchEdit->setFont(searchFont);
    searchEdit->setStyleSheet(QString(R"(
        QLineEdit {
            border: 2px solid #e1e5e9;
            border-radius: 6px;
            padding: 6px 12px;
            padding-left: 14px;
            background-color: rgba(255, 255, 255, 0.5);
            font-size: 12px;
            color: %1;
        }
        QLineEdit:focus {
            border-color: #2196f3;
            outline: none;
        }
    )").arg(searchTextColorName));
    
    // 创建大小写敏感图标按钮
    QToolButton* caseSensitiveButton = new QToolButton();
    caseSensitiveButton->setFixedSize(32, 28);
    caseSensitiveButton->setCheckable(true);
    caseSensitiveButton->setText(FA::CaseSensitive);
    caseSensitiveButton->setToolTip("区分大小写");

    // 创建正则表达式图标按钮
    QToolButton* regexButton = new QToolButton();
    regexButton->setFixedSize(32, 28);
    regexButton->setCheckable(true);
    regexButton->setText(FA::Regex);
    regexButton->setToolTip("正则表达式");

    // 设置图标按钮样式（显式指定 FontAwesome 字体族）- 使用自动判断的颜色
    QString fontFamily = FA::solidFontFamily();
    QString buttonStyle;
    if (fontFamily.isEmpty()) {
        // 如果字体加载失败，使用默认样式
        buttonStyle = QString(R"(
            QToolButton {
                border: none;
                border-radius: 4px;
                background-color: transparent;
                color: %1;
            }
            QToolButton:hover {
                background-color: rgba(255, 255, 255, 0.5);
                color: %1;
            }
            QToolButton:pressed {
                background-color: #e9ecef;
            }
            QToolButton:checked {
                color: #2196f3;
                background-color: #e3f2fd;
            }
            QToolButton:checked:hover {
                background-color: #bbdefb;
            }
        )").arg(searchTextColorName);
    } else {
        // 如果字体加载成功，显式指定字体族
        buttonStyle = QString(R"(
            QToolButton {
                border: none;
                border-radius: 4px;
                background-color: transparent;
                color: %1;
                font-family: '%2';
                font-size: 13px;
            }
            QToolButton:hover {
                background-color: rgba(255, 255, 255, 0.5);
                color: %1;
            }
            QToolButton:pressed {
                background-color: #e9ecef;
            }
            QToolButton:checked {
                color: #2196f3;
                background-color: #e3f2fd;
            }
            QToolButton:checked:hover {
                background-color: #bbdefb;
            }
        )").arg(searchTextColorName, fontFamily);
    }

    caseSensitiveButton->setStyleSheet(buttonStyle);
    regexButton->setStyleSheet(buttonStyle);
    
    // 设置 FontAwesome 字体到图标按钮（关键修复）
    caseSensitiveButton->setFont(FA::solidFont(12));
    regexButton->setFont(FA::solidFont(12));
    
    // 创建搜索按钮 - 使用0.5不透明度的白色背景和自动判断的颜色
    QPushButton* prevButton = new QPushButton();
    QPushButton* nextButton = new QPushButton();
    
    int btnSize = qMax(30, m_fontSize + 10);
    QString navButtonStyle = QString(R"(
        QPushButton {
            background-color: rgba(255, 255, 255, 0.5);
            border: 1px solid #dee2e6;
            border-radius: 6px;
            color: %1;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: rgba(255, 255, 255, 0.7);
            border-color: #adb5bd;
        }
        QPushButton:pressed {
            background-color: rgba(255, 255, 255, 0.9);
        }
    )").arg(searchTextColorName);
    
    for (auto* btn : {prevButton, nextButton}) {
        btn->setFixedSize(btnSize, btnSize);
        btn->setStyleSheet(navButtonStyle);
    }
    
    prevButton->setText(FA::QChevronLeft);
    prevButton->setToolTip("上一个 (Shift+Enter)");
    nextButton->setText(FA::QChevronRight);
    nextButton->setToolTip("下一个 (Enter)");

    // 设置 FontAwesome 字体
    prevButton->setFont(FA::solidFont(12));
    nextButton->setFont(FA::solidFont(12));
    
    // 将搜索框和图标按钮放入容器
    searchEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    wrapperLayout->addWidget(searchEdit);
    wrapperLayout->addWidget(caseSensitiveButton);
    wrapperLayout->addWidget(regexButton);
    
    searchLayout->addWidget(searchWrapper);
    searchLayout->addWidget(prevButton);
    searchLayout->addWidget(nextButton);
    
    mainLayout->addWidget(searchContainer);
    
    // 创建主要内容分割器
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(2);
    splitter->setChildrenCollapsible(false);
    splitter->setStyleSheet("QSplitter::handle { background-color: transparent; border: none; }");
    
    // 创建左侧面板容器
    QWidget* leftPanel = new QWidget();
    leftPanel->setMaximumWidth(300);
    leftPanel->setMinimumWidth(200);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);
    
    // 左侧标题标签 - 使用富文本格式，使图标部分使用FontAwesome字体
    QString tableFontFamily = FA::solidFontFamily();
    QString tableListText;
    if (tableFontFamily.isEmpty()) {
        // 如果字体加载失败，使用纯文本
        tableListText = QString("%1 表格列表").arg(QString(FA::QClipboardList));
    } else {
        // 如果字体加载成功，使用富文本格式
        tableListText = QString("<span style=\"font-family:'%1'; font-size:%2px;\">%3</span> 表格列表")
            .arg(tableFontFamily)
            .arg(m_fontSize - 1)
            .arg(QString(FA::QClipboardList));
    }
    QLabel* tableListLabel = new QLabel(tableListText);
    tableListLabel->setTextFormat(Qt::RichText);
    // 设置默认样式（透明背景，灰色文字）- 与 applyLabelStyle 使用完全相同的格式
    tableListLabel->setStyleSheet(R"(
        QLabel {
            font-weight: bold;
            color: #495057;
            padding: 6px 8px;
            background-color: transparent;
            border-radius: 6px;
            border: none;
        }
    )");
    
    // 创建左侧表格列表 - 极致性能优化
    DraggableTableList* tableList = new DraggableTableList();
    tableList->setSelectionMode(QAbstractItemView::SingleSelection);
    tableList->setSelectionBehavior(QAbstractItemView::SelectItems);
    tableList->setEditTriggers(QAbstractItemView::DoubleClicked);
    tableList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 极致性能优化设置 - 启用虚拟滚动和懒加载
    // 启用虚拟滚动和懒加载优化
    tableList->setUniformItemSizes(true);
    tableList->setLayoutMode(QListView::Batched);
    tableList->setBatchSize(20);
    tableList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    tableList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);           // 所有项大小相同，最大化性能
    tableList->setLayoutMode(QListView::Batched);   // 批量布局模式（虚拟滚动）
    tableList->setBatchSize(20);                     // 优化批处理大小
    tableList->setMovement(QListView::Static);       // 禁用拖动移动
    tableList->setResizeMode(QListView::Fixed);      // 固定大小模式
    tableList->setWordWrap(false);                   // 禁用自动换行
    tableList->setTextElideMode(Qt::ElideRight);     // 文本省略模式
    tableList->setAlternatingRowColors(false);       // 禁用斑马纹（用自定义绘制）
    tableList->setIconSize(QSize(0, 0));            // 禁用图标
    
    // 启用滚动优化
    tableList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    tableList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    
    // 高性能渲染优化
    tableList->setSpacing(0);
    tableList->setGridSize(QSize(0, 36));            // 更新固定网格大小
    
    // 设置性能优化样式表
    tableList->setStyleSheet(
        "QListWidget {"
        "   background-color: white;"
        "   border: 1px solid #ddd;"
        "   outline: none;"
        "   selection-background-color: transparent;"
        "}"
        "QListWidget::item {"
        "   border: none;"
        "   padding: 0px;"
        "   margin: 0px;"
        "   min-height: 36px;"
        "   max-height: 36px;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: transparent;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: transparent;"
        "}"
        "QScrollBar:vertical {"
        "   width: 12px;"
        "   background: #f5f5f5;"
        "   margin: 0px;"
        "}"
    );
    
    // 设置现代化的列表样式
    tableList->setStyleSheet(R"(
        QListWidget {
            border: 1px solid #dee2e6;
            border-radius: 8px;
            background-color: #ffffff;
            padding: 6px;
            outline: none;
            alternate-background-color: #f8f9fa;
        }
        QListWidget::item {
            border: 1px solid #e9ecef;
            border-radius: 6px;
            background-color: #ffffff;
            padding: 10px 12px;
            margin: 2px 1px;
            min-height: 18px;
        }
        QListWidget::item:selected {
            background-color: #e3f2fd;
            border-color: #2196f3;
            color: #1976d2;
            font-weight: 500;
        }
        QListWidget::item:hover {
            background-color: #f8f9fa;
            border-color: #dee2e6;
        }
        QListWidget::item:selected:hover {
            background-color: #bbdefb;
        }
    )");
    
    // 设置列表字体和行高
    QFont leftFont("Microsoft YaHei", qMax(m_fontSize - 1, 9));
    leftFont.setHintingPreference(QFont::PreferFullHinting);
    leftFont.setStyleStrategy(QFont::PreferAntialias);
    tableList->setFont(leftFont);
    tableList->setIconSize(QSize(20, 20));
    // 启用虚拟滚动和懒加载优化
    tableList->setUniformItemSizes(true);
    tableList->setLayoutMode(QListView::Batched);
    tableList->setBatchSize(20);
    tableList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    tableList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    
    // 设置右键菜单策略
    if (!tab.isWHM) {
        tableList->setContextMenuPolicy(Qt::CustomContextMenu);
    }
    
    // 创建表格列表面板容器
    QWidget* tableListPanel = new QWidget();
    QVBoxLayout* tableListPanelLayout = new QVBoxLayout(tableListPanel);
    tableListPanelLayout->setContentsMargins(0, 0, 0, 0);
    tableListPanelLayout->setSpacing(0);
    tableListPanelLayout->addWidget(tableListLabel);
    tableListPanelLayout->addSpacing(6);  // 添加间距使表格列表与右侧表格对齐
    tableListPanelLayout->addWidget(tableList, 1);
    
    // 为列表底部添加额外空间，防止按钮遮挡最后一项
    tableListPanelLayout->addSpacing(45);  // 添加45px的底部空间（与右侧键值对添加区域45px对齐）
    
    // 创建圆形添加按钮 - 放在列表控件内侧右下角
    QToolButton* addTableButton = new QToolButton(tableListPanel);
    addTableButton->setFixedSize(40, 40);  // 设置固定大小为40x40，与右侧文本渲染控件40px高度对齐
    addTableButton->setCursor(Qt::PointingHandCursor);
    addTableButton->setToolTip("创建新的表格");
    addTableButton->setEnabled(!m_isReadOnly && !tab.isWHM && !tab.isDAT);
    
    // 保存按钮指针到tab结构中
    tab.addTableButton = addTableButton;
    
    // 使用U+f65e Font Awesome图标 - plus图标
    addTableButton->setText(QChar(0xf65e));
    addTableButton->setToolButtonStyle(Qt::ToolButtonTextOnly);

    // 使用主程序欢迎页面的按钮样式风格（扁平化设计）
    QString buttonColor = "#3498db"; // 使用主程序的蓝色
    QString hoverColor = QColor(buttonColor).darker(120).name();
    QString pressedColor = QColor(buttonColor).darker(150).name();

    // 设置圆形按钮样式
    QString addTableFontFamily = FA::solidFontFamily();
    QString addTableButtonStyle = QString(R"(
        QToolButton {
            background-color: %1;
            color: white;
            border: none;
            border-radius: 20px;  /* 设置为宽度的一半，形成圆形（40px/2=20px） */
            font-size: 16px;
            font-weight: bold;
            padding: 0px;  /* 移除内边距 */
            margin: 0px;   /* 移除外边距 */
        }
        QToolButton:hover {
            background-color: %2;
        }
        QToolButton:pressed {
            background-color: %3;
        }
        QToolButton:disabled {
            background-color: #e0e0e0;
            color: #a0a0a0;
        }
    )").arg(buttonColor).arg(hoverColor).arg(pressedColor);
    addTableButton->setStyleSheet(addTableButtonStyle);
    
    // 如果Font Awesome字体可用，使用它以确保图标居中
    if (!addTableFontFamily.isEmpty()) {
        addTableButton->setFont(FA::solidFont(16));
    } else {
        addTableButton->setFont(QFont("Microsoft YaHei", 14, QFont::Bold));
    }
    
    // 初始定位按钮：右对齐列表控件右侧，下移到底部留白区域
    // 按钮大小为40x40，在底部45px的留白区域中，与文本渲染控件中间对齐
    QRect listRect = tableList->geometry();
    int buttonX = listRect.right() - 40;  // 右对齐列表控件右侧
    int buttonY = listRect.bottom() + 10;  // 下移到底部留白，与文本渲染控件中间对齐（45-40)/2≈2.5px，再加一点偏移使其视觉居中
    addTableButton->move(buttonX, buttonY);

    // 当父容器大小改变时，重新定位按钮
    ResizeEventFilter* resizeFilter = new ResizeEventFilter(tableListPanel);
    resizeFilter->setButton(addTableButton);
    resizeFilter->setListWidget(tableList);
    tableListPanel->installEventFilter(resizeFilter);
    
    // 连接信号
    connect(addTableButton, &QToolButton::clicked, this, [this]() {
        FileTab* currentTab = getCurrentTab();
        if (currentTab && !currentTab->isWHM) {
            addNewTable();
        } else if (currentTab && currentTab->isWHM) {
            QMessageBox::information(this, "提示", "WHM格式不支持添加新表");
        }
    });
    
    // 将表格列表面板添加到左侧布局
    leftLayout->addWidget(tableListPanel);
    
    // 创建右侧面板容器
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);
    
    // 右侧标题标签 - 使用富文本格式，使图标部分使用FontAwesome字体
    QString entryFontFamily = FA::solidFontFamily();
    QString entryTableText;
    if (entryFontFamily.isEmpty()) {
        // 如果字体加载失败，使用纯文本
        entryTableText = QString("%1 文本条目").arg(QString(FA::QEdit));
    } else {
        // 如果字体加载成功，使用富文本格式
        entryTableText = QString("<span style=\"font-family:'%1'; font-size:%2px;\">%3</span> 文本条目")
            .arg(entryFontFamily)
            .arg(m_fontSize - 1)
            .arg(QString(FA::QEdit));
    }
    QLabel* entryTableLabel = new QLabel(entryTableText);
    entryTableLabel->setTextFormat(Qt::RichText);
    // 设置默认样式（透明背景，灰色文字）- 与 applyLabelStyle 使用完全相同的格式
    entryTableLabel->setStyleSheet(R"(
        QLabel {
            font-weight: bold;
            color: #495057;
            padding: 6px 8px;
            background-color: transparent;
            border-radius: 6px;
            border: none;
        }
    )");
    
    // 创建右侧数据表格
    QTableView* entryTableView = new QTableView();
    entryTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    entryTableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    entryTableView->setEditTriggers(m_isReadOnly ? QAbstractItemView::NoEditTriggers : QAbstractItemView::DoubleClicked);
    entryTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 设置表格样式 - 简化设计，去除边框，优化表头，使用标准滚动条
    entryTableView->setStyleSheet(R"(
        QTableView {
            border: none;
            background-color: #ffffff;
            gridline-color: #f0f0f0;
            outline: none;
            selection-background-color: #e3f2fd;
            selection-color: #1976d2;
        }
        QTableView::item {
            padding: 6px 8px;
            border-bottom: 1px solid #f0f0f0;
            border-right: 1px solid #f0f0f0;
            outline: none;
        }
        QTableView::item:selected {
            background-color: #e3f2fd;
            color: #1976d2;
        }
        QTableView::item:hover {
            background-color: #f8f9ff;
        }
        QTableView::item:focus {
            background-color: #fff;
        }
        QHeaderView::section {
            background-color: #f5f5f5;
            border: 1px solid #ddd;
            border-bottom: 2px solid #ccc;
            padding: 4px 8px;
            font-weight: bold;
            font-size: 13px;
            color: #333;
            text-align: center;
        }
        QHeaderView::section:first {
            min-width: 120px;
        }
        QHeaderView::section:hover {
            background-color: #e8e8e8;
        }
    )");
    
    // 设置表格字体
    QFont tableFont("Microsoft YaHei", m_fontSize);
    tableFont.setHintingPreference(QFont::PreferFullHinting);
    tableFont.setStyleStrategy(QFont::PreferAntialias);
    entryTableView->setFont(tableFont);
    
    rightLayout->addWidget(entryTableLabel);
    rightLayout->addWidget(entryTableView, 1);

    // 创建添加键值对控件区域 - 仅对非WHM文件
    if (!tab.isWHM) {
        QWidget* addEntryContainer = new QWidget();
        addEntryContainer->setMaximumHeight(45);
        addEntryContainer->setMinimumHeight(40);
        QHBoxLayout* addEntryLayout = new QHBoxLayout(addEntryContainer);
        addEntryLayout->setContentsMargins(0, 8, 0, 0);
        addEntryLayout->setSpacing(8);

        // 键输入框
        QLineEdit* keyEdit = new QLineEdit();
        keyEdit->setPlaceholderText("键 (Key)");
        keyEdit->setMinimumHeight(32);
        keyEdit->setMaximumWidth(200);
        keyEdit->setStyleSheet(R"(
            QLineEdit {
                border: 2px solid #e1e5e9;
                border-radius: 6px;
                padding: 6px 12px;
                font-size: 12px;
                background-color: #ffffff;
            }
            QLineEdit:focus {
                border-color: #2196f3;
                outline: none;
            }
        )");

        // 值输入框
        QLineEdit* valueEdit = new QLineEdit();
        valueEdit->setPlaceholderText("值 (Value)");
        valueEdit->setMinimumHeight(32);
        valueEdit->setStyleSheet(R"(
            QLineEdit {
                border: 2px solid #e1e5e9;
                border-radius: 6px;
                padding: 6px 12px;
                font-size: 12px;
                background-color: #ffffff;
            }
            QLineEdit:focus {
                border-color: #2196f3;
                outline: none;
            }
        )");

        // 添加按钮 - 使用FA7符号U+002b (plus图标)
        QPushButton* addEntryButton = new QPushButton(QChar(0x002b));
        addEntryButton->setFixedSize(32, 32);  // 与文本框高度一致（32px）
        addEntryButton->setEnabled(!m_isReadOnly);

        QString buttonColor = "#3498db";
        QString hoverColor = QColor(buttonColor).darker(120).name();
        QString pressedColor = QColor(buttonColor).darker(150).name();

        addEntryButton->setStyleSheet(QString(R"(
            QPushButton {
                background-color: %1;
                color: white;
                border: none;
                border-radius: 6px;  /* 与文本框圆角一致（6px） */
                padding: 0px;  /* 移除内边距 */
                font-size: 12px;  /* 与文本框字体大小一致 */
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: %2;
            }
            QPushButton:pressed {
                background-color: %3;
            }
            QPushButton:disabled {
                background-color: #e0e0e0;
                color: #a0a0a0;
            }
        )").arg(buttonColor).arg(hoverColor).arg(pressedColor));

        // 使用Font Awesome字体渲染U+002b图标
        QString addEntryFontFamily = FA::solidFontFamily();
        if (!addEntryFontFamily.isEmpty()) {
            addEntryButton->setFont(FA::solidFont(12));  // 与文本框字体大小一致（12px）
        } else {
            addEntryButton->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
        }

        // 设置伸缩因子
        keyEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        valueEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        // 连接添加按钮信号
        connect(addEntryButton, &QPushButton::clicked, this, [this, keyEdit, valueEdit]() {
            addEntryFromInputs(keyEdit, valueEdit);
        });

        // 支持回车键添加
        connect(valueEdit, &QLineEdit::returnPressed, addEntryButton, &QPushButton::click);

        addEntryLayout->addWidget(keyEdit);
        addEntryLayout->addWidget(valueEdit);
        addEntryLayout->addWidget(addEntryButton);

        rightLayout->addWidget(addEntryContainer);

        // 保存控件引用
        tab.keyEdit = keyEdit;
        tab.valueEdit = valueEdit;
        tab.addEntryButton = addEntryButton;
    }  // 结束 if (!tab.isWHM)

    // 创建文本渲染预览窗格 - 所有文件类型都支持
    TextRenderWidget* textRenderWidget = new TextRenderWidget();
    textRenderWidget->setMaximumHeight(40);
    textRenderWidget->setMinimumHeight(40);
    rightLayout->addWidget(textRenderWidget);
    
    // 设置GTA版本 - 根据当前文件的版本设置正确的颜色映射
    int versionNum = 0;
    if (tab.isDAT || tab.isWHM || tab.isWHMRSC) {
        // DAT文件和WHM文件（RSC格式）使用GTA IV的渲染方式
        versionNum = 4;
    } else {
        switch (tab.version) {
            case GXTVersion::GTA_III:
                versionNum = 0;
                break;
            case GXTVersion::GTA_VC:
                versionNum = 1;
                break;
            case GXTVersion::GTA_SA:
                versionNum = 2;
                break;
            case GXTVersion::GTA_IV:
                versionNum = 4;
                break;
            case GXTVersion::GXT2:
            default:
                versionNum = 2; // 默认为SA版本
                break;
        }
    }
    textRenderWidget->setGtaVersion(versionNum);
    
    // 保存渲染窗格引用
    tab.textRenderWidget = textRenderWidget;
    
    
    // 将左右面板添加到分割器
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({250, 750});
    
    mainLayout->addWidget(splitter, 1);
    
    // 创建表格模型并应用
    GXTTableModel* entryTableModel = new GXTTableModel(this);
    entryTableModel->setTab(&tab);
    entryTableModel->setEditable(!m_isReadOnly);
    entryTableView->setModel(entryTableModel);

    // 连接自动保存信号 - 数据修改时重置定时器
    connect(entryTableModel, &GXTTableModel::dataModified, this, [this]() {
        if (m_autoSaveEnabled && m_autoSaveTimer) {
            // 重置定时器，延长5秒后再保存
            m_autoSaveTimer->stop();
            m_autoSaveTimer->start();
        }
    });

    // 修复表格闪烁和性能问题 - 禁用不透明绘制以使用系统背景
    entryTableView->setAttribute(Qt::WA_OpaquePaintEvent, false);
    entryTableView->setAttribute(Qt::WA_NoSystemBackground, false);
    entryTableView->setAttribute(Qt::WA_TranslucentBackground, false);
    entryTableView->setAttribute(Qt::WA_StyledBackground, true);
    entryTableView->setAttribute(Qt::WA_PaintOnScreen, false);
    entryTableView->setAutoFillBackground(true);
    
    // 应用表格优化设置
    setupTableOptimizations(entryTableView);
    
    // 应用优化的委托
    OptimizedItemDelegate* delegate = new OptimizedItemDelegate(entryTableView);
    entryTableView->setItemDelegate(delegate);
    delegate->setHashKeyColumn(true);
    
    // 视口优化 - 关键的虚拟滚动设置
    entryTableView->viewport()->setAttribute(Qt::WA_StaticContents, true);
    entryTableView->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    entryTableView->viewport()->setAttribute(Qt::WA_NoSystemBackground, false);
    entryTableView->viewport()->setAutoFillBackground(true);
    entryTableView->viewport()->setPalette(entryTableView->palette());
    
    // 设置视口大小策略以优化渲染
    entryTableView->viewport()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 【性能优化】连接滚动事件 - 使用防抖减少重绘频率
    connect(entryTableView->verticalScrollBar(), &QScrollBar::valueChanged, this, 
            [this, entryTableView](int value) {
                // 懒加载：当滚动到接近底部时预加载数据
                if (entryTableView && entryTableView->model()) {
                    int total = entryTableView->model()->rowCount();
                    int visible = entryTableView->viewport()->height() / entryTableView->verticalHeader()->defaultSectionSize();
                    int triggerPoint = total - visible - 20; // 提前20行触发
                    
                    // 这里可以添加预加载逻辑
                    Q_UNUSED(value);
                    Q_UNUSED(triggerPoint);
                }
            });
    
    // 连接选择改变事件以更新渲染窗格
    connect(entryTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, 
            [this, entryTableView](const QItemSelection& selected, const QItemSelection& deselected) {
                Q_UNUSED(deselected);
                
                // 获取当前tab
                FileTab* currentTab = getCurrentTab();
                if (!currentTab || !currentTab->textRenderWidget) {
                    return;
                }
                
                // 获取选中的索引
                QModelIndexList selectedIndexes = selected.indexes();
                if (selectedIndexes.isEmpty()) {
                    currentTab->textRenderWidget->clear();
                    return;
                }
                
                // 获取第一个选中行的值列（第二列）
                QModelIndex selectedIndex = selectedIndexes.first();
                if (selectedIndex.column() != 1) {  // 值列
                    // 尝试获取同一行的值列
                    selectedIndex = entryTableView->model()->index(selectedIndex.row(), 1);
                }
                
                QString text = selectedIndex.data(Qt::DisplayRole).toString();
                currentTab->textRenderWidget->setText(text);
            });
    
    // 优化选择行为以支持单元格编辑
    entryTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    entryTableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    
    // 初始化缓存状态
    tab.cache.lastSize = size();
    tab.cache.layoutNeedsUpdate = false;
    
    
    // 保存控件引用
    tab.tableList = tableList;
    tab.entryTableView = entryTableView;
    tab.entryTableModel = entryTableModel;
    tab.searchEdit = searchEdit;
    tab.searchPrevButton = prevButton;
    tab.searchNextButton = nextButton;
    tab.caseSensitiveButton = caseSensitiveButton;
    tab.regexButton = regexButton;
    tab.tableListLabel = tableListLabel;
    tab.entryTableLabel = entryTableLabel;
    
    // 计算标签页标题（只显示文件名）
    QString title = tab.fileName;

    // 添加标签页到tabWidget
    m_tabWidget->insertTab(tabIndex, tabWidget, title);
    
    // 连接信号 - 使用正确的信号类型
    connect(tableList, &QListWidget::currentItemChanged, this, &GXTStudio::onTableSelectionChanged);
    
    // 连接搜索相关信号
    connect(searchEdit, &QLineEdit::textChanged, this, &GXTStudio::onSearchTextChanged);
    connect(prevButton, &QPushButton::clicked, this, &GXTStudio::onSearchPrevious);
    connect(nextButton, &QPushButton::clicked, this, &GXTStudio::onSearchNext);
    connect(caseSensitiveButton, &QToolButton::toggled, this, &GXTStudio::onSearchTextChanged);
    connect(regexButton, &QToolButton::toggled, this, &GXTStudio::onSearchTextChanged);
    
    // 支持回车键搜索
    connect(searchEdit, &QLineEdit::returnPressed, this, [this]() {
        FileTab* current = getCurrentTab();
        if (current && current->searchEdit) {
            if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
                onSearchPrevious();
            } else {
                onSearchNext();
            }
        }
    });
    
    // 连接右键菜单信号
    connect(entryTableView, &QTableView::customContextMenuRequested, this, &GXTStudio::showContextMenu);
    
    // 连接表格列表的右键菜单信号（仅对GXT文件）
    if (!tab.isWHM) {
        connect(tableList, &QListWidget::customContextMenuRequested, this, &GXTStudio::showTableContextMenu);
        // 连接双击信号
        connect(tableList, &QListWidget::itemDoubleClicked, this, &GXTStudio::onTableDoubleClicked);
    }

    // 填充左侧表格列表
    updateTableList();
    
    // 【关键修复】强制刷新表格视图，确保内容立即显示
    if (entryTableView && entryTableModel) {
        // 确保模型正确设置
        if (entryTableView->model() != entryTableModel) {
            entryTableView->setModel(entryTableModel);
        }
        
        // 强制重置模型以确保数据正确
        entryTableModel->forceDataReset();
        
        // 强制立即重绘
        entryTableView->viewport()->update();
        entryTableView->update();
        entryTableView->viewport()->repaint();
        entryTableView->repaint();
        
        // 【性能优化】立即处理事件，确保UI立即更新显示
        QCoreApplication::processEvents();
    }
    
    // 如果启用了背景，更新标签颜色
    if (m_backgroundEnabled) {
        QTimer::singleShot(100, this, &GXTStudio::updateLabelColors);
    }
}








// 辅助函数：检查是否为哈希键列
// isHashKeyColumn函数已移除，因为现在统一处理GXT和WHM的键列宽度
// 不再需要区分哈希键列和普通键列，所有键列都使用固定10字符宽度

void GXTStudio::setupTableOptimizations(QTableView* table)
{
    if (!table) return;
    
    // 启用虚拟滚动和懒加载优化
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 基础性能优化
    table->setSortingEnabled(false);
    table->setWordWrap(false);
    table->setAlternatingRowColors(false); // 禁用，使用自定义斑马纹
    table->setShowGrid(false);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    
    // 【关键优化】虚拟滚动优化设置 - 提升渲染性能
    table->setAttribute(Qt::WA_StaticContents, true);      // 静态内容，减少重绘
    table->setAttribute(Qt::WA_OpaquePaintEvent, true);   // 启用不透明绘制，减少合成开销
    table->setAttribute(Qt::WA_NoSystemBackground, true);  // 禁用系统背景，减少绘制层级
    table->setAutoFillBackground(false);                  // 禁用自动填充背景
    table->setAttribute(Qt::WA_PaintOnScreen, true);      // 直接在屏幕上绘制，跳过中间缓冲
    
    // 【关键优化】视口优化
    table->viewport()->setAttribute(Qt::WA_StaticContents, true);
    table->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, true);
    table->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
    table->viewport()->setAutoFillBackground(false);
    table->viewport()->setAttribute(Qt::WA_PaintOnScreen, true);
    
    // 表头优化
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(m_fontSize + 10);
    
    auto* header = table->horizontalHeader();
    header->setFixedHeight(28);
    if (table->model() && table->model()->columnCount() > 0) {
        header->setSectionResizeMode(0, QHeaderView::Fixed);
        int keyColumnWidth = calculateKeyColumnWidth();
        header->resizeSection(0, keyColumnWidth);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
    }
    header->setStretchLastSection(false);
    header->setHighlightSections(false);
    
    // 滚动条优化 - 始终显示避免布局抖动
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    
    // 禁用拖拽
    table->setDragEnabled(false);
    table->setDragDropMode(QAbstractItemView::NoDragDrop);
    table->setDropIndicatorShown(false);
    table->setAcceptDrops(false);
    
    // 批处理模式优化
    table->verticalHeader()->setDefaultSectionSize(m_fontSize + 10);
    
    // 编辑触发器
    table->setEditTriggers(m_isReadOnly ? QAbstractItemView::NoEditTriggers : QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    
    // 【关键优化】禁用不必要的功能以提升性能
    table->setAutoScroll(false);
    table->setTabKeyNavigation(true);
    table->setMouseTracking(false);           // 禁用鼠标跟踪减少事件处理
    table->setFocusPolicy(Qt::StrongFocus);    // 优化焦点策略
    
    // 【关键优化】移除selectionChanged和valueChanged连接，减少不必要的重绘
    // 这些连接会导致频繁的视口更新，严重影响性能
    
    // 启用统一的行高，优化布局计算
    table->verticalHeader()->setDefaultSectionSize(m_fontSize + 10);
}

int GXTStudio::calculateKeyColumnWidth()
{
    // 【性能优化】使用缓存的字体和度量，避免重复创建
    static QFont cachedFont;
    static QFontMetrics cachedMetrics(cachedFont);
    static int cachedFontSize = -1;
    
    // 只在字体大小变化时才重新计算
    if (cachedFontSize != m_fontSize) {
        cachedFont.setFamily("Consolas");
        cachedFont.setPointSize(m_fontSize);
        cachedFont.setStyleHint(QFont::Monospace);
        cachedFont.setFixedPitch(true);
        cachedFont.setHintingPreference(QFont::PreferFullHinting);
        cachedFont.setStyleStrategy(QFont::PreferAntialias);
        
        cachedMetrics = QFontMetrics(cachedFont);
        cachedFontSize = m_fontSize;
    }
    
    // 使用10个字符宽的字符串进行测试，确保宽度准确
    static const QString testString = "000123456789"; // 确切的10个字符
    const int width = cachedMetrics.horizontalAdvance(testString);
    
    // 添加少量边距确保显示完整
    return width + 8; // 添加边距防止截断
}

void GXTStudio::showLogMessage(const QString& message)
{
    // 将消息显示在状态栏中，根据消息类型设置不同时长
    if (m_statusLabel) {
        // 确保使用UTF-8编码显示中文
        m_statusLabel->setText(message);
        m_statusLabel->setTextFormat(Qt::PlainText);

        // 根据消息内容判断显示时长
        int displayTime = 3000; // 默认3秒
        if (message.contains("失败") || message.contains("错误") || message.contains("异常")) {
            displayTime = 8000; // 错误消息显示8秒
        } else if (message.contains("成功")) {
            displayTime = 5000; // 成功消息显示5秒
        }

        // 使用QTimer在指定时间后恢复原状态
        // 使用QPointer确保在对象销毁时不会访问已释放的内存
        QPointer<GXTStudio> self = this;
        QTimer::singleShot(displayTime, this, [self]() {
            if (self) {
                self->updateStatusBar();
            }
        });
    }

    // 同时输出到控制台用于调试
    qDebug() << "[GXTStudio]" << message;
}



QString GXTStudio::getVersionString(GXTVersion version) const
{
    switch (version) {
        case GXTVersion::GTA_III: return "GTA III";
        case GXTVersion::GTA_VC: return "GTA Vice City";
        case GXTVersion::GTA_SA: return "GTA San Andreas";
        case GXTVersion::GTA_IV: return "GTA IV";
        case GXTVersion::GXT2: return "GXT2";
        default: return "Unknown";
    }
}

void GXTStudio::dragEnterEvent(QDragEnterEvent* event)
{
    // 接受文件拖拽
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        bool hasValidFiles = false;
        
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                QFileInfo fileInfo(filePath);
                
                // 检查文件扩展名是否为支持的格式
                QString suffix = fileInfo.suffix().toLower();
                if (suffix == "gxt" || suffix == "gxt2" || suffix == "whm" || suffix == "dat") {
                    hasValidFiles = true;
                    break;
                }
            }
        }
        
        if (hasValidFiles) {
            event->acceptProposedAction();
            return;
        }
    }
    
    // 接受表格拖拽
    if (event->mimeData()->hasFormat("application/x-gxt-table")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void GXTStudio::dropEvent(QDropEvent* event)
{
    // 处理文件拖拽
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        QStringList validFiles;
        
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                QFileInfo fileInfo(filePath);
                
                // 检查文件扩展名是否为支持的格式
                QString suffix = fileInfo.suffix().toLower();
                if (suffix == "gxt" || suffix == "gxt2" || suffix == "whm" || suffix == "dat") {
                    validFiles << filePath;
                }
            }
        }
        
        if (!validFiles.isEmpty()) {
            // 异步解析所有有效文件
            for (const QString& filePath : validFiles) {
                startAsyncParse(filePath);
            }
            
            QString message = validFiles.size() == 1 
                ? QString("已添加文件: %1").arg(QFileInfo(validFiles.first()).fileName())
                : QString("已添加 %1 个文件").arg(validFiles.size());
            
            showLogMessage(message);
            event->acceptProposedAction();
            return;
        }
    }
    
    // 处理表格拖拽
    if (!event->mimeData()->hasFormat("application/x-gxt-table")) {
        event->ignore();
        return;
    }
    
    // 解析拖拽数据
    QByteArray data = event->mimeData()->data("application/x-gxt-table");
    QDataStream stream(&data, QIODevice::ReadOnly);
    
    int tableIndex;
    QString tableName;
    stream >> tableIndex >> tableName;
    
    // 获取当前标签页
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || currentTab->isWHM) {
        event->ignore();
        return;
    }
    
    // 验证表索引
    if (tableIndex < 0 || tableIndex >= static_cast<int>(currentTab->tables.size())) {
        event->ignore();
        return;
    }
    
    // 选择保存位置
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出表: " + tableName, 
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/" + tableName + ".txt",
        "文本文件 (*.txt);;所有文件 (*.*)");
    
    if (!fileName.isEmpty()) {
        // 执行导出
        exportTableToFolder(tableIndex, fileName);
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

// DraggableTableList的方法实现
QString DraggableTableList::createTempTableFile(int tableIndex, const QString& tableName)
{
    // 获取父窗口中的GXTStudio实例
    QWidget* parentW = parentWidget();
    while (parentW) {
        GXTStudio* studio = qobject_cast<GXTStudio*>(parentW);
        if (studio) {
            // 获取当前标签页
            FileTab* currentTab = studio->getCurrentTab();
            if (!currentTab || currentTab->isWHM || tableIndex < 0 || 
                tableIndex >= static_cast<int>(currentTab->tables.size())) {
                return QString();
            }
            
            const auto& table = currentTab->tables[tableIndex];
            
            // 在临时目录创建文件
            QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            QString fileName = QString("%1.txt").arg(tableName);
            QString tempFilePath = tempDir + "/" + fileName;
            
            QFile file(tempFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream.setEncoding(QStringConverter::Utf8);
                
                // 写入表头
                if (currentTab->tables.size() > 1 || tableName != "MAIN") {
                    stream << QString("[%1]").arg(tableName) << Qt::endl;
                }
                
                // 写入所有条目
                for (const auto& entry : table.entries) {
                    QString keyStr;
                    // 优先使用原始键值
                    if (!entry.originalKey.empty()) {
                        keyStr = QString::fromStdString(entry.originalKey);
                    } else {
                        // 尝试使用SATKEY/IVTKEY映射
                        QString entryKey = QString::fromStdString(entry.key);
                        bool foundInLst = false;
                        
                        if (currentTab->version == GXTVersion::GTA_SA && !GXTTableModel::isSATKeyMapEmpty()) {
                            bool ok;
                            uint32_t hash = entryKey.toUInt(&ok, 16);
                            if (ok) {
                                QString lstKey;
                                if (GXTTableModel::findSATKey(hash, lstKey)) {
                                    keyStr = lstKey;
                                    foundInLst = true;
                                }
                            }
                        } else if (currentTab->version == GXTVersion::GTA_IV && !GXTTableModel::isIVTKeyMapEmpty()) {
                            bool ok;
                            uint32_t hash = entryKey.toUInt(&ok, 16);
                            if (ok) {
                                QString lstKey;
                                if (GXTTableModel::findIVTKey(hash, lstKey)) {
                                    keyStr = lstKey;
                                    foundInLst = true;
                                }
                            }
                        }
                        
                        if (!foundInLst) {
                            keyStr = GXTStudio::formatKeyForDisplay(entryKey);
                        }
                    }
                    QString line = QString("%1=%2").arg(keyStr, QString::fromStdString(entry.value));
                    stream << line << Qt::endl;
                }
                
                file.close();
                studio->showLogMessage(QString("创建临时导出文件: %1").arg(tempFilePath));
                return tempFilePath;
            }
            break;
        }
        parentW = parentW->parentWidget();
    }
    
    return QString();
}

void GXTStudio::resizeEvent(QResizeEvent* event)
{
    // 调用父类resizeEvent
    QMainWindow::resizeEvent(event);
    
    // 更严格的尺寸变化过滤，大幅减少不必要的计算
    QSize newSize = event->size();
    QSize oldSize = m_lastWindowSize;
    
    int widthDiff = newSize.width() - oldSize.width();
    
    // 【关键优化1】忽略小的宽度变化，提高阈值到30px
    if (abs(widthDiff) < 30) {
        // 宽度变化太小，直接返回，不做任何处理
        return;
    }
    
    // 【关键优化2】只有在宽度显著变化时才调整列宽
    bool needsColumnResize = abs(widthDiff) >= 50;
    
    if (needsColumnResize && m_resizeDebouncer) {
        // 停止之前的防抖定时器，重置防抖
        if (m_resizeDebouncer->isActive()) {
            m_resizeDebouncer->stop();
        }
        
        // 【关键优化3】只标记需要更新，不立即执行任何操作
        FileTab* currentTab = getCurrentTab();
        if (currentTab) {
            currentTab->cache.layoutNeedsUpdate = true;
        }
        
        // 启动新的防抖定时器，延迟执行实际操作
        m_resizeDebouncer->start();
    }
    
    // 记录新的大小
    m_lastWindowSize = newSize;
    
    // 如果启用了背景，更新标签颜色以匹配新位置的背景
    if (m_backgroundEnabled) {
        updateLabelColors();
    }
}

// ==================== 自定义背景相关方法 ====================

void GXTStudio::paintEvent(QPaintEvent* event)
{
    // 先调用父类的绘制（绘制默认背景）
    QMainWindow::paintEvent(event);

    // 如果启用了背景并且背景图片有效，则绘制自定义背景
    if (m_backgroundEnabled && !m_backgroundPixmap.isNull()) {
        QPainter painter(this);
        drawBackground(&painter);
    }
}

void GXTStudio::drawBackground(QPainter* painter)
{
    if (!painter || m_backgroundPixmap.isNull()) {
        return;
    }

    // 保存画家状态
    painter->save();

    // 设置透明度
    painter->setOpacity(m_backgroundOpacity);

    // 获取窗口中央部件的几何区域（用于绘制背景）
    QWidget* centralWidget = this->centralWidget();
    QRect targetRect;
    if (centralWidget) {
        targetRect = centralWidget->geometry();
    } else {
        targetRect = this->rect();
    }

    // 设置裁剪区域，只绘制到中央部件区域，避免覆盖工具栏和状态栏
    painter->setClipRect(targetRect);

    // 缩放图片以适应目标区域
    QPixmap scaledPixmap = m_backgroundPixmap.scaled(
        targetRect.size(),
        m_backgroundAspectRatioMode,
        Qt::SmoothTransformation
    );

    // 计算图片居中显示的位置
    int x = targetRect.x() + (targetRect.width() - scaledPixmap.width()) / 2;
    int y = targetRect.y() + (targetRect.height() - scaledPixmap.height()) / 2;

    // 绘制背景图片
    painter->drawPixmap(x, y, scaledPixmap);

    // 恢复画家状态
    painter->restore();
}

QColor GXTStudio::getTextColorForPosition(const QPoint& pos)
{
    if (!m_backgroundEnabled || m_backgroundPixmap.isNull()) {
        return QColor("#495057");  // 默认灰色
    }

    // 获取窗口中央部件的几何区域
    QWidget* centralWidget = this->centralWidget();
    QRect targetRect;
    if (centralWidget) {
        targetRect = centralWidget->geometry();
    } else {
        targetRect = this->rect();
    }

    // 计算缩放后的图片位置和大小
    QPixmap scaledPixmap = m_backgroundPixmap.scaled(
        targetRect.size(),
        m_backgroundAspectRatioMode,
        Qt::SmoothTransformation
    );

    int imgX = targetRect.x() + (targetRect.width() - scaledPixmap.width()) / 2;
    int imgY = targetRect.y() + (targetRect.height() - scaledPixmap.height()) / 2;

    // 将窗口坐标转换为图片坐标
    int pixelX = pos.x() - imgX;
    int pixelY = pos.y() - imgY;

    // 确保坐标在图片范围内
    if (pixelX < 0 || pixelX >= scaledPixmap.width() ||
        pixelY < 0 || pixelY >= scaledPixmap.height()) {
        return QColor("#495057");  // 图片外的区域返回默认灰色
    }

    // 获取该位置的像素颜色
    QImage image = scaledPixmap.toImage();
    QColor bgColor = image.pixelColor(pixelX, pixelY);

    // 计算亮度 (使用相对亮度公式)
    qreal luminance = (0.299 * bgColor.red() + 0.587 * bgColor.green() + 0.114 * bgColor.blue()) / 255.0;

    // 考虑透明度
    luminance = luminance * bgColor.alphaF() * m_backgroundOpacity + 0.95 * (1 - bgColor.alphaF() * m_backgroundOpacity);

    // 如果亮度大于0.5，使用灰色文字；否则使用白色
    if (luminance > 0.5) {
        return QColor("#495057");  // 深色文字（原来的灰色）
    } else {
        return QColor("#ffffff");  // 白色文字
    }
}

// 辅助函数：应用标签样式，确保除颜色外所有样式一致
static void applyLabelStyle(QLabel* label, const QColor& textColor)
{
    if (!label) return;
    
    // 使用完全一致的样式模板，只改变颜色
    static const QString styleTemplate = R"(
        QLabel {
            font-weight: bold;
            color: %1;
            padding: 6px 8px;
            background-color: transparent;
            border-radius: 6px;
            border: none;
        }
    )";
    
    label->setStyleSheet(styleTemplate.arg(textColor.name()));
}

void GXTStudio::updateLabelColors()
{
    // 更新所有标签页的标签颜色和搜索UI颜色
    for (auto& tab : m_fileTabs) {
        if (tab.tableListLabel) {
            QPoint pos = tab.tableListLabel->mapTo(this, QPoint(tab.tableListLabel->width() / 2, tab.tableListLabel->height() / 2));
            QColor textColor = getTextColorForPosition(pos);
            applyLabelStyle(tab.tableListLabel, textColor);
        }
        
        if (tab.entryTableLabel) {
            QPoint pos = tab.entryTableLabel->mapTo(this, QPoint(tab.entryTableLabel->width() / 2, tab.entryTableLabel->height() / 2));
            QColor textColor = getTextColorForPosition(pos);
            applyLabelStyle(tab.entryTableLabel, textColor);
        }
        
        // 更新搜索UI颜色
        updateSearchUIColors(tab);
    }
}

void GXTStudio::updateSearchUIColors(FileTab& tab)
{
    // 获取搜索区域的位置（使用搜索框的中心点）
    if (!tab.searchEdit) return;
    
    QPoint searchPos = tab.searchEdit->mapTo(this, QPoint(tab.searchEdit->width() / 2, tab.searchEdit->height() / 2));
    QColor textColor = getTextColorForPosition(searchPos);
    QString colorName = textColor.name();
    
    // 更新搜索框文字颜色
    if (tab.searchEdit) {
        tab.searchEdit->setStyleSheet(QString(R"(
            QLineEdit {
                border: 2px solid #e1e5e9;
                border-radius: 6px;
                padding: 6px 12px;
                padding-left: 14px;
                background-color: rgba(255, 255, 255, 0.5);
                font-size: 12px;
                color: %1;
            }
            QLineEdit:focus {
                border-color: #2196f3;
                outline: none;
            }
        )").arg(colorName));
    }
    
    // 更新图标按钮颜色
    QString fontFamily = FA::solidFontFamily();
    QString buttonStyle;
    if (fontFamily.isEmpty()) {
        buttonStyle = QString(R"(
            QToolButton {
                border: none;
                border-radius: 4px;
                background-color: transparent;
                color: %1;
            }
            QToolButton:hover {
                background-color: rgba(255, 255, 255, 0.5);
                color: %1;
            }
            QToolButton:pressed {
                background-color: #e9ecef;
            }
            QToolButton:checked {
                color: #2196f3;
                background-color: #e3f2fd;
            }
            QToolButton:checked:hover {
                background-color: #bbdefb;
            }
        )").arg(colorName);
    } else {
        buttonStyle = QString(R"(
            QToolButton {
                border: none;
                border-radius: 4px;
                background-color: transparent;
                color: %1;
                font-family: '%2';
                font-size: 13px;
            }
            QToolButton:hover {
                background-color: rgba(255, 255, 255, 0.5);
                color: %1;
            }
            QToolButton:pressed {
                background-color: #e9ecef;
            }
            QToolButton:checked {
                color: #2196f3;
                background-color: #e3f2fd;
            }
            QToolButton:checked:hover {
                background-color: #bbdefb;
            }
        )").arg(colorName, fontFamily);
    }
    
    if (tab.caseSensitiveButton) {
        tab.caseSensitiveButton->setStyleSheet(buttonStyle);
    }
    if (tab.regexButton) {
        tab.regexButton->setStyleSheet(buttonStyle);
    }
    
    // 更新上下个按钮样式
    QString navButtonStyle = QString(R"(
        QPushButton {
            background-color: rgba(255, 255, 255, 0.5);
            border: 1px solid #dee2e6;
            border-radius: 6px;
            color: %1;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: rgba(255, 255, 255, 0.7);
            border-color: #adb5bd;
        }
        QPushButton:pressed {
            background-color: rgba(255, 255, 255, 0.9);
        }
    )").arg(colorName);
    
    if (tab.searchPrevButton) {
        tab.searchPrevButton->setStyleSheet(navButtonStyle);
    }
    if (tab.searchNextButton) {
        tab.searchNextButton->setStyleSheet(navButtonStyle);
    }
}

void GXTStudio::setBackgroundImage(const QString& imagePath)
{
    if (imagePath.isEmpty()) {
        clearBackground();
        return;
    }

    QPixmap newPixmap(imagePath);
    if (newPixmap.isNull()) {
        showLogMessage(QString("无法加载背景图片: %1").arg(imagePath));
        return;
    }

    m_backgroundPixmap = newPixmap;
    m_backgroundEnabled = true;
    if (m_clearBackgroundAction) {
        m_clearBackgroundAction->setEnabled(true);
    }
    showLogMessage(QString("已设置背景图片: %1").arg(imagePath));
    update();  // 触发重绘
    
    // 更新标签颜色以匹配背景
    QTimer::singleShot(100, this, &GXTStudio::updateLabelColors);
}

void GXTStudio::setBackgroundOpacity(qreal opacity)
{
    // 限制透明度范围在 0.0 - 1.0 之间
    m_backgroundOpacity = qBound(0.0, opacity, 1.0);
    if (m_backgroundEnabled) {
        update();  // 触发重绘
    }
}

void GXTStudio::setBackgroundEnabled(bool enabled)
{
    m_backgroundEnabled = enabled;
    update();  // 触发重绘
}

void GXTStudio::clearBackground()
{
    m_backgroundPixmap = QPixmap();
    m_backgroundEnabled = false;
    if (m_clearBackgroundAction) {
        m_clearBackgroundAction->setEnabled(false);
    }
    update();  // 触发重绘
    showLogMessage("已清除背景图片");
    updateLabelColors();  // 恢复默认颜色
}

bool GXTStudio::isBackgroundEnabled() const
{
    return m_backgroundEnabled;
}

qreal GXTStudio::backgroundOpacity() const
{
    return m_backgroundOpacity;
}

void GXTStudio::onSetBackground()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择背景图片",
        QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif);;所有文件 (*.*)"
    );

    if (!filePath.isEmpty()) {
        setBackgroundImage(filePath);
        if (m_clearBackgroundAction) {
            m_clearBackgroundAction->setEnabled(true);
        }
    }
}

void GXTStudio::onClearBackground()
{
    clearBackground();
    if (m_clearBackgroundAction) {
        m_clearBackgroundAction->setEnabled(false);
    }
}

// ==================== 自定义背景方法结束 ====================

void GXTStudio::precomputeTableMetrics()
{
    // 预计算表格度量以提高性能
    for (auto& tab : m_fileTabs) {
        if (tab.entryTableView && tab.entryTableModel) {
            // 统一处理：所有键列都使用固定10字符宽度（GXT和WHM相同）
            int keyColumnWidth = calculateKeyColumnWidth();
            auto* header = tab.entryTableView->horizontalHeader();
            if (header) {
                header->resizeSection(0, keyColumnWidth);
            }
            // 值列宽度由布局系统自动调整，不需要固定设置
            
            // 标记布局已更新，避免不必要的重新计算
            tab.cache.layoutNeedsUpdate = false;
        }
    }
}

GXTVersion GXTStudio::selectVersionDialog()
{
    // 创建版本选择对话框
    QDialog dialog(this);
    dialog.setWindowTitle("选择GXT版本");
    dialog.setModal(true);
    dialog.resize(350, 250);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel("无法确定GXT文件版本，请选择正确的版本：", &dialog);
    layout->addWidget(label);
    
    // 创建版本选择按钮组
    QButtonGroup* buttonGroup = new QButtonGroup(&dialog);
    
    QRadioButton* gxt2Button = new QRadioButton("GXT2 (GTA V)", &dialog);
    QRadioButton* gtaIvButton = new QRadioButton("GTA IV (16位字符)", &dialog);
    QRadioButton* gtaSaButton = new QRadioButton("GTA SA (8位字符)", &dialog);
    QRadioButton* gtaVcButton = new QRadioButton("GTA VC (16位字符+TABL)", &dialog);
    QRadioButton* gta3Button = new QRadioButton("GTA III (16位字符)", &dialog);
    
    buttonGroup->addButton(gxt2Button, static_cast<int>(GXTVersion::GXT2));
    buttonGroup->addButton(gtaIvButton, static_cast<int>(GXTVersion::GTA_IV));
    buttonGroup->addButton(gtaSaButton, static_cast<int>(GXTVersion::GTA_SA));
    buttonGroup->addButton(gtaVcButton, static_cast<int>(GXTVersion::GTA_VC));
    buttonGroup->addButton(gta3Button, static_cast<int>(GXTVersion::GTA_III));
    
    layout->addWidget(gxt2Button);
    layout->addWidget(gtaIvButton);
    layout->addWidget(gtaSaButton);
    layout->addWidget(gtaVcButton);
    layout->addWidget(gta3Button);
    
    // 默认选择GTA SA
    gtaSaButton->setChecked(true);
    
    // 添加按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okButton = new QPushButton("确定", &dialog);
    QPushButton* cancelButton = new QPushButton("取消", &dialog);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    // 连接信号
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    // 显示对话框
    if (dialog.exec() == QDialog::Accepted) {
        QAbstractButton* selectedButton = buttonGroup->checkedButton();
        if (selectedButton) {
            GXTVersion selected = static_cast<GXTVersion>(buttonGroup->id(selectedButton));
            showLogMessage(QString("用户选择版本: %1").arg(getVersionString(selected)));
            return selected;
        }
    }
    
    // 默认返回GTA_SA
    showLogMessage("用户取消选择，使用默认版本: GTA_SA");
    return GXTVersion::GTA_SA;
}

// ========================================
// 重构的版本感知保存系统实现
// ========================================

void GXTStudio::initializeSaveStrategies()
{
    // 初始化所有保存策略
    
    // GTA III 保存策略
    SaveStrategy gta3Strategy;
    gta3Strategy.version = GXTVersion::GTA_III;
    gta3Strategy.description = "GTA III 格式 - 16位字符，无TABL块";
    gta3Strategy.fileExtensions = {".gxt"};
    gta3Strategy.saveFunction = [](GXTEditor& editor, const std::string& path) -> bool {
        return editor.saveAsGTA_III(path);
    };
    m_saveStrategies[GXTVersion::GTA_III] = gta3Strategy;
    
    // GTA VC 保存策略
    SaveStrategy gtaVcStrategy;
    gtaVcStrategy.version = GXTVersion::GTA_VC;
    gtaVcStrategy.description = "GTA VC 格式 - 16位字符，有TABL块";
    gtaVcStrategy.fileExtensions = {".gxt"};
    gtaVcStrategy.saveFunction = [](GXTEditor& editor, const std::string& path) -> bool {
        return editor.saveAsGTA_VC(path);
    };
    m_saveStrategies[GXTVersion::GTA_VC] = gtaVcStrategy;
    
    // GTA SA 保存策略
    SaveStrategy gtaSaStrategy;
    gtaSaStrategy.version = GXTVersion::GTA_SA;
    gtaSaStrategy.description = "GTA SA 格式 - 8位字符，有版本头";
    gtaSaStrategy.fileExtensions = {".gxt"};
    gtaSaStrategy.saveFunction = [](GXTEditor& editor, const std::string& path) -> bool {
        return editor.saveAsGTA_SA(path);
    };
    m_saveStrategies[GXTVersion::GTA_SA] = gtaSaStrategy;
    
    // GTA IV 保存策略
    SaveStrategy gtaIvStrategy;
    gtaIvStrategy.version = GXTVersion::GTA_IV;
    gtaIvStrategy.description = "GTA IV 格式 - 16位字符，有版本头";
    gtaIvStrategy.fileExtensions = {".gxt"};
    gtaIvStrategy.saveFunction = [](GXTEditor& editor, const std::string& path) -> bool {
        return editor.saveAsGTA_IV(path);
    };
    m_saveStrategies[GXTVersion::GTA_IV] = gtaIvStrategy;
    
    // GXT2 保存策略
    SaveStrategy gxt2Strategy;
    gxt2Strategy.version = GXTVersion::GXT2;
    gxt2Strategy.description = "GXT2 格式 - GTA V 双头部结构";
    gxt2Strategy.fileExtensions = {".gxt2"};
    gxt2Strategy.saveFunction = [](GXTEditor& editor, const std::string& path) -> bool {
        return editor.saveAsGXT2Format(path);
    };
    m_saveStrategies[GXTVersion::GXT2] = gxt2Strategy;
    
    // 文本格式保存策略（特殊处理）
    SaveStrategy textStrategy;
    textStrategy.version = GXTVersion::UNKNOWN;
    textStrategy.description = "文本格式导出";
    textStrategy.fileExtensions = {".txt", ".whm", ".dat"};
    textStrategy.saveFunction = [](GXTEditor& editor, const std::string& path) -> bool {
        return editor.saveAsText(path);
    };
    
    // 构建所有策略列表
    m_allSaveStrategies.push_back(gta3Strategy);
    m_allSaveStrategies.push_back(gtaVcStrategy);
    m_allSaveStrategies.push_back(gtaSaStrategy);
    m_allSaveStrategies.push_back(gtaIvStrategy);
    m_allSaveStrategies.push_back(gxt2Strategy);
    m_allSaveStrategies.push_back(textStrategy);
}

bool GXTStudio::saveFileWithVersion(FileTab* tab, const QString& filePath)
{
    if (!tab) {
        showLogMessage("错误：无效的标签页");
        return false;
    }
    
    QString targetPath = filePath.isEmpty() ? tab->filePath : filePath;
    if (targetPath.isEmpty()) {
        showLogMessage("错误：文件路径为空");
        return false;
    }
    
    try {
        // 检测最佳保存策略
        SaveStrategy strategy = detectBestSaveStrategy(tab, targetPath);
        
        if (strategy.version == GXTVersion::UNKNOWN && 
            (targetPath.endsWith(".txt", Qt::CaseInsensitive) || 
             targetPath.endsWith(".whm", Qt::CaseInsensitive) || 
             targetPath.endsWith(".dat", Qt::CaseInsensitive))) {
            // 特殊处理文本格式
            return saveFileUsingStrategy(tab, strategy, targetPath);
        }
        
        // 验证版本兼容性
        GXTVersion targetVersion = strategy.version;
        if (tab->version != GXTVersion::UNKNOWN && targetVersion != GXTVersion::UNKNOWN) {
            if (!validateVersionCompatibility(tab->version, targetVersion)) {
                // 版本不兼容，需要用户确认或转换
            }
        }
        
        // 如果无法确定版本，给出明确的错误提示
        if (strategy.version == GXTVersion::UNKNOWN && 
            !targetPath.endsWith(".txt", Qt::CaseInsensitive) && 
            !targetPath.endsWith(".whm", Qt::CaseInsensitive) && 
            !targetPath.endsWith(".dat", Qt::CaseInsensitive)) {
            throw std::runtime_error("无法确定GXT文件版本，请使用版本选择功能指定正确的格式");
        }
        
        // 执行保存操作
        bool success = saveFileUsingStrategy(tab, strategy, targetPath);
        
        if (success) {
            // 更新标签页信息
            if (filePath.isEmpty()) {
                // 保存操作，更新文件状态
                tab->isModified = false;
            } else {
                // 另存为操作，更新文件路径和名称
                tab->filePath = targetPath;
                tab->fileName = QFileInfo(targetPath).fileName();
                tab->isModified = false;
                
                // 更新标签页标题
                int tabIndex = m_fileTabs.size() - 1;
                if (tabIndex >= 0 && tabIndex < m_tabWidget->count()) {
                    m_tabWidget->setTabText(tabIndex, tab->fileName);
                }
                
                updateWindowTitle();
            }
        } else {
            // 保存函数返回false但没有抛出异常，手动创建错误信息
            throw std::runtime_error(QString("保存函数执行失败 - 文件: %1, 策略: %2")
                                     .arg(targetPath)
                                     .arg(QString::fromStdString(strategy.description)).toStdString());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        return handleSaveError("保存文件", e, targetPath);
    }
}

bool GXTStudio::saveFileUsingStrategy(FileTab* tab, const SaveStrategy& strategy, const QString& filePath)
{
    if (!tab || filePath.isEmpty()) {
        showLogMessage("错误：无效的标签页或文件路径为空");
        throw std::runtime_error("无效的标签页或文件路径为空");
    }
    
    // 使用本地8位编码处理路径，避免Unicode问题
#ifdef _WIN32
    // 在Windows上，使用本地code page编码
    std::string narrowPath = filePath.toLocal8Bit().toStdString();
#else
    std::string narrowPath = filePath.toStdString();
#endif
    
    showLogMessage(QString("使用保存策略: %1, 目标文件: %2")
                  .arg(QString::fromStdString(strategy.description))
                  .arg(filePath));
    showLogMessage(QString("narrowPath 大小: %1 字节").arg(narrowPath.size()));
    
    // 对于WHM文件或文本格式，使用特殊处理
    if (tab->isWHM || strategy.version == GXTVersion::UNKNOWN) {
        showLogMessage("使用特殊格式保存处理");
        bool success = performVersionSpecificSave(tab, filePath);
        if (!success) {
            throw std::runtime_error(QString("特殊格式保存失败: %1").arg(filePath).toStdString());
        }
        return true;
    }
    
    try {
        // 准备编辑器进行保存
        GXTEditor editor;
        prepareEditorForSave(tab, editor);
        
        // 验证编辑器状态
        if (editor.getTableCount() == 0) {
            throw std::runtime_error("编辑器中没有数据可以保存");
        }
        
        // 设置版本信息
        if (strategy.version != GXTVersion::UNKNOWN) {
            editor.setVersion(strategy.version);
        } else {
            editor.setVersion(tab->version);
        }
        
        // 执行保存
        bool success = strategy.saveFunction(editor, narrowPath);
        
        if (success) {
            // 保存成功，记录操作
            logSaveOperation(QString::fromStdString(strategy.description), 
                           strategy.version, filePath, success);
            
            showLogMessage(QString("文件已成功保存: %1 (版本: %2)")
                          .arg(QFileInfo(filePath).fileName())
                          .arg(getVersionString(strategy.version)));
        } else {
            // 保存函数返回false但没有抛出异常，需要提供详细错误信息
            QString errorMsg = QString("保存函数执行失败\n文件: %1\n策略: %2\n表数量: %3\n版本: %4")
                              .arg(filePath)
                              .arg(QString::fromStdString(strategy.description))
                              .arg(editor.getTableCount())
                              .arg(getVersionString(tab->version));
            showLogMessage(errorMsg);
            throw std::runtime_error(errorMsg.toStdString());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        QString errorMsg = QString("保存异常: %1").arg(e.what());
        showLogMessage(errorMsg);
        logSaveOperation(QString::fromStdString(strategy.description), 
                       strategy.version, filePath, false);
        // 重新抛出异常，让上层处理
        throw;
    }
}

GXTStudio::SaveStrategy GXTStudio::getSaveStrategyForVersion(GXTVersion version, const QString& fileExtension)
{
    // 特殊处理文本格式
    if (fileExtension.toLower() == ".txt" || 
        fileExtension.toLower() == ".whm" || 
        fileExtension.toLower() == ".dat") {
        
        for (const auto& strategy : m_allSaveStrategies) {
            if (strategy.version == GXTVersion::UNKNOWN) {
                showLogMessage("使用文本格式策略");
                return strategy;
            }
        }
    }
    
    // 查找匹配版本的策略
    auto it = m_saveStrategies.find(version);
    if (it != m_saveStrategies.end()) {
        return it->second;
    }
    
    // 如果没有找到，返回默认策略（文本格式）
    for (const auto& strategy : m_allSaveStrategies) {
        if (strategy.version == GXTVersion::UNKNOWN) {
            return strategy;
        }
    }
    
    // 最后的回退策略
    SaveStrategy fallbackStrategy;
    fallbackStrategy.version = GXTVersion::UNKNOWN;
    fallbackStrategy.description = "回退文本格式";
    fallbackStrategy.fileExtensions = {".txt", ".whm", ".dat"};
    fallbackStrategy.saveFunction = [](GXTEditor& editor, const std::string& path) -> bool {
        return editor.saveAsText(path);
    };
    return fallbackStrategy;
}

GXTStudio::SaveStrategy GXTStudio::detectBestSaveStrategy(FileTab* tab, const QString& filePath)
{
    if (!tab) {
        showLogMessage("错误：detectBestSaveStrategy - 标签页为空");
        // 返回文本格式作为默认策略
        SaveStrategy fallbackStrategy;
        fallbackStrategy.version = GXTVersion::UNKNOWN;
        fallbackStrategy.description = "默认文本格式";
        fallbackStrategy.fileExtensions = {".txt", ".whm", ".dat"};
        fallbackStrategy.saveFunction = [](GXTEditor& editor, const std::string& path) -> bool {
            return editor.saveAsText(path);
        };
        return fallbackStrategy;
    }
    
    QString fileExtension = QFileInfo(filePath).suffix().toLower();
    
    // WHM文件特殊处理
    if (tab->isWHM) {
        SaveStrategy whmStrategy;
        whmStrategy.version = GXTVersion::UNKNOWN;
        whmStrategy.description = "WHM 格式";
        whmStrategy.fileExtensions = {".whm", ".dat"};
        whmStrategy.saveFunction = [](GXTEditor&, const std::string&) -> bool { 
            return true; 
        };
        return whmStrategy;
    }
    
    // 文本格式直接返回文本策略
    if (fileExtension == "txt" || fileExtension == "whm" || fileExtension == "dat") {
        SaveStrategy textStrategy;
        textStrategy.version = GXTVersion::UNKNOWN;
        textStrategy.description = "文本格式";
        textStrategy.fileExtensions = {".txt", ".whm", ".dat"};
        textStrategy.saveFunction = [](GXTEditor& editor, const std::string& path) -> bool {
            return editor.saveAsText(path);
        };
        return textStrategy;
    }
    
    // 对于GXT/GXT2文件，如果版本未知，返回失败策略
    if (tab->version == GXTVersion::UNKNOWN) {
        SaveStrategy failStrategy;
        failStrategy.version = GXTVersion::UNKNOWN;
        failStrategy.description = "版本未确定 - 无法保存";
        failStrategy.fileExtensions = {".gxt", ".gxt2"};
        failStrategy.saveFunction = [](GXTEditor&, const std::string&) -> bool {
            return false;
        };
        return failStrategy;
    }
    
    // 根据文件扩展名和原始版本确定策略
    GXTVersion targetVersion = detectCompatibleVersion(tab->version, fileExtension);
    SaveStrategy strategy = getSaveStrategyForVersion(targetVersion, "." + fileExtension);
    
    return strategy;
}

GXTVersion GXTStudio::detectCompatibleVersion(GXTVersion originalVersion, const QString& targetExtension)
{
    // 如果原版本未知，返回UNKNOWN让上层处理
    if (originalVersion == GXTVersion::UNKNOWN) {
        return GXTVersion::UNKNOWN;
    }
    
    // 根据目标扩展名调整版本
    if (targetExtension == "gxt2") {
        return GXTVersion::GXT2;
    } else if (targetExtension == "gxt") {
        return originalVersion;
    } else {
        return GXTVersion::UNKNOWN;
    }
}

bool GXTStudio::validateVersionCompatibility(GXTVersion sourceVersion, GXTVersion targetVersion)
{
    // 如果版本相同，完全兼容
    if (sourceVersion == targetVersion) {
        return true;
    }
    
    // 未知版本可以转换为任何版本（可能会有数据丢失）
    if (sourceVersion == GXTVersion::UNKNOWN || targetVersion == GXTVersion::UNKNOWN) {
        return true;
    }
    
    // 定义兼容性矩阵
    static std::map<GXTVersion, std::vector<GXTVersion>> compatibilityMatrix = {
        {GXTVersion::GTA_III, {GXTVersion::GTA_III, GXTVersion::GTA_VC}},
        {GXTVersion::GTA_VC, {GXTVersion::GTA_III, GXTVersion::GTA_VC, GXTVersion::GTA_SA, GXTVersion::GTA_IV}},
        {GXTVersion::GTA_SA, {GXTVersion::GTA_SA, GXTVersion::GTA_VC, GXTVersion::GTA_IV}},
        {GXTVersion::GTA_IV, {GXTVersion::GTA_IV, GXTVersion::GTA_VC, GXTVersion::GTA_SA}},
        {GXTVersion::GXT2, {GXTVersion::GXT2}} // GXT2格式特殊，不与其他格式兼容
    };
    
    auto it = compatibilityMatrix.find(sourceVersion);
    if (it != compatibilityMatrix.end()) {
        const auto& compatibleVersions = it->second;
        return std::find(compatibleVersions.begin(), compatibleVersions.end(), targetVersion) != compatibleVersions.end();
    }
    
    return false;
}

void GXTStudio::prepareEditorForSave(FileTab* tab, GXTEditor& editor)
{
    if (!tab) {
        showLogMessage("错误：prepareEditorForSave - 标签页为空");
        return;
    }
    
    showLogMessage(QString("准备编辑器数据 - 表数量: %1, WHM: %2")
                  .arg(tab->tables.size())
                  .arg(tab->isWHM ? "是" : "否"));
    
    // 清空编辑器
    editor.clear();
    
    // 转移数据到编辑器
    if (tab->isWHM) {
        // WHM文件不使用标准编辑器
        showLogMessage("WHM文件，跳过编辑器数据准备");
        return;
    }
    
    // 添加所有表
    for (size_t i = 0; i < tab->tables.size(); ++i) {
        const auto& table = tab->tables[i];
        showLogMessage(QString("添加表 %1: %2 (条目数: %3)")
                      .arg(i)
                      .arg(QString::fromStdString(table.name))
                      .arg(table.entries.size()));
        
        editor.addTable(table.name);
        int tableIndex = editor.getTableCount() - 1;
        
        // 添加所有条目
        int addedCount = 0;
        int failedCount = 0;
        for (size_t j = 0; j < table.entries.size(); ++j) {
            const auto& entry = table.entries[j];

            // 调试日志：记录原始键值
            QString debugKey = QString::fromStdString(entry.key);
            QString debugOriginalKey = QString::fromStdString(entry.originalKey);
            if (debugKey.contains("FF_WARN", Qt::CaseInsensitive) || debugOriginalKey.contains("FF_WARN", Qt::CaseInsensitive)) {
                showLogMessage(QString("[DEBUG] FF_WARN条目: entry.key=%1, entry.originalKey=%2, version=%3")
                              .arg(debugKey)
                              .arg(debugOriginalKey)
                              .arg(static_cast<int>(tab->version)));
            }

            // 使用已存储的hash值（entry.key），它已经是在添加条目时计算好的
            // 如果是通过addEntryFromInputs添加的，originalKey保存了原始字符串
            std::string saveKey = entry.key;

            // 只有当用户直接在表格中编辑了键列（没有originalKey或originalKey改变）时才需要重新计算hash
            // 如果originalKey存在，说明entry.key已经是正确的hash值，无需重新计算
            bool needsRecalculate = entry.originalKey.empty();

            if (needsRecalculate) {
                // 用户直接在表格中编辑了键列，需要重新计算hash
                QString keyToCheck = QString::fromStdString(entry.key);

                // 调试日志：记录需要重新计算的键
                if (keyToCheck.contains("FF_WARN", Qt::CaseInsensitive)) {
                    showLogMessage(QString("[DEBUG] 需要重新计算hash: keyToCheck=%1, 长度=%2")
                                  .arg(keyToCheck)
                                  .arg(keyToCheck.length()));
                }

                // 检查是否是hex格式（只检查是否有0x前缀）
                bool isHexFormat = keyToCheck.startsWith("0x", Qt::CaseInsensitive);

                // 如果不是hex格式，则需要反向转换（重新计算hash）
                if (!isHexFormat) {
                uint32_t hash;
                if (tab->version == GXTVersion::GTA_SA) {
                    // 尝试在SATKEY映射中反向查找
                    if (GXTTableModel::findSATHash(keyToCheck, hash)) {
                        saveKey = QString("%1").arg(hash, 8, 16, QChar('0')).toStdString();
                    } else {
                        // 使用CKeyGen计算hash
                        std::string upperKey = keyToCheck.toUpper().toStdString();
                        hash = CKeyGen::GetUppercaseKey(upperKey.c_str());
                        saveKey = QString("%1").arg(hash, 8, 16, QChar('0')).toStdString();
                    }
                } else if (tab->version == GXTVersion::GTA_IV) {
                    // 尝试在IVTKEY映射中反向查找
                    if (GXTTableModel::findIVTHash(keyToCheck, hash)) {
                        saveKey = QString("%1").arg(hash, 8, 16, QChar('0')).toStdString();
                        if (keyToCheck.contains("FF_WARN", Qt::CaseInsensitive)) {
                            showLogMessage(QString("[DEBUG] IVTKEY查找成功: %1 -> 0x%2").arg(keyToCheck).arg(QString::number(hash, 16)));
                        }
                    } else {
                        // 计算GTA4哈希
                        hash = GXTEditor::calculateGTA4Hash(keyToCheck.toStdString());
                        saveKey = QString("%1").arg(hash, 8, 16, QChar('0')).toStdString();
                        if (keyToCheck.contains("FF_WARN", Qt::CaseInsensitive)) {
                            showLogMessage(QString("[DEBUG] GTA4 hash计算: %1 -> 0x%2").arg(keyToCheck).arg(QString::number(hash, 16)));
                        }
                    }
                }
                }
            }

            if (j < 5) { // 只记录前5个条目，避免日志过多
                showLogMessage(QString("  添加条目: %1 = %2")
                              .arg(QString::fromStdString(saveKey))
                              .arg(QString::fromStdString(entry.value).left(20)));
            }
            bool addResult = editor.addEntry(tableIndex, saveKey, entry.value);
            if (addResult) {
                addedCount++;
            } else {
                failedCount++;
                if (failedCount <= 3) { // 只记录前3个失败
                    showLogMessage(QString("  [错误] 添加条目失败: %1 = %2")
                                  .arg(QString::fromStdString(entry.key))
                                  .arg(QString::fromStdString(entry.value).left(20)));
                }
            }
        }
        if (table.entries.size() > 5) {
            showLogMessage(QString("  ... 还有 %1 个条目").arg(table.entries.size() - 5));
        }
        showLogMessage(QString("表 %1 统计: 成功添加 %2 个条目, 失败 %3 个条目")
                      .arg(QString::fromStdString(table.name))
                      .arg(addedCount)
                      .arg(failedCount));
    }
    
    // 设置版本信息
    if (tab->version != GXTVersion::UNKNOWN) {
        editor.setVersion(tab->version);
        showLogMessage(QString("设置编辑器版本: %1").arg(getVersionString(tab->version)));
    } else {
        showLogMessage("标签页版本为UNKNOWN，不设置编辑器版本");
    }
    
    showLogMessage(QString("编辑器数据准备完成 - 最终表数量: %1").arg(editor.getTableCount()));
}

bool GXTStudio::performVersionSpecificSave(FileTab* tab, const QString& filePath)
{
    if (!tab) {
        showLogMessage("错误：performVersionSpecificSave - 标签页为空");
        return false;
    }
    
    showLogMessage(QString("执行特殊格式保存 - WHM: %1, 文件: %2")
                  .arg(tab->isWHM ? "是" : "否")
                  .arg(filePath));
    
    // WHM文件特殊处理
    if (tab->isWHM) {
        showLogMessage("使用WHM直接保存");
        return saveWHMDirectly(tab, filePath);
    }
    
    // 文本格式处理
    if (filePath.endsWith(".txt", Qt::CaseInsensitive) || 
        filePath.endsWith(".whm", Qt::CaseInsensitive) || 
        filePath.endsWith(".dat", Qt::CaseInsensitive)) {
        
        showLogMessage("使用文本格式保存");
        
        try {
            showLogMessage("准备编辑器进行文本保存...");
            GXTEditor editor;
            prepareEditorForSave(tab, editor);

            std::string narrowPath = filePath.toLocal8Bit().toStdString();
            showLogMessage(QString("调用editor.saveAsText保存到: %1").arg(QString::fromStdString(narrowPath)));
            bool success = editor.saveAsText(narrowPath);
            
            if (success) {
                showLogMessage("文本保存成功");
            } else {
                showLogMessage("文本保存失败（函数返回false）");
            }
            
            logSaveOperation("文本格式导出", GXTVersion::UNKNOWN, filePath, success);
            return success;
            
        } catch (const std::exception& e) {
            showLogMessage(QString("文本保存异常: %1").arg(e.what()));
            return handleSaveError("文本格式保存", e, filePath);
        }
    }
    
    showLogMessage("错误：未知的文件格式，无法保存");
    return false;
}

bool GXTStudio::handleSaveError(const QString& operation, const std::exception& e, const QString& filePath)
{
    QString errorMessage = QString("%1 失败: %2\n\n文件: %3")
                         .arg(operation)
                         .arg(e.what())
                         .arg(filePath);
    
    QString logMessage = QString("保存错误详情 - 操作: %1, 错误: %2, 文件: %3")
                        .arg(operation)
                        .arg(e.what())
                        .arg(filePath);
    
    showLogMessage(logMessage);
    QMessageBox::warning(this, "保存错误", errorMessage);
    
    return false;
}

void GXTStudio::logSaveOperation(const QString& operation, GXTVersion version, const QString& filePath, bool success)
{
    QString logMessage = QString("%1 - %2 (%3) - %4")
                        .arg(success ? "✓" : "✗")
                        .arg(operation)
                        .arg(getVersionString(version))
                        .arg(QFileInfo(filePath).fileName());
    
    showLogMessage(logMessage);
}

std::vector<GXTVersion> GXTStudio::getCompatibleVersions(GXTVersion sourceVersion)
{
    std::vector<GXTVersion> compatibleVersions;
    
    // 添加自身版本
    compatibleVersions.push_back(sourceVersion);
    
    // 根据兼容性矩阵添加其他版本
    static std::map<GXTVersion, std::vector<GXTVersion>> compatibilityMatrix = {
        {GXTVersion::GTA_III, {GXTVersion::GTA_VC}},
        {GXTVersion::GTA_VC, {GXTVersion::GTA_III, GXTVersion::GTA_SA, GXTVersion::GTA_IV}},
        {GXTVersion::GTA_SA, {GXTVersion::GTA_VC, GXTVersion::GTA_IV}},
        {GXTVersion::GTA_IV, {GXTVersion::GTA_VC, GXTVersion::GTA_SA}},
        {GXTVersion::GXT2, {}}, // GXT2格式特殊
        {GXTVersion::UNKNOWN, {GXTVersion::GTA_III, GXTVersion::GTA_VC, GXTVersion::GTA_SA, GXTVersion::GTA_IV, GXTVersion::GXT2}}
    };
    
    auto it = compatibilityMatrix.find(sourceVersion);
    if (it != compatibilityMatrix.end()) {
        for (GXTVersion version : it->second) {
            compatibleVersions.push_back(version);
        }
    }
    
    return compatibleVersions;
}

bool GXTStudio::canConvertBetweenVersions(GXTVersion from, GXTVersion to)
{
    if (from == to) return true;
    if (from == GXTVersion::UNKNOWN || to == GXTVersion::UNKNOWN) return true;
    if (to == GXTVersion::GXT2) return from == GXTVersion::GXT2; // GXT2特殊处理
    
    auto compatibleVersions = getCompatibleVersions(from);
    return std::find(compatibleVersions.begin(), compatibleVersions.end(), to) != compatibleVersions.end();
}

std::string GXTStudio::getVersionCompatibilityInfo(GXTVersion version)
{
    switch (version) {
        case GXTVersion::GTA_III:
            return "GTA III格式：可转换为GTA VC格式";
        case GXTVersion::GTA_VC:
            return "GTA VC格式：可转换为所有GTA III/SA/IV格式";
        case GXTVersion::GTA_SA:
            return "GTA SA格式：8位字符，可转换为VC/IV格式";
        case GXTVersion::GTA_IV:
            return "GTA IV格式：16位字符，可转换为VC/SA格式";
        case GXTVersion::GXT2:
            return "GXT2格式：GTA V专用，不与其他格式兼容";
        default:
            return "未知格式";
    }
}

bool GXTStudio::saveWHMDirectly(FileTab* tab, const QString& filePath)
{
    if (!tab) {
        showLogMessage("错误：saveWHMDirectly - 标签页为空");
        return false;
    }
    
    if (!tab->isWHM) {
        showLogMessage("错误：saveWHMDirectly - 不是WHM文件");
        return false;
    }

    showLogMessage(QString("开始WHM直接保存 - 文件: %1, 条目数: %2")
                  .arg(filePath)
                  .arg(tab->whmEntries.size()));
    
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            QString errorMsg = QString("无法创建文件: %1 (错误: %2)")
                              .arg(filePath)
                              .arg(file.errorString());
            showLogMessage(errorMsg);
            throw std::runtime_error(errorMsg.toStdString());
        }
        
        showLogMessage("文件创建成功，开始写入二进制数据");
        
        // 构建blob（文本数据块）
        QByteArray blob;
        QVector<uint32_t> offsets;
        
        for (const auto& entry : tab->whmEntries) {
            offsets.append(blob.size());
            std::string text = entry.text;
            blob.append(text.c_str(), text.length());
            blob.append('\0'); // null终止符
        }
        
        // 写入条目数
        uint32_t count = static_cast<uint32_t>(tab->whmEntries.size());
        file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
        
        // 写入条目表（hash + offset）
        for (size_t i = 0; i < tab->whmEntries.size(); ++i) {
            uint32_t hash = tab->whmEntries[i].hash;
            uint32_t offset = offsets[i];
            file.write(reinterpret_cast<const char*>(&hash), sizeof(uint32_t));
            file.write(reinterpret_cast<const char*>(&offset), sizeof(uint32_t));
        }
        
        // 写入blob大小
        uint32_t blobSize = static_cast<uint32_t>(blob.size());
        file.write(reinterpret_cast<const char*>(&blobSize), sizeof(uint32_t));
        
        // 写入blob数据
        file.write(blob);
        
        file.close();
        
        showLogMessage(QString("WHM文件保存完成 - 文件: %1, 条目: %2, blob大小: %3 字节")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(tab->whmEntries.size())
                      .arg(blob.size()));
        
        return true;
        
    } catch (const std::exception& e) {
        showLogMessage(QString("WHM保存异常: %1").arg(e.what()));
        return handleSaveError("WHM文件保存", e, filePath);
    }
}// DAT文件保存方法（完全独立于WHM）
bool GXTStudio::saveDATDirectly(FileTab* tab, const QString& filePath)
{
    if (!tab) {
        showLogMessage("错误：saveDATDirectly - 标签页为空");
        return false;
    }
    
    if (!tab->isDAT) {
        showLogMessage("错误：saveDATDirectly - 不是DAT文件");
        return false;
    }

    showLogMessage(QString("开始DAT直接保存 - 文件: %1, 条目数: %2")
                  .arg(filePath)
                  .arg(tab->datEntries.size()));
    
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            QString errorMsg = QString("无法创建文件: %1 (错误: %2)")
                              .arg(filePath)
                              .arg(file.errorString());
            showLogMessage(errorMsg);
            throw std::runtime_error(errorMsg.toStdString());
        }
        
        showLogMessage("文件创建成功，开始写入二进制数据");
        
        // 构建blob（文本数据块）
        QByteArray blob;
        QVector<uint32_t> offsets;
        
        for (const auto& entry : tab->datEntries) {
            offsets.append(blob.size());
            std::string text = entry.text;
            blob.append(text.c_str(), text.length());
            blob.append('\0'); // null终止符
        }
        
        // 写入条目数
        uint32_t count = static_cast<uint32_t>(tab->datEntries.size());
        file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
        
        // 写入条目表（hash + offset）
        for (size_t i = 0; i < tab->datEntries.size(); ++i) {
            uint32_t hash = tab->datEntries[i].hash;
            uint32_t offset = offsets[i];
            file.write(reinterpret_cast<const char*>(&hash), sizeof(uint32_t));
            file.write(reinterpret_cast<const char*>(&offset), sizeof(uint32_t));
        }
        
        // 写入blob大小
        uint32_t blobSize = static_cast<uint32_t>(blob.size());
        file.write(reinterpret_cast<const char*>(&blobSize), sizeof(uint32_t));
        
        // 写入blob数据
        file.write(blob);
        
        file.close();
        
        showLogMessage(QString("DAT文件保存完成 - 文件: %1, 条目: %2, blob大小: %3 字节")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(tab->datEntries.size())
                      .arg(blob.size()));
        
        return true;
        
    } catch (const std::exception& e) {
        showLogMessage(QString("DAT保存异常: %1").arg(e.what()));
        return handleSaveError("DAT文件保存", e, filePath);
    }
}

void GXTStudio::onSaveProgress(int percentage, const QString& message)
{
    // 更新状态栏进度条
    if (m_saveProgressBar) {
        m_saveProgressBar->setValue(percentage);
        m_saveProgressBar->setFormat(message + " %p%");
        if (!m_saveProgressBar->isVisible()) {
            m_saveProgressBar->show();
        }
    }
}

void GXTStudio::onSaveCompleted(const SaveResult& result)
{
    // 隐藏状态栏进度条
    if (m_saveProgressBar) {
        m_saveProgressBar->hide();
        m_saveProgressBar->setValue(0);
    }
    
    if (!result.success) {
        // 保存失败 - 显示自定义对话框
        showLogMessage(QString("保存失败: %1 - %2").arg(result.fileName, result.errorMessage));
        
        QDialog failDialog(this);
        failDialog.setWindowTitle("保存失败");
        failDialog.setWindowIcon(QIcon());
        failDialog.setModal(true);
        failDialog.setMinimumWidth(480);
        failDialog.setMinimumHeight(280);
        
        // 设置现代风格
        failDialog.setStyleSheet(
            "QDialog { background-color: #ffffff; border-radius: 6px; }"
            "QLabel { color: #333333; }"
            "QLabel#titleLabel { font-size: 14px; font-weight: bold; color: #e74c3c; margin-bottom: 6px; }"
            "QLabel#labelKey { font-size: 11px; font-weight: bold; color: #2c3e50; }"
            "QLabel#labelValue { font-size: 11px; color: #34495e; }"
            "QLabel#labelPath { font-size: 10px; color: #34495e; font-family: Consolas, monospace; background-color: #f8f9fa; padding: 5px; border-radius: 3px; border: 1px solid #ecf0f1; }"
            "QPushButton { "
            "  background-color: #e74c3c; color: white; border: none; border-radius: 3px; "
            "  padding: 6px 12px; font-weight: bold; font-size: 11px; "
            "} "
            "QPushButton:hover { background-color: #c0392b; } "
            "QPushButton:pressed { background-color: #a93226; }"
            "QPushButton#pathButton { padding: 4px 8px; font-size: 12px; background-color: #95a5a6; min-width: 32px; }"
            "QPushButton#pathButton:hover { background-color: #7f8c8d; }"
        );
        
        QVBoxLayout* mainLayout = new QVBoxLayout(&failDialog);
        mainLayout->setContentsMargins(20, 16, 20, 16);
        mainLayout->setSpacing(12);
        
        // 标题
        QString titleText;
        titleText += FA::QTimes;
        titleText += " 文件保存失败";
        QLabel* titleLabel = new QLabel(titleText);
        titleLabel->setObjectName("titleLabel");
        QFont faFont(FA::solidFontFamily());
        faFont.setPointSize(11);
        titleLabel->setFont(faFont);
        mainLayout->addWidget(titleLabel);
        
        // 详情框
        QGroupBox* infoGroup = new QGroupBox();
        infoGroup->setStyleSheet(
            "QGroupBox { border: 1px solid #fadbd8; background-color: #fdeaea; border-radius: 4px; }"
        );
        
        QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);
        infoLayout->setSpacing(12);
        infoLayout->setContentsMargins(16, 12, 16, 12);
        
        // 行1: 文件名
        QHBoxLayout* row1 = new QHBoxLayout();
        QLabel* fileNameLabel = new QLabel("文件名:");
        fileNameLabel->setObjectName("labelKey");
        fileNameLabel->setFixedWidth(50);
        QLabel* fileNameValue = new QLabel(result.fileName);
        fileNameValue->setObjectName("labelValue");
        fileNameValue->setWordWrap(false);
        row1->addWidget(fileNameLabel);
        row1->addWidget(fileNameValue);
        row1->addStretch();
        infoLayout->addLayout(row1);
        
        // 行2: 路径 (可复制，可打开文件夹)
        QHBoxLayout* row2 = new QHBoxLayout();
        QLabel* pathLabel = new QLabel("路径:");
        pathLabel->setObjectName("labelKey");
        pathLabel->setFixedWidth(50);
        
        QLineEdit* pathEdit = new QLineEdit(result.filePath);
        pathEdit->setObjectName("labelPath");
        pathEdit->setReadOnly(true);
        pathEdit->setFixedHeight(24);
        pathEdit->setCursor(Qt::IBeamCursor);
        
        row2->addWidget(pathLabel);
        row2->addWidget(pathEdit);
        
        // 打开文件夹按钮
        QPushButton* openFolderBtn = new QPushButton();
        openFolderBtn->setObjectName("pathButton");
        openFolderBtn->setFixedSize(28, 24);
        openFolderBtn->setToolTip("打开文件夹");
        QString folderIconText;
        folderIconText += FA::QFolder;
        openFolderBtn->setText(folderIconText);
        QFont faFont1(FA::solidFontFamily());
        faFont1.setPointSize(11);
        openFolderBtn->setFont(faFont1);
        connect(openFolderBtn, &QPushButton::clicked, [result]() {
            QFileInfo fileInfo(result.filePath);
            QString folderPath = fileInfo.absolutePath();
            QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
        });
        row2->addWidget(openFolderBtn);
        
        // 复制按钮
        QPushButton* copyBtn = new QPushButton();
        copyBtn->setObjectName("pathButton");
        copyBtn->setFixedSize(28, 24);
        copyBtn->setToolTip("复制路径");
        QString copyIconText;
        copyIconText += FA::QClipboardList;
        copyBtn->setText(copyIconText);
        QFont faFont2(FA::solidFontFamily());
        faFont2.setPointSize(11);
        copyBtn->setFont(faFont2);
        connect(copyBtn, &QPushButton::clicked, [result]() {
            QApplication::clipboard()->setText(result.filePath);
        });
        row2->addWidget(copyBtn);
        
        infoLayout->addLayout(row2);
        
        // 行3: 错误信息
        QVBoxLayout* errorSection = new QVBoxLayout();
        QLabel* errorTitleLabel = new QLabel("错误信息:");
        errorTitleLabel->setObjectName("labelKey");
        errorTitleLabel->setStyleSheet("QLabel#labelKey { margin-top: 8px; }");
        errorSection->addWidget(errorTitleLabel);
        
        QLabel* errorMessageLabel = new QLabel(result.errorMessage);
        errorMessageLabel->setWordWrap(true);
        errorMessageLabel->setStyleSheet(
            "QLabel { "
            "  color: #c0392b; background-color: #fff5f5; padding: 8px; "
            "  border-radius: 3px; border-left: 3px solid #e74c3c; font-size: 12px; "
            "}"
        );
        errorSection->addWidget(errorMessageLabel);
        infoLayout->addLayout(errorSection);
        
        mainLayout->addWidget(infoGroup, 0, Qt::AlignTop);
        
        // 提示信息
        QString tipIconText;
        tipIconText += FA::QLightbulb;
        tipIconText += " 提示: 请检查文件路径、磁盘空间和文件权限。查看日志了解详细错误信息。";
        QLabel* tipLabel = new QLabel(tipIconText);
        QFont tipFont(FA::solidFontFamily());
        tipFont.setPointSize(9);
        tipLabel->setFont(tipFont);
        tipLabel->setWordWrap(true);
        tipLabel->setStyleSheet("color: #7f8c8d; font-size: 11px; margin-top: 8px; line-height: 1.4;");
        mainLayout->addWidget(tipLabel);
        
        // 空白区域
        mainLayout->addStretch();
        
        // 按钮
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        
        QPushButton* closeButton = new QPushButton("关闭");
        closeButton->setFixedWidth(80);
        closeButton->setFixedHeight(32);
        closeButton->setCursor(Qt::PointingHandCursor);
        connect(closeButton, &QPushButton::clicked, &failDialog, &QDialog::accept);
        buttonLayout->addWidget(closeButton);
        
        mainLayout->addLayout(buttonLayout);
        
        failDialog.exec();
        return;
    }
    
    // 保存成功
    FileTab* tab = getCurrentTab();
    if (tab) {
        // 只有在另存为操作时才更新标签页路径
        if (result.isNewPath && !result.filePath.isEmpty()) {
            tab->filePath = result.filePath;
            tab->fileName = result.fileName;
            tab->isModified = false;

            // 更新标签页标题
            int tabIndex = m_tabWidget->currentIndex();
            if (tabIndex >= 0) {
                m_tabWidget->setTabText(tabIndex, result.fileName);
            }

            // 更新窗口标题
            updateWindowTitle();
        } else if (!result.isNewPath) {
            // 普通保存操作，只更新修改状态
            tab->isModified = false;
        }
    }

    // 只在手动保存时显示保存成功对话框
    if (!result.isAutoSave) {
        // 创建保存成功详情对话框
        showSaveSuccessDialog(result);
        showLogMessage(QString("✓ 保存完成: %1").arg(result.fileName));
    } else {
        // 自动保存完全静默，不显示任何消息
        qDebug() << "自动保存完成:" << result.fileName;
    }

    updateActions();
}

// ========== 自动保存相关槽函数 ==========

void GXTStudio::onAutoSaveToggle()
{
    m_autoSaveEnabled = !m_autoSaveEnabled;

    // 更新按钮图标和提示
    if (m_autoSaveEnabled) {
        m_autoSaveButton->setText(QString(QChar(0xf205)));  // 开启状态图标 U+f205
        m_autoSaveButton->setToolTip("自动保存（开启）");
        // 启用状态使用蓝色 - 紧凑设计
        m_autoSaveButton->setStyleSheet(QString(R"(
            QToolButton {
                border: none;
                background: transparent;
                color: #4a90e2;
                padding: 0;
                border-radius: 3px;
            }
            QToolButton:hover {
                background: rgba(74,144,226,0.1);
                color: #3a80d2;
            }
            QToolButton:pressed {
                background: rgba(74,144,226,0.2);
            }
        )"));
        showLogMessage("自动保存已启用");
    } else {
        m_autoSaveButton->setText(QString(QChar(0xf204)));  // 关闭状态图标 U+f204
        m_autoSaveButton->setToolTip("自动保存（关闭）");
        // 关闭状态使用黑色 - 紧凑设计
        m_autoSaveButton->setStyleSheet(QString(R"(
            QToolButton {
                border: none;
                background: transparent;
                color: #333333;
                padding: 0;
                border-radius: 3px;
            }
            QToolButton:hover {
                background: rgba(0,0,0,0.05);
                color: #1a73e8;
            }
            QToolButton:pressed {
                background: rgba(0,0,0,0.1);
            }
        )"));
        showLogMessage("自动保存已禁用");

        // 停止定时器
        m_autoSaveTimer->stop();
    }

    // 保存设置
    QSettings settings;
    settings.setValue("autoSaveEnabled", m_autoSaveEnabled);
}

void GXTStudio::onAutoSaveTimer()
{
    // 检查自动保存是否启用
    if (!m_autoSaveEnabled) {
        return;
    }

    // 检查是否有已修改的文件
    FileTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->isModified) {
        return;
    }

    // 检查是否有文件路径
    if (currentTab->filePath.isEmpty()) {
        qDebug() << "跳过自动保存：文件尚未保存";
        return;
    }

    // 检查是否已有正在进行的自动保存任务
    if (m_autoSaveFutureWatcher && m_autoSaveFutureWatcher->isRunning()) {
        qDebug() << "自动保存已在进行中，跳过本次";
        return;
    }

    qDebug() << "正在自动保存:" << currentTab->fileName;

    // 显示进度条
    m_saveProgressBar->setFormat("正在自动保存 %p%");
    m_saveProgressBar->show();
    m_saveProgressBar->setValue(10);

    // 捕获必要的信息（只复制简单的元数据，数据本身在后台线程复制）
    QString filePath = currentTab->filePath;
    GXTVersion version = currentTab->version;
    bool isWHM = currentTab->isWHM;
    bool isWHMRSC = currentTab->isWHMRSC;
    bool isDAT = currentTab->isDAT;

    // 【关键】使用 QtConcurrent 在后台线程中执行数据复制和保存
    // 这样主线程完全不参与数据操作，零阻塞
    QFuture<SaveResult> future = QtConcurrent::run([this, filePath, version, isWHM, isDAT, isWHMRSC]() -> SaveResult {
        SaveResult result;
        result.filePath = filePath;
        result.fileName = QFileInfo(filePath).fileName();
        result.isAutoSave = true;
        result.success = false;
        result.bytesWritten = 0;
        
        QElapsedTimer timer;
        timer.start();
        
        // 在后台线程中安全地复制数据（此时主线程可以继续响应用户操作）
        FileTab* tab = nullptr;
        
        // 在主线程中获取数据副本（使用 invokeMethod 确保线程安全）
        QMetaObject::invokeMethod(this, [this, &tab]() {
            tab = getCurrentTab();
        }, Qt::BlockingQueuedConnection);
        
        if (!tab || tab->filePath != filePath) {
            result.errorMessage = "标签页已切换或关闭";
            return result;
        }
        
        // 现在复制数据（在后台线程中进行，不影响主线程响应）
        AutoSaveData saveData;
        saveData.filePath = filePath;
        saveData.version = version;
        saveData.isWHM = isWHM;
        saveData.isWHMRSC = isWHMRSC;
        saveData.isDAT = isDAT;

        if (isWHM) {
            saveData.whmEntries = tab->whmEntries;
        } else if (isDAT) {
            saveData.datEntries = tab->datEntries;
        } else {
            saveData.tables = tab->tables;
        }

        // 执行保存
        bool success = false;
        try {
            GXTEditor editor;
            if (isWHMRSC) {
                // WHM/RSC 格式 - 使用 parser 的保存方法
                GXTParser parser;
                success = parser.saveWHMRSC(saveData.filePath.toStdString(), saveData.whmEntries);
            } else if (isWHM) {
                success = editor.saveAsWHM(saveData.filePath.toStdString(), saveData.whmEntries);
            } else if (isDAT) {
                success = editor.saveAsDAT(saveData.filePath.toStdString(), saveData.datEntries);
            } else {
                // GXT/GXT2 文件 - 使用批量添加提高性能
                editor.setVersion(saveData.version);
                for (const auto& table : saveData.tables) {
                    if (!editor.addTable(table.name)) continue;
                    size_t tableIndex = editor.getTableCount() - 1;
                    // 批量添加条目（跳过重复检查，O(n) 复杂度）
                    std::vector<std::pair<std::string, std::string>> entriesBatch;
                    entriesBatch.reserve(table.entries.size());
                    for (const auto& entry : table.entries) {
                        entriesBatch.emplace_back(entry.key, entry.value);
                    }
                    editor.addEntriesBatch(tableIndex, entriesBatch);
                }
                success = editor.saveToFile(saveData.filePath.toStdString());
            }
        } catch (...) {
            success = false;
        }
        
        result.success = success;
        result.elapsedMs = timer.elapsed();
        
        if (success) {
            QFileInfo fi(filePath);
            result.bytesWritten = fi.size();
        }
        
        return result;
    });
    
    // 设置 FutureWatcher 监听完成信号
    if (!m_autoSaveFutureWatcher) {
        m_autoSaveFutureWatcher = new QFutureWatcher<SaveResult>(this);
        connect(m_autoSaveFutureWatcher, &QFutureWatcher<SaveResult>::finished, this, [this]() {
            SaveResult result = m_autoSaveFutureWatcher->result();
            onAutoSaveCompleted(result);
        });
    }
    
    m_autoSaveFutureWatcher->setFuture(future);
    
    // 更新进度条显示
    m_saveProgressBar->setValue(30);
}

void GXTStudio::onAutoSaveProgress(int percentage, const QString& message)
{
    // 静默更新状态栏进度条
    if (m_saveProgressBar) {
        m_saveProgressBar->setValue(percentage);
        m_saveProgressBar->setFormat("自动保存 %p%");
        if (!m_saveProgressBar->isVisible()) {
            m_saveProgressBar->show();
        }
    }
}

void GXTStudio::onAutoSaveCompleted(const SaveResult& result)
{
    // 隐藏状态栏进度条
    if (m_saveProgressBar) {
        m_saveProgressBar->hide();
        m_saveProgressBar->setValue(0);
    }

    if (result.success) {
        // 保存成功 - 完全静默，不显示任何弹窗
        FileTab* tab = getCurrentTab();
        if (tab && tab->filePath == result.filePath) {
            tab->isModified = false;
        }
        // 只在日志中记录，不显示弹窗
        qDebug() << "自动保存成功:" << result.fileName;
        updateActions();
    } else {
        // 保存失败 - 只在日志中记录，不显示弹窗
        qDebug() << "自动保存失败:" << result.fileName << "-" << result.errorMessage;
    }
}

SaveResult GXTStudio::performSaveTask(const SaveTask& task)
{
    QElapsedTimer timer;
    timer.start();
    
    SaveResult result;
    result.filePath = task.filePath;
    result.isNewPath = task.isNewPath;
    result.fileName = QFileInfo(task.filePath).fileName();
    result.bytesWritten = 0;
    result.success = false;
    
    try {
        if (!task.tabPtr) {
            result.errorMessage = "标签页为空";
            result.elapsedMs = timer.elapsed();
            return result;
        }
        
        FileTab* tab = task.tabPtr;
        
        // 检查文件路径有效性
        if (task.filePath.isEmpty()) {
            result.errorMessage = "文件路径为空";
            result.elapsedMs = timer.elapsed();
            return result;
        }
        
        // 调用保存方法
        bool success = saveFileWithVersion(tab, task.filePath);
        
        if (success) {
            // 如果是新路径，更新标签页信息
            if (task.isNewPath) {
                result.filePath = task.filePath;
                result.fileName = QFileInfo(task.filePath).fileName();
            }
            
            // 计算字节数（通过检查文件大小）
            QFile file(task.filePath);
            if (file.exists()) {
                result.bytesWritten = file.size();
            }
            
            result.success = true;
        } else {
            result.success = false;
            result.errorMessage = "保存失败：版本检测或格式转换错误";
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = QString("保存异常: %1").arg(e.what());
    }
    
    result.elapsedMs = timer.elapsed();
    return result;
}

// 智能翻译功能实现
void GXTStudio::setupTranslationProgress()
{
    // 创建翻译线程
    m_smartTranslatorThread = new QThread(this);
    m_smartTranslatorThread->setObjectName("SmartTranslatorThread");
    
    // 创建智能翻译器
    m_smartTranslator = new SmartTranslator();
    
    // 立即加载配置应用到翻译器
    QSettings settings;
    settings.beginGroup("Translate");
    
    // 加载并应用配置 - 使用32并发优化配置
    QString apiKey = settings.value("apiKey", "").toString();
    QString systemPrompt = settings.value("systemPrompt", "").toString();
    QString batchPrompt = settings.value("batchPrompt", "").toString();
    int batchSize = settings.value("batchSize", SmartTranslator::DEFAULT_BATCH_SIZE).toInt();  // 32行每批次
    int maxConcurrent = settings.value("maxConcurrent", SmartTranslator::DEFAULT_MAX_CONCURRENT_REQUESTS).toInt();  // 32并发
    int maxRetries = settings.value("maxRetries", SmartTranslator::DEFAULT_MAX_RETRIES).toInt();  // 3次重试
    settings.endGroup();
    
    // 应用配置到翻译器
    m_smartTranslator->setApiKey(apiKey);
    m_smartTranslator->setSystemPrompt(systemPrompt);
    m_smartTranslator->setBatchPrompt(batchPrompt);
    m_smartTranslator->setBatchSize(batchSize);
    m_smartTranslator->setMaxConcurrentRequests(maxConcurrent);
    m_smartTranslator->setMaxRetries(maxRetries);
    
    // 移动翻译器到子线程
    m_smartTranslator->moveToThread(m_smartTranslatorThread);
    
    // 连接信号 - 使用 Qt::QueuedConnection 确保在主线程中处理
    connect(m_smartTranslator, &SmartTranslator::translationProgress,
            this, &GXTStudio::onTranslationProgress, Qt::QueuedConnection);
    connect(m_smartTranslator, &SmartTranslator::translationCompleted,
            this, &GXTStudio::onTranslationCompleted, Qt::QueuedConnection);
    connect(m_smartTranslator, &SmartTranslator::translationError,
            this, &GXTStudio::onTranslationError, Qt::QueuedConnection);
    connect(m_smartTranslator, &SmartTranslator::translationCancelled,
            this, &GXTStudio::onTranslationCancelled, Qt::QueuedConnection);
    
    // 设置线程优先级为高优先级，提高翻译响应速度
    m_smartTranslatorThread->start(QThread::HighPriority);
    
    // 优化线程等待时间，增加更多监控信息
    QTimer::singleShot(25, this, [this]() {
        if (m_smartTranslatorThread->isRunning()) {
            showLogMessage("翻译线程已启动 (32并发高性能模式)");
            qWarning() << "翻译线程成功启动，ID:" << m_smartTranslatorThread->currentThreadId() 
                       << "并发数:" << SmartTranslator::DEFAULT_MAX_CONCURRENT_REQUESTS;
        } else {
            showLogMessage("警告：翻译线程启动失败");
            qCritical() << "翻译线程启动失败";
        }
    });
    
    // 添加定期健康检查
    QTimer* healthCheckTimer = new QTimer(this);
    connect(healthCheckTimer, &QTimer::timeout, this, [this]() {
        if (m_smartTranslatorThread && !m_smartTranslatorThread->isRunning()) {
            qWarning() << "检测到翻译线程意外停止";
            showLogMessage("警告：检测到翻译线程意外停止");
        }
    });
    healthCheckTimer->start(5000); // 每5秒检查一次
}

void GXTStudio::onConfigTranslate()
{
    TranslateConfigDialog dialog(this);
    
    // 加载当前配置
    QSettings settings;
    settings.beginGroup("Translate");
    QString apiKey = settings.value("apiKey", "").toString();
    QString systemPrompt = settings.value("systemPrompt", "").toString();
    QString batchPrompt = settings.value("batchPrompt", "").toString();
    
    // 加载性能配置 - 使用配置界面的默认值
    int batchSize = settings.value("batchSize", SmartTranslator::DEFAULT_BATCH_SIZE).toInt();
    int maxConcurrent = settings.value("maxConcurrent", SmartTranslator::DEFAULT_MAX_CONCURRENT_REQUESTS).toInt();
    int maxRetries = settings.value("maxRetries", SmartTranslator::DEFAULT_MAX_RETRIES).toInt();
    settings.endGroup();
    
    dialog.setApiKey(apiKey);
    dialog.setSystemPrompt(systemPrompt);
    dialog.setBatchPrompt(batchPrompt);
    
    // 设置性能配置
    dialog.setBatchSize(batchSize);
    dialog.setMaxConcurrentRequests(maxConcurrent);
    dialog.setMaxRetries(maxRetries);
    
    if (dialog.exec() == QDialog::Accepted) {
        // 保存配置
        QString newApiKey = dialog.getApiKey();
        
        // 以 queued 方式调用翻译器的设置方法，确保在翻译线程执行
        QMetaObject::invokeMethod(m_smartTranslator, "setApiKey", Qt::QueuedConnection,
                                  Q_ARG(QString, newApiKey));
        QMetaObject::invokeMethod(m_smartTranslator, "setSystemPrompt", Qt::QueuedConnection,
                                  Q_ARG(QString, dialog.getSystemPrompt()));
        QMetaObject::invokeMethod(m_smartTranslator, "setBatchPrompt", Qt::QueuedConnection,
                                  Q_ARG(QString, dialog.getBatchPrompt()));
        
        // 应用性能配置
        QMetaObject::invokeMethod(m_smartTranslator, "setBatchSize", Qt::QueuedConnection,
                                  Q_ARG(int, dialog.getBatchSize()));
        QMetaObject::invokeMethod(m_smartTranslator, "setMaxConcurrentRequests", Qt::QueuedConnection,
                                  Q_ARG(int, dialog.getMaxConcurrentRequests()));
        QMetaObject::invokeMethod(m_smartTranslator, "setMaxRetries", Qt::QueuedConnection,
                                  Q_ARG(int, dialog.getMaxRetries()));
        
        // 更新按钮状态
        if (!newApiKey.trimmed().isEmpty()) {
            m_executeTranslateAction->setEnabled(true);
            m_executeTranslateAction->setText("执行翻译(&T)");
            
            // 重新连接按钮点击事件为执行翻译
            if (m_smartTranslateButton) {
                m_smartTranslateButton->disconnect();
                connect(m_smartTranslateButton, &QToolButton::clicked, this, &GXTStudio::onExecuteTranslate);
            }
            
            showLogMessage("翻译API配置成功");
        } else {
            m_executeTranslateAction->setEnabled(false);
            m_executeTranslateAction->setText("执行翻译(&T) - 需要配置密钥");
            
            // 重新连接按钮点击事件为配置密钥
            if (m_smartTranslateButton) {
                m_smartTranslateButton->disconnect();
                connect(m_smartTranslateButton, &QToolButton::clicked, this, &GXTStudio::onConfigTranslate);
            }
        }
    }
}

void GXTStudio::onExecuteTranslate()
{
    // 以同步方式检查是否就绪（简短的检查）
    if (!m_smartTranslator || !m_smartTranslator->isReady()) {
        QMessageBox::warning(this, "警告", "翻译功能未配置，请先配置API密钥。");
        onConfigTranslate();
        return;
    }
    
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) {
        QMessageBox::information(this, "提示", "请先打开一个GXT文件。");
        return;
    }
    
    if (m_isReadOnly) {
        QMessageBox::warning(this, "警告", "当前为只读模式，无法执行翻译。请先切换到编辑模式。");
        return;
    }
    
    // 获取所有待翻译的文本 - 翻译所有表
    // 使用更安全的任务列表创建方式，避免QList溢出
    QList<TranslateTask> tasks;
    
    // 检查任务数量是否会导致内存问题
    const int MAX_SAFE_TASKS = SmartTranslator::MAX_SAFE_TASKS;
    const int MAX_QLIST_ITEMS = SmartTranslator::MAX_QLIST_SAFE_ITEMS;
    const int MAX_MEMORY_USAGE_MB = 256; // 与SmartTranslator保持一致
    
    // 计算预估任务数量
    size_t estimatedTasks = 0;
    if (currentTab->isWHM) {
        estimatedTasks = currentTab->whmEntries.size();
    } else {
        for (const auto& table : currentTab->tables) {
            estimatedTasks += table.entries.size();
        }
    }
    
    // 严格的安全检查
    if (estimatedTasks > MAX_QLIST_ITEMS) {
        QMessageBox::critical(this, "任务过多",
            QString("检测到 %1 个翻译任务，超出QList容器最大容量限制。\n"
                   "这会导致程序崩溃，请使用更小的文件或分批处理。\n"
                   "QList安全限制: %2 个任务").arg(estimatedTasks).arg(MAX_QLIST_ITEMS));
        return;
    }
    
    if (estimatedTasks > MAX_SAFE_TASKS) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("大批量任务警告");
        msgBox.setText(QString("检测到 %1 个翻译任务，可能影响性能。\n"
                              "建议分批处理或使用更小的文件。\n"
                              "推荐最大限制: %2 个任务").arg(estimatedTasks).arg(MAX_SAFE_TASKS));
        msgBox.setInformativeText("是否继续翻译？");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        
        if (msgBox.exec() != QMessageBox::Yes) {
            return;
        }
    }
    
    // 额外检查：预估内存使用量
    const size_t ESTIMATED_TASK_SIZE = 64; // 字节
    size_t estimatedMemory = estimatedTasks * ESTIMATED_TASK_SIZE * 2; // 2倍安全系数
    
    if (estimatedMemory > static_cast<size_t>(MAX_MEMORY_USAGE_MB) * 1024 * 1024) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("内存使用警告");
        msgBox.setText(QString("预估内存使用: %1 MB\n推荐限制: %2 MB\n\n是否继续？")
                      .arg(estimatedMemory / (1024 * 1024))
                      .arg(MAX_MEMORY_USAGE_MB));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        
        if (msgBox.exec() != QMessageBox::Yes) {
            return;
        }
    }
    
    // 使用安全的方式构建任务列表
    try {
        if (currentTab->isWHM) {
            // WHM文件翻译 - 使用更安全的循环和检查
            for (int i = 0; i < static_cast<int>(currentTab->whmEntries.size()); ++i) {
                // 安全检查：防止越界访问
                if (i >= static_cast<int>(currentTab->whmEntries.size())) {
                    break;
                }
                
                // 额外检查：防止无限循环
                if (i > MAX_QLIST_ITEMS) {
                    qWarning() << "WHM翻译：达到最大安全限制";
                    break;
                }
                
                const auto& entry = currentTab->whmEntries[i];
                QString value = QString::fromStdString(entry.text);
                QString key = "0x" + QString::number(entry.hash, 16).toUpper().rightJustified(8, '0');
                
                // 跳过空值和过长数据
                if (!value.trimmed().isEmpty() && value.length() <= 10000 && key.length() <= 1000) {
                    // 额外检查：确保value不是系统消息
                    if (value.contains("MiMo") || value.contains("assistant") || value.contains("Xiaomi")) {
                        qWarning() << "跳过系统消息类型的文本:" << value.left(50);
                        continue;
                    }
                    // 检查tasks大小，防止溢出
                    if (tasks.size() >= MAX_SAFE_TASKS) {
                        QMessageBox::warning(this, "任务限制",
                            QString("已达到最大任务数量限制: %1").arg(MAX_SAFE_TASKS));
                        break;
                    }
                    
                    // 每处理1000个任务检查一次内存状态
                    if (tasks.size() > 0 && tasks.size() % 1000 == 0) {
                        QCoreApplication::processEvents();
                    }
                    
                    // 使用try-catch防止内存分配失败
                    try {
                        tasks.append(TranslateTask(key, value, i));
                    } catch (...) {
                        QMessageBox::critical(this, "内存错误", "内存不足，无法添加更多任务");
                        return;
                    }
                }
            }
        } else {
            // GXT文件翻译 - 翻译所有表的所有条目
            for (size_t tableIdx = 0; tableIdx < currentTab->tables.size(); ++tableIdx) {
                // 安全检查：防止越界
                if (tableIdx >= currentTab->tables.size()) {
                    break;
                }
                
                // 额外检查：防止无限循环
                if (tableIdx > 1000) {
                    qWarning() << "GXT翻译：表索引过大";
                    break;
                }
                
                const auto& table = currentTab->tables[tableIdx];
                QString tableName = QString::fromStdString(table.name);
                
                // 检查表名长度
                if (tableName.length() > 100) {
                    qWarning() << "表名过长，跳过:" << tableName;
                    continue;
                }
                
                for (int i = 0; i < static_cast<int>(table.entries.size()); ++i) {
                    // 安全检查：防止越界
                    if (i >= static_cast<int>(table.entries.size())) {
                        break;
                    }
                    
                    // 额外检查：防止无限循环
                    if (i > MAX_QLIST_ITEMS) {
                        qWarning() << "GXT翻译：条目索引过大";
                        break;
                    }
                    
                    const auto& entry = table.entries[i];
                    QString value = QString::fromStdString(entry.value);
                    QString key = QString::fromStdString(entry.key);
                    
                    // 跳过空值和过长数据
                    if (!value.trimmed().isEmpty() && value.length() <= 10000 && key.length() <= 1000) {
                        // 额外检查：确保value不是系统消息
                        if (value.contains("MiMo") || value.contains("assistant") || value.contains("Xiaomi")) {
                            qWarning() << "跳过系统消息类型的文本:" << value.left(50);
                            continue;
                        }
                        // 检查tasks大小，防止溢出
                        if (tasks.size() >= MAX_SAFE_TASKS) {
                            QMessageBox::warning(this, "任务限制",
                                QString("已达到最大任务数量限制: %1").arg(MAX_SAFE_TASKS));
                            break;
                        }
                        
                        // 每处理1000个任务检查一次内存状态
                        if (tasks.size() > 0 && tasks.size() % 1000 == 0) {
                            QCoreApplication::processEvents();
                        }
                        
                        // 直接使用key，因为每个表内的key是唯一的
                        // 我们会在翻译结果应用时通过row来找到正确的条目
                        
                        // 使用try-catch防止内存分配失败
                        try {
                            tasks.append(TranslateTask(key, value, tasks.size()));
                        } catch (...) {
                            QMessageBox::critical(this, "内存错误", "内存不足，无法添加更多任务");
                            return;
                        }
                    }
                }
                
                // 如果达到限制，跳出外层循环
                if (tasks.size() >= MAX_SAFE_TASKS) {
                    QMessageBox::information(this, "任务截断",
                        QString("由于达到任务数量限制(%1)，部分内容未被添加到翻译队列。").arg(MAX_SAFE_TASKS));
                    break;
                }
            }
        }
    } catch (...) {
        QMessageBox::critical(this, "处理错误", "处理翻译任务时发生未知错误");
        return;
    }
    
    // 最终检查：确保任务列表本身是安全的
    if (tasks.size() > MAX_QLIST_ITEMS) {
        QMessageBox::critical(this, "内存安全错误",
            QString("任务列表大小 %1 超过QList安全限制 %2，无法安全传输到翻译线程")
            .arg(tasks.size()).arg(MAX_QLIST_ITEMS));
        return;
    }
    
    if (tasks.isEmpty()) {
        QMessageBox::information(this, "提示", "当前表格中没有需要翻译的内容。");
        return;
    }
    
    // 性能检查：根据任务数量提供用户选择
    if (tasks.size() > 5000) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("大批量翻译确认");
        msgBox.setText(QString("检测到大量文本需要翻译（共 %1 条）。\n\n建议您："
                             "\n• 使用极速模式进行批量翻译"
                             "\n• 确保网络连接稳定"
                             "\n• 避免在翻译过程中进行其他操作").arg(tasks.size()));
        msgBox.setInformativeText("是否继续翻译？");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        
        if (msgBox.exec() != QMessageBox::Yes) {
            return;
        }
    }
    
    // 创建多线程进度对话框，优化性能
    if (m_translateProgressDialog) {
        m_translateProgressDialog->close();
        m_translateProgressDialog->deleteLater();
        m_translateProgressDialog = nullptr;
    }
    
    m_translateProgressDialog = new MultiThreadProgressDialog(this);
    m_translateProgressDialog->setWindowModality(Qt::WindowModal);

    // 连接对话框的取消信号到GXTStudio的处理函数（带选项参数）
    connect(m_translateProgressDialog, QOverload<MultiThreadProgressDialog::CancelOption>::of(&MultiThreadProgressDialog::cancelTranslationRequested),
            this, &GXTStudio::onCancelTranslationRequested, Qt::QueuedConnection);

    // 设置总体进度和开始时间
    m_translateProgressDialog->setTotalThreads(SmartTranslator::DEFAULT_MAX_CONCURRENT_REQUESTS);
    m_translateProgressDialog->updateOverallProgress(0, tasks.size());
    m_translateProgressDialog->setStartTime(QDateTime::currentMSecsSinceEpoch());

    // 优化大量任务时的显示性能
    if (tasks.size() > 1000) {
        m_translateProgressDialog->setWindowTitle("大批量翻译中...");
    }

    m_translateProgressDialog->show();
    QApplication::processEvents(); // 确保对话框立即显示
    
    // 以 queued 方式调用翻译，在翻译线程执行，避免阻塞主线程
    try {
        // 最后的安全检查：确保任务列表本身是安全的
        if (tasks.size() > SmartTranslator::MAX_QLIST_SAFE_ITEMS) {
            QMessageBox::critical(this, "内存安全错误",
                QString("任务列表大小 %1 超过QList安全限制 %2，无法安全传输到翻译线程")
                .arg(tasks.size()).arg(SmartTranslator::MAX_QLIST_SAFE_ITEMS));
            return;
        }
        
        // 检查内存使用预估
        size_t estimatedMemory = tasks.size() * sizeof(TranslateTask) * 2; // 2倍安全系数
        if (estimatedMemory > 50 * 1024 * 1024) { // 50MB限制
            QMessageBox::warning(this, "内存使用警告",
                QString("预估内存使用 %1 MB 可能过高，是否继续？")
                .arg(estimatedMemory / (1024 * 1024)));
        }
        
        QMetaObject::invokeMethod(m_smartTranslator, "translateTexts", Qt::QueuedConnection,
                                  Q_ARG(QList<TranslateTask>, tasks));
        
        showLogMessage(QString("开始翻译 %1 条文本...").arg(tasks.size()));
    } catch (...) {
        QMessageBox::critical(this, "系统错误", "启动翻译时发生异常，可能是内存不足");
        showLogMessage("启动翻译失败：发生异常");
    }
}

void GXTStudio::onTranslationProgress(int completed, int total, const QString& message)
{
    // 使用临时指针避免在检查和使用之间被其他线程修改
    MultiThreadProgressDialog* progressDialog = m_translateProgressDialog;

    if (progressDialog) {
        // 解析请求ID和消息
        int requestId = 0; // 默认请求ID
        QString cleanMessage = message;

        // 从消息中提取更多信息来生成唯一的请求ID
        static int requestCounter = 0;
        requestId = (++requestCounter % 50); // 限制在0-49范围内，对应最大50个并发请求

        // 尝试从消息中提取更具体的标识信息
        if (message.contains("批次")) {
            QRegularExpression re("批次(\\d+)");
            QRegularExpressionMatch match = re.match(message);
            if (match.hasMatch()) {
                // 使用批次ID的一部分来创建更稳定的请求ID
                int batchId = match.captured(1).toInt();
                requestId = (batchId - 1) % 50; // 批次从1开始，转换为0-49
            }
        } else if (message.contains("活动请求:")) {
            QRegularExpression re("活动请求:(\\d+)");
            QRegularExpressionMatch match = re.match(message);
            if (match.hasMatch()) {
                // 如果能提取到活动请求数，使用它的一部分
                int activeRequests = match.captured(1).toInt();
                if (activeRequests > 0 && activeRequests <= 50) {
                    requestId = activeRequests - 1;
                }
            }
        }

        // 清理消息，移除过长的技术细节
        if (cleanMessage.length() > 200) {
            cleanMessage = cleanMessage.left(200) + "...";
        }

        try {
            // 更新请求进度，内部会处理总体进度
            progressDialog->updateThreadProgress(requestId, completed, total, cleanMessage);

            // 解析成功/失败数量更新统计
            QRegularExpression successRe("成功(\\d+)条");
            QRegularExpression failedRe("失败(\\d+)条");

            QRegularExpressionMatch successMatch = successRe.match(message);
            QRegularExpressionMatch failedMatch = failedRe.match(message);

            if (successMatch.hasMatch() || failedMatch.hasMatch()) {
                int successCount = successMatch.hasMatch() ? successMatch.captured(1).toInt() : 0;
                int failedCount = failedMatch.hasMatch() ? failedMatch.captured(1).toInt() : 0;

                // 更新对话框的统计信息（通过公有接口）
                progressDialog->updateSuccessCount(successCount);
                progressDialog->updateFailedCount(failedCount);
            }

        } catch (...) {
            qWarning() << "更新多线程进度对话框时发生异常";
        }

        // 确保对话框可见
        if (!progressDialog->isVisible()) {
            try {
                progressDialog->show();
                // 将对话框移到屏幕中央并保持在前面
                progressDialog->raise();
                progressDialog->activateWindow();
                QApplication::processEvents();
            } catch (...) {
                qWarning() << "显示进度对话框时发生异常";
            }
        }
    }

    // 进一步降低状态栏更新频率
    static int statusBarInterval = 10;
    if (total > 1000) {
        statusBarInterval = 100;
    } else if (total > 500) {
        statusBarInterval = 50;
    } else {
        statusBarInterval = 10;
    }

    if (completed % statusBarInterval == 0 || completed == total) {
        try {
            // 构建更详细的状态栏信息
            if (total > 0) {
                double progress = (double)completed / total * 100;
                QString statusMsg = QString("翻译中: %1/%2 (%3%) - %4")
                    .arg(completed)
                    .arg(total)
                    .arg(progress, 0, 'f', 1)
                    .arg(message);
                m_statusLabel->setText(statusMsg);
            }
            updateStatusBar();
        } catch (...) {
            qWarning() << "更新状态栏时发生异常";
        }
    }
}

QString GXTStudio::calculateRemainingTime(int completed, int total)
{
    // 简单的剩余时间估算
    if (completed <= 0 || total <= 0) return "计算中...";
    
    // 假设每个任务平均需要2秒（根据API响应时间）
    const double AVG_TIME_PER_TASK = 2.0; // 秒
    int remainingTasks = total - completed;
    double remainingSeconds = remainingTasks * AVG_TIME_PER_TASK;
    
    if (remainingSeconds < 60) {
        return QString("%1秒").arg(qRound(remainingSeconds));
    } else if (remainingSeconds < 3600) {
        return QString("%1分钟").arg(qRound(remainingSeconds / 60));
    } else {
        int hours = qRound(remainingSeconds / 3600);
        int minutes = qRound((remainingSeconds - hours * 3600) / 60);
        return QString("%1小时%2分钟").arg(hours).arg(minutes);
    }
}

QPair<int, int> GXTStudio::applyTranslationResultsToFile(const QList<TranslateResult>& results)
{
    FileTab* currentTab = getCurrentTab();
    if (!currentTab) {
        return qMakePair(0, 0);
    }

    int successCount = 0;
    int failureCount = 0;
    int totalResults = results.size();

    try {
        // 应用翻译结果 - 按照原始row映射
        for (const auto& result : results) {
            // 防止处理过多结果
            if (successCount + failureCount >= SmartTranslator::MAX_SAFE_TASKS) {
                showLogMessage("达到处理结果数量限制，停止处理更多结果");
                break;
            }

            // 额外检查：防止无限循环
            if (successCount + failureCount > SmartTranslator::MAX_QLIST_SAFE_ITEMS) {
                showLogMessage("达到QList安全限制，停止处理更多结果");
                break;
            }

            if (result.success) {
                if (currentTab->isWHM) {
                    // WHM文件：直接按row索引，添加边界检查
                    if (result.row >= 0 && result.row < static_cast<int>(currentTab->whmEntries.size())) {
                        try {
                            // 安全地更新WHM条目
                            std::string translatedText = result.translatedValue.toStdString();

                            // 验证翻译结果长度和内容
                            if (translatedText.length() > 10000) { // 限制单个条目长度
                                qWarning() << "WHM翻译结果过长，已截断:" << translatedText.length();
                                translatedText = translatedText.substr(0, 10000);
                            }

                            // 检查是否包含恶意内容（可选）
                            if (translatedText.find('\0') != std::string::npos) {
                                qWarning() << "WHM翻译结果包含空字符，跳过";
                                failureCount++;
                                continue;
                            }

                            currentTab->whmEntries[result.row].text = translatedText;
                            successCount++;
                        } catch (...) {
                            failureCount++;
                            qWarning() << "应用WHM翻译结果时发生异常，row:" << result.row;
                        }
                    } else {
                        failureCount++;
                        qWarning() << "WHM翻译结果row越界:" << result.row << "大小:" << currentTab->whmEntries.size();
                    }
                } else {
                    // GXT文件：使用key和row字段找到对应的条目
                    bool found = false;
                    QString taskKey = result.key; // 直接使用任务的key来查找

                    qWarning() << "查找GXT条目，任务key:" << taskKey.left(50) << "row:" << result.row;

                    // 安全地检查key
                    if (taskKey.isEmpty()) {
                        qWarning() << "任务key为空";
                        failureCount++;
                        continue;
                    }

                    // 防止key过长
                    if (taskKey.length() > 1000) {
                        qWarning() << "任务key过长，跳过处理";
                        failureCount++;
                        continue;
                    }

                    // 防止key包含非法字符
                    if (taskKey.contains('\0')) {
                        qWarning() << "任务key包含非法字符，跳过处理";
                        failureCount++;
                        continue;
                    }

                    // 在所有表中搜索匹配的key
                    for (auto& table : currentTab->tables) {
                        for (auto& entry : table.entries) {
                            if (QString::fromStdString(entry.key) == taskKey) {
                                try {
                                    // 安全地更新GXT条目
                                    std::string translatedText = result.translatedValue.toStdString();

                                    // 验证翻译结果长度和内容
                                    if (translatedText.length() > 10000) { // 限制单个条目长度
                                        qWarning() << "GXT翻译结果过长，已截断:" << translatedText.length();
                                        translatedText = translatedText.substr(0, 10000);
                                    }

                                    // 检查是否包含恶意内容
                                    if (translatedText.find('\0') != std::string::npos) {
                                        qWarning() << "GXT翻译结果包含空字符，跳过";
                                        failureCount++;
                                        continue;
                                    }

                                    entry.value = translatedText;
                                    successCount++;
                                    found = true;

                                    qWarning() << "成功更新GXT条目，表:" << QString::fromStdString(table.name)
                                              << "键:" << taskKey
                                              << "原文:" << result.originalValue.left(30)
                                              << "译文:" << result.translatedValue.left(30);
                                    break;
                                } catch (...) {
                                    qWarning() << "应用GXT翻译结果时发生异常，键:" << taskKey;
                                }
                            }
                        }
                        if (found) break;
                    }

                    if (!found) {
                        failureCount++;
                        qWarning() << "未找到匹配的GXT条目，任务key:" << taskKey;
                    }
                }
            } else {
                failureCount++;
            }
        }
    } catch (...) {
        qWarning() << "处理翻译结果时发生异常";
    }

    // 标记文件已修改
    if (successCount > 0) {
        currentTab->isModified = true;
        try {
            updateEntryTable();
            updateWindowTitle();
        } catch (...) {
            qWarning() << "更新UI时发生异常";
        }
    }

    return qMakePair(successCount, failureCount);
}

void GXTStudio::onTranslationCompleted(const QList<TranslateResult>& results)
{
    qWarning() << "GXTStudio::onTranslationCompleted() 被调用，results.size() =" << results.size();

    if (m_translateProgressDialog) {
        qWarning() << "调用对话框的 translationCompleted() 方法";
        m_translateProgressDialog->translationCompleted();

        // 显示完成状态2秒后自动关闭
        QTimer::singleShot(2000, this, [this]() {
            if (m_translateProgressDialog) {
                qWarning() << "2秒后自动关闭翻译进度对话框";
                m_translateProgressDialog->close();
                m_translateProgressDialog->deleteLater();
                m_translateProgressDialog = nullptr;
            }
        });
    }

    // 应用翻译结果
    QPair<int, int> stats = applyTranslationResultsToFile(results);
    int successCount = stats.first;
    int failureCount = stats.second;
    int totalOriginalTasks = results.size();

    // 额外检查：如果失败率过高，给出警告
    double failureRate = (totalOriginalTasks > 0) ?
                        (double)failureCount / totalOriginalTasks * 100.0 : 0.0;

    if (failureRate > 50.0 && totalOriginalTasks > 10) {
        showLogMessage(QString("警告：翻译失败率过高 (%1%)，建议检查网络连接或API配置")
                      .arg(QString::number(failureRate, 'f', 1)));
    }

    // 显示现代化的完成消息 - 基于原始任务总数计算成功率
    double successRate = (totalOriginalTasks > 0) ?
                        (double)successCount / totalOriginalTasks * 100.0 : 0.0;

    // 详细日志：显示实际的翻译统计
    showLogMessage(QString("翻译结果统计 - 原始任务: %1, 成功: %2, 失败: %3, 成功率: %4%")
                  .arg(totalOriginalTasks)
                  .arg(successCount)
                  .arg(failureCount)
                  .arg(QString::number(successRate, 'f', 1)));

    // 创建自定义对话框
    QDialog resultDialog(this);
    resultDialog.setWindowTitle("翻译结果");
    resultDialog.setFixedSize(400, 300);
    resultDialog.setWindowFlags(resultDialog.windowFlags() | Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    QVBoxLayout* layout = new QVBoxLayout(&resultDialog);
    layout->setSpacing(15);
    layout->setContentsMargins(25, 25, 25, 25);

    // 状态图标和标题
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* iconLabel = new QLabel();

    // 获取 FontAwesome 字体族
    QString iconFontFamily = FA::solidFontFamily();

    if (successRate >= 95.0) {
        iconLabel->setText(FA::QCheck);
        if (!iconFontFamily.isEmpty()) {
            iconLabel->setStyleSheet(QString("font-family: '%1'; font-size: 24px; color: #28a745;").arg(iconFontFamily));
        } else {
            iconLabel->setStyleSheet("font-size: 24px; color: #28a745;");
            iconLabel->setFont(FA::solidFont(24));
        }
    } else if (successRate >= 80.0) {
        iconLabel->setText(FA::QExclamationTriangle);
        if (!iconFontFamily.isEmpty()) {
            iconLabel->setStyleSheet(QString("font-family: '%1'; font-size: 24px; color: #ffc107;").arg(iconFontFamily));
        } else {
            iconLabel->setStyleSheet("font-size: 24px; color: #ffc107;");
            iconLabel->setFont(FA::solidFont(24));
        }
    } else {
        iconLabel->setText(FA::QTimes);
        if (!iconFontFamily.isEmpty()) {
            iconLabel->setStyleSheet(QString("font-family: '%1'; font-size: 24px; color: #dc3545;").arg(iconFontFamily));
        } else {
            iconLabel->setStyleSheet("font-size: 24px; color: #dc3545;");
            iconLabel->setFont(FA::solidFont(24));
        }
    }

    QLabel* titleLabel = new QLabel("翻译完成");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50;");

    titleLayout->addWidget(iconLabel);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    // 统计信息 - 使用富文本格式，使图标部分使用FontAwesome字体
    QLabel* statsLabel = new QLabel();
    statsLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #f8f9fa;"
        "  border: 1px solid #e9ecef;"
        "  border-radius: 8px;"
        "  padding: 15px;"
        "  font-size: 14px;"
        "  color: #495057;"
        "}"
    );

    // 获取 FontAwesome 字体族
    QString statsFontFamily = FA::solidFontFamily();
    QString statsText;
    if (statsFontFamily.isEmpty()) {
        // 如果字体加载失败，使用纯文本
        statsText = QString(
            "<b>翻译统计：</b><br><br>"
            "%1 原始条目：<b>%2</b><br>"
            "%3 成功翻译：<b style='color: #28a745;'>%4</b><br>"
            "%5 翻译失败：<b style='color: #dc3545;'>%6</b><br>"
            "%7 成功率：<b style='color: %8;'>%9%</b>"
        ).arg(QString(FA::QChartBar))
         .arg(totalOriginalTasks)
         .arg(QString(FA::QCheck))
         .arg(successCount)
         .arg(QString(FA::QTimes))
         .arg(failureCount)
         .arg(QString(FA::QLineChart))
         .arg(successRate >= 90.0 ? "#28a745" : successRate >= 70.0 ? "#ffc107" : "#dc3545")
         .arg(QString::number(successRate, 'f', 1));
    } else {
        // 如果字体加载成功，使用富文本格式
        statsText = QString(
            "<b>翻译统计：</b><br><br>"
            "<span style=\"font-family:'%10';\">%1</span> 原始条目：<b>%2</b><br>"
            "<span style=\"font-family:'%10';\">%3</span> 成功翻译：<b style='color: #28a745;'>%4</b><br>"
            "<span style=\"font-family:'%10';\">%5</span> 翻译失败：<b style='color: #dc3545;'>%6</b><br>"
            "<span style=\"font-family:'%10';\">%7</span> 成功率：<b style='color: %8;'>%9%</b>"
        ).arg(QString(FA::QChartBar))
         .arg(totalOriginalTasks)
         .arg(QString(FA::QCheck))
         .arg(successCount)
         .arg(QString(FA::QTimes))
         .arg(failureCount)
         .arg(QString(FA::QLineChart))
         .arg(successRate >= 90.0 ? "#28a745" : successRate >= 70.0 ? "#ffc107" : "#dc3545")
         .arg(QString::number(successRate, 'f', 1))
         .arg(statsFontFamily);
    }

    statsLabel->setText(statsText);
    statsLabel->setTextFormat(Qt::RichText);
    statsLabel->setAlignment(Qt::AlignCenter);

    // 建议信息 - 使用富文本格式，使图标部分使用FontAwesome字体
    QLabel* suggestionLabel = new QLabel();

    // 获取 FontAwesome 字体族
    QString suggestionFontFamily = FA::solidFontFamily();
    QString suggestionText;
    if (suggestionFontFamily.isEmpty()) {
        // 如果字体加载失败，使用纯文本
        if (successRate >= 95.0) {
            suggestionText = QString("%1 翻译质量优秀！所有条目都已成功翻译。").arg(QString(FA::QGift));
        } else if (successRate >= 80.0) {
            suggestionText = QString("%1 翻译基本完成，少量失败条目可以手动编辑或稍后重试。").arg(QString(FA::QEdit));
        } else if (failureCount > 0) {
            suggestionText = QString("%1 建议检查网络连接、API密钥配置，或调整翻译设置后重试。").arg(QString(FA::QExclamationTriangle));
        }
    } else {
        // 如果字体加载成功，使用富文本格式
        if (successRate >= 95.0) {
            suggestionText = QString("<span style=\"font-family:'%1'; font-size:13px;\">%2</span> 翻译质量优秀！所有条目都已成功翻译。").arg(suggestionFontFamily).arg(QString(FA::QGift));
        } else if (successRate >= 80.0) {
            suggestionText = QString("<span style=\"font-family:'%1'; font-size:13px;\">%2</span> 翻译基本完成，少量失败条目可以手动编辑或稍后重试。").arg(suggestionFontFamily).arg(QString(FA::QEdit));
        } else if (failureCount > 0) {
            suggestionText = QString("<span style=\"font-family:'%1'; font-size:13px;\">%2</span> 建议检查网络连接、API密钥配置，或调整翻译设置后重试。").arg(suggestionFontFamily).arg(QString(FA::QExclamationTriangle));
        }
    }

    suggestionLabel->setText(suggestionText);
    suggestionLabel->setTextFormat(Qt::RichText);
    suggestionLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #e3f2fd;"
        "  border-left: 4px solid #2196f3;"
        "  padding: 10px;"
        "  font-size: 13px;"
        "  color: #1565c0;"
        "}"
    );
    suggestionLabel->setWordWrap(true);

    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* okButton = new QPushButton("确定");
    okButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #007bff;"
        "  color: white;"
        "  border: none;"
        "  padding: 8px 20px;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0056b3;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #004085;"
        "}"
    );

    if (failureCount > 0 && successRate < 80.0) {
        QPushButton* retryButton = new QPushButton("重试失败项");
        retryButton->setStyleSheet(
            "QPushButton {"
            "  background-color: #6c757d;"
            "  color: white;"
            "  border: none;"
            "  padding: 8px 16px;"
            "  border-radius: 6px;"
            "  font-size: 14px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #545b62;"
            "}"
        );
        buttonLayout->addWidget(retryButton);
        connect(retryButton, &QPushButton::clicked, &resultDialog, [&resultDialog]() {
            resultDialog.reject();
        });
    }

    buttonLayout->addWidget(okButton);
    connect(okButton, &QPushButton::clicked, &resultDialog, &QDialog::accept);

    // 布局组装
    layout->addLayout(titleLayout);
    layout->addWidget(statsLabel);
    layout->addWidget(suggestionLabel);
    layout->addStretch();
    layout->addLayout(buttonLayout);

    // 显示对话框
    resultDialog.exec();

    showLogMessage(QString("翻译完成 - 成功: %1, 失败: %2, 成功率: %3%")
                  .arg(successCount)
                  .arg(failureCount)
                  .arg(QString::number(successRate, 'f', 1)));
}

void GXTStudio::onTranslationError(const QString& error)
{
    if (m_translateProgressDialog) {
        try {
            m_translateProgressDialog->close();
            m_translateProgressDialog->deleteLater();
            m_translateProgressDialog = nullptr;
        } catch (...) {
            qWarning() << "关闭进度对话框时发生异常";
        }
    }
    
    try {
        QMessageBox::critical(this, "翻译错误", QString("翻译过程中发生错误：\n%1").arg(error));
        showLogMessage("翻译错误: " + error);
    } catch (...) {
        qWarning() << "显示错误对话框时发生异常";
    }
}

void GXTStudio::applyPartialTranslationResults(const QList<TranslateResult>& results)
{
    qWarning() << "应用部分翻译结果，results.size() =" << results.size();

    // 应用翻译结果
    QPair<int, int> stats = applyTranslationResultsToFile(results);
    int successCount = stats.first;
    int failureCount = stats.second;

    // 显示结果对话框
    QDialog resultDialog(this);
    resultDialog.setWindowTitle("部分翻译结果");
    resultDialog.setFixedSize(400, 280);
    resultDialog.setWindowFlags(resultDialog.windowFlags() | Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    QVBoxLayout* layout = new QVBoxLayout(&resultDialog);
    layout->setSpacing(15);
    layout->setContentsMargins(25, 25, 25, 25);

    // 状态图标和标题
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* iconLabel = new QLabel(QString(FA::QSave));

    // 设置样式表，显式指定 FontAwesome 字体族
    QString iconFontFamily = FA::solidFontFamily();
    if (!iconFontFamily.isEmpty()) {
        iconLabel->setStyleSheet(QString("font-family: '%1'; font-size: 32px; color: #17a2b8;").arg(iconFontFamily));
    } else {
        iconLabel->setStyleSheet("font-size: 32px; color: #17a2b8;");
        iconLabel->setFont(FA::solidFont(32));
    }

    QLabel* titleLabel = new QLabel("已保存部分翻译");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50;");

    titleLayout->addWidget(iconLabel);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    // 统计信息 - 使用富文本格式，使图标部分使用FontAwesome字体
    QLabel* statsLabel = new QLabel();
    statsLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #f8f9fa;"
        "  border: 1px solid #e9ecef;"
        "  border-radius: 8px;"
        "  padding: 15px;"
        "  font-size: 14px;"
        "  color: #495057;"
        "}"
    );

    // 获取 FontAwesome 字体族
    QString statsFontFamily = FA::solidFontFamily();
    QString statsText;
    if (statsFontFamily.isEmpty()) {
        // 如果字体加载失败，使用纯文本
        statsText = QString(
            "<b>翻译统计：</b><br><br>"
            "%1 成功保存：<b style='color: #28a745;'>%2</b><br>"
            "%3 失败条目：<b style='color: #dc3545;'>%4</b><br>"
            "%5 保存率：<b style='color: #17a2b8;'>%6%</b>"
        ).arg(QString(FA::QCheck))
         .arg(successCount)
         .arg(QString(FA::QTimes))
         .arg(failureCount)
         .arg(QString(FA::QLineChart))
         .arg(results.size() > 0 ? QString::number((double)successCount / results.size() * 100.0, 'f', 1) : "0.0");
    } else {
        // 如果字体加载成功，使用富文本格式
        statsText = QString(
            "<b>翻译统计：</b><br><br>"
            "<span style=\"font-family:'%7';\">%1</span> 成功保存：<b style='color: #28a745;'>%2</b><br>"
            "<span style=\"font-family:'%7';\">%3</span> 失败条目：<b style='color: #dc3545;'>%4</b><br>"
            "<span style=\"font-family:'%7';\">%5</span> 保存率：<b style='color: #17a2b8;'>%6%</b>"
        ).arg(QString(FA::QCheck))
         .arg(successCount)
         .arg(QString(FA::QTimes))
         .arg(failureCount)
         .arg(QString(FA::QLineChart))
         .arg(results.size() > 0 ? QString::number((double)successCount / results.size() * 100.0, 'f', 1) : "0.0")
         .arg(statsFontFamily);
    }

    statsLabel->setText(statsText);
    statsLabel->setTextFormat(Qt::RichText);
    statsLabel->setAlignment(Qt::AlignCenter);

    // 说明信息
    QLabel* infoLabel = new QLabel(
        "已完成的翻译已应用到当前文件。\n"
        "您可以继续编辑未完成的条目，或稍后重新翻译。"
    );
    infoLabel->setStyleSheet("color: #6c757d; font-size: 13px;");
    infoLabel->setWordWrap(true);
    infoLabel->setAlignment(Qt::AlignCenter);

    // 确定按钮
    QPushButton* okButton = new QPushButton("确定");
    okButton->setMinimumWidth(100);
    okButton->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a90e2, stop:1 #357abd);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 8px 24px;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5aa0f5, stop:1 #4688c9);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #357abd, stop:1 #2a669a);"
        "}"
    );
    connect(okButton, &QPushButton::clicked, &resultDialog, &QDialog::accept);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addStretch();

    layout->addLayout(titleLayout);
    layout->addWidget(statsLabel);
    layout->addSpacing(10);
    layout->addWidget(infoLabel);
    layout->addStretch();
    layout->addLayout(buttonLayout);

    resultDialog.exec();
}

void GXTStudio::onCancelTranslationRequested(MultiThreadProgressDialog::CancelOption option)
{
    qWarning() << "GXTStudio::onCancelTranslationRequested() 被调用，option:" << option;

    // 保存用户的选择
    m_cancelOption = option;

    // 调用翻译器的取消方法
    if (m_smartTranslator) {
        QMetaObject::invokeMethod(m_smartTranslator, "cancelTranslation", Qt::QueuedConnection);
    }
}

void GXTStudio::onTranslationCancelled()
{
    qWarning() << "GXTStudio::onTranslationCancelled() 被调用";

    // 获取已完成的翻译结果
    QList<TranslateResult> completedResults;
    if (m_smartTranslator) {
        completedResults = m_smartTranslator->getCurrentResults();
    }

    qWarning() << "已完成的翻译结果数量:" << completedResults.size();

    // 关闭进度对话框
    if (m_translateProgressDialog) {
        try {
            m_translateProgressDialog->close();
            m_translateProgressDialog->deleteLater();
            m_translateProgressDialog = nullptr;
        } catch (...) {
            qWarning() << "关闭进度对话框时发生异常";
        }
    }

    // 根据用户的选择处理已完成的翻译
    if (!completedResults.isEmpty()) {
        if (m_cancelOption == MultiThreadProgressDialog::CancelAndKeep) {
            // 用户选择保留已完成的翻译
            qWarning() << "用户选择保留已完成的翻译结果";
            try {
                applyPartialTranslationResults(completedResults);
                showLogMessage(QString("已保存 %1 个已完成的翻译").arg(completedResults.size()));
            } catch (...) {
                qWarning() << "保存已完成翻译时发生异常";
            }
        } else {
            // 用户选择不保留已完成的翻译
            qWarning() << "用户选择不保留已完成的翻译结果";
            showLogMessage("翻译已取消，未保存已完成结果");
        }
    } else {
        try {
            QMessageBox::information(this, "翻译取消", "翻译已被用户取消。\n\n没有已完成的翻译结果可保存。");
            showLogMessage("翻译已取消");
        } catch (...) {
            qWarning() << "显示取消对话框时发生异常";
        }
    }

    // 重置取消选项
    m_cancelOption = MultiThreadProgressDialog::NoCancel;
}

void GXTStudio::onTranslationRequestProgress(int completed, int total, const QString& message)
{
    // 这个函数在头文件中声明但未使用，保留空实现
    Q_UNUSED(completed)
    Q_UNUSED(total)
    Q_UNUSED(message)
}

void GXTStudio::onTranslationBatchProgress(int completed, int total, const QString& message)
{
    // 这个函数在头文件中声明但未使用，保留空实现
    Q_UNUSED(completed)
    Q_UNUSED(total)
    Q_UNUSED(message)
}

void GXTStudio::onTranslationTotalProgress(int completed, int total, const QString& message)
{
    // 这个函数在头文件中声明但未使用，保留空实现
    Q_UNUSED(completed)
    Q_UNUSED(total)
    Q_UNUSED(message)
}

CharTableWidget* GXTStudio::createCharTableTab(const QString& fileName, const CharTableData& data)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8,8,8,8);
    layout->setSpacing(6);

    // Top controls: file name label + zoom slider + save button + export button
    QHBoxLayout* topRow = new QHBoxLayout();
    QLabel* nameLabel = new QLabel(QString("%1 (%2 x %3)").arg(fileName).arg(data.cols).arg(data.rows));
    nameLabel->setStyleSheet("font-weight:600; margin-left:6px;");
    topRow->addWidget(nameLabel);
    topRow->addStretch();

    QPushButton* exportBtn = new QPushButton("导出为图片");
    topRow->addWidget(exportBtn);

    layout->addLayout(topRow);

    // CharTableWidget inside scroll area
    CharTableWidget* widget = new CharTableWidget(page);
    widget->setData(data);

    QScrollArea* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setWidget(widget);
    layout->addWidget(scroll);

    // 底部状态栏
    QHBoxLayout* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(4, 0, 4, 0);
    
    // 行列位置标签
    QLabel* posLabel = new QLabel("第 1 行, 第 1 列", page);
    posLabel->setStyleSheet("color: #666; font-size: 12px;");
    posLabel->setMinimumWidth(120);
    
    // 字符数提示标签
    QLabel* hintLabel = new QLabel(page);
    hintLabel->setStyleSheet("color: #666; font-size: 12px;");
    hintLabel->setMinimumWidth(200);
    
    bottomRow->addWidget(posLabel);
    bottomRow->addStretch();
    bottomRow->addWidget(hintLabel);
    layout->addLayout(bottomRow);

    // 连接光标位置变化信号
    connect(widget, &CharTableWidget::cursorPositionChanged, [posLabel](int line, int column) {
        posLabel->setText(QString("第 %1 行, 第 %2 列").arg(line).arg(column));
    });
    
    // 设置提示标签
    widget->setHintLabel(hintLabel);
    
    // 连接字符数超限信号
    connect(widget, &CharTableWidget::maxCharsReached, [hintLabel]() {
        hintLabel->setText("字符数最多 64 x 64 (已达上限)");
        hintLabel->setStyleSheet("color: red; font-size: 12px;");
    });
    
    // 连接重复字符信号
    connect(widget, &CharTableWidget::duplicateChar, [hintLabel](QChar ch) {
        hintLabel->setText(QString("字符 '%1' 已存在，不允许重复").arg(ch));
        hintLabel->setStyleSheet("color: red; font-size: 12px;");
    });

    // export button: render widget to image
    connect(exportBtn, &QPushButton::clicked, [this, widget, fileName]() {
        QString out = QFileDialog::getSaveFileName(this, "导出为PNG", QFileInfo(fileName).completeBaseName() + ".png", "PNG 图片 (*.png)");
        if (out.isEmpty()) return;
        // 使用文档的实际大小
        QSize imgSize = widget->document()->size().toSize();
        if (imgSize.isEmpty()) {
            imgSize = QSize(800, 600);
        }
        QImage image(imgSize, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        widget->render(&painter);
        image.save(out);
        showLogMessage("已导出: " + out);
    });

    // 返回CharTableWidget指针，但它的父窗口是page
    widget->setProperty("parentPage", QVariant::fromValue<QWidget*>(page));
    return widget;
}

// 当字符表中选中某个字符时调用：尝试插入到当前编辑框，否则复制到剪贴板
void GXTStudio::onCharTableCharacterSelected(uint16_t charCode, const QString& displayChar)
{
    FileTab* currentTab = getCurrentTab();
    if (currentTab) {
        // 优先插入到 valueEdit（如果存在并可编辑）
        if (currentTab->valueEdit && currentTab->valueEdit->isEnabled()) {
            QLineEdit* edit = currentTab->valueEdit;
            int pos = edit->cursorPosition();
            QString text = edit->text();
            text.insert(pos, displayChar);
            edit->setText(text);
            edit->setCursorPosition(pos + displayChar.length());
            showLogMessage(QString("已插入字符: %1 (0x%2)").arg(displayChar).arg(charCode, 0, 16));
            return;
        }
    }

    // 回退：复制到剪贴板
    QClipboard* cb = QApplication::clipboard();
    cb->setText(displayChar);
    showLogMessage(QString("字符已复制到剪贴板: %1 (0x%2)").arg(displayChar).arg(charCode, 0, 16));
}