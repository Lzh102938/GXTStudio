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
    
    struct UnifiedPromptTemplate {
        QString name;
        QString description;
        QString systemPart;
        QString userPart;
    };
    
    QTabWidget* m_tabWidget;
    
    QLineEdit* m_apiKeyEdit;
    QPushButton* m_testButton;
    QLabel* m_apiStatusIcon;
    
    QComboBox* m_unifiedPresetCombo;
    QTextEdit* m_systemPromptEdit;
    QTextEdit* m_userPromptEdit;
    QLabel* m_unifiedPromptDesc;
    
    QSpinBox* m_batchSizeEdit;
    QSpinBox* m_maxConcurrentEdit;
    QSpinBox* m_maxRetriesEdit;
    
    QPushButton* m_restoreDefaultsButton;
    
public:
    static const QString DEFAULT_SYSTEM_PROMPT;
    static const QString DEFAULT_USER_PROMPT;
    static const int DEFAULT_BATCH_SIZE;
    static const int DEFAULT_MAX_CONCURRENT;
    static const int DEFAULT_MAX_RETRIES;
    static const int DEFAULT_REQUEST_TIMEOUT;
    
private:
    static const QList<UnifiedPromptTemplate> s_presets;
    static const QString s_mainStyle;
};
