#include "AboutDialog.h"
#include "VersionInfo.h"
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
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

static const QString s_cardStyle = 
    "QFrame { "
    "background-color: #ffffff; "
    "border-radius: 12px; "
    "border: none; "
    "}";

static const QString s_sectionTitleStyle = 
    "QLabel { "
    "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
    "font-size: 15px; "
    "font-weight: 600; "
    "color: #2c3e50; "
    "}";

static const QString s_versionCardStyle = 
    "VersionCard { "
    "background-color: #f8f9fa; "
    "border-radius: 10px; "
    "border: none; "
    "} "
    "VersionCard:hover { "
    "background-color: #f1f3f5; "
    "}";

static const QString s_scrollAreaStyle = 
    "QScrollArea { "
    "border: none; "
    "background-color: #f5f7fa; "
    "}";

static const QString s_labelStyle = 
    "QLabel { "
    "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
    "font-size: 13px; "
    "color: #34495e; "
    "}";

static const QString s_descStyle = 
    "QLabel { "
    "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
    "font-size: 13px; "
    "color: #7f8c8d; "
    "}";

VersionCard::VersionCard(const QString& version, const QString& date, const QString& changes, QWidget* parent)
    : QFrame(parent), m_version(version), m_date(date), m_changes(changes), m_expanded(false)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

QString VersionCard::getVersionCardStyle() const
{
    return s_versionCardStyle;
}

void VersionCard::setupUI()
{
    setMinimumHeight(50);
    setStyleSheet(s_versionCardStyle);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_contentLayout = new QVBoxLayout(this);
    m_contentLayout->setContentsMargins(16, 12, 16, 12);
    m_contentLayout->setSpacing(8);

    QHBoxLayout* headerLayout = new QHBoxLayout();

    QLabel* versionLabel = new QLabel(m_version);
    versionLabel->setStyleSheet(s_labelStyle);

    QLabel* dateLabel = new QLabel(m_date);
    dateLabel->setStyleSheet(s_descStyle);

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

    m_changesLabel = new QLabel(m_changes);
    m_changesLabel->setStyleSheet(s_descStyle);
    m_changesLabel->setWordWrap(true);
    m_changesLabel->hide();

    m_contentLayout->addWidget(m_changesLabel);
    installEventFilter(this);
}

void VersionCard::toggleExpand()
{
    m_expanded = !m_expanded;
    
    setUpdatesEnabled(false);
    
    if (m_expanded) {
        m_changesLabel->show();
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setMinimumHeight(0);
        setMaximumHeight(16777215);
    } else {
        m_changesLabel->hide();
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(50);
    }
    
    updateArrowIcon();
    
    setUpdatesEnabled(true);
    update();
    
    QWidget* parent = parentWidget();
    if (parent) {
        parent->updateGeometry();
        parent->adjustSize();
    }
}

void VersionCard::updateArrowIcon()
{
    m_arrowLabel->setText(m_expanded ? QString(FA::QChevronUp) : QString(FA::QChevronDown));
    m_arrowLabel->update();
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
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#f8f9fa"));
    painter.drawRoundedRect(rect(), 10, 10);
}

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent), m_networkManager(new QNetworkAccessManager(this))
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
    
    setStyleSheet("QDialog { background-color: #f5f7fa; }");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(s_scrollAreaStyle);

    m_contentWidget = new QWidget();
    m_contentWidget->setStyleSheet("QWidget { background-color: #f5f7fa; }");

    QVBoxLayout* contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(24, 24, 24, 24);
    contentLayout->setSpacing(16);

    createHeaderSection();
    createInfoAndFileTypeSection();
    createDonationSection();
    createChangelogSection();
    createTeamSection();

    contentLayout->addStretch();

    m_scrollArea->setWidget(m_contentWidget);
    m_mainLayout->addWidget(m_scrollArea);

    createFooterSection();
}

void AboutDialog::createHeaderSection()
{
    QVBoxLayout* layout = static_cast<QVBoxLayout*>(m_contentWidget->layout());

    QFrame* headerFrame = new QFrame(m_contentWidget);
    headerFrame->setStyleSheet(s_cardStyle);

    QVBoxLayout* headerLayout = new QVBoxLayout(headerFrame);
    headerLayout->setContentsMargins(28, 24, 28, 24);
    headerLayout->setSpacing(12);

    QHBoxLayout* iconTitleLayout = new QHBoxLayout();
    iconTitleLayout->setSpacing(14);

    QString iconPath = QCoreApplication::applicationDirPath() + "/icon/favicon.png";
    QPixmap pixmap(iconPath);
    QLabel* iconLabel = new QLabel(headerFrame);
    if (!pixmap.isNull()) {
        pixmap = pixmap.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        iconLabel->setPixmap(pixmap);
    } else {
        iconLabel->setText(QString(FA::QGamepadModern));
        iconLabel->setFont(FA::solidFont(42));
        iconLabel->setStyleSheet("QLabel { color: #4a90e2; }");
    }
    iconLabel->setFixedSize(56, 56);
    iconLabel->setAlignment(Qt::AlignCenter);

    QVBoxLayout* nameLayout = new QVBoxLayout();
    nameLayout->setSpacing(6);

    QLabel* nameLabel = new QLabel("GXTStudio", headerFrame);
    nameLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 28px; "
        "font-weight: 600; "
        "color: #2c3e50; "
        "}");

    QLabel* versionLabel = new QLabel("版本 " + VersionInfo::currentVersion, headerFrame);
    versionLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 13px; "
        "color: #4a90e2; "
        "font-weight: 500; "
        "}");

    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(versionLabel);

    iconTitleLayout->addWidget(iconLabel);
    iconTitleLayout->addLayout(nameLayout);
    iconTitleLayout->addStretch();

    QLabel* descLabel = new QLabel("专业的GTA游戏文本文件编辑器", headerFrame);
    descLabel->setStyleSheet(s_descStyle);
    descLabel->setAlignment(Qt::AlignCenter);

    headerLayout->addLayout(iconTitleLayout);
    headerLayout->addWidget(descLabel);

    layout->addWidget(headerFrame);
}

void AboutDialog::createInfoAndFileTypeSection()
{
    QVBoxLayout* layout = static_cast<QVBoxLayout*>(m_contentWidget->layout());

    QFrame* hContainer = new QFrame(m_contentWidget);
    hContainer->setStyleSheet("QFrame { background-color: transparent; border: none; }");

    QHBoxLayout* hLayout = new QHBoxLayout(hContainer);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(12);

    QFrame* infoFrame = new QFrame(hContainer);
    infoFrame->setStyleSheet(s_cardStyle);
    infoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QVBoxLayout* infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->setContentsMargins(20, 18, 20, 18);
    infoLayout->setSpacing(12);

    QLabel* sectionTitle = new QLabel("基本信息", infoFrame);
    sectionTitle->setStyleSheet(s_sectionTitleStyle);
    infoLayout->addWidget(sectionTitle);

    QHBoxLayout* dateLayout = new QHBoxLayout();
    dateLayout->setSpacing(10);
    QLabel* dateIcon = new QLabel(QString(FA::QCalendar), infoFrame);
    dateIcon->setFont(FA::solidFont(15));
    dateIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* dateLabel = new QLabel("2026年3月？？日", infoFrame);
    dateLabel->setStyleSheet(s_labelStyle);
    dateLayout->addWidget(dateIcon);
    dateLayout->addWidget(dateLabel);
    dateLayout->addStretch();
    infoLayout->addLayout(dateLayout);

    QHBoxLayout* authorLayout = new QHBoxLayout();
    authorLayout->setSpacing(10);
    QLabel* authorIcon = new QLabel(QString(FA::QUser), infoFrame);
    authorIcon->setFont(FA::solidFont(15));
    authorIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* authorLabel = new QLabel("Lzh10_慕黑", infoFrame);
    authorLabel->setStyleSheet(s_labelStyle);
    authorLayout->addWidget(authorIcon);
    authorLayout->addWidget(authorLabel);
    authorLayout->addStretch();
    infoLayout->addLayout(authorLayout);

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

    QFrame* fileTypesFrame = new QFrame(hContainer);
    fileTypesFrame->setStyleSheet(s_cardStyle);
    fileTypesFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QVBoxLayout* fileTypesLayout = new QVBoxLayout(fileTypesFrame);
    fileTypesLayout->setContentsMargins(20, 18, 20, 18);
    fileTypesLayout->setSpacing(12);

    QLabel* ftTitle = new QLabel("支持的文件类型", fileTypesFrame);
    ftTitle->setStyleSheet(s_sectionTitleStyle);
    fileTypesLayout->addWidget(ftTitle);

    QGridLayout* tagsGrid = new QGridLayout();
    tagsGrid->setSpacing(10);

    static const QStringList fileTypes = { ".gxt", ".gxt2", ".dat (WHM)", ".dat (CHACHAR)" };
    static const QStringList tagColors = { "#4a90e2", "#6c5ce7", "#00b894", "#e17055" };

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
            "} "
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
    donationFrame->setStyleSheet(s_cardStyle);

    QHBoxLayout* donationLayout = new QHBoxLayout(donationFrame);
    donationLayout->setContentsMargins(48, 48, 48, 48);
    donationLayout->setSpacing(36);

    QLabel* imageLabel = new QLabel(donationFrame);
    QString imagePath = QCoreApplication::applicationDirPath() + "/icon/weixin.png";
    QPixmap pixmap(imagePath);
    if (!pixmap.isNull()) {
        pixmap = pixmap.scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        imageLabel->setPixmap(pixmap);
    } else {
        QString alipayPath = QCoreApplication::applicationDirPath() + "/icon/alipay.png";
        QPixmap alipayPixmap(alipayPath);
        if (!alipayPixmap.isNull()) {
            alipayPixmap = alipayPixmap.scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            imageLabel->setPixmap(alipayPixmap);
        } else {
            imageLabel->setText(QString(FA::QHeart));
            imageLabel->setFont(FA::solidFont(140));
            imageLabel->setStyleSheet("QLabel { color: #e74c3c; }");
            imageLabel->setAlignment(Qt::AlignCenter);
        }
    }
    imageLabel->setFixedSize(240, 240);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setProperty("donationImage", true);

    QVBoxLayout* contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(16);

    QLabel* titleLabel = new QLabel("请作者喝杯奶茶叭~\n感谢您的支持！", donationFrame);
    titleLabel->setStyleSheet(
        "QLabel { "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 20px; "
        "font-weight: 300; "
        "color: #2c3e50; "
        "line-height: 1.6; "
        "}");
    titleLabel->setWordWrap(true);

    QHBoxLayout* optionLayout = new QHBoxLayout();
    optionLayout->setSpacing(20);

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
        "} "
        "QLabel:hover { "
        "color: #00b80b; "
        "text-decoration: underline; "
        "}");
    
    weixinLayout->addWidget(weixinIcon);
    weixinLayout->addWidget(weixinText);

    QWidget* weixinWidget = new QWidget(donationFrame);
    weixinWidget->setLayout(weixinLayout);
    weixinWidget->setCursor(Qt::PointingHandCursor);
    weixinWidget->installEventFilter(this);
    weixinWidget->setProperty("donationType", "weixin");
    weixinWidget->setStyleSheet("QWidget { background-color: transparent; }");

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
        "} "
        "QLabel:hover { "
        "color: #0056d3; "
        "text-decoration: underline; "
        "}");
    
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
    changelogFrame->setStyleSheet(s_cardStyle);

    QVBoxLayout* changelogLayout = new QVBoxLayout(changelogFrame);
    changelogLayout->setContentsMargins(20, 18, 20, 18);
    changelogLayout->setSpacing(12);

    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(10);
    QLabel* titleIcon = new QLabel(QString(FA::QTag), changelogFrame);
    titleIcon->setFont(FA::solidFont(16));
    titleIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* sectionTitle = new QLabel("更新日志", changelogFrame);
    sectionTitle->setStyleSheet(s_sectionTitleStyle);
    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(sectionTitle);
    titleLayout->addStretch();
    changelogLayout->addLayout(titleLayout);

    QVector<VersionInfo::VersionEntry> versions = VersionInfo::getVersionHistory();

    for (const auto& v : versions) {
        VersionCard* card = new VersionCard(v.version, v.date, v.changes, changelogFrame);
        changelogLayout->addWidget(card);
    }

    layout->addWidget(changelogFrame);
}

void AboutDialog::createTeamSection()
{
    QVBoxLayout* layout = static_cast<QVBoxLayout*>(m_contentWidget->layout());

    QFrame* teamFrame = new QFrame(m_contentWidget);
    teamFrame->setStyleSheet(s_cardStyle);

    QVBoxLayout* teamLayout = new QVBoxLayout(teamFrame);
    teamLayout->setContentsMargins(20, 18, 20, 18);
    teamLayout->setSpacing(12);

    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(10);
    QLabel* titleIcon = new QLabel(QString(FA::QUserGroup), teamFrame);
    titleIcon->setFont(FA::solidFont(16));
    titleIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* sectionTitle = new QLabel("开发团队", teamFrame);
    sectionTitle->setStyleSheet(s_sectionTitleStyle);
    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(sectionTitle);
    titleLayout->addStretch();
    teamLayout->addLayout(titleLayout);

    QHBoxLayout* devLayout = new QHBoxLayout();
    devLayout->setSpacing(10);
    QLabel* devIcon = new QLabel(QString(FA::QUser), teamFrame);
    devIcon->setFont(FA::solidFont(14));
    devIcon->setStyleSheet("QLabel { color: #4a90e2; }");
    QLabel* devLabel = new QLabel("Lzh10_慕黑 - 主要开发者", teamFrame);
    devLabel->setStyleSheet(s_labelStyle);
    devLayout->addWidget(devIcon);
    devLayout->addWidget(devLabel);
    devLayout->addStretch();
    teamLayout->addLayout(devLayout);

    QLabel* teamDescLabel = new QLabel("GXTStudio Team", teamFrame);
    teamDescLabel->setStyleSheet(s_descStyle);
    teamLayout->addWidget(teamDescLabel);

    layout->addWidget(teamFrame);
}

void AboutDialog::createFooterSection()
{
    QFrame* footerFrame = new QFrame(this);
    footerFrame->setStyleSheet(
        "QFrame { "
        "background-color: #ffffff; "
        "border-top: 1px solid #e9ecef; "
        "}");

    QHBoxLayout* footerLayout = new QHBoxLayout(footerFrame);
    footerLayout->setContentsMargins(24, 16, 24, 16);

    footerLayout->addStretch();

    m_checkUpdateButton = new QPushButton("检查更新", footerFrame);
    m_checkUpdateButton->setStyleSheet(
        "QPushButton { "
        "background-color: #4a90e2; "
        "color: white; "
        "border: none; "
        "border-radius: 6px; "
        "padding: 8px 20px; "
        "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 13px; "
        "font-weight: 500; "
        "} "
        "QPushButton:hover { "
        "background-color: #357abd; "
        "} "
        "QPushButton:pressed { "
        "background-color: #2a5f8f; "
        "}");
    m_checkUpdateButton->setCursor(Qt::PointingHandCursor);
    connect(m_checkUpdateButton, &QPushButton::clicked, this, &AboutDialog::onCheckUpdateClicked);

    footerLayout->addWidget(m_checkUpdateButton);

    m_mainLayout->addWidget(footerFrame);
}

QString AboutDialog::getCardStyle() const
{
    return s_cardStyle;
}

QString AboutDialog::getSectionTitleStyle() const
{
    return s_sectionTitleStyle;
}

QString AboutDialog::getButtonStyle() const
{
    return QString();
}

bool AboutDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget* widget = qobject_cast<QWidget*>(obj);
        if (widget) {
            QString donationType = widget->property("donationType").toString();
            if (!donationType.isEmpty()) {
                QString imagePath;
                QString title;
                if (donationType == "weixin") {
                    imagePath = QCoreApplication::applicationDirPath() + "/icon/weixin.png";
                    title = "微信支付";
                } else if (donationType == "alipay") {
                    imagePath = QCoreApplication::applicationDirPath() + "/icon/alipay.png";
                    title = "支付宝";
                }

                if (!imagePath.isEmpty()) {
                    QPixmap pixmap(imagePath);
                    if (!pixmap.isNull()) {
                        QDialog* imageDialog = new QDialog(this);
                        imageDialog->setWindowTitle(title);
                        imageDialog->setStyleSheet("QDialog { background-color: white; }");
                        
                        QVBoxLayout* dialogLayout = new QVBoxLayout(imageDialog);
                        dialogLayout->setContentsMargins(20, 20, 20, 20);
                        
                        QLabel* imageLabel = new QLabel(imageDialog);
                        pixmap = pixmap.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        imageLabel->setPixmap(pixmap);
                        imageLabel->setAlignment(Qt::AlignCenter);
                        dialogLayout->addWidget(imageLabel);
                        
                        imageDialog->setFixedSize(imageDialog->sizeHint());
                        imageDialog->exec();
                        imageDialog->deleteLater();
                    }
                }
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}

void AboutDialog::onCheckUpdateClicked()
{
    m_checkUpdateButton->setEnabled(false);
    m_checkUpdateButton->setText("检查中...");
    
    QNetworkRequest request;
    request.setUrl(QUrl("https://api.github.com/repos/Lzh102938/GXTStudio/releases/latest"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "GXTStudio-UpdateChecker/1.0");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    
    QNetworkReply* reply = m_networkManager->get(request);
    
    QTimer* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(15000);
    timeoutTimer->start();
    
    connect(timeoutTimer, &QTimer::timeout, [this, reply, timeoutTimer]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
        timeoutTimer->deleteLater();
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, timeoutTimer]() {
        m_checkUpdateButton->setEnabled(true);
        m_checkUpdateButton->setText("检查更新");
        
        if (timeoutTimer->isActive()) {
            timeoutTimer->stop();
            timeoutTimer->deleteLater();
        }
        
        onCheckUpdateReply(reply);
    });
}

void AboutDialog::onCheckUpdateReply(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg;
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            errorMsg = "请求超时，请检查网络连接";
        } else {
            errorMsg = QString("网络错误：%1").arg(reply->errorString());
        }
        QMessageBox::warning(this, "检查更新", errorMsg);
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    reply->deleteLater();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "检查更新", "解析响应数据失败");
        return;
    }
    
    QJsonObject obj = doc.object();
    
    if (obj.contains("message") && obj["message"].toString() == "Not Found") {
        QMessageBox::information(this, "检查更新", "暂无发布版本信息");
        return;
    }
    
    QString latestVersion = obj.value("tag_name").toString();
    QString releaseName = obj.value("name").toString();
    QString releaseUrl = obj.value("html_url").toString();
    QString releaseNotes = obj.value("body").toString();
    
    if (latestVersion.isEmpty()) {
        QMessageBox::warning(this, "检查更新", "无法获取版本信息");
        return;
    }
    
    if (isNewerVersion(latestVersion)) {
            QString message = QString("发现新版本：%1\n\n%2\n\n是否前往下载？")
                .arg(latestVersion)
                .arg(releaseName);
            
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("发现新版本");
            msgBox.setText(message);
            msgBox.setInformativeText(releaseNotes.left(200) + (releaseNotes.length() > 200 ? "..." : ""));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);
            msgBox.setIcon(QMessageBox::Information);
            
            if (msgBox.exec() == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl(releaseUrl));
            }
        } else {
            QMessageBox::information(this, "检查更新", QString("当前已是最新版本！\n\n当前版本：v%1\n最新版本：%2").arg(VersionInfo::currentVersion, latestVersion));
        }
}

bool AboutDialog::isNewerVersion(const QString& remoteVersion) const
{
    QString localVer = VersionInfo::currentVersion;
    QString remoteVer = remoteVersion;
    
    if (remoteVer.startsWith('v') || remoteVer.startsWith('V')) {
        remoteVer = remoteVer.mid(1);
    }
    
    QStringList localParts = localVer.split('.');
    QStringList remoteParts = remoteVer.split('.');
    
    int maxParts = qMax(localParts.size(), remoteParts.size());
    
    for (int i = 0; i < maxParts; ++i) {
        int localNum = (i < localParts.size()) ? localParts[i].toInt() : 0;
        int remoteNum = (i < remoteParts.size()) ? remoteParts[i].toInt() : 0;
        
        if (remoteNum > localNum) {
            return true;
        } else if (remoteNum < localNum) {
            return false;
        }
    }
    
    return false;
}

void AboutDialog::openHomePage()
{
    QDesktopServices::openUrl(QUrl("https://github.com/Lzh102938/GXTStudio"));
}
