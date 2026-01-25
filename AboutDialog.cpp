#include "AboutDialog.h"
#include <QStyle>
#include <QPainter>
#include <QEvent>
#include <QMessageBox>
#include <QPaintEvent>
#include <QGridLayout>
#include <QSizePolicy>
#include <QGraphicsDropShadowEffect>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

// VersionCard 实现
VersionCard::VersionCard(const QString& version, const QString& date, const QString& changes, QWidget* parent)
    : QFrame(parent), m_version(version), m_date(date), m_changes(changes), m_expanded(false)
{
    setupUI();
}

QString VersionCard::getVersionCardStyle() const
{
    return R"(
        VersionCard {
            background-color: #f8f9fa;
            border-radius: 10px;
            border: 1px solid #e9ecef;
        }
        VersionCard:hover {
            background-color: #f1f3f5;
            border: 1px solid #dee2e6;
        }
    )";
}

void VersionCard::setupUI()
{
    setMinimumHeight(50);
    setStyleSheet(getVersionCardStyle());
    setCursor(Qt::PointingHandCursor);

    m_contentLayout = new QVBoxLayout(this);
    m_contentLayout->setContentsMargins(16, 12, 16, 12);
    m_contentLayout->setSpacing(8);

    // 标题行
    QHBoxLayout* headerLayout = new QHBoxLayout();

    // 版本标签
    QLabel* versionLabel = new QLabel(m_version);
    versionLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 13px; "
        "font-weight: 500; "
        "color: #34495e; "
        "}"
    );

    // 日期标签
    QLabel* dateLabel = new QLabel(m_date);
    dateLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 12px; "
        "color: #95a5a6; "
        "}"
    );

    // 箭头标签（使用FA图标）
    m_arrowLabel = new QLabel(QString(FA::QChevronDown));
    m_arrowLabel->setFont(FA::solidFont(12));
    m_arrowLabel->setStyleSheet("QLabel { color: #4a90e2; }");
    m_arrowLabel->setAlignment(Qt::AlignCenter);
    m_arrowLabel->setFixedSize(20, 20);

    headerLayout->addWidget(versionLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(dateLabel);
    headerLayout->addSpacing(12);
    headerLayout->addWidget(m_arrowLabel);

    m_contentLayout->addLayout(headerLayout);

    // 更新内容（初始隐藏）
    m_changesLabel = new QLabel(m_changes);
    m_changesLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 12px; "
        "color: #7f8c8d; "
        "line-height: 1.5; "
        "}"
    );
    m_changesLabel->setWordWrap(true);
    m_changesLabel->hide();

    m_contentLayout->addWidget(m_changesLabel);

    // 鼠标悬停事件
    installEventFilter(this);
}

void VersionCard::toggleExpand()
{
    m_expanded = !m_expanded;

    if (m_expanded) {
        m_changesLabel->show();
        setMaximumHeight(16777215);
        updateArrowIcon();
    } else {
        m_changesLabel->hide();
        setMaximumHeight(50);
        updateArrowIcon();
    }
}

void VersionCard::updateArrowIcon()
{
    m_arrowLabel->setText(m_expanded ? QString(FA::QChevronUp) : QString(FA::QChevronDown));
}

bool VersionCard::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        toggleExpand();
        return true;
    }
    return QFrame::eventFilter(obj, event);
}

void VersionCard::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    Q_UNUSED(event)
}

// AboutDialog 实现
AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

AboutDialog::~AboutDialog()
{
}

void AboutDialog::setupUI()
{
    setWindowTitle("关于 GXTStudio");
    setMinimumSize(680, 500);
    setMaximumSize(680, 620);
    setModal(true);

    // 简洁的纯色背景
    setStyleSheet(
        "QDialog { "
        "background-color: #f5f7fa; "
        "}"
    );

    // 主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 创建滚动区域 - 优化性能
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 启用滚动优化
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    m_scrollArea->viewport()->setAttribute(Qt::WA_NoSystemBackground, false);
    
    m_scrollArea->setStyleSheet(
        "QScrollArea { "
        "border: none; "
        "background-color: #f5f7fa; "
        "}"
        "QScrollBar:vertical { "
        "background: #e9ecef; "
        "width: 8px; "
        "border-radius: 4px; "
        "margin: 0px; "
        "} "
        "QScrollBar::handle:vertical { "
        "background: #adb5bd; "
        "border-radius: 4px; "
        "min-height: 30px; "
        "margin: 2px; "
        "} "
        "QScrollBar::handle:vertical:hover { "
        "background: #6c757d; "
        "}"
    );

    // 内容区域
    m_contentWidget = new QWidget();
    m_contentWidget->setStyleSheet("QWidget { background-color: #f5f7fa; }");

    QVBoxLayout* contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(24, 24, 24, 24);
    contentLayout->setSpacing(16);

    // 创建各个部分
    createHeaderSection();
    createInfoAndFileTypeSection();
    createChangelogSection();
    createTeamSection();

    contentLayout->addStretch();

    m_scrollArea->setWidget(m_contentWidget);
    m_mainLayout->addWidget(m_scrollArea);

    // 创建底部按钮
    createFooterSection();
}

void AboutDialog::createHeaderSection()
{
    QVBoxLayout* layout = static_cast<QVBoxLayout*>(m_contentWidget->layout());

    QFrame* headerFrame = new QFrame(m_contentWidget);
    headerFrame->setStyleSheet(getCardStyle());

    QVBoxLayout* headerLayout = new QVBoxLayout(headerFrame);
    headerLayout->setContentsMargins(28, 24, 28, 24);
    headerLayout->setSpacing(12);

    // 图标和标题行
    QHBoxLayout* iconTitleLayout = new QHBoxLayout();
    iconTitleLayout->setSpacing(14);

    // 游戏手柄图标
    QLabel* iconLabel = new QLabel(QString(FA::QGamepadModern), headerFrame);
    iconLabel->setFont(FA::solidFont(42));
    iconLabel->setStyleSheet("QLabel { color: #4a90e2; }");
    iconLabel->setFixedSize(56, 56);
    iconLabel->setAlignment(Qt::AlignCenter);

    // 软件名称和版本
    QVBoxLayout* nameLayout = new QVBoxLayout();
    nameLayout->setSpacing(6);

    QLabel* nameLabel = new QLabel("GXTStudio", headerFrame);
    nameLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 28px; "
        "font-weight: 600; "
        "color: #2c3e50; "
        "}"
    );

    QLabel* versionLabel = new QLabel("版本 1.0.0", headerFrame);
    versionLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 13px; "
        "color: #4a90e2; "
        "font-weight: 500; "
        "}"
    );

    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(versionLabel);

    iconTitleLayout->addWidget(iconLabel);
    iconTitleLayout->addLayout(nameLayout);
    iconTitleLayout->addStretch();

    // 简介文本
    QLabel* descLabel = new QLabel("专业的GTA游戏文本文件编辑器", headerFrame);
    descLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 13px; "
        "color: #7f8c8d; "
        "}"
    );
    descLabel->setAlignment(Qt::AlignCenter);

    headerLayout->addLayout(iconTitleLayout);
    headerLayout->addWidget(descLabel);

    layout->addWidget(headerFrame);
}

void AboutDialog::createInfoAndFileTypeSection()
{
    QVBoxLayout* layout = static_cast<QVBoxLayout*>(m_contentWidget->layout());

    // 创建水平容器
    QFrame* hContainer = new QFrame(m_contentWidget);
    hContainer->setStyleSheet("QFrame { background-color: transparent; border: none; }");

    QHBoxLayout* hLayout = new QHBoxLayout(hContainer);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(12);

    // ========== 左侧：基本信息 ==========
    QFrame* infoFrame = new QFrame(hContainer);
    infoFrame->setStyleSheet(getCardStyle());
    infoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QVBoxLayout* infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->setContentsMargins(20, 18, 20, 18);
    infoLayout->setSpacing(12);

    // 分区标题
    QLabel* sectionTitle = new QLabel("基本信息", infoFrame);
    sectionTitle->setStyleSheet(getSectionTitleStyle());
    infoLayout->addWidget(sectionTitle);

    // 更新日期（带图标）
    QHBoxLayout* dateLayout = new QHBoxLayout();
    dateLayout->setSpacing(10);
    QLabel* dateIcon = new QLabel(QString(FA::QCalendar), infoFrame);
    dateIcon->setFont(FA::solidFont(15));
    dateIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* dateLabel = new QLabel("2024年1月25日", infoFrame);
    dateLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 13px; "
        "color: #34495e; "
        "}"
    );
    dateLayout->addWidget(dateIcon);
    dateLayout->addWidget(dateLabel);
    dateLayout->addStretch();
    infoLayout->addLayout(dateLayout);

    // 作者信息（带图标）
    QHBoxLayout* authorLayout = new QHBoxLayout();
    authorLayout->setSpacing(10);
    QLabel* authorIcon = new QLabel(QString(FA::QUser), infoFrame);
    authorIcon->setFont(FA::solidFont(15));
    authorIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* authorLabel = new QLabel("GTAmod中文组", infoFrame);
    authorLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 13px; "
        "color: #34495e; "
        "}"
    );
    authorLayout->addWidget(authorIcon);
    authorLayout->addWidget(authorLabel);
    authorLayout->addStretch();
    infoLayout->addLayout(authorLayout);

    // 主页链接（带图标）
    QLabel* homeLink = new QLabel(
        QString("<span style='font-family: %1; font-size: 14px; color: #4a90e2;'>%2</span> <a href='https://github.com' style='color: #4a90e2; text-decoration: none; font-size: 13px; font-weight: 500;'>访问主页</a>")
        .arg(FA::solidFontFamily())
        .arg(QString(FA::QLink))
    , infoFrame);
    homeLink->setTextFormat(Qt::RichText);
    homeLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    homeLink->setOpenExternalLinks(true);
    homeLink->setStyleSheet("QLabel { padding: 2px 0; }");
    infoLayout->addWidget(homeLink);

    hLayout->addWidget(infoFrame);

    // ========== 右侧：文件类型 ==========
    QFrame* fileTypesFrame = new QFrame(hContainer);
    fileTypesFrame->setStyleSheet(getCardStyle());
    fileTypesFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QVBoxLayout* fileTypesLayout = new QVBoxLayout(fileTypesFrame);
    fileTypesLayout->setContentsMargins(20, 18, 20, 18);
    fileTypesLayout->setSpacing(12);

    // 分区标题
    QLabel* ftTitle = new QLabel("支持的文件类型", fileTypesFrame);
    ftTitle->setStyleSheet(getSectionTitleStyle());
    fileTypesLayout->addWidget(ftTitle);

    // 文件类型标签 - 使用网格布局2x2
    QGridLayout* tagsGrid = new QGridLayout();
    tagsGrid->setSpacing(10);

    QStringList fileTypes = {
        ".gxt", ".gxt2", ".dat (WHM)", ".dat (CHACHAR)"
    };

    QStringList tagColors = {
        "#4a90e2", "#6c5ce7", "#00b894", "#e17055"
    };

    for (int i = 0; i < fileTypes.size(); ++i) {
        QLabel* tagLabel = new QLabel(fileTypes[i], fileTypesFrame);
        QString tagStyle = QString(
            "QLabel { "
            "background-color: %1; "
            "color: white; "
            "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
            "font-size: 12px; "
            "font-weight: 500; "
            "padding: 8px 16px; "
            "border-radius: 14px; "
            "}"
            "QLabel:hover { "
            "background-color: %2; "
            "}"
        ).arg(tagColors[i]).arg(tagColors[(i + 1) % tagColors.size()]);
        tagLabel->setStyleSheet(tagStyle);
        tagLabel->setAlignment(Qt::AlignCenter);
        tagLabel->setCursor(Qt::PointingHandCursor);
        tagsGrid->addWidget(tagLabel, i / 2, i % 2);
    }

    fileTypesLayout->addLayout(tagsGrid);

    hLayout->addWidget(fileTypesFrame);

    layout->addWidget(hContainer);
}

QLabel* AboutDialog::createInfoLabel(const QString& title, const QString& value)
{
    QLabel* label = new QLabel();
    QString text = QString("<span style='font-size: 11px; font-weight: bold; color: #5f6368;'>%1</span> "
                          "<span style='font-size: 13px; color: #202124;'>%2</span>").arg(title, value);
    label->setTextFormat(Qt::RichText);
    label->setText(text);
    label->setStyleSheet("QLabel { padding: 4px 0; }");
    return label;
}

void AboutDialog::createChangelogSection()
{
    QVBoxLayout* layout = static_cast<QVBoxLayout*>(m_contentWidget->layout());

    QFrame* changelogFrame = new QFrame(m_contentWidget);
    changelogFrame->setStyleSheet(getCardStyle());

    QVBoxLayout* changelogLayout = new QVBoxLayout(changelogFrame);
    changelogLayout->setContentsMargins(20, 18, 20, 18);
    changelogLayout->setSpacing(12);

    // 分区标题（带图标）
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(10);
    QLabel* titleIcon = new QLabel(QString(FA::QTag), changelogFrame);
    titleIcon->setFont(FA::solidFont(16));
    titleIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* sectionTitle = new QLabel("更新日志", changelogFrame);
    sectionTitle->setStyleSheet(getSectionTitleStyle());
    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(sectionTitle);
    titleLayout->addStretch();
    changelogLayout->addLayout(titleLayout);

    // 版本卡片列表
    QFrame* cardsContainer = new QFrame(changelogFrame);
    cardsContainer->setStyleSheet("QFrame { background-color: transparent; border: none; }");

    QVBoxLayout* cardsLayout = new QVBoxLayout(cardsContainer);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(10);

    // 添加版本卡片
    VersionCard* v1_0_0 = new VersionCard(
        "v1.0.0",
        "2024-01-25",
        "• 初始版本发布\n"
        "• 支持 GXT 文件编辑 (GTA III, VC, SA, IV)\n"
        "• 支持 GXT2 文件 (GTA V)\n"
        "• 支持 WHM 文件格式\n"
        "• 智能翻译功能\n"
        "• 码表转换功能\n"
        "• 字符表预览功能",
        cardsContainer
    );
    cardsLayout->addWidget(v1_0_0);

    changelogLayout->addWidget(cardsContainer);
    layout->addWidget(changelogFrame);
}

void AboutDialog::createTeamSection()
{
    QVBoxLayout* layout = static_cast<QVBoxLayout*>(m_contentWidget->layout());

    QFrame* teamFrame = new QFrame(m_contentWidget);
    teamFrame->setStyleSheet(getCardStyle());

    QVBoxLayout* teamLayout = new QVBoxLayout(teamFrame);
    teamLayout->setContentsMargins(20, 18, 20, 18);
    teamLayout->setSpacing(12);

    // 分区标题（带图标）
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(10);
    QLabel* titleIcon = new QLabel(QString(FA::QBuilding), teamFrame);
    titleIcon->setFont(FA::solidFont(16));
    titleIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* sectionTitle = new QLabel("参与人员和组织", teamFrame);
    sectionTitle->setStyleSheet(getSectionTitleStyle());
    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(sectionTitle);
    titleLayout->addStretch();
    teamLayout->addLayout(titleLayout);

    // 团队信息
    QFrame* teamInfo = new QFrame(teamFrame);
    teamInfo->setStyleSheet("QFrame { background-color: transparent; border: none; }");

    QVBoxLayout* teamInfoLayout = new QVBoxLayout(teamInfo);
    teamInfoLayout->setContentsMargins(0, 0, 0, 0);
    teamInfoLayout->setSpacing(8);

    // 团队名称
    QHBoxLayout* teamNameLayout = new QHBoxLayout();
    teamNameLayout->setSpacing(10);
    QLabel* teamIcon = new QLabel(QString(FA::QUserGroup), teamInfo);
    teamIcon->setFont(FA::solidFont(15));
    teamIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* teamLabel = new QLabel("GTAmod中文组", teamInfo);
    teamLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 14px; "
        "font-weight: 500; "
        "color: #34495e; "
        "}"
    );
    teamNameLayout->addWidget(teamIcon);
    teamNameLayout->addWidget(teamLabel);
    teamNameLayout->addStretch();
    teamInfoLayout->addLayout(teamNameLayout);

    // 团队描述
    QLabel* teamDesc = new QLabel("致力于为中文玩家提供最好的GTA游戏模组开发工具和汉化资源", teamInfo);
    teamDesc->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 13px; "
        "color: #7f8c8d; "
        "line-height: 1.5; "
        "}"
    );
    teamDesc->setWordWrap(true);
    teamInfoLayout->addWidget(teamDesc);

    teamLayout->addWidget(teamInfo);
    layout->addWidget(teamFrame);
}

void AboutDialog::createFooterSection()
{
    // 创建底部栏 - 使用纯色背景，舍弃透明和模糊效果
    QWidget* footerContainer = new QWidget(this);
    footerContainer->setStyleSheet(
        "QWidget { "
        "background-color: #ffffff; "
        "border-top: 1px solid #e9ecef; "
        "}"
    );
    footerContainer->setMaximumHeight(60);

    // 创建内容布局
    QHBoxLayout* footerLayout = new QHBoxLayout(footerContainer);
    footerLayout->setContentsMargins(20, 12, 20, 12);
    footerLayout->setSpacing(12);

    footerLayout->addStretch();

    // 检查更新按钮 - 使用FA图标
    m_checkUpdateButton = new QPushButton(QString(FA::QSync) + " 检查更新", footerContainer);
    m_checkUpdateButton->setFont(FA::solidFont(14));
    m_checkUpdateButton->setStyleSheet(getButtonStyle());
    m_checkUpdateButton->setMinimumWidth(140);
    m_checkUpdateButton->setCursor(Qt::PointingHandCursor);

    connect(m_checkUpdateButton, &QPushButton::clicked, this, &AboutDialog::onCheckUpdateClicked);

    footerLayout->addWidget(m_checkUpdateButton);
    footerLayout->addStretch();

    m_mainLayout->addWidget(footerContainer);
}

bool AboutDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        onCheckUpdateClicked();
        return true;
    }
    return QDialog::eventFilter(obj, event);
}

void AboutDialog::onCheckUpdateClicked()
{
    // 预留功能 - 暂时不做
    QMessageBox::information(this, "检查更新", "当前已是最新版本！");
}

void AboutDialog::openHomePage()
{
    QDesktopServices::openUrl(QUrl("https://github.com"));
}

QString AboutDialog::getCardStyle() const
{
    return R"(
        QFrame {
            background-color: #ffffff;
            border-radius: 12px;
        }
        QFrame:hover {
            background-color: #ffffff;
        }
    )";
}

QString AboutDialog::getSectionTitleStyle() const
{
    return R"(
        QLabel {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif;
            font-size: 15px;
            font-weight: 600;
            color: #2c3e50;
            padding: 0px 0px 10px 0px;
        }
    )";
}

QString AboutDialog::getButtonStyle() const
{
    return R"(
        QPushButton {
            background-color: #4a90e2;
            color: white;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif;
            font-size: 14px;
            font-weight: 500;
            padding: 10px 24px;
            border: none;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #357abd;
        }
        QPushButton:pressed {
            background-color: #2a64a8;
        }
        QPushButton:disabled {
            background-color: #bdc3c7;
            color: #7f8c8d;
        }
    )";
}