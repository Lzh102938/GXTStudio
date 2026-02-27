#include "TranslateConfigDialog.h"
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFormLayout>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QTimer>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFrame>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QFontDatabase>

// 默认系统提示词 - 专业游戏翻译风格
const QString TranslateConfigDialog::DEFAULT_SYSTEM_PROMPT =
    "你是一个专业的游戏文本翻译专家，擅长准确翻译GTA系列游戏内容。\n"
    "翻译要求：\n"
    "1. 保持游戏文本的风格和语气，符合GTA系列的游戏氛围\n"
    "2. 确保翻译准确且符合游戏语境，避免过于正式或生硬\n"
    "3. 保留原有的格式和标记，包括~g~、~y~、~r~、~b~、~w~等颜色代码\n"
    "4. 对于游戏专用术语（如车名、武器名、地点名），请使用通用的中文游戏术语\n"
    "5. 文本要简洁明了，适合游戏界面显示，避免过长\n"
    "6. 对于对话文本，保持口语化和幽默感\n"
    "7. 对于UI提示文本，保持简洁清晰\n"
    "8. 人名和专有名词通常使用英文原名，除非有官方中文翻译";

// 默认用户提示词 - 统一的用户请求格式
const QString TranslateConfigDialog::DEFAULT_USER_PROMPT =
    "请将以下游戏文本逐条翻译，严格按照JSON格式返回。\n\n"
    "输入数据：\n{contents}\n\n"
    "【重要】输出要求：\n"
    "1. 只输出JSON，不要任何其他文字（包括开头结尾的说明）\n"
    "2. 不要使用代码块标记（如```json或```）\n"
    "3. JSON必须是完整的、可直接解析的格式\n\n"
    "必须严格按照以下格式输出：\n"
    "{\"result\": [{\"index\": 0, \"text\": \"翻译内容\"}, {\"index\": 1, \"text\": \"翻译内容\"}, ...]}\n\n"
    "【注意】\n"
    "- 每个对象必须有花括号包裹：{\"index\": 0, \"text\": \"...\"}\n"
    "- 所有对象必须在方括号内：[{...}, {...}, ...]\n"
    "- 确保JSON格式完全正确，包括逗号分隔和括号匹配\n\n"
    "如果提示要求翻译成特定语言，请严格按要求翻译。";

const int TranslateConfigDialog::DEFAULT_BATCH_SIZE = 32;  // 32行每批次
const int TranslateConfigDialog::DEFAULT_MAX_CONCURRENT = 12;  // 12并发请求
const int TranslateConfigDialog::DEFAULT_MAX_RETRIES = 3;      // 3次重试
const int TranslateConfigDialog::DEFAULT_REQUEST_TIMEOUT = 180;  // 180秒超时
TranslateConfigDialog::TranslateConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
    setWindowTitle("智能翻译配置");
    setModal(true);
    resize(750, 600);
    
    // 居中显示
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
}

TranslateConfigDialog::~TranslateConfigDialog()
{
}

QList<TranslateConfigDialog::UnifiedPromptTemplate> TranslateConfigDialog::getUnifiedPromptPresets() const
{
    QList<UnifiedPromptTemplate> presets;

    // 预设1：默认 - 完整的游戏翻译系统
    presets.append(UnifiedPromptTemplate{
        "默认（专业游戏翻译）",
        "适合GTA系列游戏的专业翻译，保持游戏风格和语气，使用标准JSON格式输出",
        DEFAULT_SYSTEM_PROMPT,
        DEFAULT_USER_PROMPT
    });

    // 预设2：快速翻译
    presets.append(UnifiedPromptTemplate{
        "快速翻译",
        "简化要求，快速翻译，适合大量文本的批量处理",
        "你是一个游戏文本翻译助手。请将以下文本翻译成中文。\n"
        "要求：\n"
        "1. 翻译简洁明了，适合游戏界面显示\n"
        "2. 保留颜色代码标记（如~g~、~y~等）\n"
        "3. 保持原有的格式\n"
        "4. 避免过于冗长的表达",
        "请将以下游戏文本逐条翻译成JSON格式：\n\n"
        "输入数据：\n{contents}\n\n"
        "输出格式：{\"result\": [{\"index\": 0, \"text\": \"翻译\"}, {\"index\": 1, \"text\": \"翻译\"}]}\n\n"
        "只输出JSON，不要任何其他文字。"
    });

    // 预设3：严格格式
    presets.append(UnifiedPromptTemplate{
        "严格格式",
        "强调JSON格式的严格性，减少解析错误，适合对格式要求高的场景",
        "你是一个专业的游戏文本翻译专家，擅长准确翻译GTA系列游戏内容。\n"
        "翻译要求：\n"
        "1. 保持游戏文本的风格和语气，符合GTA系列的游戏氛围\n"
        "2. 保留原有的格式和标记，包括~g~、~y~、~r~、~b~、~w~等颜色代码\n"
        "3. 文本要简洁明了，适合游戏界面显示\n"
        "4. 必须严格按照JSON格式返回翻译结果",
        "请将以下游戏文本逐条翻译成中文。\n\n"
        "输入数据：\n{contents}\n\n"
        "【最严格要求】必须严格按照JSON格式返回：\n"
        "{\"result\": [{\"index\": 0, \"text\": \"翻译\"}, {\"index\": 1, \"text\": \"翻译\"}]}\n\n"
        "规则：\n"
        "1. 只返回纯JSON，绝对不要添加任何开场白、结束语、解释或注释\n"
        "2. index必须与输入行号对应\n"
        "3. text字段包含翻译内容\n"
        "4. 保留所有格式标记\n"
        "5. 确保JSON格式正确，可以被解析\n\n"
        "【再次强调】直接输出JSON，不要说任何多余的话"
    });

    // 预设4：详细翻译
    presets.append(UnifiedPromptTemplate{
        "详细翻译",
        "提供更详细的翻译说明，适合需要更准确翻译的场景",
        "你是一个专业的游戏文本翻译专家。请将以下文本翻译成中文。\n"
        "要求：\n"
        "1. 准确理解游戏语境，提供最贴切的翻译\n"
        "2. 保持游戏文本的风格和语气\n"
        "3. 对于专有名词，使用通用的中文游戏术语\n"
        "4. 保留所有格式标记和颜色代码\n"
        "5. 对话文本保持口语化，UI文本保持清晰\n"
        "6. 确保翻译准确，符合游戏世界观\n"
        "7. 按照JSON格式返回翻译结果",
        "请将以下游戏文本逐条翻译，严格按照JSON格式返回：\n\n"
        "输入数据：\n{contents}\n\n"
        "输出格式：{\"result\": [{\"index\": 0, \"text\": \"翻译\"}, {\"index\": 1, \"text\": \"翻译\"}]}\n\n"
        "注意：只输出JSON，不要其他文字",
    });

    // 预设5：自定义
    presets.append(UnifiedPromptTemplate{
        "自定义",
        "选择此选项后，请在下方编辑框中自定义系统提示词和用户提示词",
        "",
        ""
    });

    return presets;
}

void TranslateConfigDialog::setupPromptTemplates()
{
    // 设置统一提示词预设
    QList<UnifiedPromptTemplate> unifiedPresets = getUnifiedPromptPresets();
    m_unifiedPresetCombo->clear();

    for (const auto& preset : unifiedPresets) {
        m_unifiedPresetCombo->addItem(preset.name);
    }

    // 默认选择第一个预设
    m_unifiedPresetCombo->setCurrentIndex(0);

    // 更新描述
    m_unifiedPromptDesc->setText(unifiedPresets[0].description);
}

void TranslateConfigDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 创建标签页
    m_tabWidget = new QTabWidget(this);
    
    // ========== API配置页 ==========
    QWidget* apiTab = new QWidget();
    QVBoxLayout* apiLayout = new QVBoxLayout(apiTab);
    apiLayout->setSpacing(15);
    
    QGroupBox* keyGroup = new QGroupBox("🔑 API配置", apiTab);
    keyGroup->setStyleSheet(
        "QGroupBox {"
        "font-size: 13px; font-weight: bold;"
        "border: 2px solid #3498db;"
        "border-radius: 6px;"
        "margin-top: 10px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 10px;"
        "padding: 0 5px 0 5px;"
        "color: #3498db;"
        "}"
    );
    QVBoxLayout* keyGroupLayout = new QVBoxLayout(keyGroup);
    keyGroupLayout->setSpacing(10);
    
    // API密钥输入
    QHBoxLayout* apiKeyLayout = new QHBoxLayout();
    QLabel* apiKeyLabel = new QLabel("API密钥：", keyGroup);
    apiKeyLabel->setStyleSheet("font-weight: bold; color: #2c3e50; font-size: 12px;");
    
    m_apiKeyEdit = new QLineEdit(keyGroup);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("请输入小米MIMO API密钥");
    m_apiKeyEdit->setStyleSheet(
        "QLineEdit {"
        "padding: 8px;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 4px;"
        "font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "border: 2px solid #3498db;"
        "}"
    );
    
    m_apiStatusIcon = new QLabel("", keyGroup);
    m_apiStatusIcon->setFixedSize(24, 24);
    m_apiStatusIcon->setAlignment(Qt::AlignCenter);

    // 设置样式表，显式指定 FontAwesome 字体
    QString iconFontFamily = FA::solidFontFamily();
    if (!iconFontFamily.isEmpty()) {
        m_apiStatusIcon->setStyleSheet(QString("font-family: '%1'; font-size: 16px;").arg(iconFontFamily));
    } else {
        m_apiStatusIcon->setFont(FA::solidFont(16));
    }
    
    apiKeyLayout->addWidget(apiKeyLabel);
    apiKeyLayout->addWidget(m_apiKeyEdit, 1);
    apiKeyLayout->addWidget(m_apiStatusIcon);
    keyGroupLayout->addLayout(apiKeyLayout);
    
    // 测试连接按钮
    QHBoxLayout* testLayout = new QHBoxLayout();
    testLayout->addStretch();
    m_testButton = new QPushButton(QString("%1 测试连接").arg(QString(FA::QSync)), keyGroup);
    m_testButton->setFixedWidth(120);

    // 设置样式表，显式指定 FontAwesome 字体
    QString fontFamily = FA::solidFontFamily();
    QString buttonStyle;
    if (fontFamily.isEmpty()) {
        // 如果字体加载失败，使用默认样式
        buttonStyle =
            "QPushButton {"
            "background-color: #27ae60;"
            "color: white;"
            "border: none;"
            "border-radius: 4px;"
            "padding: 8px 16px;"
            "font-weight: bold;"
            "font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "background-color: #229954;"
            "}"
            "QPushButton:pressed {"
            "background-color: #1e8449;"
            "}"
            "QPushButton:disabled {"
            "background-color: #95a5a6;"
            "}"
        ;
    } else {
        // 如果字体加载成功，显式指定字体族
        buttonStyle = QString(
            "QPushButton {"
            "background-color: #27ae60;"
            "color: white;"
            "border: none;"
            "border-radius: 4px;"
            "padding: 8px 16px;"
            "font-weight: bold;"
            "font-size: 12px;"
            "font-family: '%1';"
            "}"
            "QPushButton:hover {"
            "background-color: #229954;"
            "}"
            "QPushButton:pressed {"
            "background-color: #1e8449;"
            "}"
            "QPushButton:disabled {"
            "background-color: #95a5a6;"
            "}"
        ).arg(fontFamily);
    }
    m_testButton->setStyleSheet(buttonStyle);
    testLayout->addWidget(m_testButton);
    keyGroupLayout->addLayout(testLayout);
    
    // API使用说明
    QLabel* apiHelpLabel = new QLabel(
        QString("💡 提示：获取API密钥请访问小米MIMO官网 "
        "<a href=\"https://platform.xiaomimimo.com\">https://platform.xiaomimimo.com/</a>"),
        keyGroup);
    apiHelpLabel->setWordWrap(true);
    apiHelpLabel->setOpenExternalLinks(true);
    apiHelpLabel->setStyleSheet(
        "color: #7f8c8d;"
        "font-size: 11px;"
        "padding: 8px;"
        "background-color: #f8f9fa;"
        "border-radius: 4px;"
        "margin-top: 5px;"
    );
    keyGroupLayout->addWidget(apiHelpLabel);
    
    apiLayout->addWidget(keyGroup);
    apiLayout->addStretch();
    
    // ========== 统一提示词配置页 ==========
    QWidget* promptTab = new QWidget();
    QScrollArea* promptScroll = new QScrollArea(promptTab);
    promptScroll->setWidgetResizable(true);
    promptScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    promptScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget* promptContainer = new QWidget();
    QVBoxLayout* promptLayout = new QVBoxLayout(promptContainer);
    promptLayout->setSpacing(20);
    promptLayout->setContentsMargins(0, 0, 0, 0);

    // 统一提示词部分
    QGroupBox* unifiedGroup = new QGroupBox(QString("%1 统一提示词配置").arg(QString(FA::QPen)), promptContainer);

    // 设置样式表，显式指定 FontAwesome 字体
    QString groupBoxFontFamily = FA::solidFontFamily();
    QString groupBoxStyle;
    if (groupBoxFontFamily.isEmpty()) {
        // 如果字体加载失败，使用默认样式
        groupBoxStyle =
            "QGroupBox {"
            "font-size: 13px; font-weight: bold;"
            "border: 2px solid #3498db;"
            "border-radius: 6px;"
            "margin-top: 10px;"
            "padding-top: 10px;"
            "}"
            "QGroupBox::title {"
            "subcontrol-origin: margin;"
            "left: 10px;"
            "padding: 0 5px 0 5px;"
            "color: #3498db;"
            "}"
        ;
    } else {
        // 如果字体加载成功，显式指定字体族
        groupBoxStyle = QString(
            "QGroupBox {"
            "font-size: 13px; font-weight: bold;"
            "border: 2px solid #3498db;"
            "border-radius: 6px;"
            "margin-top: 10px;"
            "padding-top: 10px;"
            "font-family: '%1';"
            "}"
            "QGroupBox::title {"
            "subcontrol-origin: margin;"
            "left: 10px;"
            "padding: 0 5px 0 5px;"
            "color: #3498db;"
            "}"
        ).arg(groupBoxFontFamily);
    }
    unifiedGroup->setStyleSheet(groupBoxStyle);
    QVBoxLayout* unifiedLayout = new QVBoxLayout(unifiedGroup);
    unifiedLayout->setSpacing(15);

    // 预设选择
    QLabel* presetLabel = new QLabel("快速选择预设模板：", unifiedGroup);
    presetLabel->setStyleSheet("font-weight: bold; color: #2c3e50; font-size: 12px;");

    QHBoxLayout* presetLayout = new QHBoxLayout();
    m_unifiedPresetCombo = new QComboBox(unifiedGroup);
    m_unifiedPresetCombo->setMinimumWidth(300);
    m_unifiedPresetCombo->setStyleSheet(
        "QComboBox {"
        "padding: 6px;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 4px;"
        "font-size: 12px;"
        "}"
        "QComboBox:focus {"
        "border: 2px solid #3498db;"
        "}"
        "QComboBox QAbstractItemView {"
        "border: 2px solid #bdc3c7;"
        "selection-background-color: #3498db;"
        "selection-color: white;"
        "}"
    );

    QPushButton* applyPresetButton = new QPushButton("应用", unifiedGroup);
    applyPresetButton->setFixedWidth(70);
    applyPresetButton->setStyleSheet(
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "border-radius: 4px;"
        "padding: 6px 12px;"
        "font-weight: bold;"
        "font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}"
    );

    presetLayout->addWidget(presetLabel);
    presetLayout->addSpacing(5);
    presetLayout->addWidget(m_unifiedPresetCombo, 1);
    presetLayout->addWidget(applyPresetButton);
    unifiedLayout->addLayout(presetLayout);

    m_unifiedPromptDesc = new QLabel(unifiedGroup);
    m_unifiedPromptDesc->setWordWrap(true);
    m_unifiedPromptDesc->setStyleSheet(
        "color: #7f8c8d;"
        "font-size: 11px;"
        "padding: 8px;"
        "background-color: #e8f4fc;"
        "border-left: 4px solid #3498db;"
        "border-radius: 4px;"
        "margin-bottom: 8px;"
    );
    unifiedLayout->addWidget(m_unifiedPromptDesc);

    // 系统提示词部分
    QLabel* systemLabel = new QLabel("🎭 系统提示词（System Prompt - 定义AI角色）：", unifiedGroup);
    systemLabel->setStyleSheet("font-weight: bold; color: #2c3e50; font-size: 13px;");
    unifiedLayout->addWidget(systemLabel);

    m_systemPromptEdit = new QTextEdit(unifiedGroup);
    m_systemPromptEdit->setMinimumHeight(150);
    m_systemPromptEdit->setMaximumHeight(250);
    m_systemPromptEdit->setAcceptRichText(false);
    m_systemPromptEdit->setPlaceholderText("在此输入系统提示词，定义AI的角色和翻译风格...");
    m_systemPromptEdit->setStyleSheet(
        "QTextEdit {"
        "padding: 10px;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 4px;"
        "font-size: 12px;"
        "font-family: 'Consolas', 'Courier New', monospace;"
        "background-color: #ffffff;"
        "}"
        "QTextEdit:focus {"
        "border: 2px solid #3498db;"
        "}"
    );
    unifiedLayout->addWidget(m_systemPromptEdit);

    // 用户提示词部分
    QLabel* userLabel = new QLabel("💬 用户提示词（User Prompt - 定义翻译任务）：", unifiedGroup);
    userLabel->setStyleSheet("font-weight: bold; color: #2c3e50; font-size: 13px;");
    unifiedLayout->addWidget(userLabel);

    m_userPromptEdit = new QTextEdit(unifiedGroup);
    m_userPromptEdit->setMinimumHeight(180);
    m_userPromptEdit->setMaximumHeight(300);
    m_userPromptEdit->setAcceptRichText(false);
    m_userPromptEdit->setPlaceholderText("在此输入用户提示词，定义翻译任务的格式和要求...\n使用 {contents} 作为待翻译文本的占位符");
    m_userPromptEdit->setStyleSheet(
        "QTextEdit {"
        "padding: 10px;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 4px;"
        "font-size: 12px;"
        "font-family: 'Consolas', 'Courier New', monospace;"
        "background-color: #ffffff;"
        "}"
        "QTextEdit:focus {"
        "border: 2px solid #3498db;"
        "}"
    );
    unifiedLayout->addWidget(m_userPromptEdit);

    // 提示词说明
    QLabel* promptHelpLabel = new QLabel(
        "💡 提示词说明：\n"
        "● 系统提示词：定义AI的角色、翻译风格和规则（如\"你是专业的游戏翻译专家\"）\n"
        "● 用户提示词：定义具体的翻译任务和输出格式（必须包含{contents}占位符）\n"
        "● 预设模板已经配置好，建议选择预设后根据需要微调\n"
        "● 系统会自动将两个提示词组合为完整的API请求",
        unifiedGroup);
    promptHelpLabel->setWordWrap(true);
    promptHelpLabel->setStyleSheet(
        "color: #7f8c8d;"
        "font-size: 11px;"
        "padding: 12px;"
        "background-color: #fff3cd;"
        "border-radius: 4px;"
        "margin-top: 5px;"
        "border-left: 4px solid #f39c12;"
    );
    unifiedLayout->addWidget(promptHelpLabel);

    promptLayout->addWidget(unifiedGroup);
    promptLayout->addStretch();
    
    promptContainer->setLayout(promptLayout);
    promptScroll->setWidget(promptContainer);
    
    // ========== 性能配置页 ==========
    QWidget* performanceTab = new QWidget();
    QVBoxLayout* performanceLayout = new QVBoxLayout(performanceTab);
    performanceLayout->setSpacing(15);
    
    QGroupBox* performanceGroup = new QGroupBox("配置 翻译性能配置", performanceTab);
    performanceGroup->setStyleSheet(
        "QGroupBox {"
        "font-size: 13px; font-weight: bold;"
        "border: 2px solid #27ae60;"
        "border-radius: 6px;"
        "margin-top: 10px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 10px;"
        "padding: 0 5px 0 5px;"
        "color: #27ae60;"
        "}"
    );
    QVBoxLayout* performanceForm = new QVBoxLayout(performanceGroup);
    performanceForm->setSpacing(15);
    
    // 批处理大小
    QHBoxLayout* batchSizeLayout = new QHBoxLayout();
    QLabel* batchSizeLabel = new QLabel("每批行数：", performanceGroup);
    batchSizeLabel->setStyleSheet("font-weight: bold; color: #2c3e50; font-size: 12px; min-width: 100px;");
    m_batchSizeEdit = new QSpinBox(performanceGroup);
    m_batchSizeEdit->setRange(5, 50);
    m_batchSizeEdit->setValue(DEFAULT_BATCH_SIZE);
    m_batchSizeEdit->setToolTip("每批翻译的文本行数，推荐20-40，提高吞吐量");
    m_batchSizeEdit->setStyleSheet(
        "QSpinBox {"
        "padding: 6px;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 4px;"
        "font-size: 12px;"
        "}"
        "QSpinBox:focus {"
        "border: 2px solid #27ae60;"
        "}"
    );
    batchSizeLayout->addWidget(batchSizeLabel);
    batchSizeLayout->addWidget(m_batchSizeEdit);
    batchSizeLayout->addStretch();
    QLabel* batchSizeHelp = new QLabel("5-50，推荐32", performanceGroup);
    batchSizeHelp->setStyleSheet("color: #95a5a6; font-size: 11px;");
    batchSizeLayout->addWidget(batchSizeHelp);
    performanceForm->addLayout(batchSizeLayout);
    
    // 并发请求
    QHBoxLayout* concurrentLayout = new QHBoxLayout();
    QLabel* concurrentLabel = new QLabel("并发请求数：", performanceGroup);
    concurrentLabel->setStyleSheet("font-weight: bold; color: #2c3e50; font-size: 12px; min-width: 100px;");
    m_maxConcurrentEdit = new QSpinBox(performanceGroup);
    m_maxConcurrentEdit->setRange(1, 50);
    m_maxConcurrentEdit->setValue(DEFAULT_MAX_CONCURRENT);
    m_maxConcurrentEdit->setToolTip("同时进行的API请求数量，推荐8-16，提升翻译速度");
    m_maxConcurrentEdit->setStyleSheet(
        "QSpinBox {"
        "padding: 6px;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 4px;"
        "font-size: 12px;"
        "}"
        "QSpinBox:focus {"
        "border: 2px solid #27ae60;"
        "}"
    );
    concurrentLayout->addWidget(concurrentLabel);
    concurrentLayout->addWidget(m_maxConcurrentEdit);
    concurrentLayout->addStretch();
    QLabel* concurrentHelp = new QLabel("1-50，推荐12", performanceGroup);
    concurrentHelp->setStyleSheet("color: #95a5a6; font-size: 11px;");
    concurrentLayout->addWidget(concurrentHelp);
    performanceForm->addLayout(concurrentLayout);
    
    // 重试次数
    QHBoxLayout* retriesLayout = new QHBoxLayout();
    QLabel* retriesLabel = new QLabel("重试次数：", performanceGroup);
    retriesLabel->setStyleSheet("font-weight: bold; color: #2c3e50; font-size: 12px; min-width: 100px;");
    m_maxRetriesEdit = new QSpinBox(performanceGroup);
    m_maxRetriesEdit->setRange(0, 10);
    m_maxRetriesEdit->setValue(DEFAULT_MAX_RETRIES);
    m_maxRetriesEdit->setToolTip("API请求失败时的重试次数，推荐2-3次，减少延迟");
    m_maxRetriesEdit->setStyleSheet(
        "QSpinBox {"
        "padding: 6px;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 4px;"
        "font-size: 12px;"
        "}"
        "QSpinBox:focus {"
        "border: 2px solid #27ae60;"
        "}"
    );
    retriesLayout->addWidget(retriesLabel);
    retriesLayout->addWidget(m_maxRetriesEdit);
    retriesLayout->addStretch();
    QLabel* retriesHelp = new QLabel("0-10，推荐3", performanceGroup);
    retriesHelp->setStyleSheet("color: #95a5a6; font-size: 11px;");
    retriesLayout->addWidget(retriesHelp);
    performanceForm->addLayout(retriesLayout);
    
    // 智能频率控制说明 - 使用富文本格式，使图标部分使用FontAwesome字体
    QString rateFontFamily = FA::solidFontFamily();
    QString rateControlText;
    if (rateFontFamily.isEmpty()) {
        // 如果字体加载失败，使用纯文本
        rateControlText = QString("%1 智能频率控制（自动启用）\n"
            "● 自适应请求间隔 (50-2000ms)\n"
            "● 连续错误保护机制\n"
            "● 根据成功率动态调速\n"
            "● 响应时间自适应调整并发").arg(QString(FA::QCheck));
    } else {
        // 如果字体加载成功，使用富文本格式
        rateControlText = QString("<span style=\"font-family:'%1'; font-size:12px; font-weight: bold;\">%2</span> 智能频率控制（自动启用）<br>"
            "● 自适应请求间隔 (50-2000ms)<br>"
            "● 连续错误保护机制<br>"
            "● 根据成功率动态调速<br>"
            "● 响应时间自适应调整并发").arg(rateFontFamily).arg(QString(FA::QCheck));
    }
    QLabel* rateControlLabel = new QLabel(rateControlText, performanceGroup);
    rateControlLabel->setTextFormat(Qt::RichText);
    rateControlLabel->setWordWrap(true);
    rateControlLabel->setStyleSheet(
        "color: #27ae60; font-size: 12px; padding: 12px; "
        "background-color: #d4edda; border-radius: 6px; margin-top: 10px;"
        "font-weight: bold;"
    );
    performanceForm->addWidget(rateControlLabel);
    
    performanceLayout->addWidget(performanceGroup);
    performanceLayout->addStretch();
    
    // ========== 添加标签页 ==========
    m_tabWidget->addTab(apiTab, QString("%1 API配置").arg(QString(FA::QKey)));
    m_tabWidget->addTab(promptScroll, QString("%1 提示词配置").arg(QString(FA::QPen)));
    m_tabWidget->addTab(performanceTab, QString("%1 性能配置").arg(QString(FA::QCog)));

    // 设置标签页样式，显式指定 FontAwesome 字体
    QString tabFontFamily = FA::solidFontFamily();
    QString tabWidgetStyle;
    if (tabFontFamily.isEmpty()) {
        // 如果字体加载失败，使用默认样式
        tabWidgetStyle =
            "QTabWidget::pane {"
            "border: 1px solid #bdc3c7;"
            "border-radius: 6px;"
            "}"
            "QTabBar::tab {"
            "background: #ecf0f1;"
            "padding: 10px 20px;"
            "margin-right: 2px;"
            "border-top-left-radius: 4px;"
            "border-top-right-radius: 4px;"
            "font-size: 12px;"
            "}";
    } else {
        // 如果字体加载成功，显式指定字体族
        tabWidgetStyle = QString(
            "QTabWidget::pane {"
            "border: 1px solid #bdc3c7;"
            "border-radius: 6px;"
            "}"
            "QTabBar::tab {"
            "background: #ecf0f1;"
            "padding: 10px 20px;"
            "margin-right: 2px;"
            "border-top-left-radius: 4px;"
            "border-top-right-radius: 4px;"
            "font-size: 12px;"
            "font-family: '%1';"
            "}"
            "QTabBar::tab:selected {"
            "background: #3498db;"
            "color: white;"
            "font-weight: bold;"
            "}"
        ).arg(tabFontFamily);
    }
    m_tabWidget->setStyleSheet(tabWidgetStyle);

    mainLayout->addWidget(m_tabWidget);
    
    // ========== 恢复默认按钮 ==========
    QHBoxLayout* restoreLayout = new QHBoxLayout();
    restoreLayout->addStretch();
    m_restoreDefaultsButton = new QPushButton("🔄 恢复所有默认设置", this);
    m_restoreDefaultsButton->setStyleSheet(
        "QPushButton {"
        "background-color: #95a5a6;"
        "color: white;"
        "border: none;"
        "border-radius: 4px;"
        "padding: 8px 20px;"
        "font-weight: bold;"
        "font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "background-color: #7f8c8d;"
        "}"
    );
    restoreLayout->addWidget(m_restoreDefaultsButton);
    mainLayout->addLayout(restoreLayout);
    
    // ========== 对话框按钮 ==========
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox->setStyleSheet(
        "QPushButton {"
        "padding: 8px 24px;"
        "font-size: 12px;"
        "font-weight: bold;"
        "border-radius: 4px;"
        "}"
        "QPushButton[okButton=\"true\"] {"
        "background-color: #3498db;"
        "color: white;"
        "}"
        "QPushButton[okButton=\"true\"]:hover {"
        "background-color: #2980b9;"
        "}"
        "QPushButton[cancelButton=\"true\"] {"
        "background-color: #e74c3c;"
        "color: white;"
        "}"
        "QPushButton[cancelButton=\"true\"]:hover {"
        "background-color: #c0392b;"
        "}"
    );
    mainLayout->addWidget(m_buttonBox);
    
    // ========== 连接信号 ==========
    setupPromptTemplates();
    connect(m_testButton, &QPushButton::clicked, this, &TranslateConfigDialog::onTestConnection);
    connect(m_restoreDefaultsButton, &QPushButton::clicked, this, &TranslateConfigDialog::onRestoreDefaults);
    connect(m_unifiedPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TranslateConfigDialog::onUnifiedPromptPresetChanged);
    connect(applyPresetButton, &QPushButton::clicked, this, &TranslateConfigDialog::onApplyUnifiedPreset);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void TranslateConfigDialog::onUnifiedPromptPresetChanged(int index)
{
    QList<UnifiedPromptTemplate> presets = getUnifiedPromptPresets();
    if (index >= 0 && index < presets.size()) {
        m_unifiedPromptDesc->setText(presets[index].description);
    }
}

void TranslateConfigDialog::onApplyUnifiedPreset()
{
    int index = m_unifiedPresetCombo->currentIndex();
    QList<UnifiedPromptTemplate> presets = getUnifiedPromptPresets();

    if (index >= 0 && index < presets.size()) {
        const auto& preset = presets[index];
        m_systemPromptEdit->setPlainText(preset.systemPart);
        m_userPromptEdit->setPlainText(preset.userPart);
        m_apiStatusIcon->setText(QString(FA::QCheck));
    }
}

QString TranslateConfigDialog::getApiKey() const
{
    return m_apiKeyEdit->text().trimmed();
}

QString TranslateConfigDialog::getSystemPrompt() const
{
    return m_systemPromptEdit->toPlainText().trimmed();
}

QString TranslateConfigDialog::getBatchPrompt() const
{
    return m_userPromptEdit->toPlainText().trimmed();
}

void TranslateConfigDialog::setApiKey(const QString& apiKey)
{
    m_apiKeyEdit->setText(apiKey);
}

void TranslateConfigDialog::setSystemPrompt(const QString& prompt)
{
    m_systemPromptEdit->setPlainText(prompt);
}

void TranslateConfigDialog::setBatchPrompt(const QString& prompt)
{
    m_userPromptEdit->setPlainText(prompt);
}

int TranslateConfigDialog::getBatchSize() const
{
    return m_batchSizeEdit->value();
}

int TranslateConfigDialog::getMaxConcurrentRequests() const
{
    return m_maxConcurrentEdit->value();
}

int TranslateConfigDialog::getMaxRetries() const
{
    return m_maxRetriesEdit->value();
}

void TranslateConfigDialog::setBatchSize(int size)
{
    m_batchSizeEdit->setValue(size);
}

void TranslateConfigDialog::setMaxConcurrentRequests(int count)
{
    m_maxConcurrentEdit->setValue(count);
}

void TranslateConfigDialog::setMaxRetries(int retries)
{
    m_maxRetriesEdit->setValue(retries);
}

void TranslateConfigDialog::onRestoreDefaults()
{
    // 恢复默认提示词
    m_systemPromptEdit->setPlainText(DEFAULT_SYSTEM_PROMPT);
    m_userPromptEdit->setPlainText(DEFAULT_USER_PROMPT);

    // 重置性能配置为默认值
    m_batchSizeEdit->setValue(DEFAULT_BATCH_SIZE);
    m_maxConcurrentEdit->setValue(DEFAULT_MAX_CONCURRENT);
    m_maxRetriesEdit->setValue(DEFAULT_MAX_RETRIES);

    // 重置预设选择
    m_unifiedPresetCombo->setCurrentIndex(0);

    // 静默恢复，不显示打扰弹窗
    m_apiStatusIcon->setText(QString(FA::QSync));
}

void TranslateConfigDialog::onTestConnection()
{
    QString apiKey = getApiKey();
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先输入API密钥");
        m_apiStatusIcon->setText(QString(FA::QTimes));
        return;
    }

    m_testButton->setEnabled(false);
    m_testButton->setText("测试中..");
    m_apiStatusIcon->setText(QString(FA::QHourglassHalf));
    
    // 创建网络请求
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    
    QJsonObject requestObj;
    requestObj["model"] = "mimo-v2-flash";
    requestObj["temperature"] = 0.1;
    requestObj["stream"] = false;
    requestObj["max_tokens"] = 100;
    
    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", "You are MiMo, an AI assistant developed by Xiaomi."}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", "请用中文回复：连接成功"}
    });
    requestObj["messages"] = messages;
    
    QNetworkRequest request;
    request.setUrl(QUrl("https://api.xiaomimimo.com/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("api-key", apiKey.toUtf8());
    request.setRawHeader("User-Agent", "GXTStudio-Translator/2.0");
    
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    QNetworkReply* reply = manager->post(request, QJsonDocument(requestObj).toJson());
    
    QTimer* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(30000);
    timeoutTimer->start();
    
    connect(timeoutTimer, &QTimer::timeout, [this, reply, timeoutTimer]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
        timeoutTimer->deleteLater();
    });
    
    connect(reply, &QNetworkReply::finished, [this, reply, timeoutTimer]() {
        m_testButton->setEnabled(true);
        m_testButton->setText("🧪 测试连接");
        
        if (timeoutTimer->isActive()) {
            timeoutTimer->stop();
            timeoutTimer->deleteLater();
        }
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument response = QJsonDocument::fromJson(responseData);
            QJsonObject obj = response.object();
            
            if (obj.contains("error")) {
                QJsonObject errorObj = obj["error"].toObject();
                QString errorMsg = errorObj.value("message").toString("未知错误");
                int errorCode = errorObj.value("code").toInt(-1);
                QMessageBox::warning(this, "测试失败",
                    QString("API返回错误 [%1]: %2").arg(errorCode).arg(errorMsg));
                m_apiStatusIcon->setText("❌");
                reply->deleteLater();
                return;
            }
            
            if (obj.contains("choices") && obj["choices"].isArray()) {
                QJsonArray choices = obj["choices"].toArray();
                if (!choices.isEmpty() && choices[0].isObject()) {
                    QJsonObject choice = choices[0].toObject();
                    if (choice.contains("message") && choice["message"].isObject()) {
                        QJsonObject message = choice["message"].toObject();
                        if (message.contains("content")) {
                            QString content = message["content"].toString();
                            QMessageBox::information(this, QString("%1 测试成功").arg(QString(FA::QCheck)),
                                QString("API连接成功！\n\n响应内容：%1").arg(content));
                            m_apiStatusIcon->setText(QString(FA::QCheck));
                            reply->deleteLater();
                            return;
                        }
                    }
                }
            }
            
            QMessageBox::warning(this, "测试失败",
                QString("API响应格式异常。\n\n原始响应: %1").arg(QString::fromUtf8(responseData)));
            m_apiStatusIcon->setText(QString(FA::QExclamationTriangle));
        } else {
            QString errorStr = reply->errorString();
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            
            QByteArray errorData = reply->readAll();
            if (!errorData.isEmpty()) {
                QJsonDocument errorDoc = QJsonDocument::fromJson(errorData);
                if (errorDoc.isObject()) {
                    QJsonObject errorObj = errorDoc.object();
                    if (errorObj.contains("error")) {
                        QJsonObject errorDetail = errorObj["error"].toObject();
                        QString errorMsg = errorDetail.value("message").toString(errorStr);
                        int errorCode = errorDetail.value("code").toInt(-1);
                        
                        if (errorCode != -1) {
                            errorStr = QString("[%1] %2").arg(errorCode).arg(errorMsg);
                        } else {
                            errorStr = errorMsg;
                        }
                    }
                }
            }
            
            if (statusCode == 401) {
                errorStr = "API密钥无效或已过期";
            } else if (statusCode == 429) {
                errorStr = "API调用频率超限，请稍后再试";
            } else if (statusCode == 400) {
                errorStr = "请求参数错误: " + errorStr;
            } else if (statusCode >= 500) {
                errorStr = "服务器内部错误 " + errorStr;
            } else if (statusCode >= 400) {
                errorStr = QString("客户端错误 %1").arg(errorStr);
            } else if (reply->error() == QNetworkReply::OperationCanceledError) {
                errorStr = "请求超时（30秒）";
            }
            
            QMessageBox::critical(this, QString("%1 测试失败").arg(QString(FA::QTimes)),
                QString("API连接失败！\n\n状态码: %1\n错误信息: %2").arg(statusCode).arg(errorStr));
            m_apiStatusIcon->setText(QString(FA::QTimes));
        }
        
        reply->deleteLater();
    });
}

void TranslateConfigDialog::loadSettings()
{
    QSettings settings;
    settings.beginGroup("Translate");

    QString apiKey = settings.value("apiKey", "").toString();
    m_apiKeyEdit->setText(apiKey);

    // 加载提示词 - 如果存储为空，使用默认值
    QString systemPrompt = settings.value("systemPrompt", "").toString();
    QString userPrompt = settings.value("batchPrompt", "").toString();

    if (systemPrompt.isEmpty()) {
        m_systemPromptEdit->setPlainText(DEFAULT_SYSTEM_PROMPT);
    } else {
        m_systemPromptEdit->setPlainText(systemPrompt);
    }

    if (userPrompt.isEmpty()) {
        m_userPromptEdit->setPlainText(DEFAULT_USER_PROMPT);
    } else {
        m_userPromptEdit->setPlainText(userPrompt);
    }

    // 加载性能配置
    m_batchSizeEdit->setValue(settings.value("batchSize", DEFAULT_BATCH_SIZE).toInt());
    m_maxConcurrentEdit->setValue(settings.value("maxConcurrent", DEFAULT_MAX_CONCURRENT).toInt());
    m_maxRetriesEdit->setValue(settings.value("maxRetries", DEFAULT_MAX_RETRIES).toInt());

    settings.endGroup();

    // 如果API密钥不为空，显示成功图标
    if (!apiKey.isEmpty()) {
        m_apiStatusIcon->setText(QString(FA::QCheck));
    }
}

void TranslateConfigDialog::saveSettings()
{
    QSettings settings;
    settings.beginGroup("Translate");
    
    settings.setValue("apiKey", getApiKey());
    settings.setValue("systemPrompt", getSystemPrompt());
    settings.setValue("batchPrompt", getBatchPrompt());
    
    settings.setValue("batchSize", getBatchSize());
    settings.setValue("maxConcurrent", getMaxConcurrentRequests());
    settings.setValue("maxRetries", getMaxRetries());
    
    settings.endGroup();
}

void TranslateConfigDialog::accept()
{
    if (getApiKey().isEmpty()) {
        QMessageBox::warning(this, "警告", "API密钥不能为空");
        return;
    }
    
    saveSettings();
    QDialog::accept();
}
