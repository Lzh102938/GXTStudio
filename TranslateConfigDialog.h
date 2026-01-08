#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QScrollArea>
#include <QtCore/QSettings>

// Font Awesome 图标 - 使用 FA 命名空间中的图标
#include "GXTStudio.h"

class TranslateConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TranslateConfigDialog(QWidget *parent = nullptr);
    ~TranslateConfigDialog();

    QString getApiKey() const;
    QString getSystemPrompt() const;
    QString getBatchPrompt() const;
    
    // 性能配置
    int getBatchSize() const;
    int getMaxConcurrentRequests() const;
    int getMaxRetries() const;
    
    void setApiKey(const QString& apiKey);
    void setSystemPrompt(const QString& prompt);
    void setBatchPrompt(const QString& prompt);
    void setBatchSize(int size);
    void setMaxConcurrentRequests(int count);
    void setMaxRetries(int retries);

protected slots:
    void accept() override;

private slots:
    void onRestoreDefaults();
    void onTestConnection();
    void onUnifiedPromptPresetChanged(int index);
    void onApplyUnifiedPreset();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void setupPromptTemplates();
    
    // 统一提示词预设管理
    struct UnifiedPromptTemplate {
        QString name;           // 模板名称
        QString description;    // 模板描述
        QString systemPart;     // 系统部分（role=system）
        QString userPart;        // 用户部分（role=user）
    };

    // 获取统一预设模板列表
    QList<UnifiedPromptTemplate> getUnifiedPromptPresets() const;
    
    QTabWidget* m_tabWidget;
    
    // API配置
    QLineEdit* m_apiKeyEdit;
    QPushButton* m_testButton;
    QLabel* m_apiStatusIcon;
    
    // 统一提示词配置
    QComboBox* m_unifiedPresetCombo;
    QTextEdit* m_systemPromptEdit;
    QTextEdit* m_userPromptEdit;
    QLabel* m_unifiedPromptDesc;
    
    // 性能配置
    QSpinBox* m_batchSizeEdit;
    QSpinBox* m_maxConcurrentEdit;
    QSpinBox* m_maxRetriesEdit;
    
    // 按钮
    QPushButton* m_restoreDefaultsButton;
    QDialogButtonBox* m_buttonBox;
    
public:
    // 默认配置
    static const QString DEFAULT_SYSTEM_PROMPT;
    static const QString DEFAULT_USER_PROMPT;
    static const int DEFAULT_BATCH_SIZE;
    static const int DEFAULT_MAX_CONCURRENT;
    static const int DEFAULT_MAX_RETRIES;
    static const int DEFAULT_REQUEST_TIMEOUT;

private:
};
