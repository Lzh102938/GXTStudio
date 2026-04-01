#include "TranslateConfigDialog.h"
#include "AppConfig.h"
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QTimer>
#include <QtWidgets/QLabel>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

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

const int TranslateConfigDialog::DEFAULT_BATCH_SIZE = 32;
const int TranslateConfigDialog::DEFAULT_MAX_CONCURRENT = 12;
const int TranslateConfigDialog::DEFAULT_MAX_RETRIES = 3;
const int TranslateConfigDialog::DEFAULT_REQUEST_TIMEOUT = 180;
const QString TranslateConfigDialog::DEFAULT_MODEL = "mimo-v2-flash";
const QString TranslateConfigDialog::DEFAULT_PROVIDER = "xiaomi";

const QList<TranslateConfigDialog::ProviderInfo> TranslateConfigDialog::s_providers = {
    { "xiaomi", "小米 MiMo", 
      "https://api.xiaomimimo.com/v1/chat/completions",
      "api-key", "", "https://platform.xiaomimimo.com" },
    { "deepseek", "DeepSeek", 
      "https://api.deepseek.com/chat/completions",
      "Authorization", "Bearer ", "https://platform.deepseek.com" }
};

const QList<TranslateConfigDialog::ModelInfo> TranslateConfigDialog::s_models = {
    { "mimo-v2-flash", "MiMo Flash (快速)", 
      "快速响应，适合大批量翻译，性价比高", 65536, 0.3, "xiaomi" },
    { "mimo-v2-pro", "MiMo Pro (专业)", 
      "专业版本，更强的理解和推理能力，翻译质量更高", 131072, 1.0, "xiaomi" },
    { "mimo-v2-omni", "MiMo Omni (全能)", 
      "全能版本，平衡速度与质量，支持多模态", 32768, 1.0, "xiaomi" },
    { "deepseek-chat", "DeepSeek Chat", 
      "DeepSeek-V3 非思考模式，适合日常翻译任务", 64000, 0.3, "deepseek" },
    { "deepseek-reasoner", "DeepSeek Reasoner", 
      "DeepSeek-V3 思考模式，更强的推理能力，翻译质量更高", 64000, 0.3, "deepseek" }
};

const QList<TranslateConfigDialog::UnifiedPromptTemplate> TranslateConfigDialog::s_presets = {
    { "默认（专业游戏翻译）", "适合GTA系列游戏的专业翻译，保持游戏风格和语气", DEFAULT_SYSTEM_PROMPT, DEFAULT_USER_PROMPT },
    { "快速翻译", "简化要求，快速翻译，适合大量文本的批量处理",
        "你是一个游戏文本翻译助手。请将以下文本翻译成中文。\n要求：\n1. 翻译简洁明了，适合游戏界面显示\n2. 保留颜色代码标记（如~g~、~y~等）\n3. 保持原有的格式\n4. 避免过于冗长的表达",
        "请将以下游戏文本逐条翻译成JSON格式：\n\n输入数据：\n{contents}\n\n输出格式：{\"result\": [{\"index\": 0, \"text\": \"翻译\"}, {\"index\": 1, \"text\": \"翻译\"}]}\n\n只输出JSON，不要任何其他文字。" },
    { "严格格式", "强调JSON格式的严格性，减少解析错误",
        "你是一个专业的游戏文本翻译专家，擅长准确翻译GTA系列游戏内容。\n翻译要求：\n1. 保持游戏文本的风格和语气，符合GTA系列的游戏氛围\n2. 保留原有的格式和标记，包括~g~、~y~、~r~、~b~、~w~等颜色代码\n3. 文本要简洁明了，适合游戏界面显示\n4. 必须严格按照JSON格式返回翻译结果",
        "请将以下游戏文本逐条翻译成中文。\n\n输入数据：\n{contents}\n\n【最严格要求】必须严格按照JSON格式返回： {\"result\": [{\"index\": 0, \"text\": \"翻译\"}, {\"index\": 1, \"text\": \"翻译\"}]}\n\n规则：\n1. 只返回纯JSON，绝对不要添加任何开场白、结束语、解释或注释\n2. index必须与输入行号对应\n3. text字段包含翻译内容\n4. 保留所有格式标记\n5. 确保JSON格式正确，可以被解析\n\n【再次强调】直接输出JSON，不要说任何多余的话" },
    { "详细翻译", "提供更详细的翻译说明，适合需要更准确翻译的场景",
        "你是一个专业的游戏文本翻译专家。请将以下文本翻译成中文。\n要求：\n1. 准确理解游戏语境，提供最贴切的翻译\n2. 保持游戏文本的风格和语气\n3. 对于专有名词，使用通用的中文游戏术语\n4. 保留所有格式标记和颜色代码\n5. 对话文本保持口语化，UI文本保持清晰\n6. 确保翻译准确，符合游戏世界观\n7. 按照JSON格式返回翻译结果",
        "请将以下游戏文本逐条翻译，严格按照JSON格式返回： {\"result\": [{\"index\": 0, \"text\": \"翻译\"}, {\"index\": 1, \"text\": \"翻译\"}]}\n\n注意：只输出JSON，不要其他文字" },
    { "自定义", "选择此选项后，请在下方编辑框中自定义系统提示词和用户提示词", "", "" }
};

const QString TranslateConfigDialog::s_mainStyle = R"(
    QGroupBox {
        font-weight: bold;
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        margin-top: 10px;
        padding-top: 10px;
        background: #fafafa;
    }
    QGroupBox::title {
        subcontrol-origin: margin;
        left: 12px;
        padding: 0 6px;
        color: #333;
    }
    QTabWidget::pane {
        border: 1px solid #e0e0e0;
        background-color: #fff;
        border-radius: 6px;
    }
    QTabBar::tab {
        background: rgba(255, 255, 255, 0.45);
        padding: 10px 24px;
        margin-right: 3px;
        border: 1px solid rgba(200, 210, 220, 0.3);
        border-bottom: none;
        border-top-left-radius: 10px;
        border-top-right-radius: 10px;
        color: rgba(80, 90, 100, 0.75);
    }
    QTabBar::tab:selected {
        background: rgba(255, 255, 255, 0.85);
        border: 1px solid rgba(180, 200, 220, 0.5);
        border-bottom: none;
        font-weight: 550;
        color: #1976d2;
    }
    QTabBar::tab:hover:!selected {
        background: rgba(255, 255, 255, 0.7);
        border: 1px solid rgba(180, 200, 220, 0.4);
        border-bottom: none;
        color: rgba(66, 165, 245, 0.9);
    }
    QLineEdit {
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        padding: 8px 12px;
        background: #fff;
        selection-background-color: #409eff;
    }
    QLineEdit:focus {
        border: 2px solid #409eff;
        padding: 7px 11px;
    }
    QLineEdit:hover {
        border: 1px solid #c0c0c0;
    }
    QComboBox {
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        padding: 4px 8px;
        background: #fff;
        min-height: 18px;
        font-size: 13px;
    }
    QComboBox:hover {
        border: 1px solid #c0c0c0;
        background: #fafafa;
    }
    QComboBox:focus {
        border: 2px solid #409eff;
        padding: 3px 7px;
    }
    QComboBox::drop-down {
        border: none;
        width: 20px;
        subcontrol-position: center right;
        subcontrol-origin: padding;
    }
    QComboBox::down-arrow {
        width: 12px;
        height: 12px;
    }
    QComboBox QAbstractItemView {
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        background: #fff;
        selection-background-color: #f0f7ff;
        selection-color: #333;
        padding: 2px;
        outline: none;
        font-size: 13px;
    }
    QComboBox QAbstractItemView::item {
        border-radius: 4px;
        padding: 6px 10px;
        min-height: 20px;
    }
    QComboBox QAbstractItemView::item:hover {
        background: #f5f5f5;
    }
    QComboBox QAbstractItemView::item:selected {
        background: #e6f4ff;
        color: #409eff;
    }
    QSpinBox {
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        padding: 4px 8px;
        background: #fff;
        font-size: 13px;
    }
    QSpinBox:hover {
        border: 1px solid #c0c0c0;
    }
    QSpinBox:focus {
        border: 2px solid #409eff;
        padding: 3px 7px;
    }
    QSpinBox::up-button, QSpinBox::down-button {
        border: none;
        background: transparent;
        width: 18px;
    }
    QSpinBox::up-button:hover, QSpinBox::down-button:hover {
        background: #f0f0f0;
        border-radius: 3px;
    }
    QTextEdit {
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        padding: 8px;
        background: #fff;
    }
    QTextEdit:focus {
        border: 2px solid #409eff;
        padding: 7px;
    }
    QPushButton {
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        padding: 8px 20px;
        background: #fff;
        color: #333;
    }
    QPushButton:hover {
        background: #f5f5f5;
        border-color: #c0c0c0;
    }
    QPushButton:pressed {
        background: #ebebeb;
    }
    QLabel {
        color: #333;
    }
)";

TranslateConfigDialog::TranslateConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setStyleSheet(s_mainStyle);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    m_tabWidget = new QTabWidget(this);
    
    QWidget* apiTab = new QWidget();
    QVBoxLayout* apiLayout = new QVBoxLayout(apiTab);
    apiLayout->setSpacing(12);
    apiLayout->setContentsMargins(12, 12, 12, 12);
    
    QGroupBox* keyGroup = new QGroupBox("API 配置", apiTab);
    QVBoxLayout* keyGroupLayout = new QVBoxLayout(keyGroup);
    keyGroupLayout->setSpacing(10);
    
    QHBoxLayout* providerLayout = new QHBoxLayout();
    providerLayout->addWidget(new QLabel("服务商:", keyGroup));
    m_providerCombo = new QComboBox(keyGroup);
    for (const auto& provider : s_providers) {
        m_providerCombo->addItem(provider.name, provider.id);
    }
    m_providerCombo->setCurrentIndex(0);
    providerLayout->addWidget(m_providerCombo, 1);
    keyGroupLayout->addLayout(providerLayout);
    
    keyGroupLayout->addWidget(new QLabel("API 密钥:", keyGroup));
    
    QHBoxLayout* apiKeyInputLayout = new QHBoxLayout();
    m_apiKeyEdit = new QLineEdit(keyGroup);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("请输入API密钥");
    apiKeyInputLayout->addWidget(m_apiKeyEdit, 1);
    
    m_apiStatusIcon = new QLabel("", keyGroup);
    m_apiStatusIcon->setFixedWidth(20);
    apiKeyInputLayout->addWidget(m_apiStatusIcon);
    keyGroupLayout->addLayout(apiKeyInputLayout);
    
    QHBoxLayout* modelLayout = new QHBoxLayout();
    modelLayout->addWidget(new QLabel("模型选择:", keyGroup));
    m_modelCombo = new QComboBox(keyGroup);
    modelLayout->addWidget(m_modelCombo, 1);
    keyGroupLayout->addLayout(modelLayout);
    
    m_modelDescLabel = new QLabel("", keyGroup);
    m_modelDescLabel->setWordWrap(true);
    m_modelDescLabel->setStyleSheet("color: #666; font-size: 11px; padding: 6px; background: #f5f5f5; border-radius: 3px;");
    keyGroupLayout->addWidget(m_modelDescLabel);
    
    updateModelCombo();
    
    QHBoxLayout* testLayout = new QHBoxLayout();
    testLayout->addStretch();
    m_testButton = new QPushButton("测试连接", keyGroup);
    m_testButton->setFixedWidth(100);
    testLayout->addWidget(m_testButton);
    keyGroupLayout->addLayout(testLayout);
    
    m_providerHelpLabel = new QLabel("", keyGroup);
    m_providerHelpLabel->setOpenExternalLinks(true);
    m_providerHelpLabel->setStyleSheet("color: #666; font-size: 11px;");
    keyGroupLayout->addWidget(m_providerHelpLabel);
    
    apiLayout->addWidget(keyGroup);
    apiLayout->addStretch();
    
    QWidget* promptTab = new QWidget();
    QVBoxLayout* promptTabLayout = new QVBoxLayout(promptTab);
    promptTabLayout->setContentsMargins(0, 0, 0, 0);
    
    QScrollArea* promptScrollArea = new QScrollArea(promptTab);
    promptScrollArea->setWidgetResizable(true);
    promptScrollArea->setFrameShape(QFrame::NoFrame);
    promptScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    QWidget* promptScrollContent = new QWidget();
    QVBoxLayout* promptScrollLayout = new QVBoxLayout(promptScrollContent);
    promptScrollLayout->setContentsMargins(12, 12, 12, 12);
    
    QGroupBox* unifiedGroup = new QGroupBox("提示词配置", promptScrollContent);
    QVBoxLayout* unifiedLayout = new QVBoxLayout(unifiedGroup);
    unifiedLayout->setSpacing(10);

    QHBoxLayout* presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel("预设模板:", unifiedGroup));
    m_unifiedPresetCombo = new QComboBox(unifiedGroup);
    presetLayout->addWidget(m_unifiedPresetCombo, 1);
    
    QPushButton* applyPresetButton = new QPushButton("应用", unifiedGroup);
    applyPresetButton->setMinimumWidth(60);
    presetLayout->addWidget(applyPresetButton);
    unifiedLayout->addLayout(presetLayout);

    m_unifiedPromptDesc = new QLabel(unifiedGroup);
    m_unifiedPromptDesc->setWordWrap(true);
    m_unifiedPromptDesc->setStyleSheet("color: #666; font-size: 11px; padding: 6px; background: #f5f5f5; border-radius: 3px;");
    unifiedLayout->addWidget(m_unifiedPromptDesc);

    unifiedLayout->addWidget(new QLabel("系统提示词:", unifiedGroup));
    m_systemPromptEdit = new QTextEdit(unifiedGroup);
    m_systemPromptEdit->setMinimumHeight(100);
    m_systemPromptEdit->setAcceptRichText(false);
    m_systemPromptEdit->setPlaceholderText("定义AI的角色和翻译风格...");
    unifiedLayout->addWidget(m_systemPromptEdit);

    unifiedLayout->addWidget(new QLabel("用户提示词:", unifiedGroup));
    m_userPromptEdit = new QTextEdit(unifiedGroup);
    m_userPromptEdit->setMinimumHeight(120);
    m_userPromptEdit->setAcceptRichText(false);
    m_userPromptEdit->setPlaceholderText("定义翻译任务的格式和要求...\n使用 {contents} 作为待翻译文本的占位符");
    unifiedLayout->addWidget(m_userPromptEdit);

    QLabel* promptHelpLabel = new QLabel(
        "系统提示词定义AI角色，用户提示词定义翻译任务（必须包含{contents}占位符）", unifiedGroup);
    promptHelpLabel->setWordWrap(true);
    promptHelpLabel->setStyleSheet("color: #888; font-size: 10px;");
    unifiedLayout->addWidget(promptHelpLabel);

    promptScrollLayout->addWidget(unifiedGroup);
    promptScrollLayout->addStretch();
    
    promptScrollArea->setWidget(promptScrollContent);
    promptTabLayout->addWidget(promptScrollArea);
    
    QWidget* performanceTab = new QWidget();
    QVBoxLayout* performanceLayout = new QVBoxLayout(performanceTab);
    performanceLayout->setSpacing(12);
    performanceLayout->setContentsMargins(12, 12, 12, 12);
    
    QGroupBox* performanceGroup = new QGroupBox("性能配置", performanceTab);
    QFormLayout* performanceForm = new QFormLayout(performanceGroup);
    performanceForm->setSpacing(12);
    
    m_batchSizeEdit = new QSpinBox(performanceGroup);
    m_batchSizeEdit->setRange(5, 50);
    m_batchSizeEdit->setValue(DEFAULT_BATCH_SIZE);
    m_batchSizeEdit->setToolTip("每批翻译的文本行数，推荐32");
    performanceForm->addRow("每批行数:", m_batchSizeEdit);
    
    m_maxConcurrentEdit = new QSpinBox(performanceGroup);
    m_maxConcurrentEdit->setRange(1, 50);
    m_maxConcurrentEdit->setValue(DEFAULT_MAX_CONCURRENT);
    m_maxConcurrentEdit->setToolTip("同时进行的API请求数量，推荐12");
    performanceForm->addRow("并发请求数:", m_maxConcurrentEdit);
    
    m_maxRetriesEdit = new QSpinBox(performanceGroup);
    m_maxRetriesEdit->setRange(0, 10);
    m_maxRetriesEdit->setValue(DEFAULT_MAX_RETRIES);
    m_maxRetriesEdit->setToolTip("API请求失败时的重试次数，推荐3");
    performanceForm->addRow("重试次数:", m_maxRetriesEdit);
    
    QLabel* rateControlLabel = new QLabel("智能频率控制已启用", performanceGroup);
    rateControlLabel->setStyleSheet("color: #666; font-size: 11px; padding: 8px; background: #f0f0f0; border-radius: 3px;");
    performanceForm->addRow("", rateControlLabel);
    
    performanceLayout->addWidget(performanceGroup);
    performanceLayout->addStretch();
    
    m_tabWidget->addTab(apiTab, "API 配置");
    m_tabWidget->addTab(promptTab, "提示词配置");
    m_tabWidget->addTab(performanceTab, "性能配置");

    mainLayout->addWidget(m_tabWidget);
    
    QHBoxLayout* restoreLayout = new QHBoxLayout();
    restoreLayout->addStretch();
    m_restoreDefaultsButton = new QPushButton("恢复默认设置", this);
    m_restoreDefaultsButton->setStyleSheet("QPushButton { color: #666; border: none; } QPushButton:hover { color: #333; }");
    restoreLayout->addWidget(m_restoreDefaultsButton);
    mainLayout->addLayout(restoreLayout);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton* cancelButton = new QPushButton("取消", this);
    cancelButton->setFixedWidth(80);
    buttonLayout->addWidget(cancelButton);
    
    QPushButton* okButton = new QPushButton("确定", this);
    okButton->setFixedWidth(80);
    okButton->setDefault(true);
    okButton->setStyleSheet("QPushButton { background-color: #409eff; color: white; border: none; padding: 6px 16px; } QPushButton:hover { background-color: #337ecc; }");
    buttonLayout->addWidget(okButton);
    
    mainLayout->addLayout(buttonLayout);
    
    for (const auto& preset : s_presets) {
        m_unifiedPresetCombo->addItem(preset.name);
    }
    m_unifiedPresetCombo->setCurrentIndex(0);
    m_unifiedPromptDesc->setText(s_presets[0].description);
    
    loadSettings();
    
    setWindowTitle("智能翻译配置");
    setModal(true);
    resize(600, 500);
    
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 22;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
    
    connect(m_testButton, &QPushButton::clicked, this, &TranslateConfigDialog::onTestConnection);
    connect(m_restoreDefaultsButton, &QPushButton::clicked, this, &TranslateConfigDialog::onRestoreDefaults);
    connect(m_unifiedPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TranslateConfigDialog::onUnifiedPromptPresetChanged);
    connect(applyPresetButton, &QPushButton::clicked, this, &TranslateConfigDialog::onApplyUnifiedPreset);
    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TranslateConfigDialog::onProviderChanged);
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        QString currentProvider = m_providerCombo->currentData().toString();
        QString modelId = m_modelCombo->itemData(index).toString();
        for (const auto& model : s_models) {
            if (model.id == modelId && model.providerId == currentProvider) {
                m_modelDescLabel->setText(model.description);
                break;
            }
        }
    });
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(this, &QDialog::finished, [this]() {
        m_apiKeyEdit->setText(getApiKey());
    });
    
    onProviderChanged(m_providerCombo->currentIndex());
}

TranslateConfigDialog::~TranslateConfigDialog()
{
}

void TranslateConfigDialog::onUnifiedPromptPresetChanged(int index)
{
    if (index >= 0 && index < s_presets.size()) {
        m_unifiedPromptDesc->setText(s_presets[index].description);
    }
}

void TranslateConfigDialog::onApplyUnifiedPreset()
{
    int index = m_unifiedPresetCombo->currentIndex();
    if (index >= 0 && index < s_presets.size()) {
        const auto& preset = s_presets[index];
        m_systemPromptEdit->setPlainText(preset.systemPart);
        m_userPromptEdit->setPlainText(preset.userPart);
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
QString TranslateConfigDialog::getModel() const
{
    int index = m_modelCombo->currentIndex();
    if (index >= 0) {
        return m_modelCombo->itemData(index).toString();
    }
    return DEFAULT_MODEL;
}

QString TranslateConfigDialog::getProvider() const
{
    int index = m_providerCombo->currentIndex();
    if (index >= 0) {
        return m_providerCombo->itemData(index).toString();
    }
    return DEFAULT_PROVIDER;
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

void TranslateConfigDialog::setModel(const QString& model)
{
    for (int i = 0; i < m_modelCombo->count(); ++i) {
        if (m_modelCombo->itemData(i).toString() == model) {
            m_modelCombo->setCurrentIndex(i);
            break;
        }
    }
}

void TranslateConfigDialog::setProvider(const QString& provider)
{
    for (int i = 0; i < m_providerCombo->count(); ++i) {
        if (m_providerCombo->itemData(i).toString() == provider) {
            m_providerCombo->setCurrentIndex(i);
            break;
        }
    }
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
    m_systemPromptEdit->setPlainText(DEFAULT_SYSTEM_PROMPT);
    m_userPromptEdit->setPlainText(DEFAULT_USER_PROMPT);
    m_batchSizeEdit->setValue(DEFAULT_BATCH_SIZE);
    m_maxConcurrentEdit->setValue(DEFAULT_MAX_CONCURRENT);
    m_maxRetriesEdit->setValue(DEFAULT_MAX_RETRIES);
    m_unifiedPresetCombo->setCurrentIndex(0);
    m_providerCombo->setCurrentIndex(0);
    updateModelCombo();
}

void TranslateConfigDialog::onProviderChanged(int index)
{
    if (index < 0 || index >= s_providers.size()) return;
    
    const auto& provider = s_providers[index];
    
    m_providerHelpLabel->setText(
        QString("获取API密钥请访问 <a href=\"%1\">%2官网</a>")
        .arg(provider.helpUrl, provider.name));
    
    updateModelCombo();
}

void TranslateConfigDialog::updateModelCombo()
{
    QString currentProvider = m_providerCombo->currentData().toString();
    QString currentModel = m_modelCombo->currentData().toString();
    
    m_modelCombo->clear();
    
    for (const auto& model : s_models) {
        if (model.providerId == currentProvider) {
            m_modelCombo->addItem(model.name, model.id);
        }
    }
    
    bool modelFound = false;
    for (int i = 0; i < m_modelCombo->count(); ++i) {
        if (m_modelCombo->itemData(i).toString() == currentModel) {
            m_modelCombo->setCurrentIndex(i);
            modelFound = true;
            break;
        }
    }
    
    if (!modelFound && m_modelCombo->count() > 0) {
        m_modelCombo->setCurrentIndex(0);
    }
    
    if (m_modelDescLabel && m_modelCombo->currentIndex() >= 0) {
        QString modelId = m_modelCombo->currentData().toString();
        for (const auto& model : s_models) {
            if (model.id == modelId && model.providerId == currentProvider) {
                m_modelDescLabel->setText(model.description);
                break;
            }
        }
    }
}
void TranslateConfigDialog::onTestConnection()
{
    QString apiKey = getApiKey();
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先输入API密钥");
        return;
    }

    m_testButton->setEnabled(false);
    m_testButton->setText("测试中...");
    
    QString providerId = getProvider();
    QString providerName;
    QString apiUrl;
    QString authHeader;
    QString authPrefix;
    QString helpUrl;
    
    for (const auto& provider : s_providers) {
        if (provider.id == providerId) {
            providerName = provider.name;
            apiUrl = provider.apiUrl;
            authHeader = provider.authHeader;
            authPrefix = provider.authPrefix;
            helpUrl = provider.helpUrl;
            break;
        }
    }
    
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    
    QJsonObject requestObj;
    requestObj["model"] = getModel();
    requestObj["temperature"] = 0.1;
    requestObj["stream"] = false;
    requestObj["max_tokens"] = 100;
    
    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", "You are a helpful assistant."}});
    messages.append(QJsonObject{{"role", "user"}, {"content", "请用中文回复：连接成功"}});
    requestObj["messages"] = messages;
    
    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(authHeader.toUtf8(), (authPrefix + apiKey).toUtf8());
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
    
    connect(reply, &QNetworkReply::finished, [this, reply, timeoutTimer, providerName, helpUrl]() {
        m_testButton->setEnabled(true);
        m_testButton->setText("测试连接");
        
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
                QString errorCode = errorObj.value("code").toString();
                
                QString displayMsg;
                if (errorMsg.contains("余额不足") || errorMsg.contains("insufficient") || 
                    errorMsg.contains("balance") || errorCode == "insufficient_quota") {
                    displayMsg = QString("账户余额不足\n\n请前往 %1 充值后继续使用").arg(helpUrl);
                } else if (errorMsg.contains("API密钥") || errorMsg.contains("api key") || 
                          errorMsg.contains("invalid") || errorCode == "invalid_api_key") {
                    displayMsg = "API密钥无效或已过期\n\n请检查您的API密钥是否正确";
                } else if (errorMsg.contains("rate limit") || errorMsg.contains("频率") || 
                          errorCode == "rate_limit_exceeded") {
                    displayMsg = "API调用频率超限\n\n请稍后重试";
                } else {
                    displayMsg = QString("API返回错误: %1").arg(errorMsg);
                }
                
                QMessageBox::warning(this, "测试失败", displayMsg);
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
                            QMessageBox::information(this, "测试成功", QString("%1 API连接成功！\n响应：%2").arg(providerName, content));
                            reply->deleteLater();
                            return;
                        }
                    }
                }
            }
            QMessageBox::warning(this, "测试失败", "API响应格式异常");
        } else {
            QString errorStr = reply->errorString();
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            
            if (statusCode == 401) errorStr = "API密钥无效或已过期\n\n请检查您的API密钥是否正确";
            else if (statusCode == 402) errorStr = QString("账户余额不足\n\n请前往 %1 充值后继续使用").arg(helpUrl);
            else if (statusCode == 429) errorStr = "API调用频率超限\n\n请稍后重试";
            else if (reply->error() == QNetworkReply::OperationCanceledError) errorStr = "请求超时\n\n请检查网络连接";
            
            QMessageBox::critical(this, "测试失败", QString("连接失败：%1").arg(errorStr));
        }
        reply->deleteLater();
    });
}
void TranslateConfigDialog::loadSettings()
{
    AppConfig& config = AppConfig::instance();

    QString apiKey = config.getTranslateApiKey();
    m_apiKeyEdit->setText(apiKey);

    QString systemPrompt = config.getTranslateSystemPrompt();
    QString userPrompt = config.getTranslateBatchPrompt();

    m_systemPromptEdit->setPlainText(systemPrompt.isEmpty() ? DEFAULT_SYSTEM_PROMPT : systemPrompt);
    m_userPromptEdit->setPlainText(userPrompt.isEmpty() ? DEFAULT_USER_PROMPT : userPrompt);

    m_batchSizeEdit->setValue(config.getTranslateBatchSize());
    m_maxConcurrentEdit->setValue(config.getTranslateMaxConcurrent());
    m_maxRetriesEdit->setValue(config.getTranslateMaxRetries());
    
    QString provider = config.getTranslateProvider();
    setProvider(provider.isEmpty() ? DEFAULT_PROVIDER : provider);
    
    QString model = config.getTranslateModel();
    setModel(model.isEmpty() ? DEFAULT_MODEL : model);
}
void TranslateConfigDialog::saveSettings()
{
    AppConfig& config = AppConfig::instance();
    
    config.setTranslateApiKey(getApiKey());
    config.setTranslateSystemPrompt(getSystemPrompt());
    config.setTranslateBatchPrompt(getBatchPrompt());
    config.setTranslateBatchSize(getBatchSize());
    config.setTranslateMaxConcurrent(getMaxConcurrentRequests());
    config.setTranslateMaxRetries(getMaxRetries());
    config.setTranslateProvider(getProvider());
    config.setTranslateModel(getModel());
    
    config.save();
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
