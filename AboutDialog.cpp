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
#include <QCoreApplication>
#include <QPixmap>

// VersionCard 实现
VersionCard::VersionCard(const QString& version, const QString& date, const QString& changes, QWidget* parent)
    : QFrame(parent), m_version(version), m_date(date), m_changes(changes), m_expanded(false)
{
    // 优化绘制属性，减少闪烁
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAttribute(Qt::WA_StyledBackground, true); // 使用样式表背景
    
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
    setFixedHeight(50);
    setStyleSheet(getVersionCardStyle());
    setCursor(Qt::PointingHandCursor);
    
    // 固定宽度避免抖动
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

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
        // 展开：显示内容
        m_changesLabel->show();
        setFixedHeight(QWIDGETSIZE_MAX); // 自动调整高度
    } else {
        // 收起：隐藏内容
        m_changesLabel->hide();
        setFixedHeight(50); // 固定高度
    }
    
    updateArrowIcon();
}



void VersionCard::updateArrowIcon()
{
    m_arrowLabel->setText(m_expanded ? QString(FA::QChevronUp) : QString(FA::QChevronDown));
    // 箭头图标变化已经足够明显，不需要额外旋转动画
    m_arrowLabel->update(); // 立即更新箭头图标
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
    // 使用样式表绘制背景，不需要自定义绘制
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
    
    // 优化窗口属性，减少闪烁
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground, false);

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
    createDonationSection();
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

    // 应用程序图标
    QString iconPath = QCoreApplication::applicationDirPath() + "/icon/favicon.png";
    QPixmap pixmap(iconPath);
    QLabel* iconLabel = new QLabel(headerFrame);
    if (!pixmap.isNull()) {
        pixmap = pixmap.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        iconLabel->setPixmap(pixmap);
    } else {
        // 加载失败时使用默认图标
        iconLabel->setText(QString(FA::QGamepadModern));
        iconLabel->setFont(FA::solidFont(42));
        iconLabel->setStyleSheet("QLabel { color: #4a90e2; }");
    }
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
    QLabel* dateLabel = new QLabel("2026年3月？？日", infoFrame);
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
    QLabel* authorLabel = new QLabel("Lzh10_慕黑", infoFrame);
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
        QString("<span style='font-family: %1; font-size: 14px; font-weight: bold; color: #4a90e2;'>%2</span> <a href='https://github.com/Lzh102938/GXTStudio' style='color: #4a90e2; text-decoration: none; font-size: 13px; font-weight: 500;'>访问项目仓库</a>")
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

void AboutDialog::createDonationSection()
{
    QVBoxLayout* layout = static_cast<QVBoxLayout*>(m_contentWidget->layout());

    QFrame* donationFrame = new QFrame(m_contentWidget);
    donationFrame->setStyleSheet(getCardStyle());

    QHBoxLayout* donationLayout = new QHBoxLayout(donationFrame);
    donationLayout->setContentsMargins(48, 48, 48, 48);
    donationLayout->setSpacing(36);

    // 左侧图片
    QLabel* imageLabel = new QLabel(donationFrame);
    QString imagePath = QCoreApplication::applicationDirPath() + "/icon/weixin.png";
    QPixmap pixmap(imagePath);
    if (!pixmap.isNull()) {
        pixmap = pixmap.scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        imageLabel->setPixmap(pixmap);
    } else {
        // 尝试加载支付宝图片
        QString alipayPath = QCoreApplication::applicationDirPath() + "/icon/alipay.png";
        QPixmap alipayPixmap(alipayPath);
        if (!alipayPixmap.isNull()) {
            alipayPixmap = alipayPixmap.scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            imageLabel->setPixmap(alipayPixmap);
        } else {
            // 加载失败时使用默认图标
            imageLabel->setText(QString(FA::QHeart));
            imageLabel->setFont(FA::solidFont(140));
            imageLabel->setStyleSheet("QLabel { color: #e74c3c; }");
            imageLabel->setAlignment(Qt::AlignCenter);
        }
    }
    imageLabel->setFixedSize(240, 240);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setProperty("donationImage", true);

    // 右侧内容
    QVBoxLayout* contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(16);

    // 标题文字
    QLabel* titleLabel = new QLabel("请作者喝杯奶茶叭~\n感谢您的支持！", donationFrame);
    titleLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 20px; "
        "font-weight: 300; "
        "color: #2c3e50; "
        "line-height: 1.6; "
        "}"
    );
    titleLabel->setWordWrap(true);

    // 捐赠选项
    QHBoxLayout* optionLayout = new QHBoxLayout();
    optionLayout->setSpacing(20);

    // 微信选项
    QHBoxLayout* weixinLayout = new QHBoxLayout();
    weixinLayout->setSpacing(6);
    
    QLabel* weixinIcon = new QLabel(QString(FA::QWeixin), donationFrame);
    weixinIcon->setFont(FA::brandsFont(14));
    weixinIcon->setStyleSheet("QLabel { color: #00d20d; }");
    
    QLabel* weixinText = new QLabel("微信", donationFrame);
    weixinText->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 14px; "
        "font-weight: 500; "
        "color: #00d20d; "
        "}"
        "QLabel:hover { "
        "color: #00b80b; "
        "text-decoration: underline; "
        "}"
    );
    
    weixinLayout->addWidget(weixinIcon);
    weixinLayout->addWidget(weixinText);

    QWidget* weixinWidget = new QWidget(donationFrame);
    weixinWidget->setLayout(weixinLayout);
    weixinWidget->setCursor(Qt::PointingHandCursor);
    weixinWidget->installEventFilter(this);
    weixinWidget->setProperty("donationType", "weixin");
    weixinWidget->setStyleSheet("QWidget { background-color: transparent; }");

    // 支付宝选项
    QHBoxLayout* alipayLayout = new QHBoxLayout();
    alipayLayout->setSpacing(6);
    
    QLabel* alipayIcon = new QLabel(QString(FA::QAlipay), donationFrame);
    alipayIcon->setFont(FA::brandsFont(14));
    alipayIcon->setStyleSheet("QLabel { color: #1677ff; }");
    
    QLabel* alipayText = new QLabel("支付宝", donationFrame);
    alipayText->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 14px; "
        "font-weight: 500; "
        "color: #1677ff; "
        "}"
        "QLabel:hover { "
        "color: #0056d3; "
        "text-decoration: underline; "
        "}"
    );
    
    alipayLayout->addWidget(alipayIcon);
    alipayLayout->addWidget(alipayText);

    QWidget* alipayWidget = new QWidget(donationFrame);
    alipayWidget->setLayout(alipayLayout);
    alipayWidget->setCursor(Qt::PointingHandCursor);
    alipayWidget->installEventFilter(this);
    alipayWidget->setProperty("donationType", "alipay");
    alipayWidget->setStyleSheet("QWidget { background-color: transparent; }");

    optionLayout->addWidget(weixinWidget);
    optionLayout->addWidget(alipayWidget);
    optionLayout->addStretch();

    contentLayout->addWidget(titleLabel);
    contentLayout->addLayout(optionLayout);
    contentLayout->addStretch();

    donationLayout->addWidget(imageLabel);
    donationLayout->addLayout(contentLayout);

    layout->addWidget(donationFrame);
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
    // 添加伸缩，防止底部留白
    cardsLayout->addStretch();

    // 添加版本卡片
    VersionCard* v1_0_0 = new VersionCard(
        "v1.0.0",
        "2026-03-??",
        "• 完全重写的编辑器发布\n"
        "• 革新的操作逻辑，支持多标签页\n"
        "• 支持 GXT2 文件 (GTA V)\n"
        "• 支持 WHM 文件格式 (由于WHM文件是完整的网页档案，目前仅支持解析功能)\n"
        "• 更自由的智能翻译，可自定义提示词、自定义翻译范围\n"
        "• 添加人性化的文本预览器\n"
        "• 实现DAT字符表编辑功能\n"
        "• 支持自动保存，批量替换\n",
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
    QLabel* sectionTitle = new QLabel("相关人员、组织与特别鸣谢", teamFrame);
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
    QLabel* teamDesc = new QLabel("致力于为中文玩家提供最好的GTA汉化资源", teamInfo);
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

    // 特别鸣谢
    QFrame* thanksFrame = new QFrame(teamFrame);
    thanksFrame->setStyleSheet("QFrame { background-color: transparent; border: none; }");
    
    QVBoxLayout* thanksLayout = new QVBoxLayout(thanksFrame);
    thanksLayout->setContentsMargins(0, 12, 0, 0);
    thanksLayout->setSpacing(8);
    
    // 鸣谢标题
    QLabel* thanksTitle = new QLabel("特别鸣谢", teamFrame);
    thanksTitle->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 13px; "
        "font-weight: 600; "
        "color: #34495e; "
        "padding: 4px 0; "
        "}"
    );
    thanksLayout->addWidget(thanksTitle);
    
    // 鸣谢列表
    QStringList thanksList = {
        "无名汉化组",
        "罪吧汉化组",
        "天道汉化组（GTAIV贴吧汉化组）",
        "@倾城剑舞"
    };
    
    for (const QString& name : thanksList) {
        QHBoxLayout* itemLayout = new QHBoxLayout();
        itemLayout->setSpacing(10);
        
        QLabel* icon = new QLabel(QString(FA::QHeart), thanksFrame);
        icon->setFont(FA::solidFont(14));
        icon->setStyleSheet("QLabel { color: #e74c3c; }");
        
        QLabel* label = new QLabel(name, thanksFrame);
        label->setStyleSheet(
            "QLabel { "
            "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
            "font-size: 13px; "
            "color: #34495e; "
            "}"
        );
        
        itemLayout->addWidget(icon);
        itemLayout->addWidget(label);
        itemLayout->addStretch();
        thanksLayout->addLayout(itemLayout);
    }
    
    teamLayout->addWidget(teamInfo);
    teamLayout->addWidget(thanksFrame);
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
    // 确保按钮文本使用FA字体
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
        QWidget* widget = qobject_cast<QWidget*>(obj);
        if (widget && widget->property("donationType").isValid()) {
            QString donationType = widget->property("donationType").toString();
            QString imagePath;
            
            if (donationType == "weixin") {
                imagePath = QCoreApplication::applicationDirPath() + "/icon/weixin.png";
            } else if (donationType == "alipay") {
                imagePath = QCoreApplication::applicationDirPath() + "/icon/alipay.png";
            }
            
            if (!imagePath.isEmpty()) {
                QPixmap pixmap(imagePath);
                if (!pixmap.isNull()) {
                    // 找到捐赠板块中的图片标签并更新
                    QWidget* donationFrame = widget->parentWidget();
                    if (donationFrame) {
                        QList<QLabel*> labels = donationFrame->findChildren<QLabel*>();
                        for (QLabel* imgLabel : labels) {
                            if (imgLabel->property("donationImage").isValid()) {
                                pixmap = pixmap.scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                                imgLabel->setPixmap(pixmap);
                                break;
                            }
                        }
                    }
                }
            }
            return true;
        }
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
            font-family: "Font Awesome 7 Free Solid", -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif;
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