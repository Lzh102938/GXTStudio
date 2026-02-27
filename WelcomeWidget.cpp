#include "WelcomeWidget.h"
#include "FontAwesome.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QScrollArea>
#include <QScrollBar>

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: white;");
    initializeUI();
}

WelcomeWidget::~WelcomeWidget() {
}



void WelcomeWidget::initializeUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // ============ 顶部装饰区域 ============
    QWidget* decorWidget = new QWidget();
    decorWidget->setStyleSheet(
        "QWidget {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "                                stop:0 #5b9fd6, "
        "                                stop:0.8 #7eb3e0, "
        "                                stop:1 #e8f2f9);"
        "}"
    );
    decorWidget->setFixedHeight(100);
    decorWidget->setMinimumHeight(80);
    mainLayout->addWidget(decorWidget, 0);
    
    // ============ 可滚动内容区域 ============
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background-color: white; }"
        "QScrollBar:vertical { width: 8px; background-color: #f5f5f5; }"
        "QScrollBar::handle:vertical { background-color: #ccc; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background-color: #999; }"
    );
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background-color: white;");
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(50, 40, 50, 40);
    contentLayout->setSpacing(15);
    
    // ============ 标题区域 ============
    QString faTitle = QString("GXTStudio");
    QLabel* titleLabel = new QLabel(faTitle);
    QFont titleFont("Microsoft YaHei", 52, QFont::Bold);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet(
        "color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, "
        "                       stop:0 #4a90d9, "
        "                       stop:1 #5b9fd6);"
    );
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    contentLayout->addWidget(titleLabel);
    
    // 分隔线
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(2);
    line->setStyleSheet(
        "QFrame {"
        "    border: none;"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                                stop:0 transparent, "
        "                                stop:0.2 #5b9fd6, "
        "                                stop:0.8 #5b9fd6, "
        "                                stop:1 transparent);"
        "}"
    );
    contentLayout->addWidget(line);
    
    // 副标题
    QLabel* subtitleLabel = new QLabel("GTA 游戏文本编辑工具");
    QFont subtitleFont("Microsoft YaHei", 16);
    subtitleFont.setStyleStrategy(QFont::PreferAntialias);
    subtitleLabel->setFont(subtitleFont);
    subtitleLabel->setStyleSheet("color: #5b9fd6; font-weight: 500; letter-spacing: 1px;");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    contentLayout->addWidget(subtitleLabel);
    
    // 描述语
    QLabel* descLabel = new QLabel("简洁 • 高效 • 专业");
    QFont descFont("Microsoft YaHei", 11);
    descFont.setStyleStrategy(QFont::PreferAntialias);
    descLabel->setFont(descFont);
    descLabel->setStyleSheet("color: #999; letter-spacing: 2px;");
    descLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(descLabel);
    
    // 弹性空间
    contentLayout->addStretch();
    
    // ============ 操作按钮区域 ============
    QWidget* actionWidget = new QWidget();
    actionWidget->setStyleSheet("background-color: transparent;");
    QVBoxLayout* actionLayout = new QVBoxLayout(actionWidget);
    actionLayout->setContentsMargins(0, 20, 0, 20);
    actionLayout->setSpacing(15);
    
    // 按钮容器（支持自动换行）
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(20);
    
    // 打开文件按钮 - 主按钮
    QPushButton* openBtn = new QPushButton(QString("%1  打开文件").arg(QString(FA::QFolderOpen)));
    openBtn->setFont(FA::solidFont(11));
    openBtn->setMinimumHeight(48);
    openBtn->setMinimumWidth(140);
    openBtn->setCursor(Qt::PointingHandCursor);
    openBtn->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(y1:0, y2:1, "
        "                                stop:0 #6ba8d8, "
        "                                stop:1 #5b9fd6);"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 10px 20px;"
        "    font-weight: bold;"
        "    letter-spacing: 1px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(y1:0, y2:1, "
        "                                stop:0 #7bb5dd, "
        "                                stop:1 #6ba8d8);"
        "}"
        "QPushButton:pressed {"
        "    background: #4a90d9;"
        "}"
    );
    
    // 帮助按钮 - 次按钮
    QPushButton* helpBtn = new QPushButton(QString("%1  帮助").arg(QString(FA::QInfoCircle)));
    helpBtn->setFont(FA::solidFont(11));
    helpBtn->setMinimumHeight(48);
    helpBtn->setMinimumWidth(140);
    helpBtn->setCursor(Qt::PointingHandCursor);
    helpBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #f5f5f5;"
        "    color: #333;"
        "    border: 2px solid #e0e0e0;"
        "    border-radius: 6px;"
        "    padding: 10px 20px;"
        "    font-weight: bold;"
        "    letter-spacing: 1px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #efefef;"
        "    border: 2px solid #5b9fd6;"
        "    color: #5b9fd6;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #e0e0e0;"
        "}"
    );
    
    btnLayout->addStretch();
    btnLayout->addWidget(openBtn);
    btnLayout->addWidget(helpBtn);
    btnLayout->addStretch();
    
    connect(openBtn, &QPushButton::clicked, this, &WelcomeWidget::openFileRequested);
    connect(helpBtn, &QPushButton::clicked, this, &WelcomeWidget::helpRequested);
    
    actionLayout->addLayout(btnLayout);
    contentLayout->addWidget(actionWidget);
    
    // ============ 提示信息区域 ============
    QWidget* tipsWidget = new QWidget();
    tipsWidget->setStyleSheet("background-color: transparent;");
    QVBoxLayout* tipsLayout = new QVBoxLayout(tipsWidget);
    tipsLayout->setContentsMargins(0, 10, 0, 0);
    tipsLayout->setSpacing(8);
    
    QLabel* tipsLabel = new QLabel(QString("%1  拖拽文件到窗口也可以打开").arg(QString(FA::QUpload)));
    tipsLabel->setFont(FA::solidFont(10));
    tipsLabel->setStyleSheet("color: #bbb;");
    tipsLabel->setAlignment(Qt::AlignCenter);
    tipsLabel->setWordWrap(true);
    tipsLayout->addWidget(tipsLabel);
    
    QLabel* versionLabel = new QLabel(QString("%1  版本 v3.0").arg(QString(FA::QTag)));
    versionLabel->setFont(FA::solidFont(10));
    versionLabel->setStyleSheet("color: #ddd;");
    versionLabel->setAlignment(Qt::AlignCenter);
    tipsLayout->addWidget(versionLabel);
    
    contentLayout->addWidget(tipsWidget);
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea, 1);
}

void WelcomeWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
}